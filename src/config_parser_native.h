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

#include "config_parser.h"

namespace wind_farm_plugin {

/**
 * @brief Config parser for native format
 * In the plugin configuration native format, wind turbine specs and layout 
 * are defined in the same file. Example:
 * 
 * wind_turbine_defaults:
 *   hub_height: 100.0
 *   radius: 77.5
 *   power:
 *     type: tabular_wind_curve
 *     is_coefficient: false
 *     wind_speeds: [0.0, 2.9, 3.0, 4.0, 27.0, 28.0, 28.1, 50.0]
 *     values: [0.0, 0.0, 90000.0, 320000.0, 8000000.0, 8000000.0, 0.0, 0.0]
 *   thrust:
 *     type: tabular_wind_curve
 *     is_coefficient: true
 *     wind_speeds: [0.0, 1.3, 1.8, 2.0, 24.0, 25.0, 30.5, 50.0]
 *     values: [0.0, 0.0, 0.2, 0.8, 0.4, 0.01, 0.0, 0.0]
 *   rho_hub: 1.25
 * wind_turbines:
 *   - lat: 61.5982055543
 *     lon: 3.0036262910
 *   - lat: 61.6024827485
 *     lon: 3.0126192737
 *   - lat: 61.6067593523
 *     lon: 3.0216122565
 */
class ConfigParserNative : public ConfigParser {
public:
    WindFarmConfig parse(const eckit::Configuration& coreConfig) override;
};

}  // namespace wind_farm_plugin
