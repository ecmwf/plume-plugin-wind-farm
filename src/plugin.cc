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

#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
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

    // Two-way coupling is INFERRED, Two independent signals, either sufficient:
    //  (1) config intent: wind_farm_model has a "target_param" key.
    //  (2) actual negotiation: the target param is present in modelData(). Presence alone is proof of a
    //      successful *writable* grant here, not just of existence.
    eckit::LocalConfiguration wfModelConf = config_.getSubConfiguration("wind_farm_model");
    const bool targetParamConfigured      = wfModelConf.has("target_param");
    const bool modelSupportsCoupling      = windFarm_.supportsCoupling();
    const bool targetParamNegotiated =
        modelSupportsCoupling && modelData().hasParameter(windFarm_.couplingTargetParam());

    twoWayEnabled_ = targetParamConfigured || targetParamNegotiated;

    // Give the user precise error messages to work with if they misconfigure two-way coupling.
    if (twoWayEnabled_) {

        // Catches signal (1) firing on a model that can't honour it, e.g. leftover target_param key under name: jensen.
        if (!modelSupportsCoupling) {
            throw eckit::UserError("wind_farm_model.target_param is configured, but model '" +
                                       wfModelConf.getString("name") + "' does not support two-way coupling.",
                                   Here());
        }

        // The model supports coupling, but signal (2) didn't confirm the write was actually negotiated.
        if (!targetParamNegotiated) {
            throw eckit::UserError("Two-way coupling requires a writable '" + windFarm_.couplingTargetParam() +
                                       "' parameter in the plugin's requested parameters.",
                                   Here());
        }

        // wind_farm_box's n_lon/n_lat are meaningless once coupled, can flag ill config from a one-way setup.
        if (config_.has("wind_farm_box")) {
            eckit::LocalConfiguration boxConf = config_.getSubConfiguration("wind_farm_box");
            if (boxConf.has("n_lon") || boxConf.has("n_lat")) {
                throw eckit::UserError(
                    "wind_farm_box has 'n_lon'/'n_lat' configured, but two-way coupling is active — those are "
                    "only used by the engineering wake-model mesh, which two-way coupling never samples. Remove "
                    "them.",
                    Here());
            }
        }

        if (exportWindBoxEnabled_) {
            Log::info() << "Two-way coupling is active — 'export_wind_box' will export the host model's real "
                           "wind at the native grid points falling inside wind_farm_box's bounds."
                        << std::endl;
        }
    }

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

    // setup 2-way coupling invariants, if enabled.
    if (twoWayEnabled_) {
        windFarm_.initialiseCoupling(*windMap_);
    }

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

    // First entry-point: write back location, apply the coupling model.
    if (twoWayEnabled_ && modelData().isUpdated(windFarm_.couplingTargetParam())) {
        windFarm_.applyCoupling(*windMap_, modelData());
        return;
    }

    // Second entry-point (or the only entry point when two-way coupling isn't active): existing guard, unchanged.
    // Skip this sub-step if wind fields are not updated, assumption: no update of "u" means no update of "v".
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
        if (twoWayEnabled_) {
            // Host model distributed data. One file per rank, skipping ranks with nothing in the box
            // rather than a single rank-0 write that would require gathering all the data.
            const WindFarm::BoxBounds& bounds = windFarm_.windFarmBoxBounds();
            std::vector<WindSample> samples =
                windMap_->windInBox(bounds.latMin, bounds.latMax, bounds.lonMin, bounds.lonMax);
            if (!samples.empty()) {
                std::string filename = assembleFilename(windFilenamePrefix_, timeStep, eckit::mpi::comm().rank());
                exportWindSamples(samples, filename);
            }
        }
        else {
            std::vector<WindPoint> windBoxPoints = windFarm_.computeWindBox(*windMap_);
            if (!eckit::mpi::comm().rank()) {
                std::string filename = assembleFilename(windFilenamePrefix_, timeStep);
                exportWindPoints(windBoxPoints, filename);
            }
        }
    }
}

// Assemble a filename with a given prefix and (optionally) time step
std::string WindFarmPluginCore::assembleFilename(const std::string& prefix, std::optional<int> timeStep,
                                                 std::optional<size_t> rank) const {
    std::ostringstream oss;
    oss << prefix;
    if (timeStep) {
        oss << "_step_" << std::setw(6) << std::setfill('0') << *timeStep;
    }
    if (rank) {
        oss << "_rank_" << std::setw(4) << std::setfill('0') << *rank;
    }
    oss << ".csv";
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
