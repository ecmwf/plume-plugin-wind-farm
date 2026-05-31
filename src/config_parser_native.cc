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

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"

#include "config_parser_native.h"

namespace wind_farm_plugin {

WindFarmConfig ConfigParserNative::parse(const eckit::Configuration& coreConfig) {
    WindFarmConfig windFarmConfig;

    if (!coreConfig.has("wind_turbines_filename")) {
        throw eckit::BadParameter("No wind turbines file defined in the configuration", Here());
    }

    eckit::PathName windTurbinesFilename = coreConfig.getString("wind_turbines_filename");
    auto yamlConfig = eckit::YAMLConfiguration(windTurbinesFilename);
    auto wtConfig = eckit::LocalConfiguration(yamlConfig);

    if (wtConfig.has("wind_turbine_defaults")) {
        windFarmConfig.setTurbineDefaults(wtConfig.getSubConfiguration("wind_turbine_defaults"));
    }

    auto wts = wtConfig.getSubConfigurations("wind_turbines");
    if (wts.empty()) {
        throw eckit::BadParameter("No wind turbines defined in the configuration", Here());
    }

    windFarmConfig.reserveTurbines(wts.size());
    for (const auto& wtConf : wts) {
        windFarmConfig.addTurbine(wtConf);
    }

    return windFarmConfig;
}

}  // namespace wind_farm_plugin
