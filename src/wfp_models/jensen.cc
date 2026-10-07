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

#include "eckit/config/Configuration.h"
#include "eckit/mpi/Comm.h"

#include "atlas/runtime/Log.h"

#include "../utils.h"
#include "jensen.h"


namespace wind_farm_plugin {


static WFPModelBuilder<JensenModel> JensenModelBuilder;

JensenModel::JensenModel(const eckit::Configuration& conf) : WFPModel{conf}, kw_{conf.getDouble("kw", 0.04)} {
    // do nothing
}

JensenModel::WakeContext JensenModel::buildWakeContext(const WindMap& wMap, const WindFarm& windFarm) const {

    auto avgWind = windFarm.computeAvgWindSpeed(wMap);

    WakeContext ctx;
    ctx.ambientWind        = Vec3(avgWind.first, avgWind.second, 0.0);
    ctx.windDirection      = ctx.ambientWind.normalise();
    ctx.crossWindDirection = ctx.ambientWind.cross(Vec3(0, 0, 1)).normalise();
    ctx.wfCenter           = windFarm.averageLatLonGlob();

    Log::debug() << " >>> ambientWind: " << ctx.ambientWind << std::endl;
    Log::debug() << " >>> windDirection: " << ctx.windDirection << std::endl;
    Log::debug() << " >>> crossWindDirection: " << ctx.crossWindDirection << std::endl;

    // Turbine positions relative to the farm centre don't depend on the query point — computed once here rather
    // than once per point (used to be recomputed for every point in the wake loop, an O(N_points x N_turbines)
    // cost for something that's really O(N_turbines) information).
    const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines = windFarm.windTurbinesGlobal();
    ctx.turbinePositions.reserve(globalTurbines.size());
    for (const auto& wtg : globalTurbines) {
        auto wtg_xy = lonLat2xy(wtg->lon(), wtg->lat(), ctx.wfCenter.lon(), ctx.wfCenter.lat());
        ctx.turbinePositions.emplace_back(wtg_xy.first, wtg_xy.second, 0.0);
    }
    return ctx;
}


WindPoint JensenModel::windAfterWakeAt(const WakeContext& ctx, const LatLonPoint& p,
                                       const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines) const {

    double p_vel = ctx.ambientWind.magnitude();

    auto p_xy = lonLat2xy(p.lon(), p.lat(), ctx.wfCenter.lon(), ctx.wfCenter.lat());
    Vec3 p_c  = Vec3(p_xy.first, p_xy.second, 0.0);
    Log::debug() << "p_c: " << p_c << std::endl;

    for (size_t i_wtg = 0; i_wtg < globalTurbines.size(); ++i_wtg) {

        const auto& wtg         = globalTurbines[i_wtg];
        const Vec3& wtg_c       = ctx.turbinePositions[i_wtg];
        Vec3 wtg_2_p            = p_c - wtg_c;
        double wtg_2_p_wind_len = wtg_2_p.dot(ctx.windDirection);
        Vec3 wtg_2_p_wind       = ctx.windDirection * wtg_2_p_wind_len;

        if (wtg_2_p_wind_len > 0) {

            Log::debug() << " -- Wake affecting.." << std::endl;
            Log::debug() << " -- wtg_c: " << wtg_c << std::endl;

            // wake expansion factor
            double a = (1 - sqrt(1 - wtg->Ct(p_vel))) / 2.0;

            // pt center wake
            Vec3 pt_wake_c       = wtg_c + wtg_2_p_wind;
            Vec3 pt_wake_e       = ctx.crossWindDirection * wtg_2_p_wind_len * kw_;
            double pt_wake_e_mag = pt_wake_e.magnitude();

            // from wake ctr to p_c
            Vec3 pt_wake_p       = p_c - pt_wake_c;
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

    Vec3 wind_res_p = ctx.windDirection * p_vel;
    return WindPoint(p, wind_res_p.getX(), wind_res_p.getY());
}


std::vector<WindPoint> JensenModel::computeWindAtTurbines(const WindMap& wMap, const WindFarm& windFarm) const {

    WakeContext ctx                                                 = buildWakeContext(wMap, windFarm);
    const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines = windFarm.windTurbinesGlobal();

    // Iterate the turbines directly — a WindTurbine already is-a LatLonPoint, no adapter list needed.
    std::vector<WindPoint> windAtTurbines;
    windAtTurbines.reserve(windFarm.windTurbines().size());
    for (const auto& wt : windFarm.windTurbines()) {
        windAtTurbines.push_back(windAfterWakeAt(ctx, *wt, globalTurbines));
    }
    return windAtTurbines;
}


std::vector<WindPoint> JensenModel::computeWindAtPoints(const WindMap& wMap, const WindFarm& windFarm,
                                                        const std::vector<std::unique_ptr<LatLonPoint>>& points) const {

    WakeContext ctx                                                 = buildWakeContext(wMap, windFarm);
    const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines = windFarm.windTurbinesGlobal();

    std::vector<WindPoint> velPoints;
    velPoints.reserve(points.size());
    for (const auto& p : points) {
        velPoints.push_back(windAfterWakeAt(ctx, *p, globalTurbines));
    }
    return velPoints;
}


}  // namespace wind_farm_plugin
