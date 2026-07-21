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

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/log/Log.h"

#include "config_parser_windio.h"
#include "utils.h"

namespace wind_farm_plugin {

namespace {

eckit::LocalConfiguration buildCurveConfiguration(const eckit::Configuration& curveSource,
                                                  const char* valuesKey,
                                                  const char* windSpeedsKey,
                                                  bool isCoefficient) {
    if (!curveSource.has(valuesKey) || !curveSource.has(windSpeedsKey)) {
        throw eckit::BadParameter("WindIO curve must provide wind speed and value arrays", Here());
    }

    eckit::LocalConfiguration curveConfig;
    curveConfig.set("type", "tabular_wind_curve");
    curveConfig.set("is_coefficient", isCoefficient);
    curveConfig.set("wind_speeds", curveSource.getDoubleVector(windSpeedsKey));
    curveConfig.set("values", curveSource.getDoubleVector(valuesKey));
    return curveConfig;
}

eckit::LocalConfiguration buildWindIOTurbineConfiguration(const std::vector<double>& lons,
                                                          const std::vector<double>& lats,
                                                          const std::vector<std::string>& identifiers,
                                                          const std::string& crs,
                                                          const eckit::Configuration& turbineConfig,
                                                          size_t index) {
    eckit::LocalConfiguration turbine;
    if (index >= lons.size()) {
        throw eckit::BadParameter("WindIO turbine index exceeds layout coordinate array size", Here());
    }

    auto lonLat = xy2LonLat(lons[index], lats[index], crs);
    turbine.set("lon", lonLat.first);
    turbine.set("lat", lonLat.second);
    turbine.set("crs", crs);
    turbine.set("identifier", identifiers[index]);

    if (!turbineConfig.has("hub_height")) {
        throw eckit::BadParameter("WindIO turbine must define hub_height", Here());
    }
    turbine.set("hub_height", turbineConfig.getDouble("hub_height"));

    if (!turbineConfig.has("rotor_diameter")) {
        throw eckit::BadParameter("WindIO turbine must define rotor_diameter", Here());
    }
    turbine.set("radius", turbineConfig.getDouble("rotor_diameter") * 0.5);

    if (!turbineConfig.has("performance")) {
        throw eckit::BadParameter("WindIO turbine must define performance", Here());
    }
    auto performance = turbineConfig.getSubConfiguration("performance");

    if (!performance.has("power_curve")) {
        throw eckit::BadParameter("WindIO turbine must define performance.power_curve", Here());
    }
    if (!performance.has("Ct_curve")) {
        throw eckit::BadParameter("WindIO turbine must define performance.Ct_curve", Here());
    }

    auto powerCurve = performance.getSubConfiguration("power_curve");
    auto ctCurve = performance.getSubConfiguration("Ct_curve");

    turbine.set("power", buildCurveConfiguration(powerCurve, "power_values", "power_wind_speeds", false));
    turbine.set("thrust", buildCurveConfiguration(ctCurve, "Ct_values", "Ct_wind_speeds", true));


    // here we check if "rho_hub" is defined, if not emit a warning and assign default value of 1.25 kg/m3
    if (!turbineConfig.has("rho_hub")) {
        eckit::Log::warning() << "Warning: WindIO turbine configuration does not define 'rho_hub', using default value of 1.25 kg/m3" << std::endl;
        turbine.set("rho_hub", 1.25);
    } else {
        turbine.set("rho_hub", turbineConfig.getDouble("rho_hub"));
    }

    return turbine;
}

}  // namespace

WindFarmConfig ConfigParserWindIO::parse(const eckit::Configuration& coreConfig) {
    WindFarmConfig windFarmConfig;

    auto wfConfig = loadWindTurbinesConfig(coreConfig);


    // Basic checks on the expected structure of the WindIO configuration
    if (!wfConfig.has("turbines") || !wfConfig.has("layouts")) {
        throw eckit::BadParameter("WindIO wind farm must define turbines and layouts", Here());
    }

    // Layouts
    auto layoutConfig = wfConfig.getSubConfiguration("layouts");

    // Coordinates
    if (!layoutConfig.has("coordinates")) {
        throw eckit::BadParameter("WindIO layouts must define coordinates", Here());
    }    
    auto coordinates = layoutConfig.getSubConfiguration("coordinates");
    auto lons = coordinates.getDoubleVector("x");
    auto lats = coordinates.getDoubleVector("y");

    // check that lat/lons are not empty
    if (lons.empty() || lats.empty()) {
        throw eckit::BadParameter("WindIO layout coordinate arrays cannot be empty", Here());
    }
        
    if (lons.size() != lats.size()) {
        throw eckit::BadParameter("WindIO layout x/y coordinate arrays must have the same size", Here());
    }

    // Reference system
    if (!coordinates.has("crs")) {
        throw eckit::BadParameter("WindIO coordinates must define crs", Here());
    }
    auto crs = coordinates.getString("crs");


    // Identifiers
    if (!layoutConfig.has("identifiers")) {
        throw eckit::BadParameter("WindIO layouts must define identifiers", Here());
    }
    auto identifiers = layoutConfig.getStringVector("identifiers");
    if (identifiers.size() != lons.size()) {
        throw eckit::BadParameter("WindIO identifiers array must match the number of layout points", Here());
    }

    // Turbines
    if (!wfConfig.has("turbines")) {
        throw eckit::BadParameter("WindIO wind farm must define turbines", Here());
    }

    // helper function that removes the leading "!include" from the string
    const auto turbinePathString = wfConfig.getString("turbines");
    const auto turbinePathStringTrimmed = stripIncludePrefix(turbinePathString);

    // file path
    eckit::PathName turbineFilePath(turbinePathStringTrimmed);
    auto turbineConfig = eckit::YAMLConfiguration(turbineFilePath);

    windFarmConfig.reserveTurbines(lons.size());
    for (size_t i = 0; i < lons.size(); ++i) {
        windFarmConfig.addTurbine(buildWindIOTurbineConfiguration(lons, lats, identifiers, crs, turbineConfig, i));
    }

    // we still need to fill-in the defaults (that for the case of WindIO are the same as the first turbine, but we want to be explicit about it)
    if (windFarmConfig.turbines().size() > 0) {
        windFarmConfig.setTurbineDefaults(windFarmConfig.turbines().front());
    }

    return windFarmConfig;
}

}  // namespace wind_farm_plugin
