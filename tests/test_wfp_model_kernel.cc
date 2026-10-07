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

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"
#include "eckit/types/FloatCompare.h"

#include "test_utils.h"
#include "wfp_models/wfp_model.h"
#include "wind_farm.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;
using eckit::types::is_approximately_equal;

namespace {

const double kWindU = 7.0;
const double kWindV = 0.0;

// Isolates the shared computePower/computePowerByTurbine kernel from any real model: fixed wind at every local
// turbine, so correctness of the MPI-reduced, ID-ordered assembly can be checked independent of Jensen/NoWake.
class FakeWFPModel : public WFPModel {
public:
    FakeWFPModel(double windU, double windV) : WFPModel(eckit::LocalConfiguration()), windU_(windU), windV_(windV) {}

    double windMag() const { return std::sqrt(windU_ * windU_ + windV_ * windV_); }

protected:
    std::vector<WindPoint> computeWindAtTurbines(const WindMap&, const WindFarm& windFarm) const override {
        std::vector<WindPoint> result;
        for (const auto& wt : windFarm.windTurbines()) {
            result.push_back(WindPoint(*wt, windU_, windV_));
        }
        return result;
    }

private:
    double windU_, windV_;
};

const char* getMpiTurbinesFilePath() {
    const char* path = std::getenv("PLUME_WIND_FARM_TEST_TURBINES_MPI");
    EXPECT(path != nullptr);
    return path;
}

// Turbines spread across the MPI-partitioned latitude bands, asserted when building the farm.
// wind_farm_model here is whatever plume-config-native.yml sets (jensen) — irrelevant to this file:
// every CASE calls WFPModel::computePower/computePowerByTurbine(wMap, windFarm) directly on
// FakeWFPModel, never WindFarm::computePower(wMap), so windFarm's own internal model is never invoked.
const eckit::LocalConfiguration& mpiCoreConfig() {
    static eckit::LocalConfiguration config = [] {
        eckit::LocalConfiguration coreConfig = test::loadCoreConfig(test::getTestConfigPath());
        coreConfig.set("wind_turbines_filename", std::string(getMpiTurbinesFilePath()));
        return coreConfig;
    }();
    return config;
}

// Every CASE in this file uses the same farm — built once on first use and reused, rather than
// re-allocating the underlying structures per CASE. FakeWFPModel::computeWindAtTurbines() ignores its
// WindMap argument entirely so we just need any field to setup the farm.
const WindFarm& sharedWindFarm() {
    static WindFarm windFarm(mpiCoreConfig());
    static const bool setupDone = [] {
        windFarm.setupWindTurbines(test::sharedUField().functionspace().lonlat());
        // Self-check the fixture's whole premise: MPI 3's checkerboard partitioning must actually put
        // exactly one turbine on each rank.
        EXPECT_EQUAL(windFarm.windTurbines().size(), 1u);
        return true;
    }();
    (void)setupDone;
    return windFarm;
}

}  // namespace

namespace test {

CASE("test_fake_model_power_by_turbine_matches_own_curve") {

    // Exercises computePowerByTurbine()'s cross-rank assembly: every global turbine's slot in the
    // ID-ordered, MPI-gathered result must carry its own lat/lon and its own curve's power value,
    // regardless of which rank actually computed it.
    FakeWFPModel fakeModel(kWindU, kWindV);

    const auto& globalTurbines      = sharedWindFarm().windTurbinesGlobal();
    std::vector<LatLonValue> powers = fakeModel.computePowerByTurbine(test::sharedWindMap(), sharedWindFarm());

    EXPECT_EQUAL(powers.size(), globalTurbines.size());
    for (const auto& wt : globalTurbines) {
        const LatLonValue& p = powers[wt->ID()];
        EXPECT(is_approximately_equal(p.lat(), wt->lat(), 1e-9));
        EXPECT(is_approximately_equal(p.lon(), wt->lon(), 1e-9));
        EXPECT(is_approximately_equal(p.value(), wt->computePower(fakeModel.windMag()), 1e-6));
    }
}

CASE("test_fake_model_power_matches_sum_of_by_turbine") {

    // Exercises computePower() against computePowerByTurbine(): per wfp_model.h, the two are deliberately independent
    // local-sum-then-MPI-reduce paths, so this is a cross-rank consistency check between them.
    FakeWFPModel fakeModel(kWindU, kWindV);

    double power                    = fakeModel.computePower(test::sharedWindMap(), sharedWindFarm());
    std::vector<LatLonValue> powers = fakeModel.computePowerByTurbine(test::sharedWindMap(), sharedWindFarm());

    double sum = 0.0;
    for (const auto& p : powers) {
        sum += p.value();
    }
    EXPECT(is_approximately_equal(sum, power, 1e-6));
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
