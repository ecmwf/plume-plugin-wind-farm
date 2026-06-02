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

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/testing/Test.h"
#include "config_parser_native.h"
#include "config_parser_windio.h"
#include "utils.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace {

const char* getTestConfigNativePath() {
    return std::getenv("PLUME_WIND_FARM_TEST_CONFIG_NATIVE");
}

const char* getTestConfigWindIOPath() {
    return std::getenv("PLUME_WIND_FARM_TEST_CONFIG_WINDIO");
}

eckit::LocalConfiguration loadCoreConfig(const char* configPathEnv) {
    eckit::PathName configPath(configPathEnv);
    eckit::YAMLConfiguration config = eckit::YAMLConfiguration(configPath);
    std::vector<eckit::LocalConfiguration> pluginConfigs = config.getSubConfigurations("plugins");
    return pluginConfigs[0].getSubConfiguration("core-config");
}

void expectVectorClose(const std::vector<double>& left, const std::vector<double>& right, double tolerance) {
    EXPECT_EQUAL(left.size(), right.size());
    for (size_t i = 0; i < left.size(); ++i) {
        EXPECT(std::abs(left[i] - right[i]) < tolerance);
    }
}

void expectDoubleClose(double left, double right, double tolerance) {
    EXPECT(std::abs(left - right) < tolerance);
}

double getDoubleWithDefault(const eckit::LocalConfiguration& turbine,
                            const eckit::LocalConfiguration& defaults,
                            const char* key) {
    if (turbine.has(key)) {
        return turbine.getDouble(key);
    }
    return defaults.getDouble(key);
}

eckit::LocalConfiguration getSubConfigWithDefault(const eckit::LocalConfiguration& turbine,
                                                  const eckit::LocalConfiguration& defaults,
                                                  const char* key) {
    if (turbine.has(key)) {
        return turbine.getSubConfiguration(key);
    }
    return defaults.getSubConfiguration(key);
}

void expectCurveClose(const eckit::LocalConfiguration& left,
                      const eckit::LocalConfiguration& right,
                      double tolerance) {
    EXPECT_EQUAL(left.getString("type"), right.getString("type"));
    EXPECT_EQUAL(left.getBool("is_coefficient"), right.getBool("is_coefficient"));
    expectVectorClose(left.getDoubleVector("wind_speeds"),
                      right.getDoubleVector("wind_speeds"),
                      tolerance);
    expectVectorClose(left.getDoubleVector("values"),
                      right.getDoubleVector("values"),
                      tolerance);
}

}  // namespace

namespace test {

CASE("test_user_formats") {

    const double tolerance = 10e-6;

    const char* testConfigNativePath = getTestConfigNativePath();
    EXPECT(testConfigNativePath != nullptr);

    const char* testConfigWindIOPath = getTestConfigWindIOPath();
    EXPECT(testConfigWindIOPath != nullptr);

    // load native core config
    const auto nativeConfig = loadCoreConfig(testConfigNativePath);
    ConfigParserNative nativeParser;
    WindFarmConfig nativeWindFarmConfig = nativeParser.parse(nativeConfig);

    // load windio config
    const auto windioConfig = loadCoreConfig(testConfigWindIOPath);
    ConfigParserWindIO windioParser;
    WindFarmConfig windioWindFarmConfig = windioParser.parse(windioConfig);

    const auto& nativeDefaults = nativeWindFarmConfig.turbineDefaults();
    const auto& windioDefaults = windioWindFarmConfig.turbineDefaults();

    expectDoubleClose(nativeDefaults.getDouble("hub_height"),
                      windioDefaults.getDouble("hub_height"),
                      tolerance);
    expectDoubleClose(nativeDefaults.getDouble("radius"),
                      windioDefaults.getDouble("radius"),
                      tolerance);
    expectDoubleClose(nativeDefaults.getDouble("rho_hub"),
                      windioDefaults.getDouble("rho_hub"),
                      tolerance);
    expectCurveClose(nativeDefaults.getSubConfiguration("power"),
                     windioDefaults.getSubConfiguration("power"),
                     tolerance);
    expectCurveClose(nativeDefaults.getSubConfiguration("thrust"),
                     windioDefaults.getSubConfiguration("thrust"),
                     tolerance);

    const auto& nativeTurbines = nativeWindFarmConfig.turbines();
    const auto& windioTurbines = windioWindFarmConfig.turbines();
    EXPECT_EQUAL(nativeTurbines.size(), windioTurbines.size());

    for (size_t i = 0; i < nativeTurbines.size(); ++i) {
        const auto& nativeTurbine = nativeTurbines[i];
        const auto& windioTurbine = windioTurbines[i];

        expectDoubleClose(nativeTurbine.getDouble("lat"),
                          windioTurbine.getDouble("lat"),
                          tolerance);
        expectDoubleClose(nativeTurbine.getDouble("lon"),
                          windioTurbine.getDouble("lon"),
                          tolerance);

        expectDoubleClose(getDoubleWithDefault(nativeTurbine, nativeDefaults, "hub_height"),
                          getDoubleWithDefault(windioTurbine, windioDefaults, "hub_height"),
                          tolerance);
        expectDoubleClose(getDoubleWithDefault(nativeTurbine, nativeDefaults, "radius"),
                          getDoubleWithDefault(windioTurbine, windioDefaults, "radius"),
                          tolerance);
        expectDoubleClose(getDoubleWithDefault(nativeTurbine, nativeDefaults, "rho_hub"),
                          getDoubleWithDefault(windioTurbine, windioDefaults, "rho_hub"),
                          tolerance);

        expectCurveClose(getSubConfigWithDefault(nativeTurbine, nativeDefaults, "power"),
                         getSubConfigWithDefault(windioTurbine, windioDefaults, "power"),
                         tolerance);
        expectCurveClose(getSubConfigWithDefault(nativeTurbine, nativeDefaults, "thrust"),
                         getSubConfigWithDefault(windioTurbine, windioDefaults, "thrust"),
                         tolerance);
    }


}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
