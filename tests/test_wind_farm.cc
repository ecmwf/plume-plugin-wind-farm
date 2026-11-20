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
#include <memory>
#include <numeric>
#include <optional>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/testing/Test.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"

#include "plume/data/ModelData.h"

#include "plugin.h"
#include "test_utils.h"
#include "wind_farm.h"
#include "wind_turbine.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace {

void setupWindFarm(WindFarm& windFarm, const atlas::Field& uField) {
    atlas::Field latLonField = uField.functionspace().lonlat();
    windFarm.setupWindTurbines(latLonField);
}

eckit::LocalConfiguration withWindFarmBox(const eckit::LocalConfiguration& coreConfig, const std::string& boxYaml) {
    eckit::LocalConfiguration updated(coreConfig);
    updated.set("wind_farm_box", eckit::YAMLConfiguration(boxYaml));
    return updated;
}

const char* kBoxBoundsOnlyYaml = R"YAML(
    lon_min: 6.900
    lat_min: 54.900
    lon_max: 7.100
    lat_max: 55.100
)YAML";

// Used for cases that do not need a full wind map, but just need to create a field to pass to setup the wind farm.
atlas::Field createMinimalField(const std::string& name) {
    return atlas::Field(name, atlas::array::make_datatype<double>(), atlas::array::make_shape(2, 1));
}

}  // namespace

