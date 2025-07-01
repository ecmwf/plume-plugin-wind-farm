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

#include "point.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace test {

CASE("test_latlon_point") {

    double tolerance = 1e-6;

    // Create a LatLonPoint instance
    LatLonPoint point(55.0, 7.0);

    // Check latitude and longitude
    EXPECT(std::abs(point.lat() - 55.0) < tolerance);
    EXPECT(std::abs(point.lon() - 7.0) < tolerance);

    // Output the point to a stream
    std::ostringstream oss;
    oss << point;

    std::string text = oss.str();
    std::size_t pos = text.find("Point");
    EXPECT(pos != std::string::npos);

}

// test wind point
CASE("test_wind_point") {

    double tolerance = 1e-6;

    // Create a LatLonPoint instance
    LatLonPoint point(55.0, 7.0);
    WindPoint windPoint(point, 10.0, 5.0);

    // Check latitude and longitude
    EXPECT(std::abs(windPoint.point().lat() - 55.0) < tolerance);
    EXPECT(std::abs(windPoint.point().lon() - 7.0) < tolerance);

    // Check wind magnitude
    EXPECT(std::abs(windPoint.wind_mag() - std::sqrt(10.0 * 10.0 + 5.0 * 5.0)) < tolerance);
}


}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}