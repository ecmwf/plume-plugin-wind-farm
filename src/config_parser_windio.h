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

#include "config_parser.h"

namespace wind_farm_plugin {

/**
 * @brief Config parser for WindIO format
 * @note This parser does NOT support the full WindIO specification, but 
 * only a subset relevant for the wind farm plugin. In WindIO format, wind turbine specs and layout 
 * are defined in separate files (currently tested with windio 2.1.1)
 * 
 * Example of wind turbine file:
 * 
 * performance:
 *   Ct_curve:
 *     Ct_values: [0.0, 0.0, 0.2, 0.8, 0.4, 0.01, 0.0, 0.0]
 *     Ct_wind_speeds: [0.0, 1.3, 1.8, 2.0, 24.0, 25.0, 30.5, 50.0]
 *   power_curve:
 *     power_values: [0.0, 0.0, 90000, 320000, 8000000, 8000000, 0.0, 0.0]
 *     power_wind_speeds: [0.0, 2.9, 3.0, 4.0, 27.0, 28.0, 28.1, 50.0]
*
Example of layout file:
 *
 * name: Generic WF
 * turbines: <turbines_file>
 * layouts:
 *   coordinates:
 *     x: [334000.01, 335000.02, 336000.03]
 *     y: [8755000.101, 8756000.102, 8757000.103]
 *     z: [0.0, 0.0, 0.0]
 *     crs: +proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs +type=crs
 *   identifiers: [WT0, WT1, WT2]
 */
class ConfigParserWindIO : public ConfigParser {
public:
    WindFarmConfig parse(const eckit::Configuration& coreConfig) override;
};

}  // namespace wind_farm_plugin
