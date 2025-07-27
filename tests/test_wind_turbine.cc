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
#include <iostream>
#include <memory>

#include "eckit/testing/Test.h"
#include "eckit/config/Configuration.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"


#include "wind_turbine.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace test {

// test the wind turbine class
CASE("test_wind_turbine_config") {

    double tolerance = 1e-6;

    // Default configuration with all parameters
    std::string default_conf_str(R"YAML(
        hub_height: 100.0
        radius: 50.0
        power:
            type: tabular_wind_curve
            is_coefficient: false
            wind_speeds: [0, 5, 10, 15, 20]
            values: [0, 1000, 2000, 3000, 4000]
        thrust:
            type: tabular_wind_curve
            is_coefficient: true
            wind_speeds: [0, 5, 10, 15, 20]
            values: [0.1, 0.2, 0.3, 0.4, 0.5]
        rho_hub: 1.225
    )YAML");

    // WT configuration only has lat/lon
    std::string conf_str(R"YAML(
        lat: 55.0
        lon: 7.0
    )YAML");

    eckit::YAMLConfiguration conf(conf_str);
    eckit::YAMLConfiguration default_conf(default_conf_str);

    // Create a WindTurbine instance

    WindTurbine turbine(999, conf, -888, 1000.0, -999, default_conf);

    // Check properties
    EXPECT_EQUAL(turbine.ID(), 999);
    EXPECT(std::abs(turbine.lat() - 55.0) < tolerance);
    EXPECT(std::abs(turbine.lon() - 7.0) < tolerance);

    // from default configuration
    EXPECT(std::abs(turbine.hubHeight() - 100.0) < tolerance);
    EXPECT(std::abs(turbine.radius() - 50.0) < tolerance);
    EXPECT(std::abs(turbine.rhoHub() - 1.225) < tolerance);

    EXPECT_EQUAL(turbine.nearestPointID(), -888);
    EXPECT(std::abs(turbine.minDistanceLocal() - 1000.0) < tolerance);
    EXPECT_EQUAL(turbine.minRankGlob(), -999);

    // check compute power
    EXPECT(std::abs(turbine.computePower(0.0) - 0.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(5.0) - 1000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(10.0) - 2000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(15.0) - 3000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(20.0) - 4000.0) < tolerance);

    // check thrust coefficient
    EXPECT(std::abs(turbine.Ct(0.0) - 0.1) < tolerance);
    EXPECT(std::abs(turbine.Ct(5.0) - 0.2) < tolerance);
    EXPECT(std::abs(turbine.Ct(10.0) - 0.3) < tolerance);
    EXPECT(std::abs(turbine.Ct(15.0) - 0.4) < tolerance);
    EXPECT(std::abs(turbine.Ct(20.0) - 0.5) < tolerance);

}



// test the wind turbine class (with overridded parameters)
CASE("test_wind_turbine_config_overridden_params") {

    double tolerance = 1e-6;

    // Default configuration with all parameters
    std::string default_conf_str(R"YAML(
        hub_height: 100.0
        radius: 50.0
        power:
            type: tabular_wind_curve
            is_coefficient: false
            wind_speeds: [0, 5, 10, 15, 20]
            values: [0, 1000, 2000, 3000, 4000]
        thrust:
            type: tabular_wind_curve
            is_coefficient: true
            wind_speeds: [0, 5, 10, 15, 20]
            values: [0.1, 0.2, 0.3, 0.4, 0.5]
        rho_hub: 1.225
    )YAML");


    // WT configuration only has lat/lon
    std::string conf_str(R"YAML(
        lat: 55.0
        lon: 7.0
        hub_height: 111.1
        radius: 55.5
        power:
            type: tabular_wind_curve
            is_coefficient: false
            wind_speeds: [0, 5, 10, 15, 20]
            values: [0, 2000, 4000, 6000, 8000]
        thrust:
            type: tabular_wind_curve
            is_coefficient: true
            wind_speeds: [0, 5, 10, 15, 20]
            values: [0.2, 0.4, 0.6, 0.8, 1.0]
        rho_hub: 1.4
    )YAML");

    eckit::YAMLConfiguration conf(conf_str);
    eckit::YAMLConfiguration default_conf(default_conf_str);

    // Create a WindTurbine instance
    WindTurbine turbine(999, conf, -888, 1000.0, -999, default_conf);

    // Check properties
    EXPECT_EQUAL(turbine.ID(), 999);
    EXPECT(std::abs(turbine.lat() - 55.0) < tolerance);
    EXPECT(std::abs(turbine.lon() - 7.0) < tolerance);

    // from default configuration
    EXPECT(std::abs(turbine.hubHeight() - 111.1) < tolerance);
    EXPECT(std::abs(turbine.radius() - 55.5) < tolerance);
    EXPECT(std::abs(turbine.rhoHub() - 1.4) < tolerance);

    // check nearest point info
    EXPECT_EQUAL(turbine.nearestPointID(), -888);
    EXPECT(std::abs(turbine.minDistanceLocal() - 1000.0) < tolerance);
    EXPECT_EQUAL(turbine.minRankGlob(), -999);

    // check compute power
    EXPECT(std::abs(turbine.computePower(0.0) - 0.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(5.0) - 2000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(10.0) - 4000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(15.0) - 6000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(20.0) - 8000.0) < tolerance);

    // test interpolation
    EXPECT(std::abs(turbine.computePower(2.5) - 1000.0) < tolerance);
    EXPECT(std::abs(turbine.computePower(30.0) - 8000.0) < tolerance);

    // check thrust coefficient
    EXPECT(std::abs(turbine.Ct(0.0) - 0.2) < tolerance);
    EXPECT(std::abs(turbine.Ct(5.0) - 0.4) < tolerance);
    EXPECT(std::abs(turbine.Ct(10.0) - 0.6) < tolerance);

    // test interpolation
    EXPECT(std::abs(turbine.Ct(2.5) - 0.3) < tolerance);
    EXPECT(std::abs(turbine.Ct(30.0) - 1.0) < tolerance);

}



}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}