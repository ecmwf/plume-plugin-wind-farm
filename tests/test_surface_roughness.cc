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
#include <set>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/testing/Test.h"
#include "eckit/types/FloatCompare.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackLedger.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/ModelData.h"
#include "plume/data/ModelDataView.h"

#include "plugin.h"
#include "test_utils.h"
#include "wind_farm.h"

using namespace eckit::testing;
using namespace wind_farm_plugin;

namespace {

// Wires just enough real ModelData/WriteBackLedger machinery to call applyCoupling() for real — no Manager, no
// negotiation, a single-consumer grant. The write lands in place in z0m field's own buffer, so the caller just
// reviews z0m field afterwards.
void applyCouplingWithLedger(WindFarm& windFarm, const WindMap& windMap, atlas::Field& z0mField) {
    plume::data::ModelData data;
    data.provideParam("z0m", &z0mField);
    plume::WriteAuthorisation auth;
    auth.grant("wind_farm_plugin", "z0m");
    plume::coupling::WriteBackLedger ledger(auth, plume::WriteBackPolicy::single_writer);
    data.enrollWritebackParams(ledger, auth);
    data.attachWritebackLedger(&ledger);
    plume::data::ModelDataView view = data.filter(std::set<std::string>{"z0m"}, "wind_farm_plugin");
    ledger.open();

    windFarm.applyCoupling(windMap, view);

    ledger.flush();
    data.acknowledgeWriteback("z0m");
    data.detachWritebackLedger();
}

// wf_roughness_constant overrides SurfaceRoughness's dynamic Frandsen-based estimate with a fixed value.
eckit::LocalConfiguration withConstantRoughness(const eckit::LocalConfiguration& coreConfig, double wfRoughness) {
    eckit::LocalConfiguration modelConfig = coreConfig.getSubConfiguration("wind_farm_model");
    modelConfig.set("wf_roughness_constant", wfRoughness);
    eckit::LocalConfiguration updated(coreConfig);
    updated.set("wind_farm_model", modelConfig);
    return updated;
}

}  // namespace

