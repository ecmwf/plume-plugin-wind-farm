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
#include <sstream>
#include <string>

#include "eckit/mpi/Comm.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/runtime/Log.h"

#include "plugin.h"
#include "utils.h"


using atlas::Log;

namespace wind_farm_plugin {


// ----------------- WindFarmPluginCore ------------------
static plume::PluginCoreBuilder<WindFarmPluginCore> WindFarmPluginCoreBuilder;

WindFarmPluginCore::WindFarmPluginCore(const eckit::Configuration& conf) :
    PluginCore(conf), windFarm_(conf), config_{conf} {
    // do nothing
}

void WindFarmPluginCore::setup() {

    atlas::Field fieldU = modelData().getAtlasFieldShared("100u");
    atlas::Field fieldV = modelData().getAtlasFieldShared("100v");

    // check that in the configuration, either "compute_power" or "export_wind_box" is set to true
    if (!config_.getBool("compute_power", true) && !config_.getBool("export_wind_box", false)) {
        Log::warning() << "Neither 'compute_power' nor 'export_wind_box' is set to true!" << std::endl;
    }

    // wind speed output filename prefix
    windFilenamePrefix_ = config_.getString("wind_box_filename_prefix", "wind_box_at_step_");

    // setup wind map
    windMap_ = std::make_unique<WindMap>(fieldU, fieldV);

    // setup wind turbines
    windFarm_.setupWindTurbines(windMap_->lonlat());
};


void WindFarmPluginCore::run() {

    int timeStep = modelData().getInt("NSTEP");
    Log::info() << "Step: " << timeStep << ") running WindFarmPluginCore.." << std::endl;

    if (config_.getBool("compute_power", true)) {
        double power = windFarm_.computePower(*windMap_);
        Log::info() << " --->>> Power output: " << power << std::endl;
    }

    if (config_.getBool("export_wind_box", false)) {
        std::vector<WindPoint> windBoxPoints = windFarm_.computeWindBox(*windMap_);
        
        // append the step to the filename
        std::ostringstream oss;
        oss << std::setw(3) << std::setfill('0') << timeStep;
        std::string filename = windFilenamePrefix_ + oss.str() + ".csv";

        // export the wind points from root rank)
        if (!eckit::mpi::comm().rank()) {
            exportWindPoints(windBoxPoints, filename);
        }
    }
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
