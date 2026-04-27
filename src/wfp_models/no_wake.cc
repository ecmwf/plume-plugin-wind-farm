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

#include <vector>
#include <numeric>

#include "eckit/mpi/Comm.h"

#include "no_wake.h"


namespace wind_farm_plugin {


static WFPModelBuilder<NoWake> NoWakeBuilder;


NoWake::NoWake(const eckit::Configuration& conf) : WFPModel{conf} {
    // do nothing
}


double NoWake::computePower(const WindMap& wMap, const WindFarm& windFarm) const {
    std::vector<LatLonValue> turbinePowers = computePowerByTurbine(wMap, windFarm);
    return std::accumulate(turbinePowers.begin(), turbinePowers.end(), 0.0,
                           [](double sum, const LatLonValue& turbinePower) {
                               return sum + turbinePower.value();
                           });
}

std::vector<LatLonValue> NoWake::computePowerByTurbine(const WindMap& wMap,
                                                        const WindFarm& windFarm) const {

    auto arrayU = wMap.arrayU();
    auto arrayV = wMap.arrayV();

    const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines = windFarm.windTurbinesGlobal();
    const size_t rank = eckit::mpi::comm().rank();

    std::vector<double> powersLocal(globalTurbines.size(), 0.0);
    std::vector<double> powersGlobal(globalTurbines.size(), 0.0);
    std::vector<LatLonValue> powersByTurbine(globalTurbines.size());

    for (const auto& wt : globalTurbines) {
        powersByTurbine[wt->ID()] = LatLonValue(wt->lat(), wt->lon(), 0.0);
    }

    for (const auto& wt : globalTurbines) {
        if (wt->minRankGlob() != rank) {
            continue;
        }

        const size_t nearestPointID = wt->nearestPointID();
        const double hubU_ms        = arrayU(nearestPointID, 0);
        const double hubV_ms        = arrayV(nearestPointID, 0);
        powersLocal[wt->ID()]       = wt->computePower(hubU_ms, hubV_ms);
    }

    // reduce powers across ranks
    eckit::mpi::comm().allReduce(powersLocal, powersGlobal, eckit::mpi::sum());

    for (size_t i = 0; i < globalTurbines.size(); ++i) {
        powersByTurbine[i].setValue(powersGlobal[i]);
    }

    return powersByTurbine;
}

std::vector<WindPoint> NoWake::computeWindAtPoints(const WindMap& wMap, const WindFarm& windFarm,
                                                   const std::vector<std::unique_ptr<LatLonPoint>>& points) const {

    std::vector<WindPoint> velPoints;
    auto avgWind = windFarm.computeAvgWindSpeed(wMap);

    // Simple possible implementation:
    // associate the average wind speed to every point
    for (const auto& p : points) {
        velPoints.push_back(WindPoint(*p, avgWind.first, avgWind.second));
    }

    return velPoints;
}

}  // namespace wind_farm_plugin