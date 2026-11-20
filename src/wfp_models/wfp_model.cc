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

#include "eckit/exception/Exceptions.h"
#include "eckit/mpi/Comm.h"

#include "wfp_model.h"

namespace wind_farm_plugin {


WFPModel::WFPModel(const eckit::Configuration& conf) {
    // do nothing
}


std::vector<WindPoint> WFPModel::computeWindAtTurbines(const WindMap& wMap, const WindFarm& windFarm) const {

    auto arrayU = wMap.arrayU();
    auto arrayV = wMap.arrayV();

    const std::vector<std::unique_ptr<WindTurbine>>& localTurbines = windFarm.windTurbines();

    std::vector<WindPoint> windAtTurbines;
    windAtTurbines.reserve(localTurbines.size());
    for (const auto& wt : localTurbines) {
        const size_t nearestPointID = wt->nearestPointID();
        const double hubU_ms        = arrayU(nearestPointID, 0);
        const double hubV_ms        = arrayV(nearestPointID, 0);
        windAtTurbines.push_back(WindPoint(*wt, hubU_ms, hubV_ms));
    }
    return windAtTurbines;
}


std::vector<WindPoint> WFPModel::computeWindAtPoints(const WindMap& wMap, const WindFarm& windFarm,
                                                     const std::vector<std::unique_ptr<LatLonPoint>>& points) const {

    std::vector<WindPoint> velPoints;
    velPoints.reserve(points.size());  // size known upfront — avoid reallocations as it grows
    auto avgWind = windFarm.computeAvgWindSpeed(wMap);

    // Simple possible implementation:
    // associate the average wind speed to every point
    for (const auto& p : points) {
        velPoints.push_back(WindPoint(*p, avgWind.first, avgWind.second));
    }

    return velPoints;
}


double WFPModel::computePower(const WindMap& wMap, const WindFarm& windFarm) const {

    // Deliberately not routed through computePowerByTurbine(): that one needs a global-turbine-sized vector
    // reduce to report per-turbine values, which this method has no use for. A single scalar sum + scalar
    // reduce gets the same total at a fraction of the communication and allocation cost.
    const std::vector<std::unique_ptr<WindTurbine>>& localTurbines = windFarm.windTurbines();
    std::vector<WindPoint> windAtTurbines                          = computeWindAtTurbines(wMap, windFarm);

    double powerLocal = 0.0;
    for (size_t i_wt = 0; i_wt < windAtTurbines.size(); i_wt++) {
        const WindPoint& wp = windAtTurbines[i_wt];
        powerLocal += localTurbines[i_wt]->computePower(wp.wind_u(), wp.wind_v());
    }

    double powerGlobal = 0.0;
    eckit::mpi::comm().allReduce(powerLocal, powerGlobal, eckit::mpi::sum());

    return powerGlobal;
}


std::vector<LatLonValue> WFPModel::computePowerByTurbine(const WindMap& wMap, const WindFarm& windFarm) const {

    const std::vector<std::unique_ptr<WindTurbine>>& localTurbines = windFarm.windTurbines();
    std::vector<WindPoint> windAtTurbines                          = computeWindAtTurbines(wMap, windFarm);

    const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines = windFarm.windTurbinesGlobal();

    std::vector<double> powersLocal(globalTurbines.size(), 0.0);
    std::vector<double> powersGlobal(globalTurbines.size(), 0.0);

    // local power at each turbine
    for (size_t i_wt = 0; i_wt < windAtTurbines.size(); i_wt++) {
        const WindPoint& wp                    = windAtTurbines[i_wt];
        powersLocal[localTurbines[i_wt]->ID()] = localTurbines[i_wt]->computePower(wp.wind_u(), wp.wind_v());
    }

    // reduce powers across ranks
    eckit::mpi::comm().allReduce(powersLocal, powersGlobal, eckit::mpi::sum());

    // Assemble the result only now that the final values are known: building it before the reduce (as this used
    // to) meant every LatLonValue got default-constructed, then overwritten with a placeholder, then mutated
    // again via setValue() — three passes/constructions per turbine for one final value.
    std::vector<LatLonValue> powersByTurbine(globalTurbines.size());
    for (const auto& wt : globalTurbines) {
        powersByTurbine[wt->ID()] = LatLonValue(wt->lat(), wt->lon(), powersGlobal[wt->ID()]);
    }

    return powersByTurbine;
}


// ---------------------------------------------------------
WFPModelFactory::WFPModelFactory() {}

WFPModelFactory::~WFPModelFactory() = default;

WFPModelFactory& WFPModelFactory::instance() {
    static WFPModelFactory theinstance;
    return theinstance;
}

void WFPModelFactory::enregister(const std::string& name, const WFPModelBuilderBase& builder) {
    std::lock_guard<std::mutex> lock(mutex_);
    ASSERT(builders_.find(name) == builders_.end());
    builders_.emplace(std::make_pair(name, std::ref(builder)));
}

void WFPModelFactory::deregister(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = builders_.find(name);
    ASSERT(it != builders_.end());
    builders_.erase(it);
}

std::vector<std::string> WFPModelFactory::list_registered() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> reg_;
    for (const auto& b : builders_) {
        reg_.push_back(b.first);
    }
    return reg_;
}

WFPModel* WFPModelFactory::build(const std::string& name, const eckit::Configuration& config) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = builders_.find(name);
    if (it == builders_.end()) {
        throw eckit::SeriousBug("Builder not found for backend " + name, Here());
    }

    return it->second.get().make(config);
}

WFPModelBuilderBase::WFPModelBuilderBase(const std::string& name) : name_(name) {
    WFPModelFactory::instance().enregister(name, *this);
}

WFPModelBuilderBase::~WFPModelBuilderBase() {
    WFPModelFactory::instance().deregister(name_);
}


}  // namespace wind_farm_plugin
