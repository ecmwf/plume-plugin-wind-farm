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
#include <string>

#include "eckit/config/LocalConfiguration.h"

#include "wind_farm_config.h"

namespace wind_farm_plugin {

/**
 * Parser of wind farm information, e.g. turbine power curves, layout, etc..
 */
class ConfigParser {
public:
    virtual ~ConfigParser() = default;

    /**
     * @brief Parse the provided core configuration.
     */
    virtual WindFarmConfig parse(const eckit::Configuration& coreConfig) = 0;

    /**
     * @brief Factory method to build the appropriate ConfigParser based on the configuration format.
     */
    static std::unique_ptr<ConfigParser> build(const std::string& format);

protected:
    /**
     * @brief Load the wind turbines YAML file referenced by `wind_turbines_filename`
     * in the provided core configuration and return it as a LocalConfiguration.
     *
     * @throws eckit::BadParameter if `wind_turbines_filename` is not defined.
     */
    static eckit::LocalConfiguration loadWindTurbinesConfig(const eckit::Configuration& coreConfig);

};

}  // namespace wind_farm_plugin
