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

#include <memory>
#include <vector>

#include "eckit/config/Configuration.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"

#include "wind_map.h"
#include "wind_turbine.h"

namespace wind_farm_plugin {

// fwd declaration
class WFPModel;


/**
 * @brief wind farm class
 *
 */
class WindFarm {

public:
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
     * @return std::pair<double,double>
     */
    const LatLonPoint& averageLatLon() const { return AvgPoint_; }

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

    /**
     * @brief Print summary
     *
     */
    void summary() const;

private:
    void setupWindFarmBoxPoints();
    
    // calculate avg lat/lon
    void calculateAvgLatLons();

    // configuration
    eckit::LocalConfiguration config_;

    // wind turbines configuration
    eckit::LocalConfiguration wtConfig_;

    // wind farm model
    std::unique_ptr<WFPModel> wfpModel_;

    // local Wind turbines
    std::vector<std::unique_ptr<WindTurbine>> windTurbines_;

    // global Wind turbines
    std::vector<std::unique_ptr<WindTurbine>> windTurbinesGlobal_;

    // wind farm box points
    std::vector<std::unique_ptr<LatLonPoint>> boxPoints_;

    // average wind farm lat/lon (local)
    LatLonPoint AvgPoint_{-999.999, -999.999};

    // average wind farm lat/lon (global)
    LatLonPoint AvgPointGlob_{-999.999, -999.999};
};


}  // namespace wind_farm_plugin