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

// Regression test for the WFPModel, built on captured "golden" reference values, so any changes to the expected values
// indicate a behavioural regression, not just an implementation change.

#include <cmath>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"
#include "eckit/types/FloatCompare.h"

#include "atlas/field/Field.h"

#include "test_utils.h"
#include "wind_farm.h"
#include "wind_turbine.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;
using eckit::types::is_approximately_equal;

namespace {

constexpr double kTol = 1e-6;

// Loads the plugin's core-config from configPath, with wind_farm_model.name set to modelName.
eckit::LocalConfiguration loadConfig(const char* configPath, const std::string& modelName) {
    eckit::LocalConfiguration coreConfig = test::loadCoreConfig(configPath);
    eckit::LocalConfiguration config     = test::overrideModel(coreConfig, modelName);
    return config;
}

void setupWindFarm(WindFarm& windFarm) {
    windFarm.setupWindTurbines(test::sharedUField().functionspace().lonlat());
}

}  // namespace

namespace test {

CASE("test_golden_no_wake_power") {
    eckit::LocalConfiguration noWakeConfig = loadConfig(getTestConfigPath(), "no_wake");

    WindFarm windFarm(noWakeConfig);
    setupWindFarm(windFarm);

    double power                    = windFarm.computePower(sharedWindMap());
    std::vector<LatLonValue> powers = windFarm.computePowerByTurbine(sharedWindMap());

    EXPECT(is_approximately_equal(power, 4136562.7013396714, kTol));
    EXPECT_EQUAL(powers.size(), 3u);
    EXPECT(is_approximately_equal(powers[0].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(powers[1].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(powers[2].value(), 1378854.2337798905, kTol));
}

CASE("test_golden_jensen_power") {
    eckit::LocalConfiguration jensenConfig = loadConfig(getTestConfigPath(), "jensen");

    WindFarm windFarm(jensenConfig);
    setupWindFarm(windFarm);

    double power                    = windFarm.computePower(sharedWindMap());
    std::vector<LatLonValue> powers = windFarm.computePowerByTurbine(sharedWindMap());

    // Identical to no_wake: the dummy test turbines are ~500-1000km apart (see
    // native_dummy_windfarm.yml), while the wake cone at kw=0.04 is only tens of km
    // wide at that range, so Jensen's model correctly produces zero inter-turbine
    // wake interaction for this fixture.
    EXPECT(is_approximately_equal(power, 4136562.7013396714, kTol));
    EXPECT_EQUAL(powers.size(), 3u);
    EXPECT(is_approximately_equal(powers[0].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(powers[1].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(powers[2].value(), 1378854.2337798905, kTol));
}

CASE("test_golden_jensen_wind_box_sample") {
    eckit::LocalConfiguration jensenConfig = loadConfig(getTestConfigPath(), "jensen");

    WindFarm windFarm(jensenConfig);
    setupWindFarm(windFarm);

    std::vector<WindPoint> windBox = windFarm.computeWindBox(sharedWindMap());
    EXPECT_EQUAL(windBox.size(), 100u);

    // The configured wind_farm_box (lon 6.9-7.1, lat 54.9-55.1) sits nowhere near the
    // dummy turbines (lat ~57-66, lon ~2-4 — see native_dummy_windfarm.yml), so no wake
    // deficit reaches it; all 100 points read back the uniform ambient field unchanged.
    // Captured faithfully rather than "fixed" — this test guards the current fixture,
    // not a more interesting one. (The close-packed fixture below is where the box is
    // deliberately placed to land inside a real wake cone.)
    EXPECT(is_approximately_equal(windBox[0].wind_u(), 10.0, kTol));
    EXPECT(is_approximately_equal(windBox[0].wind_v(), 5.0, kTol));
    EXPECT(is_approximately_equal(windBox[25].wind_u(), 10.0, kTol));
    EXPECT(is_approximately_equal(windBox[25].wind_v(), 5.0, kTol));
    EXPECT(is_approximately_equal(windBox[50].wind_u(), 10.0, kTol));
    EXPECT(is_approximately_equal(windBox[50].wind_v(), 5.0, kTol));
    EXPECT(is_approximately_equal(windBox[75].wind_u(), 10.0, kTol));
    EXPECT(is_approximately_equal(windBox[75].wind_v(), 5.0, kTol));
    EXPECT(is_approximately_equal(windBox[99].wind_u(), 10.0, kTol));
    EXPECT(is_approximately_equal(windBox[99].wind_v(), 5.0, kTol));
}

// --- Close-packed farm: turbines ~2.5km apart, in line with the wind direction ---
// Exercises real Jensen wake interaction, unlike the tests above where the dummy
// turbines are too far apart for the wake cone to ever reach a neighbour.

CASE("test_golden_no_wake_power_close_farm") {
    eckit::LocalConfiguration noWakeConfig = loadConfig(getCloseTestConfigPath(), "no_wake");

    WindFarm windFarm(noWakeConfig);
    setupWindFarm(windFarm);

    double power                    = windFarm.computePower(sharedWindMap());
    std::vector<LatLonValue> powers = windFarm.computePowerByTurbine(sharedWindMap());

    // Identical to the far-apart fixture: no_wake never considers turbine proximity.
    EXPECT(is_approximately_equal(power, 4136562.7013396714, kTol));
    EXPECT_EQUAL(powers.size(), 3u);
    EXPECT(is_approximately_equal(powers[0].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(powers[1].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(powers[2].value(), 1378854.2337798905, kTol));
}

CASE("test_golden_jensen_power_close_farm") {
    eckit::LocalConfiguration jensenConfig = loadConfig(getCloseTestConfigPath(), "jensen");
    eckit::LocalConfiguration noWakeConfig = loadConfig(getCloseTestConfigPath(), "no_wake");

    WindFarm jensenFarm(jensenConfig);
    setupWindFarm(jensenFarm);
    double jensenPower                    = jensenFarm.computePower(sharedWindMap());
    std::vector<LatLonValue> jensenPowers = jensenFarm.computePowerByTurbine(sharedWindMap());

    WindFarm noWakeFarm(noWakeConfig);
    setupWindFarm(noWakeFarm);
    double noWakePower = noWakeFarm.computePower(sharedWindMap());

    // The real point of this fixture: unlike the far-apart dummy farm above, Jensen
    // must actually produce less power than NoWake here — proof the wake model is
    // engaging, not just matching NoWake trivially.
    EXPECT(jensenPower < noWakePower - 1.0);

    // turbine[0] (most upwind, nothing ahead of it) is unaffected and matches no_wake
    // exactly; turbine[1] and turbine[2] sit progressively further into the cumulative
    // wake and lose ~10% and ~14% respectively — the real, physically-expected shape
    // of a wake deficit, not a coincidence of this fixture.
    EXPECT(is_approximately_equal(jensenPower, 3808498.2343621659, kTol));
    EXPECT_EQUAL(jensenPowers.size(), 3u);
    EXPECT(is_approximately_equal(jensenPowers[0].value(), 1378854.2337798905, kTol));
    EXPECT(is_approximately_equal(jensenPowers[1].value(), 1238513.9231028082, kTol));
    EXPECT(is_approximately_equal(jensenPowers[2].value(), 1191130.0774794673, kTol));
}

CASE("test_golden_jensen_wind_box_sample_close_farm") {
    eckit::LocalConfiguration jensenConfig = loadConfig(getCloseTestConfigPath(), "jensen");

    WindFarm windFarm(jensenConfig);
    setupWindFarm(windFarm);

    std::vector<WindPoint> windBox = windFarm.computeWindBox(sharedWindMap());
    EXPECT_EQUAL(windBox.size(), 9u);

    // The box (see plume-config-native-close.yml.in) is a 3x3 grid spanning exactly
    // turbine 0 -> turbine 2, so with n_lat=3 the diagonal indices land exactly on the
    // turbines: idx 0 = turbine 0 (nothing upstream, undisturbed ambient), idx 4 =
    // turbine 1 (inside the wake, one turbine upstream), idx 8 = turbine 2 (inside the
    // wake, two turbines upstream — the deepest point in the farm). Magnitude must
    // strictly decrease 0 -> 4 -> 8: a direct, geometric confirmation of the same wake
    // deficit already seen via per-turbine power above, this time in the wind-box export.
    EXPECT(windBox[0].wind_mag() > windBox[4].wind_mag());
    EXPECT(windBox[4].wind_mag() > windBox[8].wind_mag());

    EXPECT(is_approximately_equal(windBox[0].wind_u(), 10.0, kTol));
    EXPECT(is_approximately_equal(windBox[0].wind_v(), 5.0, kTol));
    EXPECT(is_approximately_equal(windBox[4].wind_u(), 9.5122862212830803, kTol));
    EXPECT(is_approximately_equal(windBox[4].wind_v(), 4.7561431106415402, kTol));
    EXPECT(is_approximately_equal(windBox[8].wind_u(), 9.3476168229544161, kTol));
    EXPECT(is_approximately_equal(windBox[8].wind_v(), 4.6738084114772080, kTol));
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