namespace test {

CASE("test_surface_roughness_single_turbine") {

    eckit::LocalConfiguration coreConfig = loadCoreConfig(getTestConfigPath());
    coreConfig.set("wind_turbines_filename", std::string(getSingleTurbineFilePath()));

    // Dynamic mode needs an inter-turbine spacing estimate (Frandsen's array framework), which doesn't
    // exist for a single-turbine farm.
    eckit::LocalConfiguration roughnessConfig = overrideModel(coreConfig, "roughness");
    WindFarm windFarm(roughnessConfig);
    windFarm.setupWindTurbines(sharedUField().functionspace().lonlat());
    EXPECT_THROWS_AS(windFarm.initialiseCoupling(sharedWindMap()), eckit::UserError);

    // Constant mode needs no inter-turbine spacing estimate, so a single-turbine farm is valid here.
    eckit::LocalConfiguration roughnessConfigCst = withConstantRoughness(overrideModel(coreConfig, "roughness"), 0.5);
    WindFarm windFarmCst(roughnessConfigCst);
    windFarmCst.setupWindTurbines(sharedUField().functionspace().lonlat());
    EXPECT_NO_THROW(windFarmCst.initialiseCoupling(sharedWindMap()));
}

CASE("test_golden_roughness_z0m_close_farm") {

    eckit::LocalConfiguration coreConfig      = loadCoreConfig(getCloseTestConfigPath());
    eckit::LocalConfiguration roughnessConfig = overrideModel(coreConfig, "roughness");

    WindFarm windFarm(roughnessConfig);
    windFarm.setupWindTurbines(sharedUField().functionspace().lonlat());
    windFarm.initialiseCoupling(sharedWindMap());

    const auto byPoint = windFarm.turbinesByGridPoint();
    EXPECT_EQUAL(byPoint.size(), 1u);
    size_t farmPointID  = byPoint.begin()->first;
    size_t otherPointID = farmPointID == 0 ? 1 : 0;

    const double z0mBackground = 0.001;
    atlas::Field z0mField      = test::createUniform2DField("z0m", z0mBackground);
    applyCouplingWithLedger(windFarm, sharedWindMap(), z0mField);

    // Frozen output: the turbine-spec-dependent estimate (spacing, Ct, hub height) is expected to keep changing as the
    // science is refined, so this is deliberately just a blind tripwire for drift, intentional or not, mirroring
    // Jensen/NoWake's golden tests. Update the literal deliberately whenever SurfaceRoughness's formula changes.
    auto z0mView = atlas::array::make_view<double, 2>(z0mField);
    EXPECT(eckit::types::is_approximately_equal(z0mView(farmPointID, 0), 0.0010001884650557603, 1e-12));
    // A point with no local turbine is never in cellData_ — must be left exactly as seeded. This is plain
    // bookkeeping, not part of the formula under iteration, so it's fine to assert directly rather than freeze.
    EXPECT(eckit::types::is_approximately_equal(z0mView(otherPointID, 0), z0mBackground, 1e-12));
}

CASE("test_surface_roughness_constant_mode_matches_configured_value") {

    // wf_roughness_constant lets users plug in a literature constant (e.g. Frandsen et al. 2009's ~0.5m)
    // directly instead of the turbine-spec-dependent estimate.
    const double wfRoughnessConstant = 0.5;

    eckit::LocalConfiguration coreConfig = loadCoreConfig(getCloseTestConfigPath());
    eckit::LocalConfiguration roughnessConfig =
        withConstantRoughness(overrideModel(coreConfig, "roughness"), wfRoughnessConstant);

    WindFarm windFarm(roughnessConfig);
    windFarm.setupWindTurbines(sharedUField().functionspace().lonlat());
    windFarm.initialiseCoupling(sharedWindMap());

    size_t pointID             = windFarm.turbinesByGridPoint().begin()->first;
    const double z0mBackground = 0.001;
    atlas::Field z0mField      = test::createUniform2DField("z0m", z0mBackground);
    applyCouplingWithLedger(windFarm, sharedWindMap(), z0mField);

    // Frozen output, same as test_golden_roughness_z0m_close_farm — no re-derivation, just a tripwire so devs
    // notice if the blend or the constant-mode wiring drifts. Update deliberately if either changes.
    auto z0mView = atlas::array::make_view<double, 2>(z0mField);
    EXPECT(eckit::types::is_approximately_equal(z0mView(pointID, 0), 0.0010011185879928279, 1e-12));
}

CASE("test_surface_roughness_increases_with_wind") {

    eckit::LocalConfiguration coreConfig      = loadCoreConfig(getCloseTestConfigPath());
    eckit::LocalConfiguration roughnessConfig = overrideModel(coreConfig, "roughness");

    auto z0mAtWind = [&](double windU, double windV) {
        WindFarm windFarm(roughnessConfig);
        atlas::Field uField = test::createUniform2DField("u", windU);
        atlas::Field vField = test::createUniform2DField("v", windV);
        windFarm.setupWindTurbines(uField.functionspace().lonlat());

        WindMap windMap(plume::data::FieldView{uField}, plume::data::FieldView{vField});
        windFarm.initialiseCoupling(windMap);

        atlas::Field z0mField = test::createUniform2DField("z0m", 0.001);
        applyCouplingWithLedger(windFarm, windMap, z0mField);

        size_t pointID = windFarm.turbinesByGridPoint().begin()->first;
        auto z0mView   = atlas::array::make_view<double, 2>(z0mField);
        return z0mView(pointID, 0);
    };

    // Both wind speeds sit on the same rising segment of the tabular thrust curve (3.3 -> 21.21, Ct 0.6 ->
    // 0.8, see native_dummy_windfarm_close.yml), so Ct(15) > Ct(4) strictly. And since wf_roughness increases
    // with Ct, z0m at the farm point must strictly increase too, not merely differ.
    double z0mLow  = z0mAtWind(4.0, 0.0);
    double z0mHigh = z0mAtWind(15.0, 0.0);
    EXPECT(z0mHigh > z0mLow + 1e-9);
}

CASE("test_target_param_without_writable_z0m_throws_via_plugin_setup") {

    eckit::LocalConfiguration coreConfig      = loadCoreConfig(getTestConfigPath());
    eckit::LocalConfiguration roughnessConfig = overrideModel(coreConfig, "roughness");
    roughnessConfig.set("wind_farm_model.target_param", "z0m");

    plume::data::ModelData data;
    data.provideParam("u;hl;110", &sharedUField());
    data.provideParam("v;hl;110", &sharedVField());
    // z0m deliberately never provided — matches the misconfiguration under test.

    WindFarmPluginCore pluginCore(roughnessConfig);
    pluginCore.grabData(data.filter({"u;hl;110", "v;hl;110"}));

    EXPECT_THROWS_AS(pluginCore.setup(), eckit::UserError);
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
