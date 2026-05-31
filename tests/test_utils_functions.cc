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
#include <string>
#include <utility>
#include <vector>

#include "eckit/testing/Test.h"

#include "utils.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace {

const double kTol = 1e-6;

const std::string kWgs84MercCrs =
    "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs +type=crs";

}  // namespace

namespace test {

CASE("test_lonlat2xy_zero_distance") {
    auto xy = lonLat2xy(10.0, 20.0, 10.0, 20.0);
    EXPECT(std::abs(xy.first) < kTol);
    EXPECT(std::abs(xy.second) < kTol);
}

CASE("test_xyToLonLat_origin_params") {
    const std::string crs = "+proj=merc +lon_0=15.5 +k=1 +x_0=123.0 +y_0=-456.0 +ellps=WGS84 +units=m +no_defs +type=crs";
    auto lonLat = xyToLonLat(123.0, -456.0, crs);
    EXPECT(std::abs(lonLat.first - 15.5) < kTol);
    EXPECT(std::abs(lonLat.second) < kTol);
}

CASE("test_lonlat2xy_xyToLonLat_roundtrip") {
    std::vector<std::pair<double, double>> points{
        {0.0, 0.0},
        {2.5, 45.0},
        {-75.25, 10.0},
    };

    for (const auto& point : points) {
        auto xy = lonLat2xy(point.first, point.second, 0.0, 0.0);
        auto lonLat = xyToLonLat(xy.first, xy.second, kWgs84MercCrs);

        EXPECT(std::abs(lonLat.first - point.first) < kTol);
        EXPECT(std::abs(lonLat.second - point.second) < kTol);
    }
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
