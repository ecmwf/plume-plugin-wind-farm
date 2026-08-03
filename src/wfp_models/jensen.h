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

#pragma once

#include "eckit/config/Configuration.h"

#include "../wind_map.h"
#include "../wind_turbine.h"
#include "wfp_model.h"


namespace wind_farm_plugin {

class JensenModel final : public WFPModel {

public:
    JensenModel(const eckit::Configuration& conf);

    /**
     * @brief Computes wind speed at given points
     *
     * @param avgWind
     * @param windFarm
     * @param points
     * @return std::vector<WindPoint>
     */
    std::vector<WindPoint> computeWindAtPoints(const WindMap& wMap, const WindFarm& windFarm,
                                               const std::vector<std::unique_ptr<LatLonPoint>>& points) const override;

    /**
     * @brief class name
     *
     * @return constexpr const char*
     */
    static const char* type() { return "jensen"; }

protected:
    /**
     * @brief Wind at each local turbine, via the analytic wake-deficit formula.
     *
     * Loops directly over windFarm.windTurbines() and calls windAfterWakeAt() per turbine.
     * Peer of computeWindAtPoints(), not defined in terms of it: both are thin loops around the same per-point
     * primitive, over different sources of points.
     */
    std::vector<WindPoint> computeWindAtTurbines(const WindMap& wMap, const WindFarm& windFarm) const override;

private:
    // wake expansion factor
    double kw_;

    /**
     * @brief Quantities the wake formula needs that don't vary across query points within one call.
     *
     * Built once by buildWakeContext() and reused for every point/turbine, instead of being recomputed each iteration.
     *
     * @todo (open discussion, raised in an earlier PR without a conclusive outcome — noting it again here so it
     * isn't lost) turbinePositions is rebuilt from each turbine's lat/lon on every call to buildWakeContext(), i.e.
     * every timestep, even though turbine positions are invariant for the whole run. For a WindIO-sourced farm,
     * that lat/lon was itself derived from the config's own native x/y via xy2LonLat() (config_parser_windio.cc)
     * at setup — so the effective per-timestep cost here is a full round trip: native x/y -> lat/lon (setup, once)
     * -> local x/y (here, every timestep), through two separate map projections (with the attendant floating-point
     * error) to recover a coordinate the config already had. Worth another look now that this class has its own
     * WakeContext to cache into. Candidate alternatives, neither implemented: (a) cache each turbine's local
     * (x, y) once at setup instead of recomputing it here on every call; (b) since a config can plausibly have
     * hundreds of turbines within a couple of model grid points, reproject the (few) grid points into the
     * turbines' native frame instead of reprojecting the (many) turbines twice.
     */
    struct WakeContext {
        Vec3 ambientWind;         // undisturbed wind (u, v, 0), before any turbine's wake reduces it
        Vec3 windDirection;       // unit vector along ambientWind
        Vec3 crossWindDirection;  // unit vector perpendicular to windDirection — the wake-width axis

        // Local map-projection origin (the farm's own average lat/lon) — every Cartesian position below
        // (this point and turbinePositions) is relative to it, not to any global origin.
        LatLonPoint wfCenter{0.0, 0.0};  // LatLonPoint has no default ctor — overwritten in buildWakeContext()

        // windTurbinesGlobal(), each projected once to local (x, y) metres relative to wfCenter. Turbines don't
        // have "centers" — this just caches each one's *position* so windAfterWakeAt() doesn't reproject the
        // same turbine's lat/lon on every query point.
        // @todo see struct-level note above: this is itself a repeated, avoidable reprojection,
        // done fresh every timestep instead of once at setup.
        std::vector<Vec3> turbinePositions;
    };

    WakeContext buildWakeContext(const WindMap& wMap, const WindFarm& windFarm) const;

    /**
     * @brief The actual analytic wake-deficit formula, evaluated at a single point.
     *
     * Shared by members computing wind. The formula doesn't care whether p is a turbine or an arbitrary location.
     */
    WindPoint windAfterWakeAt(const WakeContext& ctx, const LatLonPoint& p,
                              const std::vector<std::unique_ptr<WindTurbine>>& globalTurbines) const;
};


}  // namespace wind_farm_plugin
