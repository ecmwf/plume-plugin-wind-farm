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

#include <fstream>
#include <vector>

#include "eckit/config/Configuration.h"
#include "eckit/mpi/Comm.h"

#include "atlas/runtime/Log.h"

#include "../utils.h"
#include "jensen.h"

using atlas::Log;


namespace wind_farm_plugin {


static WFPModelBuilder<JensenModel> JensenModelBuilder;

JensenModel::JensenModel(const eckit::Configuration& conf) : WFPModel{conf}, kw_{conf.getDouble("kw", 0.04)} {
    // do nothing
}

double JensenModel::computePower(const WindMap& wMap, const WindFarm& windFarm) const {

    Log::debug() << " >>> Computing average wind speed in wind farm.." << std::endl;

    const std::vector<std::unique_ptr<WindTurbine>>& windTurbines = windFarm.windTurbines();
    std::vector<std::unique_ptr<LatLonPoint>> wtLatLons;
    wtLatLons.reserve(windTurbines.size());
    std::transform(
        windTurbines.begin(), windTurbines.end(), std::back_inserter(wtLatLons),
        [](const std::unique_ptr<WindTurbine>& wt) { return std::make_unique<LatLonPoint>(wt->lat(), wt->lon()); });

    // calculate the wind at each turbine point
    std::vector<WindPoint> windPointsAtTurbines = computeWindAtPoints(wMap, windFarm, wtLatLons);

    // local power at each turbine
    double localPower = 0.0;
    for (int i_wt = 0; i_wt < windPointsAtTurbines.size(); i_wt++) {
        const WindPoint& wp = windPointsAtTurbines[i_wt];
        localPower += windTurbines[i_wt]->computePower(wp.wind_u(), wp.wind_v());
    }

    double globalPower{0};
    eckit::mpi::comm().allReduce(localPower, globalPower, eckit::mpi::sum());

    return globalPower;
}


std::vector<WindPoint> JensenModel::computeWindAtPoints(const WindMap& wMap, const WindFarm& windFarm,
                                                        const std::vector<std::unique_ptr<LatLonPoint>>& points) const {

    std::vector<WindPoint> velPoints;

    auto avgWind = windFarm.computeAvgWindSpeed(wMap);

    Vec3 wind(avgWind.first, avgWind.second, 0.0);
    Vec3 wind1     = wind.normalise();
    Vec3 wind_perp = wind.cross(Vec3(0, 0, 1)).normalise();

    Log::debug() << " >>> wind: " << wind << std::endl;
    Log::debug() << " >>> wind1: " << wind1 << std::endl;
    Log::debug() << " >>> wind_perp: " << wind_perp << std::endl;

    LatLonPoint wfCenter = windFarm.averageLatLonGlob();

    // Calculate the power for each wind turbine
    for (const auto& p : points) {

        double p_vel = wind.magnitude();

        auto p_xy = lonLat2xy(p->lon(), p->lat(), wfCenter.lon(), wfCenter.lat());
        Vec3 p_c  = Vec3(p_xy.first, p_xy.second, 0.0);
        Log::debug() << "p_c: " << p_c << std::endl;

        for (const auto& wtg : windFarm.windTurbinesGlobal()) {

            // wt global ctr
            auto wtg_xy             = lonLat2xy(wtg->lon(), wtg->lat(), wfCenter.lon(), wfCenter.lat());
            Vec3 wtg_c              = Vec3(wtg_xy.first, wtg_xy.second, 0.0);
            Vec3 wtg_2_p            = p_c - wtg_c;
            double wtg_2_p_wind_len = wtg_2_p.dot(wind1);
            Vec3 wtg_2_p_wind       = wind1 * wtg_2_p_wind_len;

            if (wtg_2_p_wind_len > 0) {

                Log::debug() << " -- Wake affecting.." << std::endl;
                Log::debug() << " -- wtg_c: " << wtg_c << std::endl;

                // wake expansion factor
                double a = (1 - sqrt(1 - wtg->Ct(p_vel))) / 2.0;

                // pt center wake
                Vec3 pt_wake_c = wtg_c + wtg_2_p_wind;
                Vec3 pt_wake_e = wind_perp * wtg_2_p_wind_len * kw_;
                double pt_wake_e_mag = pt_wake_e.magnitude();

                // from wake ctr to p_c
                Vec3 pt_wake_p = p_c - pt_wake_c;
                double pt_wake_p_mag = pt_wake_p.magnitude();

                double a0_ij_over_ai = (pt_wake_e_mag > pt_wake_p_mag) ? 1.0 : 0.0;

                double x_ij     = wtg_2_p_wind_len;
                double delta_ij = (2 * a) / std::pow(1 + kw_ * x_ij / wtg->radius(), 2);

                p_vel -= delta_ij * a0_ij_over_ai * p_vel;
            }
            else {
                Log::debug() << " -- Wake not affecting.." << std::endl;
            }
        }

        p_vel = std::max(0.0, p_vel);
        Log::debug() << "-> p_vel: " << p_vel << std::endl;

        Vec3 wind1_p = wind1 * p_vel;
        velPoints.push_back(WindPoint(*p, wind1_p.getX(), wind1_p.getY()));
    }
    return velPoints;
}


}  // namespace wind_farm_plugin