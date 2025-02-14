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


#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/mpi/Comm.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/runtime/Log.h"


#include "utils.h"
#include "wfp_models/wfp_model.h"
#include "wind_farm.h"

using atlas::Log;


namespace wind_farm_plugin {


WindFarm::WindFarm(const eckit::Configuration& conf) : config_{conf} {
    std::string modelName = config_.getSubConfiguration("wind_farm_model").getString("name");
    wfpModel_.reset(WFPModelFactory::instance().build(modelName, conf));
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

    // throw exception if the path to wind turbines file is not defined
    if (!config_.has("wind_turbines_filename")) {
        throw eckit::BadParameter("No wind turbines file defined in the configuration", Here());
    }

    // load wind turbines configuration
    eckit::PathName wind_turbines_filename = config_.getString("wind_turbines_filename");
    auto yamlConfig                        = eckit::YAMLConfiguration(wind_turbines_filename);

    wtConfig_ = eckit::LocalConfiguration(yamlConfig);
    auto wts  = wtConfig_.getSubConfigurations("wind_turbines");

    // wind turbine defaults
    auto turbineDefaults = wtConfig_.getSubConfiguration("wind_turbine_defaults");

    // here we go through the wind turbines in the configuration and append only
    // the ones whose closest grid point falls in the local domain
    size_t wt_id = 0;
    for (const auto& wt_conf : wts) {

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

    // setup wind farm box points
    setupWindFarmBoxPoints();

    // print summary
    summary();
}


double WindFarm::computePower(const WindMap& wMap) const {
    Log::info() << " ---> computing power.." << std::endl;
    return wfpModel_->computePower(wMap, *this);
}


std::vector<WindPoint> WindFarm::computeWindBox(const WindMap& wMap) const {
    const std::vector<std::unique_ptr<LatLonPoint>>& BoxPoints = WindFarmBoxPoints();
    return wfpModel_->computeWindAtPoints(wMap, *this, BoxPoints);
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


std::vector<std::unique_ptr<WindTurbine>> WindFarm::gatherLocalWindTurbines() {

    std::vector<std::unique_ptr<WindTurbine>> gatheredWindTurbines;

    // mpi info
    size_t mpi_size = eckit::mpi::comm().size();
    size_t mpi_rank = eckit::mpi::comm().rank();

    // unroll the wind turbines
    std::vector<int> id;
    std::vector<double> lat;
    std::vector<double> lon;
    std::vector<double> hubHeight;
    std::vector<double> radius;
    std::vector<double> Cp;
    std::vector<double> Ct;
    std::vector<double> cutoffMax;
    std::vector<double> cutoffMin;
    std::vector<double> rhoHub;
    std::vector<size_t> nearestPointID;
    std::vector<double> minDistanceLocal;
    std::vector<size_t> minRankGlob;

    for (const auto& wt : windTurbines_) {
        id.push_back(wt->ID());
        lat.push_back(wt->lat());
        lon.push_back(wt->lon());
        hubHeight.push_back(wt->hubHeight());
        radius.push_back(wt->radius());
        Cp.push_back(wt->Cp());
        Ct.push_back(wt->Ct());
        cutoffMax.push_back(wt->cutoffMax());
        cutoffMin.push_back(wt->cutoffMin());
        rhoHub.push_back(wt->rhoHub());
        nearestPointID.push_back(wt->nearestPointID());
        minDistanceLocal.push_back(wt->minDistanceLocal());
        minRankGlob.push_back(wt->minRankGlob());
    }

    // gather all wind turbines
    eckit::mpi::Buffer<int> id_recv(mpi_size);
    eckit::mpi::Buffer<double> lat_recv(mpi_size);
    eckit::mpi::Buffer<double> lon_recv(mpi_size);
    eckit::mpi::Buffer<double> hubHeight_recv(mpi_size);
    eckit::mpi::Buffer<double> radius_recv(mpi_size);
    eckit::mpi::Buffer<double> Cp_recv(mpi_size);
    eckit::mpi::Buffer<double> Ct_recv(mpi_size);
    eckit::mpi::Buffer<double> cutoffMax_recv(mpi_size);
    eckit::mpi::Buffer<double> cutoffMin_recv(mpi_size);
    eckit::mpi::Buffer<double> rhoHub_recv(mpi_size);
    eckit::mpi::Buffer<size_t> nearestPointID_recv(mpi_size);
    eckit::mpi::Buffer<double> minDistanceLocal_recv(mpi_size);
    eckit::mpi::Buffer<size_t> minRankGlob_recv(mpi_size);

    eckit::mpi::comm().allGatherv(id.begin(), id.end(), id_recv);
    eckit::mpi::comm().allGatherv(lat.begin(), lat.end(), lat_recv);
    eckit::mpi::comm().allGatherv(lon.begin(), lon.end(), lon_recv);
    eckit::mpi::comm().allGatherv(hubHeight.begin(), hubHeight.end(), hubHeight_recv);
    eckit::mpi::comm().allGatherv(radius.begin(), radius.end(), radius_recv);
    eckit::mpi::comm().allGatherv(Cp.begin(), Cp.end(), Cp_recv);
    eckit::mpi::comm().allGatherv(Ct.begin(), Ct.end(), Ct_recv);
    eckit::mpi::comm().allGatherv(cutoffMax.begin(), cutoffMax.end(), cutoffMax_recv);
    eckit::mpi::comm().allGatherv(cutoffMin.begin(), cutoffMin.end(), cutoffMin_recv);
    eckit::mpi::comm().allGatherv(rhoHub.begin(), rhoHub.end(), rhoHub_recv);
    eckit::mpi::comm().allGatherv(nearestPointID.begin(), nearestPointID.end(), nearestPointID_recv);
    eckit::mpi::comm().allGatherv(minDistanceLocal.begin(), minDistanceLocal.end(), minDistanceLocal_recv);
    eckit::mpi::comm().allGatherv(minRankGlob.begin(), minRankGlob.end(), minRankGlob_recv);

    for (size_t iwt = 0; iwt < lat_recv.buffer.size(); iwt++) {
        gatheredWindTurbines.push_back(std::make_unique<WindTurbine>(
            id_recv.buffer[iwt], lat_recv.buffer[iwt], lon_recv.buffer[iwt], hubHeight_recv.buffer[iwt],
            radius_recv.buffer[iwt], Cp_recv.buffer[iwt], Ct_recv.buffer[iwt], cutoffMax_recv.buffer[iwt],
            cutoffMin_recv.buffer[iwt], rhoHub_recv.buffer[iwt], nearestPointID_recv.buffer[iwt],
            minDistanceLocal_recv.buffer[iwt], minRankGlob_recv.buffer[iwt]));
    }

    return gatheredWindTurbines;
}

void WindFarm::calculateAvgLatLons() {

    auto sumLat = [](double total, const std::unique_ptr<WindTurbine>& t) { return total + t->lat(); };
    auto sumLon = [](double total, const std::unique_ptr<WindTurbine>& t) { return total + t->lon(); };

    // local
    double AvgLat;
    double AvgLon;
    if (!windTurbines_.empty()) {
        AvgLat = std::accumulate(windTurbines_.begin(), windTurbines_.end(), 0.0, sumLat) / windTurbines_.size();
        AvgLon = std::accumulate(windTurbines_.begin(), windTurbines_.end(), 0.0, sumLon) / windTurbines_.size();
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

    AvgPoint_     = LatLonPoint(AvgLat, AvgLon);
    AvgPointGlob_ = LatLonPoint(AvgLatGlob, AvgLonGlob);
}


void WindFarm::setupWindFarmBoxPoints() {

    // check if the configuration has the wind_farm_box section
    if (!wtConfig_.has("wind_farm_box")) {
        Log::info() << "No wind_farm_box configuration found" << std::endl;
        return;
    }

    auto box_config = wtConfig_.getSubConfiguration("wind_farm_box");

    Log::info() << "Wind Farm Box: " << box_config << std::endl;

    double lonMin = box_config.getDouble("lon_min");
    double latMin = box_config.getDouble("lat_min");
    double lonMax = box_config.getDouble("lon_max");
    double latMax = box_config.getDouble("lat_max");

    double n_lon = box_config.getInt("n_lon");
    double n_lat = box_config.getInt("n_lat");

    for (size_t ilon = 0; ilon < n_lon; ilon++) {
        for (size_t ilat = 0; ilat < n_lat; ilat++) {
            double lon = lonMin + ilon * (lonMax - lonMin) / (n_lon - 1);
            double lat = latMin + ilat * (latMax - latMin) / (n_lat - 1);
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