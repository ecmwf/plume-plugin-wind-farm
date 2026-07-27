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

#include <iomanip>
#include <iostream>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "eckit/mpi/Comm.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/runtime/Log.h"

#include "plugin.h"
#include "utils.h"


namespace wind_farm_plugin {


// ----------------- WindFarmPluginCore ------------------
static plume::PluginCoreBuilder<WindFarmPluginCore> WindFarmPluginCoreBuilder;

WindFarmPluginCore::WindFarmPluginCore(const eckit::Configuration& conf) :
    PluginCore(conf), windFarm_(conf), config_{conf} {
    // do nothing
}

void WindFarmPluginCore::setup() {

    computePowerEnabled_              = config_.getBool("compute_power", true);

    // wind speed output
    exportWindBoxEnabled_             = config_.getBool("export_wind_box", false);
    windFilenamePrefix_               = config_.getString("wind_box_filename_prefix", "wind_box");

    // wind turbine power output
    exportWtPowerEnabled_             = config_.getBool("export_wind_turbine_power", false);
    exportWtPowerAppend_              = config_.getBool("export_wind_turbine_append", true);
    windTurbinePowerFilenamePrefix_   = config_.getString("wind_turbine_power_filename_prefix", "wind_turbine_power");

    auto fieldU = modelData().getParam<atlas::Field>("u", config_.getString("wind_field_height"));
    auto fieldV = modelData().getParam<atlas::Field>("v", config_.getString("wind_field_height"));

    // check that in the configuration, at least one output option is enabled
    if (!computePowerEnabled_ && !exportWindBoxEnabled_ && !exportWtPowerEnabled_) {
        Log::warning()
            << "None of 'compute_power', 'export_wind_box', or 'export_wind_turbine_power' is set to true!"
            << std::endl;
    }

    // setup wind map
    windMap_ = std::make_unique<WindMap>(fieldU, fieldV);

    // setup wind turbines
    windFarm_.setupWindTurbines(windMap_->lonlat());

    // When append mode is enabled, warn the user if the target file already exists,
    // since new rows will be appended to pre-existing content.
    if (exportWtPowerEnabled_ && exportWtPowerAppend_ && !eckit::mpi::comm().rank()
        && windTurbinePowerAppendFileExists()) {
        Log::warning() << "WARNING: Wind turbine power output file '"
                       << assembleFilename(windTurbinePowerFilenamePrefix_, std::nullopt)
                       << "' already exists! - new rows will be appended to it." << std::endl;
    }
};


void WindFarmPluginCore::run() {

    // skip this step if wind fields are not updated, assumption: no update of "u" means no update of "v"
    if (!modelData().isUpdated("u", config_.getString("wind_field_height"))) {
        return;
    }

    // get current time step
    int timeStep = modelData().getParam<int>("NSTEP");
    Log::info() << "Step: " << timeStep << ") running WindFarmPluginCore.." << std::endl;

    // Power output per turbine (used for computing total power and exporting, if enabled)
    std::vector<LatLonValue> windTurbinePowers;
    if (computePowerEnabled_ || exportWtPowerEnabled_) {
        windTurbinePowers = windFarm_.computePowerByTurbine(*windMap_);
    }

    // Compute total power and print summary, if enabled
    if (computePowerEnabled_) {
        const double power = std::accumulate(windTurbinePowers.begin(), windTurbinePowers.end(), 0.0,
                                             [](double sum, const LatLonValue& turbinePower) {
                                                 return sum + turbinePower.value();
                                             });
        Log::info() << " --->>> Power output: " << power << std::endl;
    }

    // Export wind turbine power, if enabled
    if (exportWtPowerEnabled_ && !eckit::mpi::comm().rank()) {
        if (exportWtPowerAppend_) {
            // Append all steps to a single file: <prefix>.csv
            std::string filename = assembleFilename(windTurbinePowerFilenamePrefix_, std::nullopt);
            exportWindTurbinePowers(windTurbinePowers, filename, timeStep);
        }
        else {
            std::string filename = assembleFilename(windTurbinePowerFilenamePrefix_, timeStep);
            exportWindTurbinePowers(windTurbinePowers, filename);
        }
    }

    // Export wind box, if enabled
    if (exportWindBoxEnabled_) {
        std::vector<WindPoint> windBoxPoints = windFarm_.computeWindBox(*windMap_);        
        if (!eckit::mpi::comm().rank()) {
            std::string filename = assembleFilename(windFilenamePrefix_, timeStep);
            exportWindPoints(windBoxPoints, filename);
        }
    }
}

// Assemble a filename with a given prefix and (optionally) time step
std::string WindFarmPluginCore::assembleFilename(const std::string& prefix, std::optional<int> timeStep) const {
    std::ostringstream oss;
    if (timeStep) {
        oss << prefix << "_step_" << std::setw(6) << std::setfill('0') << *timeStep << ".csv";
    } else {
        oss << prefix << ".csv";
    }
    return oss.str();
}

// Check whether the (single) wind turbine power output file used in append mode
// already exists on disk.
bool WindFarmPluginCore::windTurbinePowerAppendFileExists() const {
    const std::string filename = assembleFilename(windTurbinePowerFilenamePrefix_, std::nullopt);
    std::ifstream f(filename);
    return f.good();
}
// ------------------------------------------------------

// ------------------------------------------------------
REGISTER_LIBRARY(WindFarmPlugin)

WindFarmPlugin::WindFarmPlugin() : Plugin("WindFarmPlugin") {};

const WindFarmPlugin& WindFarmPlugin::instance() {
    static WindFarmPlugin instance;
    return instance;
}
// ------------------------------------------------------

}  // namespace wind_farm_plugin
