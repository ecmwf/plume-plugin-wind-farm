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
     * @brief wind farm parametrization model to compute power output
     *
     * @param wMap
     * @param windFarm
     * @return double
     */
    double computePower(const WindMap& wMap, const WindFarm& windFarm) const override;

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

private:
    // wake expansion factor
    double kw_;
};


}  // namespace wind_farm_plugin