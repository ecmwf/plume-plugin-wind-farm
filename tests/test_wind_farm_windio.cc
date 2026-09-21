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
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"

#include "atlas/field/Field.h"

#include "config_parser_windio.h"
#include "test_utils.h"
#include "utils.h"
#include "wind_farm.h"
#include "wind_turbine.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace test {

CASE("test_windio_parser_from_files") {

    const double tolerance = 1e-6;

    // test config path
    const char* testConfigPath = getTestConfigPath();
    EXPECT(testConfigPath != nullptr);

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
        auto expectedLonLat = xy2LonLat(xs[i], ys[i], crs);
        EXPECT(std::abs(turbine.getDouble("lon") - expectedLonLat.first) < tolerance);
        EXPECT(std::abs(turbine.getDouble("lat") - expectedLonLat.second) < tolerance);

        EXPECT(std::abs(turbine.getDouble("hub_height") - 110.0) < tolerance);
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

CASE("test_windio_wind_farm_setup") {

    const double tolerance = 1e-6;

    // test config path
    const char* testConfigPath = getTestConfigPath();
    EXPECT(testConfigPath != nullptr);

    // build the WindFarm directly from the WindIO-formatted core config
    eckit::LocalConfiguration coreConfig = loadCoreConfig(testConfigPath);
    WindFarm windFarm(coreConfig);

    // set up the wind turbines against a uniform lat/lon field
    atlas::Field lonLatField = test::sharedUField().functionspace().lonlat();
    EXPECT_NO_THROW(windFarm.setupWindTurbines(lonLatField));

    // expected global turbine count matches the WindIO layout
    const auto& globalWindTurbines = windFarm.windTurbinesGlobal();
    EXPECT_EQUAL(globalWindTurbines.size(), 3);

    // compute expected average lat/lon from the WindIO x/y coordinates
    const std::vector<double> xs = {222000.01, 333000.02, 444000.03};
    const std::vector<double> ys = {7777000.700, 8888000.800, 9999000.900};
    const std::string crs = "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs +type=crs";

    double sumLon = 0.0;
    double sumLat = 0.0;
    for (size_t i = 0; i < xs.size(); ++i) {
        auto lonLat = xy2LonLat(xs[i], ys[i], crs);
        sumLon += lonLat.first;
        sumLat += lonLat.second;
    }
    const double expectedLon = sumLon / static_cast<double>(xs.size());
    const double expectedLat = sumLat / static_cast<double>(xs.size());

    EXPECT(std::abs(windFarm.averageLatLonGlob().lon() - expectedLon) < tolerance);
    EXPECT(std::abs(windFarm.averageLatLonGlob().lat() - expectedLat) < tolerance);

    // verify each turbine location was preserved after projection
    for (size_t i = 0; i < globalWindTurbines.size(); ++i) {
        const auto& wt = globalWindTurbines[i];
        auto expectedLonLat = xy2LonLat(xs[i], ys[i], crs);
        EXPECT(std::abs(wt->lon() - expectedLonLat.first) < tolerance);
        EXPECT(std::abs(wt->lat() - expectedLonLat.second) < tolerance);
    }
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
