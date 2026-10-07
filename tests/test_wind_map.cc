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

#include "eckit/testing/Test.h"
#include "eckit/types/FloatCompare.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"

#include "plume/data/FieldAccess.h"

#include "test_utils.h"
#include "wind_map.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;
using eckit::types::is_approximately_equal;

namespace {

const double kTol = 1e-9;

// -------- Fixtures for `windInBox()` tests --------

// u/v encode each point's own lon/lat, so a returned WindSample's (lat,lon) can be cross-checked against its
// own (u,v), and the expected count can be computed independently by scanning the same field.
void encodeLonLatIntoUV(atlas::Field& uField, atlas::Field& vField) {
    auto lonlat = atlas::array::make_view<double, 2>(uField.functionspace().lonlat());
    auto u      = atlas::array::make_view<double, 2>(uField);
    auto v      = atlas::array::make_view<double, 2>(vField);
    for (atlas::idx_t i = 0; i < lonlat.shape(0); i++) {
        u(i, 0) = lonlat(i, 0);  // lon
        v(i, 0) = lonlat(i, 1);  // lat
    }
}

struct SharedFields {
    atlas::Field u;
    atlas::Field v;
};

// Every CASE in this file reads the same lon/lat-encoded (u,v) field built and encoded once on first use and
// reused, rather than re-allocating the underlying atlas grid/field and re-encoding it per CASE.
SharedFields& sharedFields() {
    static SharedFields fields = [] {
        atlas::Field u = test::createUniform2DField("u", 0.0);
        atlas::Field v = test::createUniform2DField("v", 0.0);
        encodeLonLatIntoUV(u, v);
        return SharedFields{u, v};
    }();
    return fields;
}

// Named distinctly from test::sharedWindMap() (test_utils.h) — that one wraps the generic u=10/v=5
// fixture, a different WindMap over the lon/lat-encoded fields above. Using the same name here would
// silently shadow this file's own version for unqualified calls made from inside `namespace test` below.
const WindMap& sharedLonLatWindMap() {
    static WindMap windMap(plume::data::FieldView{sharedFields().u}, plume::data::FieldView{sharedFields().v});
    return windMap;
}

}  // namespace

namespace test {

CASE("test_wind_in_box_returns_every_point_for_a_whole_globe_box") {

    // Wide enough to unambiguously cover the entire L200x101 grid regardless of exact edge convention.
    std::vector<WindSample> samples = sharedLonLatWindMap().windInBox(-91.0, 91.0, -1.0, 361.0);

    EXPECT_EQUAL(samples.size(), sharedFields().u.functionspace().size());
    for (const auto& s : samples) {
        EXPECT(is_approximately_equal(s.u, s.lon, kTol));  // round-trip through the u/v encoding
        EXPECT(is_approximately_equal(s.v, s.lat, kTol));
    }
}

CASE("test_wind_in_box_finds_a_single_known_point") {

    // A box tiny enough (L200x101: 1.8-degree spacing) that only the one point it's centred on can fall
    // inside, no ambiguity about which points "should" match, so nothing to re-derive.
    auto lonlat      = atlas::array::make_view<double, 2>(sharedFields().u.functionspace().lonlat());
    double lon0      = lonlat(0, 0);
    double lat0      = lonlat(0, 1);
    const double eps = 0.01;

    std::vector<WindSample> samples = sharedLonLatWindMap().windInBox(lat0 - eps, lat0 + eps, lon0 - eps, lon0 + eps);

    EXPECT_EQUAL(samples.size(), 1u);
    EXPECT(is_approximately_equal(samples[0].lon, lon0, kTol));
    EXPECT(is_approximately_equal(samples[0].lat, lat0, kTol));
    EXPECT(is_approximately_equal(samples[0].u, lon0, kTol));
    EXPECT(is_approximately_equal(samples[0].v, lat0, kTol));
}

CASE("test_wind_in_box_excludes_points_outside_bounds") {

    // Small box: must not return every point on the grid.
    const size_t totalPoints        = sharedFields().u.functionspace().size();
    std::vector<WindSample> samples = sharedLonLatWindMap().windInBox(10.0, 12.0, 10.0, 12.0);

    EXPECT(samples.size() > 0);
    EXPECT(samples.size() < totalPoints);
}

CASE("test_wind_in_box_empty_when_box_misses_grid") {

    // Deliberately narrow box, strictly between grid points (L200x101: 1.8-degree spacing), hits nothing.
    std::vector<WindSample> samples = sharedLonLatWindMap().windInBox(10.4, 10.6, 10.4, 10.6);

    EXPECT(samples.empty());
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