namespace test {

CASE("test_wind_farm_setup") {

    double tolerance = 1e-6;

    // Extract the wind farm configuration from the full plume test config
    eckit::LocalConfiguration coreConfig = loadCoreConfig(getTestConfigPath());

    // create a WindFarm instance
    WindFarm windFarm(coreConfig);
    EXPECT_NO_THROW(setupWindFarm(windFarm, sharedUField()));

    // check number of turbines and averagle lat/lon
    const std::vector<std::unique_ptr<WindTurbine>>& globalWindTurbines = windFarm.windTurbinesGlobal();
    int wt_size = globalWindTurbines.size();

    EXPECT_EQUAL(wt_size, 3);
    EXPECT(std::abs(windFarm.averageLatLonGlob().lat() - 62.04025278273082) < tolerance);
    EXPECT(std::abs(windFarm.averageLatLonGlob().lon() - 2.9913900757810636) < tolerance);
}

CASE("test_compute_power_no_wake") {

    eckit::LocalConfiguration coreConfig   = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration noWakeConfig = overrideModel(coreConfig, "no_wake");

    WindFarm windFarm(noWakeConfig);
    setupWindFarm(windFarm, sharedUField());

    double power                    = windFarm.computePower(sharedWindMap());
    std::vector<LatLonValue> powers = windFarm.computePowerByTurbine(sharedWindMap());

    EXPECT(power > 0.0);
    EXPECT_EQUAL(powers.size(), windFarm.windTurbinesGlobal().size());
    EXPECT(std::abs(std::accumulate(powers.begin(), powers.end(), 0.0,
                                    [](double sum, const LatLonValue& turbinePower) {
                                        return sum + turbinePower.value();
                                    }) - power) < 1e-6);
}

CASE("test_compute_power_jensen") {

    eckit::LocalConfiguration coreConfig = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration jensenConfig = overrideModel(coreConfig, "jensen");
    eckit::LocalConfiguration noWakeConfig = overrideModel(coreConfig, "no_wake");

    WindFarm jensenFarm(jensenConfig);
    setupWindFarm(jensenFarm, sharedUField());
    double jensenPower = jensenFarm.computePower(sharedWindMap());
    std::vector<LatLonValue> jensenPowers = jensenFarm.computePowerByTurbine(sharedWindMap());

    WindFarm noWakeFarm(noWakeConfig);
    setupWindFarm(noWakeFarm, sharedUField());
    double noWakePower = noWakeFarm.computePower(sharedWindMap());

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

    eckit::LocalConfiguration coreConfig   = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration noWakeConfig = overrideModel(coreConfig, "no_wake");

    WindFarm windFarm(noWakeConfig);
    setupWindFarm(windFarm, sharedUField());

    std::vector<WindPoint> windBox = windFarm.computeWindBox(sharedWindMap());
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

    eckit::LocalConfiguration coreConfig   = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration jensenConfig = overrideModel(coreConfig, "jensen");

    WindFarm windFarm(jensenConfig);
    setupWindFarm(windFarm, sharedUField());

    std::vector<WindPoint> windBox = windFarm.computeWindBox(sharedWindMap());
    EXPECT_EQUAL(windBox.size(), expectedPoints);

    for (const auto& point : windBox) {
        EXPECT(point.wind_mag() >= 0.0);
        EXPECT(point.wind_mag() <= baseMag + tolerance);
    }
}

CASE("test_turbine_spacing_close_farm") {

    eckit::LocalConfiguration coreConfig = loadCoreConfig(getCloseTestConfigPath());
    WindFarm windFarm(coreConfig);
    setupWindFarm(windFarm, sharedUField());

    // 3 turbines, ~2.5km apart in a line — close enough to share one grid point on the L200x101 test grid.
    auto byPoint = windFarm.turbinesByGridPoint();
    EXPECT_EQUAL(byPoint.size(), 1u);
    size_t pointID = byPoint.begin()->first;

    std::optional<double> spacing = windFarm.turbineSpacing(pointID);
    EXPECT(spacing.has_value());
    // Loose on purpose: true value is ~2499.51m (not exactly 2500), sanity bound on the fixture's own "~2.5km" intent.
    EXPECT(std::abs(*spacing - 2500.0) < 50.0);
}

CASE("test_turbine_spacing_single_turbine_is_nullopt") {

    eckit::LocalConfiguration coreConfig = loadCoreConfig(getTestConfigPath());
    coreConfig.set("wind_turbines_filename", std::string(getSingleTurbineFilePath()));

    WindFarm windFarm(coreConfig);
    setupWindFarm(windFarm, sharedUField());

    EXPECT_EQUAL(windFarm.windTurbinesGlobal().size(), 1u);
    auto byPoint   = windFarm.turbinesByGridPoint();
    size_t pointID = byPoint.begin()->first;

    // No other turbine anywhere in the farm to compare spacing against.
    EXPECT_NOT(windFarm.turbineSpacing(pointID).has_value());
}

CASE("test_single_turbine_farm_throws_via_plugin_setup") {

    // Unlike the jensen CASE below, this throw comes from deep inside initialiseCoupling(), reached only
    // after setup() has already built a real WindMap from u/v (needs a real functionspace).
    eckit::LocalConfiguration coreConfig = loadCoreConfig(getTestConfigPath());
    coreConfig.set("wind_turbines_filename", std::string(getSingleTurbineFilePath()));
    coreConfig                                = withWindFarmBox(coreConfig, kBoxBoundsOnlyYaml);
    eckit::LocalConfiguration roughnessConfig = overrideModel(coreConfig, "roughness");
    roughnessConfig.set("wind_farm_model.target_param", "z0m");

    atlas::Field z0mField = createMinimalField("z0m");

    plume::data::ModelData data;
    data.provideParam("u;hl;110", &sharedUField());
    data.provideParam("v;hl;110", &sharedVField());
    data.provideParam("z0m", &z0mField);

    WindFarmPluginCore pluginCore(roughnessConfig);
    pluginCore.grabData(data.filter({"u;hl;110", "v;hl;110", "z0m"}));

    EXPECT_THROWS_AS(pluginCore.setup(), eckit::UserError);
}

CASE("test_jensen_target_param_throws_via_plugin_setup") {

    eckit::LocalConfiguration coreConfig   = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration jensenConfig = overrideModel(coreConfig, "jensen");
    jensenConfig.set("wind_farm_model.target_param", "z0m");  // Jensen doesn't support coupling, this is misconfigured.

    atlas::Field uField = createMinimalField("u;hl;110");
    atlas::Field vField = createMinimalField("v;hl;110");

    plume::data::ModelData data;
    data.provideParam("u;hl;110", &uField);
    data.provideParam("v;hl;110", &vField);

    WindFarmPluginCore pluginCore(jensenConfig);
    pluginCore.grabData(data.filter({"u;hl;110", "v;hl;110"}));

    EXPECT_THROWS_AS(pluginCore.setup(), eckit::UserError);
}

CASE("test_wind_farm_box_no_mesh_without_n_lon_n_lat") {

    eckit::LocalConfiguration coreConfig    = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration boxOnlyBounds = withWindFarmBox(coreConfig, kBoxBoundsOnlyYaml);

    WindFarm windFarm(boxOnlyBounds);
    EXPECT_NO_THROW(setupWindFarm(windFarm, sharedUField()));

    // Absence of n_lon/n_lat means "no engineering-model mesh requested", bounds alone are cached.
    EXPECT(windFarm.WindFarmBoxPoints().empty());
}

CASE("test_wind_farm_box_partial_n_lon_n_lat_throws") {

    eckit::LocalConfiguration coreConfig = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration boxPartial = withWindFarmBox(coreConfig, R"YAML(
        lon_min: 6.900
        lat_min: 54.900
        lon_max: 7.100
        lat_max: 55.100
        n_lon: 10
    )YAML");

    WindFarm windFarm(boxPartial);
    EXPECT_THROWS_AS(setupWindFarm(windFarm, sharedUField()), eckit::UserError);
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
