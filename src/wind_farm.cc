/**
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#include <algorithm>
#include <limits>
#include <numeric>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/mpi/Comm.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/runtime/Log.h"


#include "config_parser.h"
#include "utils.h"
#include "wfp_models/wfp_model.h"
#include "wind_farm.h"



namespace wind_farm_plugin {

WindFarm::WindFarm(const eckit::Configuration& conf) : config_{conf} {
    eckit::LocalConfiguration modelConf = config_.getSubConfiguration("wind_farm_model");
    wfpModel_.reset(WFPModelFactory::instance().build(modelConf.getString("name"), modelConf));
}

WindFarm::~WindFarm() = default;

void WindFarm::setupWindTurbines(atlas::Field lonLatField) {

    // mpi info
    size_t size = eckit::mpi::comm().size();
    size_t rank = eckit::mpi::comm().rank();

    auto lonLatArray = atlas::array::make_view<double, 2>(lonLatField);
    double pointLat;
    double pointLon;

    // min distance
    double minDistanceLocal;

    // nearest local point
    size_t nearestPointID;

    // check the format of the wind turbines configuration and parse it accordingly
    std::string format = config_.getString("config_format", "native");

    auto configParser = ConfigParser::build(format);
    const auto windFarmConfig = configParser->parse(config_);
    const auto& turbines = windFarmConfig.turbines();
    const auto& turbineDefaults = windFarmConfig.turbineDefaults();

    // here we go through the wind turbines in the configuration and append only
    // the ones whose closest grid point falls in the local domain
    size_t wt_id = 0;
    for (const auto& wt_conf : turbines) {

        double wt_lat = wt_conf.getDouble("lat");
        double wt_lon = wt_conf.getDouble("lon");

        // min distance squared
        double minDist = 1e12;

        // Brute-force loop
        for (size_t iPt = 0; iPt < lonLatArray.shape(0); iPt++) {
            pointLon    = lonLatArray(iPt, 0);
            pointLat    = lonLatArray(iPt, 1);
            double dist = earthDistance(wt_lat, wt_lon, pointLat, pointLon);
            if (dist < minDist) {
                nearestPointID = iPt;
                minDist        = dist;
            }
        }

        minDistanceLocal = minDist;

        // Now we need to find the (global) nearest
        std::pair<double, int> minDistRankLocal(minDistanceLocal, rank);
        std::pair<double, int> minDistRankGlob(0.0, 0.0);
        eckit::mpi::comm().allReduce(minDistRankLocal, minDistRankGlob, eckit::mpi::minloc());
        size_t minRankGlob = minDistRankGlob.second;

        // Only the rank with global min distance will allocate the wind turbine
        if (rank == minRankGlob) {

            windTurbines_.push_back(std::make_unique<WindTurbine>(wt_id, wt_conf, nearestPointID, minDistanceLocal,
                                                                  minRankGlob, turbineDefaults));

            Log::debug() << "(" << rank << "/" << size << ") => "
                         << "WT-ID: " << wt_id << ", nearestPointID=" << nearestPointID
                         << ", minRankGlob=" << minRankGlob << std::endl;
        }

        windTurbinesGlobal_.push_back(std::make_unique<WindTurbine>(wt_id, wt_conf, nearestPointID, minDistanceLocal,
                                        minRankGlob, turbineDefaults));

        wt_id++;
    }

    // calculate average lat/lon
    calculateAvgLatLons();

    // calculate the average spacing between turbines in the farm
    setupTurbineSpacing();

    // setup wind farm box points
    setupWindFarmBoxPoints();

    // print summary
    summary();
}


double WindFarm::computePower(const WindMap& wMap) const {
    Log::info() << " ---> computing power.." << std::endl;
    return wfpModel_->computePower(wMap, *this);
}

std::vector<LatLonValue> WindFarm::computePowerByTurbine(const WindMap& wMap) const {
    Log::info() << " ---> computing power by turbine.." << std::endl;
    return wfpModel_->computePowerByTurbine(wMap, *this);
}


std::vector<WindPoint> WindFarm::computeWindBox(const WindMap& wMap) const {
    const std::vector<std::unique_ptr<LatLonPoint>>& BoxPoints = WindFarmBoxPoints();
    return wfpModel_->computeWindAtPoints(wMap, *this, BoxPoints);
}


bool WindFarm::supportsCoupling() const {
    return wfpModel_->supportsCoupling();
}

void WindFarm::initialiseCoupling(const WindMap& wMap) {
    wfpModel_->initialiseCoupling(*this, wMap);
}

void WindFarm::applyCoupling(const WindMap& wMap, plume::data::ModelDataView& modelData) const {
    wfpModel_->applyCoupling(wMap, *this, modelData);
}

const std::string& WindFarm::couplingTargetParam() const {
    return wfpModel_->couplingTargetParam();
}

std::map<size_t, std::vector<const WindTurbine*>> WindFarm::turbinesByGridPoint() const {
    std::map<size_t, std::vector<const WindTurbine*>> byPoint;
    for (const auto& wt : windTurbines_) {  // local only — turbines sharing a point already share a rank
        byPoint[wt->nearestPointID()].push_back(wt.get());
    }
    return byPoint;
}


void WindFarm::setupTurbineSpacing() {

    if (windTurbinesGlobal_.size() < 2) {
        return;  // no array to speak of — turbineSpacing_ stays empty, turbineSpacing() returns nullopt for everyone
    }

    // O(local turbines * global turbines)
    for (const auto& entry : turbinesByGridPoint()) {
        size_t pointID                             = entry.first;
        const std::vector<const WindTurbine*>& wts = entry.second;

        double spacingSum = 0.0;
        for (const auto* wt : wts) {
            double nearestDist = std::numeric_limits<double>::max();
            for (const auto& other : windTurbinesGlobal_) {
                if (other->ID() == wt->ID()) {
                    // wt (from windTurbines_) and other (from windTurbinesGlobal_) are separate WindTurbine
                    // instances for the same logical turbine so pointer identity never matches; ID does.
                    continue;
                }
                double dist = earthDistance(wt->lat(), wt->lon(), other->lat(), other->lon());
                nearestDist = std::min(nearestDist, dist);
            }
            spacingSum += nearestDist;
        }
        turbineSpacing_[pointID] = spacingSum / static_cast<double>(wts.size());
    }
}


std::optional<double> WindFarm::turbineSpacing(size_t pointID) const {
    auto it = turbineSpacing_.find(pointID);
    if (it == turbineSpacing_.end()) {
        return std::nullopt;
    }
    return it->second;
}


void WindFarm::summary() const {

    // mpi info
    size_t size = eckit::mpi::comm().size();
    size_t rank = eckit::mpi::comm().rank();

    if (windTurbines_.empty()) {
        Log::info() << ">>> Rank " << rank << " has no wind turbines" << std::endl;
    }
    else {
        Log::info() << ">>> Rank " << rank << " has " << windTurbines_.size() << " wind turbines" << std::endl;
        for (const auto& wt : windTurbines_) {
            Log::debug() << *wt << std::endl;
        }
    }

    Log::info() << ">>> Rank " << rank << " has " << windTurbinesGlobal_.size() << " global wind turbines" << std::endl;
    for (const auto& wt : windTurbinesGlobal_) {
        Log::debug() << *wt << std::endl;
    }
}


void WindFarm::calculateAvgLatLons() {

    auto sumLat = [](double total, const std::unique_ptr<WindTurbine>& t) { return total + t->lat(); };
    auto sumLon = [](double total, const std::unique_ptr<WindTurbine>& t) { return total + t->lon(); };

    // local
    if (!windTurbines_.empty()) {
        double AvgLat = std::accumulate(windTurbines_.begin(), windTurbines_.end(), 0.0, sumLat) / windTurbines_.size();
        double AvgLon = std::accumulate(windTurbines_.begin(), windTurbines_.end(), 0.0, sumLon) / windTurbines_.size();
        AvgPoint_ = LatLonPoint(AvgLat, AvgLon);
    } else {
        AvgPoint_ = std::nullopt;
    }

    // global
    double AvgLatGlob;
    double AvgLonGlob;
    if (!windTurbinesGlobal_.empty()) {
        AvgLatGlob = std::accumulate(windTurbinesGlobal_.begin(), windTurbinesGlobal_.end(), 0.0, sumLat) /
                     windTurbinesGlobal_.size();
        AvgLonGlob = std::accumulate(windTurbinesGlobal_.begin(), windTurbinesGlobal_.end(), 0.0, sumLon) /
                     windTurbinesGlobal_.size();
    }

    AvgPointGlob_ = LatLonPoint(AvgLatGlob, AvgLonGlob);
}


void WindFarm::setupWindFarmBoxPoints() {

    // check if the configuration has the wind_farm_box section
    if (!config_.has("wind_farm_box")) {
        Log::info() << "No wind_farm_box configuration found" << std::endl;
        return;
    }

    auto box_config = config_.getSubConfiguration("wind_farm_box");

    Log::info() << "Wind Farm Box: " << box_config << std::endl;

    boxBounds_.lonMin = box_config.getDouble("lon_min");
    boxBounds_.latMin = box_config.getDouble("lat_min");
    boxBounds_.lonMax = box_config.getDouble("lon_max");
    boxBounds_.latMax = box_config.getDouble("lat_max");

    const bool hasNLon = box_config.has("n_lon");
    const bool hasNLat = box_config.has("n_lat");

    if (!hasNLon && !hasNLat) {
        return;  // return early: no engineering-model mesh requested, bounds alone suffice
    }
    if (!hasNLon || !hasNLat) {
        // flag incorrectness: a partial pair can't build a mesh either way
        throw eckit::UserError("wind_farm_box: 'n_lon' and 'n_lat' must both be present or both be absent.", Here());
    }

    // carry on with point sampling: both present, build the mesh
    double n_lon = box_config.getInt("n_lon");
    double n_lat = box_config.getInt("n_lat");

    for (size_t ilon = 0; ilon < n_lon; ilon++) {
        for (size_t ilat = 0; ilat < n_lat; ilat++) {
            double lon = boxBounds_.lonMin + ilon * (boxBounds_.lonMax - boxBounds_.lonMin) / (n_lon - 1);
            double lat = boxBounds_.latMin + ilat * (boxBounds_.latMax - boxBounds_.latMin) / (n_lat - 1);
            boxPoints_.push_back(std::make_unique<LatLonPoint>(lat, lon));
        }
    }
}

std::pair<double, double> WindFarm::computeAvgWindSpeed(const WindMap& wMap) const {

    if (windTurbinesGlobal_.empty()) {
        Log::warning() << "No wind turbines found!" << std::endl;
        return std::make_pair(0.0, 0.0);
    }

    size_t size = eckit::mpi::comm().size();
    size_t rank = eckit::mpi::comm().rank();

    double Uavg{0};
    double Vavg{0};

    auto arrayU = wMap.arrayU();
    auto arrayV = wMap.arrayV();

    for (const auto& wt : windTurbines_) {
        double U = arrayU(wt->nearestPointID(), 0);
        double V = arrayV(wt->nearestPointID(), 0);
        Uavg += U;
        Vavg += V;
    }

    double UavgGlob{0};
    double VavgGlob{0};

    eckit::mpi::comm().allReduce(Uavg, UavgGlob, eckit::mpi::sum());
    eckit::mpi::comm().allReduce(Vavg, VavgGlob, eckit::mpi::sum());

    UavgGlob /= windTurbinesGlobal_.size();
    VavgGlob /= windTurbinesGlobal_.size();

    return std::make_pair(UavgGlob, VavgGlob);
}


}  // namespace wind_farm_plugin