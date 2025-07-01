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

#include "wind_turbine.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace test {

// test the wind turbine class
CASE("test_wind_turbine") {

    double tolerance = 1e-6;

    // Create a WindTurbine instance
    WindTurbine turbine(999, 55.0, 7.0, 100.0, 50.0, 0.3, 0.4, 5.0, 20.0, 1.225, -888, 1000.0, -999);

    // Check properties
    EXPECT_EQUAL(turbine.ID(), 999);
    EXPECT(std::abs(turbine.lat() - 55.0) < tolerance);
    EXPECT(std::abs(turbine.lon() - 7.0) < tolerance);
    EXPECT(std::abs(turbine.hubHeight() - 100.0) < tolerance);
    EXPECT(std::abs(turbine.radius() - 50.0) < tolerance);
    EXPECT(std::abs(turbine.Cp() - 0.3) < tolerance);
    EXPECT(std::abs(turbine.Ct() - 0.4) < tolerance);
    EXPECT(std::abs(turbine.cutoffMax() - 5.0) < tolerance);
    EXPECT(std::abs(turbine.cutoffMin() - 20.0) < tolerance);
    EXPECT(std::abs(turbine.rhoHub() - 1.225) < tolerance);
    EXPECT_EQUAL(turbine.nearestPointID(), -888);
    EXPECT(std::abs(turbine.minDistanceLocal() - 1000.0) < tolerance);
    EXPECT_EQUAL(turbine.minRankGlob(), -999);

}


}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}