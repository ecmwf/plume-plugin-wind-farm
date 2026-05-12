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
#include <iostream>
#include <memory>
#include <numeric>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"
#include "eckit/filesystem/PathName.h"

#include "atlas/field/Field.h"

#include "wind_farm.h"
#include "test_utils.h"
#include "wind_turbine.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace {

const char* getTestConfigPath() {
    return std::getenv("PLUME_WIND_FARM_TEST_CONFIG");
}

eckit::LocalConfiguration loadCoreConfig(const char* configPathEnv) {
    eckit::PathName configPath(configPathEnv);
    eckit::YAMLConfiguration config = eckit::YAMLConfiguration(configPath);
    std::vector<eckit::LocalConfiguration> pluginConfigs = config.getSubConfigurations("plugins");
    return pluginConfigs[0].getSubConfiguration("core-config");
}

eckit::LocalConfiguration overrideModel(const eckit::LocalConfiguration& coreConfig, const std::string& modelName) {
    eckit::LocalConfiguration modelConfig = coreConfig.getSubConfiguration("wind_farm_model");
    modelConfig.set("name", modelName);

    eckit::LocalConfiguration updated(coreConfig);
    updated.set("wind_farm_model", modelConfig);
    return updated;
}

void setupWindFarm(WindFarm& windFarm, const atlas::Field& uField) {
    atlas::Field latLonField = uField.functionspace().lonlat();
    windFarm.setupWindTurbines(latLonField);
}

}  // namespace

namespace test {

CASE("test_wind_farm_setup") {

    double tolerance = 1e-6;

    // Extract the wind farm configuration from the full plume test config
    const char* configPathEnv = getTestConfigPath();
    EXPECT(configPathEnv != nullptr);

    eckit::LocalConfiguration coreConfig = loadCoreConfig(configPathEnv);

    // create a WindFarm instance
    WindFarm windFarm(coreConfig);

    atlas::Field uField = createUniform2DField("u", 10.0);
    EXPECT_NO_THROW(setupWindFarm(windFarm, uField));

    // check number of turbines and averagle lat/lon
    const std::vector<std::unique_ptr<WindTurbine>>& globalWindTurbines = windFarm.windTurbinesGlobal();
    int wt_size = globalWindTurbines.size();

    EXPECT_EQUAL(wt_size, 4);
    EXPECT(std::abs(windFarm.averageLatLonGlob().lat() - 55.0) < tolerance);
    EXPECT(std::abs(windFarm.averageLatLonGlob().lon() - 7.0) < tolerance);

}

CASE("test_compute_power_no_wake") {

    const char* configPathEnv = getTestConfigPath();
    EXPECT(configPathEnv != nullptr);

    eckit::LocalConfiguration coreConfig = loadCoreConfig(configPathEnv);
    eckit::LocalConfiguration noWakeConfig = overrideModel(coreConfig, "no_wake");

    WindFarm windFarm(noWakeConfig);
    atlas::Field uField = createUniform2DField("u", 10.0);
    atlas::Field vField = createUniform2DField("v", 5.0);

    setupWindFarm(windFarm, uField);

    WindMap windMap(uField, vField);
    double power = windFarm.computePower(windMap);
    std::vector<LatLonValue> powers = windFarm.computePowerByTurbine(windMap);

    EXPECT(power > 0.0);
    EXPECT_EQUAL(powers.size(), windFarm.windTurbinesGlobal().size());
    EXPECT(std::abs(std::accumulate(powers.begin(), powers.end(), 0.0,
                                    [](double sum, const LatLonValue& turbinePower) {
                                        return sum + turbinePower.value();
                                    }) - power) < 1e-6);
}

CASE("test_compute_power_jensen") {

    const char* configPathEnv = getTestConfigPath();
    EXPECT(configPathEnv != nullptr);

    eckit::LocalConfiguration coreConfig = loadCoreConfig(configPathEnv);
    eckit::LocalConfiguration jensenConfig = overrideModel(coreConfig, "jensen");
    eckit::LocalConfiguration noWakeConfig = overrideModel(coreConfig, "no_wake");

    atlas::Field uField = createUniform2DField("u", 10.0);
    atlas::Field vField = createUniform2DField("v", 5.0);
    WindMap windMap(uField, vField);

    WindFarm jensenFarm(jensenConfig);
    setupWindFarm(jensenFarm, uField);
    double jensenPower = jensenFarm.computePower(windMap);
    std::vector<LatLonValue> jensenPowers = jensenFarm.computePowerByTurbine(windMap);

    WindFarm noWakeFarm(noWakeConfig);
    setupWindFarm(noWakeFarm, uField);
    double noWakePower = noWakeFarm.computePower(windMap);

    EXPECT(jensenPower >= 0.0);
    EXPECT_EQUAL(jensenPowers.size(), jensenFarm.windTurbinesGlobal().size());
    EXPECT(std::abs(std::accumulate(jensenPowers.begin(), jensenPowers.end(), 0.0,
                                    [](double sum, const LatLonValue& turbinePower) {
                                        return sum + turbinePower.value();
                                    }) - jensenPower) < 1e-6);
    EXPECT(jensenPower <= noWakePower + 1e-6);
}

CASE("test_compute_wind_box_no_wake") {

    double tolerance = 1e-6;
    const size_t expectedPoints = 100;

    const char* configPathEnv = getTestConfigPath();
    EXPECT(configPathEnv != nullptr);

    eckit::LocalConfiguration coreConfig = loadCoreConfig(configPathEnv);
    eckit::LocalConfiguration noWakeConfig = overrideModel(coreConfig, "no_wake");

    WindFarm windFarm(noWakeConfig);
    atlas::Field uField = createUniform2DField("u", 10.0);
    atlas::Field vField = createUniform2DField("v", 5.0);

    setupWindFarm(windFarm, uField);
    WindMap windMap(uField, vField);

    std::vector<WindPoint> windBox = windFarm.computeWindBox(windMap);
    EXPECT_EQUAL(windBox.size(), expectedPoints);

    for (const auto& point : windBox) {
        EXPECT(std::abs(point.wind_u() - 10.0) < tolerance);
        EXPECT(std::abs(point.wind_v() - 5.0) < tolerance);
    }
}

CASE("test_compute_wind_box_jensen") {

    double tolerance = 1e-6;
    const size_t expectedPoints = 100;
    const double baseMag = std::sqrt(10.0 * 10.0 + 5.0 * 5.0);

    const char* configPathEnv = getTestConfigPath();
    EXPECT(configPathEnv != nullptr);

    eckit::LocalConfiguration coreConfig = loadCoreConfig(configPathEnv);
    eckit::LocalConfiguration jensenConfig = overrideModel(coreConfig, "jensen");

    WindFarm windFarm(jensenConfig);
    atlas::Field uField = createUniform2DField("u", 10.0);
    atlas::Field vField = createUniform2DField("v", 5.0);

    setupWindFarm(windFarm, uField);
    WindMap windMap(uField, vField);

    std::vector<WindPoint> windBox = windFarm.computeWindBox(windMap);
    EXPECT_EQUAL(windBox.size(), expectedPoints);

    for (const auto& point : windBox) {
        EXPECT(point.wind_mag() >= 0.0);
        EXPECT(point.wind_mag() <= baseMag + tolerance);
    }
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}