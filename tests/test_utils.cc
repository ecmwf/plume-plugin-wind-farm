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


#include <cstdlib>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"

#include "plume/data/FieldAccess.h"

#include "test_utils.h"

// Deliberately NOT including eckit/testing/Test.h here: it defines run_tests()/specification() with external linkage
// (not inline), so including it in both this file and a test's own .cc causes duplicate-symbol link errors.

namespace {
const char* requireEnv(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        throw eckit::UserError(std::string("Required test environment variable not set: ") + name, Here());
    }
    return value;
}
}  // namespace

namespace test {

const char* getTestConfigPath() {
    return requireEnv("PLUME_WIND_FARM_TEST_CONFIG");
}

const char* getCloseTestConfigPath() {
    return requireEnv("PLUME_WIND_FARM_TEST_CONFIG_CLOSE");
}

const char* getSingleTurbineFilePath() {
    return requireEnv("PLUME_WIND_FARM_TEST_TURBINES_SINGLE");
}

eckit::LocalConfiguration loadCoreConfig(const char* configPathEnv) {
    eckit::PathName configPath(configPathEnv);
    eckit::YAMLConfiguration config                      = eckit::YAMLConfiguration(configPath);
    std::vector<eckit::LocalConfiguration> pluginConfigs = config.getSubConfigurations("plugins");
    return pluginConfigs[0].getSubConfiguration("core-config");
}

eckit::LocalConfiguration overrideModel(const eckit::LocalConfiguration& coreConfig, const std::string& modelName) {
    eckit::LocalConfiguration modelConfig = coreConfig.getSubConfiguration("wind_farm_model");
    modelConfig.set("name", modelName);
    eckit::LocalConfiguration updated(coreConfig);
    updated.set("wind_farm_model", modelConfig);
    return updated;
}

atlas::Field createUniform2DField(std::string name, double value) {

    long n_procs = atlas::mpi::comm().size();

    atlas::StructuredGrid grid = atlas::Grid("L200x101");
    atlas::functionspace::StructuredColumns fs;

    if (n_procs > 1) {
        atlas::grid::Distribution distribution(grid, atlas::util::Config("type", "checkerboard") | 
            atlas::util::Config("bands", n_procs));
        fs = atlas::functionspace::StructuredColumns(grid, distribution);        
    } else {
        fs = atlas::functionspace::StructuredColumns(grid);
    }

    atlas::Field field = fs.createField<double>(atlas::option::name(name) | atlas::option::levels(1));
    auto buff = atlas::array::make_view<double, 2>(field);

    for (atlas::idx_t i_pt = 0; i_pt < fs.size(); i_pt++) {
        buff(i_pt, 0) = value;
    }

    return field;
}

atlas::Field& sharedUField() {
    static atlas::Field field = createUniform2DField("u", 10.0);
    return field;
}

atlas::Field& sharedVField() {
    static atlas::Field field = createUniform2DField("v", 5.0);
    return field;
}

const wind_farm_plugin::WindMap& sharedWindMap() {
    static wind_farm_plugin::WindMap windMap(plume::data::FieldView{sharedUField()},
                                             plume::data::FieldView{sharedVField()});
    return windMap;
}

}  // namespace test