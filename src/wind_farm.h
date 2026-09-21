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

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "eckit/config/Configuration.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"

#include "wind_map.h"
#include "wind_turbine.h"

namespace plume {
namespace data {
class ModelDataView;  // forward declaration — only two-way coupling forwarding needs the full type
}
}  // namespace plume


namespace wind_farm_plugin {

// fwd declaration
class WFPModel;


/**
 * @brief wind farm class
 *
 */
class WindFarm {

public:
    /**
     * @brief wind_farm_box's lat/lon bounds. WindFarmBoxPoints() derives a resampled mesh from these for the
     * non-coupled wind-box export.
     */
    struct BoxBounds {
        double latMin = 0.0;
        double latMax = 0.0;
        double lonMin = 0.0;
        double lonMax = 0.0;
    };

    WindFarm(const eckit::Configuration& conf);

    ~WindFarm();

    /**
     * @brief Setup wind turbines (finds the closest grid point)
     *
     * @param lonLatField
     */
    void setupWindTurbines(atlas::Field lonLatField);

    /**
     * @brief Compute power output
     *
     * @param wMap
     * @return double
     */
    double computePower(const WindMap& wMap) const;

    /**
     * @brief Compute power output for each wind turbine (global ordering)
     *
     * @param wMap
     * @return std::vector<LatLonValue>
     */
    std::vector<LatLonValue> computePowerByTurbine(const WindMap& wMap) const;

    /**
     * @brief wind turbines (local)
     *
     * @return const std::vector<WindTurbine>&
     */
    const std::vector<std::unique_ptr<WindTurbine>>& windTurbines() const { return windTurbines_; }

    /**
     * @brief wind turbines (global)
     *
     * @return const std::vector<WindTurbine>&
     */
    const std::vector<std::unique_ptr<WindTurbine>>& windTurbinesGlobal() const { return windTurbinesGlobal_; }

    /**
     * @brief Average Lat/Lon
     *
     * @return std::optional<LatLonPoint>
     */
    const std::optional<LatLonPoint>& averageLatLon() const { return AvgPoint_; }

    /**
     * @brief Average Lat/Lon (global)
     *
     * @return std::pair<double,double>
     */
    const LatLonPoint& averageLatLonGlob() const { return AvgPointGlob_; }

    /**
     * @brief Wind farm box points
     *
     * @return std::vector<LatLonPoint*>
     */
    const std::vector<std::unique_ptr<LatLonPoint>>& WindFarmBoxPoints() const { return boxPoints_; }

    /// wind_farm_box's raw lat/lon bounds.
    const BoxBounds& windFarmBoxBounds() const { return boxBounds_; }

    /**
     * @brief Average nearest-other-turbine distance among turbines sharing grid point @p pointID, in metres.
     * @return std::nullopt if @p pointID has no local turbines, or the farm has only one turbine.
     */
    std::optional<double> turbineSpacing(size_t pointID) const;

    /**
     * @brief Average wind speed at wind farm (i.e. U_mean, V_mean)
     *
     * @return const WFPModel&
     */
    std::pair<double, double> computeAvgWindSpeed(const WindMap& wMap) const;

    /**
     * @brief Compute wind at given points
     *
     * @param points
     * @return std::vector<WindPoint>
     */
    std::vector<WindPoint> computeWindBox(const WindMap& wMap) const;

    // ---- Two-way coupling: thin forwarding to wfpModel_, same style as computePower/computePowerByTurbine ----

    /// Whether the configured model supports writing a parameter back to the host model.
    bool supportsCoupling() const;

    /// Caches per-grid-point, run-invariant quantities. Call once, from setup().
    void initialiseCoupling(const WindMap& wMap);

    /// Called when the host marks the target param updated; writes the farm's impact back.
    void applyCoupling(const WindMap& wMap, plume::data::ModelDataView& modelData) const;

    /// Writable parameter name the model writes back to, when supportsCoupling().
    const std::string& couplingTargetParam() const;

    /// Groups this rank's local turbines by nearestPointID (no MPI needed — turbines sharing a point share a rank).
    std::map<size_t, std::vector<const WindTurbine*>> turbinesByGridPoint() const;

    /**
     * @brief Print summary
     *
     */
    void summary() const;

private:
    void setupWindFarmBoxPoints();

    // calculate avg lat/lon
    void calculateAvgLatLons();

    /// Computes per-grid-point average nearest-other-turbine distance — see turbineSpacing().
    void setupTurbineSpacing();

    // configuration
    eckit::LocalConfiguration config_;

    // wind farm model
    std::unique_ptr<WFPModel> wfpModel_;

    // local Wind turbines
    std::vector<std::unique_ptr<WindTurbine>> windTurbines_;

    // global Wind turbines
    std::vector<std::unique_ptr<WindTurbine>> windTurbinesGlobal_;

    // wind farm box points
    std::vector<std::unique_ptr<LatLonPoint>> boxPoints_;

    // wind_farm_box's raw bounds
    BoxBounds boxBounds_;

    // per-grid-point average nearest-other-turbine distance, keyed by nearestPointID.
    std::map<size_t, double> turbineSpacing_;

    // average wind farm lat/lon (local)
    std::optional<LatLonPoint> AvgPoint_;

    // average wind farm lat/lon (global)
    LatLonPoint AvgPointGlob_{-999.999, -999.999};
};


}  // namespace wind_farm_plugin