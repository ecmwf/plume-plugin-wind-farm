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
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"
#include "eckit/types/FloatCompare.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackLedger.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/FieldAccess.h"
#include "plume/data/ModelData.h"
#include "plume/data/ModelDataView.h"

#include "plugin.h"
#include "test_utils.h"
#include "wfp_models/wfp_model.h"
#include "wind_farm.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;
using eckit::types::is_approximately_equal;

namespace {

std::vector<std::string> readLines(const std::string& filename) {
    std::ifstream file(filename);
    EXPECT(file.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

// Every CSV this file reads back only ever needs its last column (power, vel) — the others are structural
// (step/lat/lon), already covered by the header-format check at each call site.
double lastCsvField(const std::string& line) {
    return std::stod(line.substr(line.rfind(',') + 1));
}

}  // namespace

// --- Two-way coupled path: Entry A (target-param write) / Entry B (must never rewrite it) ---
//
// Deliberately does NOT use jensen/no_wake/roughness: a dummy WFP model, local to this file, writes one fixed,
// trivially-checkable value to its target param. This test is only about the plugin's own Entry A/B dispatch
// and write-back protocol.
namespace {

const char* kConsumer          = "wind_farm_plugin";
const char* kTargetParam       = "marker";
const double kMarkerBackground = 0.0;
const double kMarkerWritten    = 42.0;

// Writes kMarkerWritten to every point of its target param, unconditionally.
class DummyCoupledModel : public WFPModel {
public:
    DummyCoupledModel(const eckit::Configuration& conf) :
        WFPModel(conf), targetParam_(conf.getString("target_param")) {}

    bool supportsCoupling() const override { return true; }

    void applyCoupling(const WindMap&, const WindFarm&, plume::data::ModelDataView& modelData) const override {
        modelData.writeParam(targetParam_, [&](plume::data::FieldWriter& writer) {
            auto view = atlas::array::make_view<double, 2>(writer);
            for (atlas::idx_t i = 0; i < view.shape(0); ++i) {
                view(i, 0) = kMarkerWritten;
            }
        });
    }

    const std::string& couplingTargetParam() const override { return targetParam_; }

    static const char* type() { return "test_dummy_coupling"; }

private:
    std::string targetParam_;
};

eckit::LocalConfiguration coupledCoreConfig() {
    std::string yaml = std::string(R"YAML(
wind_farm_model:
  name: test_dummy_coupling
  target_param: )YAML") +
                       kTargetParam + R"YAML(
wind_field_height: 110
compute_power: false
export_wind_turbine_power: true
)YAML";
    eckit::YAMLConfiguration yamlConfig = eckit::YAMLConfiguration(yaml);
    eckit::LocalConfiguration config(yamlConfig);
    config.set("wind_turbines_filename",
               test::loadCoreConfig(test::getTestConfigPath()).getString("wind_turbines_filename"));
    return config;
}

atlas::Field& sharedMarkerField() {
    static atlas::Field field = test::createUniform2DField(kTargetParam, kMarkerBackground);
    return field;
}

// Raw-pointer statics, deliberately never auto-destroyed: ~WriteBackLedger() crashes when invoked during
// real static/atexit teardown on some toolchains. main() owns both in local unique_ptrs instead, destroying them in
// dependency order while still executing normally.
plume::WriteAuthorisation& sharedAuth() {
    static plume::WriteAuthorisation* auth = new plume::WriteAuthorisation();
    static const bool granted = [] {
        auth->grant(kConsumer, kTargetParam);
        return true;
    }();
    (void)granted;
    return *auth;
}

plume::coupling::WriteBackLedger& sharedLedger() {
    static plume::coupling::WriteBackLedger* ledger =
        new plume::coupling::WriteBackLedger(sharedAuth(), plume::WriteBackPolicy::single_writer);
    return *ledger;
}

// Fed exactly once, on first use from whichever CASE runs first.
plume::data::ModelData& sharedCoupledModelData() {
    static plume::data::ModelData data;
    static const bool fed = [] {
        data.createParam<int>("NSTEP", 0);
        data.provideParam("u;hl;110", &test::sharedUField());
        data.provideParam("v;hl;110", &test::sharedVField());
        data.provideParam(kTargetParam, &sharedMarkerField());
        data.enrollWritebackParams(sharedLedger(), sharedAuth());
        data.attachWritebackLedger(&sharedLedger());
        return true;
    }();
    (void)fed;
    return data;
}

WindFarmPluginCore& sharedCoupledPluginCore() {
    static WindFarmPluginCore pluginCore(coupledCoreConfig());
    static const bool setUp = [] {
        pluginCore.grabData(
            sharedCoupledModelData().filter({"NSTEP", "u;hl;110", "v;hl;110", kTargetParam}, kConsumer));
        pluginCore.setup();
        return true;
    }();
    (void)setUp;
    return pluginCore;
}

// Mirrors plume::Manager::run()'s own ledger cycle.
void runCoupledCycle(WindFarmPluginCore& pluginCore) {
    sharedLedger().reset();
    sharedLedger().open();
    pluginCore.run();
    sharedLedger().flush();
}

}  // namespace

// ---- Uncoupled path: today's default one-way behaviour, guard-order-safety + dispatch regression ----
namespace {

// No overrides beyond registration: WFPModel's own defaults already give a deterministic, wake-free result.
class DummyUncoupledModel : public WFPModel {
public:
    DummyUncoupledModel(const eckit::Configuration& conf) : WFPModel(conf) {}
    static const char* type() { return "test_dummy_uncoupled"; }
};

eckit::LocalConfiguration uncoupledCoreConfig() {
    eckit::LocalConfiguration config =
        test::overrideModel(test::loadCoreConfig(test::getTestConfigPath()), "test_dummy_uncoupled");
    config.set("export_wind_turbine_power", true);
    return config;
}

// Cross-check: an independently-constructed WindFarm (bypassing WindFarmPluginCore entirely).
double expectedTotalPower() {
    WindFarm windFarm(uncoupledCoreConfig());
    windFarm.setupWindTurbines(test::sharedUField().functionspace().lonlat());
    return windFarm.computePower(test::sharedWindMap());
}

// Fed exactly once, on first use from whichever CASE runs first — same idiom as the coupled section above.
plume::data::ModelData& sharedUncoupledModelData() {
    static plume::data::ModelData data;
    static const bool fed = [] {
        // Defensive: a prior interrupted run of this binary can leave these behind.
        std::remove("wind_turbine_power.csv");
        std::remove("wind_box_step_000000.csv");

        data.createParam<int>("NSTEP", 0);
        data.provideParam("u;hl;110", &test::sharedUField());
        data.provideParam("v;hl;110", &test::sharedVField());
        return true;
    }();
    (void)fed;
    return data;
}

WindFarmPluginCore& sharedUncoupledPluginCore() {
    static WindFarmPluginCore pluginCore(uncoupledCoreConfig());
    static const bool setUp = [] {
        pluginCore.grabData(sharedUncoupledModelData().filter({"NSTEP", "u;hl;110", "v;hl;110"}));
        pluginCore.setup();
        return true;
    }();
    (void)setUp;
    return pluginCore;
}

}  // namespace

namespace test {

// Deliberately runs before test_entry_a_... below: checks the marker field against the known background value.
CASE("test_entry_b_does_not_rewrite_target_param") {

    plume::data::ModelData& data   = sharedCoupledModelData();
    WindFarmPluginCore& pluginCore = sharedCoupledPluginCore();

    data.updateParam("NSTEP", 1);
    data.setUpdated({"u;hl;110", "v;hl;110"});
    runCoupledCycle(pluginCore);

    // Still the seeded background — Entry B must never touch the target param.
    auto view = atlas::array::make_view<double, 2>(sharedMarkerField());
    EXPECT(is_approximately_equal(view(0, 0), kMarkerBackground, 1e-15));
    EXPECT(is_approximately_equal(view(view.shape(0) - 1, 0), kMarkerBackground, 1e-15));
    EXPECT(data.pendingWritebacks().empty());

    // And it did genuinely run its own logic (not just skip everything): export_wind_turbine_power is on,
    // so its branch leaves this CSV behind.
    EXPECT(std::ifstream("wind_turbine_power.csv").good());
    std::remove("wind_turbine_power.csv");
}

CASE("test_entry_a_writes_target_param_and_only_target_param") {

    plume::data::ModelData& data   = sharedCoupledModelData();
    WindFarmPluginCore& pluginCore = sharedCoupledPluginCore();

    data.setUpdated({kTargetParam});
    runCoupledCycle(pluginCore);

    // Check that we have written the marker to the whole field via first and last points.
    auto view = atlas::array::make_view<double, 2>(sharedMarkerField());
    EXPECT(is_approximately_equal(view(0, 0), kMarkerWritten, 1e-15));
    EXPECT(is_approximately_equal(view(view.shape(0) - 1, 0), kMarkerWritten, 1e-15));

    auto pending = data.pendingWritebacks();
    EXPECT_EQUAL(pending.size(), std::size_t(1));
    EXPECT_EQUAL(pending[0], std::string(kTargetParam));
    data.acknowledgeWriteback(kTargetParam);
}

CASE("test_entry_a_takes_precedence_when_both_updated") {

    plume::data::ModelData& data   = sharedCoupledModelData();
    WindFarmPluginCore& pluginCore = sharedCoupledPluginCore();

    std::remove("wind_turbine_power.csv");  // defensive: the CASE above also produces this file

    // Reset from the CASE above's write first, so the check below actually proves Entry A ran during this
    // call, rather than reading a stale value left over from before.
    auto view                  = atlas::array::make_view<double, 2>(sharedMarkerField());
    view(0, 0)                 = kMarkerBackground;
    view(view.shape(0) - 1, 0) = kMarkerBackground;

    data.setUpdated({kTargetParam, "u;hl;110", "v;hl;110"});
    runCoupledCycle(pluginCore);

    // Entry A ran...
    EXPECT(is_approximately_equal(view(0, 0), kMarkerWritten, 1e-15));
    EXPECT(is_approximately_equal(view(view.shape(0) - 1, 0), kMarkerWritten, 1e-15));

    // ...and returned before Entry B's branch could also run in the same call — no power CSV appeared, even
    // though u/v were marked updated too.
    EXPECT(!std::ifstream("wind_turbine_power.csv").good());

    data.acknowledgeWriteback(kTargetParam);
}

CASE("test_uncoupled_guard_order_safety") {

    // twoWayEnabled_ is false here, so the Entry-A guard must short-circuit before ever checking a target
    // param that was never registered — otherwise this would throw on every run().
    WindFarmPluginCore& pluginCore = sharedUncoupledPluginCore();

    EXPECT_NO_THROW(pluginCore.run());  // nothing marked updated at all

    sharedUncoupledModelData().setUpdated({"NSTEP"});
    EXPECT_NO_THROW(pluginCore.run());  // only an unrelated param updated
}

CASE("test_uncoupled_power_and_wind_box_match_independent_computation") {

    WindFarmPluginCore& pluginCore = sharedUncoupledPluginCore();
    sharedUncoupledModelData().setUpdated({"u;hl;110", "v;hl;110"});
    EXPECT_NO_THROW(pluginCore.run());

    std::vector<std::string> powerLines = readLines("wind_turbine_power.csv");
    EXPECT_EQUAL(powerLines[0], std::string("step,lat,lon,power"));

    // Every turbine sees the same uniform ambient wind and shares the same power curve so every row is identical.
    double firstPower = lastCsvField(powerLines[1]);
    EXPECT(is_approximately_equal(firstPower, lastCsvField(powerLines.back()), std::abs(firstPower) * 1e-4));

    // Scaled to the CSV's ~6-significant-digit export precision (default stream precision), not to the
    // underlying computation's real precision.
    size_t turbineCount  = powerLines.size() - 1;
    double totalPower    = firstPower * static_cast<double>(turbineCount);
    double expectedPower = expectedTotalPower();
    EXPECT(is_approximately_equal(totalPower, expectedPower, std::abs(expectedPower) * 1e-4));

    std::vector<std::string> boxLines = readLines("wind_box_step_000000.csv");
    EXPECT_EQUAL(boxLines[0], std::string("lon,lat,vel"));
    EXPECT_EQUAL(boxLines.size(), std::size_t(101));  // 100 points + header

    auto uView             = atlas::array::make_view<double, 2>(test::sharedUField());
    auto vView             = atlas::array::make_view<double, 2>(test::sharedVField());
    double expectedWindMag = std::sqrt(uView(0, 0) * uView(0, 0) + vView(0, 0) * vView(0, 0));
    EXPECT(is_approximately_equal(lastCsvField(boxLines[1]), expectedWindMag, 1e-4));
    EXPECT(is_approximately_equal(lastCsvField(boxLines.back()), expectedWindMag, 1e-4));

    std::remove("wind_turbine_power.csv");
    std::remove("wind_box_step_000000.csv");
}

}  // namespace test

int main(int argc, char** argv) {
    // Local to main(), not file-scope statics, so they're destroyed on ordinary stack unwind.
    WFPModelBuilder<DummyCoupledModel> dummyCoupledModelBuilder;
    WFPModelBuilder<DummyUncoupledModel> dummyUncoupledModelBuilder;
    int result = run_tests(argc, argv);

    // Mirrors what a real Manager/PluginHandler::teardown() would do: releaseData() drops each PluginCore's
    // grabbed ModelDataView (which co-owns atlas::Field handles) here, while atlas is still alive, instead of
    // at true program exit — see PluginCore::releaseData()'s doc comment (PLUME-82) for why that matters.
    sharedCoupledPluginCore().releaseData();
    sharedUncoupledPluginCore().releaseData();

    // Detach before destroying the ledger so ~ModelData() doesn't hold a dangling pointer to it.
    sharedCoupledModelData().detachWritebackLedger();

    // sharedAuth()/sharedLedger() are deliberately never auto-destroyed. Owning them here in local unique_ptrs,
    // declared in this order, destroys the ledger first (it references auth) via ordinary RAII on ordinary stack unwind
    // before real static teardown begins.
    std::unique_ptr<plume::WriteAuthorisation> authOwner(&sharedAuth());
    std::unique_ptr<plume::coupling::WriteBackLedger> ledgerOwner(&sharedLedger());

    return result;
}
