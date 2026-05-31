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

#include "config_parser_windio.h"
#include "utils.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace {

const char* getTestDataDir() {
    return std::getenv("PLUME_WIND_FARM_TEST_DIR");
}

const char* getTestConfigPath() {
    return std::getenv("PLUME_WIND_FARM_TEST_CONFIG");
}

eckit::LocalConfiguration loadCoreConfig(const char* configPathEnv) {
    eckit::PathName configPath(configPathEnv);
    eckit::YAMLConfiguration config = eckit::YAMLConfiguration(configPath);
    std::vector<eckit::LocalConfiguration> pluginConfigs = config.getSubConfigurations("plugins");
    return pluginConfigs[0].getSubConfiguration("core-config");
}


}  // namespace

namespace test {

CASE("test_windio_parser_from_files") {

    const double tolerance = 1e-6;

    // test data directory
    const char* dataDir = getTestDataDir();
    EXPECT(dataDir != nullptr);

    // test config path
    const char* testConfigPath = getTestConfigPath();
    EXPECT(testConfigPath != nullptr);

    // read the wind_farm config directly
    eckit::PathName wfPath(std::string(dataDir) + "/windio_dummy_windfarm.yml");
    eckit::YAMLConfiguration wfConfig(wfPath);

    // read the config using the WindIO parser
    eckit::LocalConfiguration coreConfig = loadCoreConfig(testConfigPath);

    ConfigParserWindIO parser;
    WindFarmConfig windFarmConfig = parser.parse(coreConfig);

    const auto& turbines = windFarmConfig.turbines();
    EXPECT_EQUAL(turbines.size(), 3);

    const std::vector<std::string> identifiers = {"WT0", "WT1", "WT2"};
    const std::vector<double> xs = {222000.01, 333000.02, 444000.03};
    const std::vector<double> ys = {7777000.700, 8888000.800, 9999000.900};
    const std::string crs = "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs +type=crs";

    for (size_t i = 0; i < turbines.size(); ++i) {
        const auto& turbine = turbines[i];

        EXPECT_EQUAL(turbine.getString("identifier"), identifiers[i]);
        EXPECT_EQUAL(turbine.getString("crs"), crs);
        auto expectedLonLat = xyToLonLat(xs[i], ys[i], crs);
        EXPECT(std::abs(turbine.getDouble("lon") - expectedLonLat.first) < tolerance);
        EXPECT(std::abs(turbine.getDouble("lat") - expectedLonLat.second) < tolerance);

        EXPECT(std::abs(turbine.getDouble("hub_height") - 100.0) < tolerance);
        EXPECT(std::abs(turbine.getDouble("radius") - 77.5) < tolerance);

        auto power = turbine.getSubConfiguration("power");
        auto thrust = turbine.getSubConfiguration("thrust");

        EXPECT_EQUAL(power.getString("type"), "tabular_wind_curve");
        EXPECT_EQUAL(thrust.getString("type"), "tabular_wind_curve");
        EXPECT_EQUAL(power.getBool("is_coefficient"), false);
        EXPECT_EQUAL(thrust.getBool("is_coefficient"), true);

        auto powerSpeeds = power.getDoubleVector("wind_speeds");
        auto powerValues = power.getDoubleVector("values");
        auto ctSpeeds = thrust.getDoubleVector("wind_speeds");
        auto ctValues = thrust.getDoubleVector("values");

        EXPECT_EQUAL(powerSpeeds.size(), powerValues.size());
        EXPECT_EQUAL(ctSpeeds.size(), ctValues.size());
        EXPECT(powerSpeeds.size() > 0);
        EXPECT(ctSpeeds.size() > 0);
    }
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
