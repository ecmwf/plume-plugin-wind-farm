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

#pragma once

#include <iostream>
#include <string>

#include "atlas/field/Field.h"

#include "plume/Plugin.h"
#include "plume/PluginCore.h"

#include "git_sha1.h"
#include "version.h"

#include "wind_farm.h"
#include "wind_map.h"

namespace wind_farm_plugin {


class WindFarmPluginCore final : public plume::PluginCore {

public:
    WindFarmPluginCore(const eckit::Configuration& conf);

    ~WindFarmPluginCore() = default;

    /**
     * @brief setup the plugin
     *
     */
    void setup() override;

    /**
     * @brief run the plugin
     *
     */
    void run() override;

    /**
     * @brief teardown the plugin
     *
     */
    void teardown() override {
        // nothing to do here..
    };

    constexpr static const char* type() { return "WindFarmPlugin"; }

private:

    // Assemble a filename with a given prefix and (optionally) time step
    std::string assembleFilename(const std::string& prefix, std::optional<int> timeStep) const;

    // When append mode is enabled, check whether the (single) wind turbine power
    // output file already exists on disk.
    bool windTurbinePowerAppendFileExists() const;

private:
    // wind map
    std::unique_ptr<WindMap> windMap_;

    // Wind farm
    WindFarm windFarm_;

    // config
    eckit::LocalConfiguration config_;

    // filename prefix for wind output files
    std::string windFilenamePrefix_;

    // filename prefix for wind turbine power output files
    std::string windTurbinePowerFilenamePrefix_;

    // flag for computing power
    bool computePowerEnabled_;

    // flag for exporting wind box
    bool exportWindBoxEnabled_;

    // flag for exporting wind turbine power
    bool exportWtPowerEnabled_;

    // flag for appending wind turbine power to a single file (instead of per-step files)
    bool exportWtPowerAppend_;

};
// ------------------------------------------------------

// ------------------------------------------------------
class WindFarmPlugin final : public plume::Plugin {

public:
    WindFarmPlugin();

    ~WindFarmPlugin() = default;

    /**
     * @brief Negotiation with plume manager
     *
     * @return plume::Protocol
     */
    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.require<int>("NSTEP");
        protocol.require<atlas::Field>("100u");
        protocol.require<atlas::Field>("100v");
        return protocol;
    }

    // Return the static instance
    static const WindFarmPlugin& instance();

    std::string version() const override { return version(); }

    std::string gitsha1(unsigned int count) const override { return gitsha1(7); }

    std::string plugincoreName() const override { return WindFarmPluginCore::type(); }
};
// ------------------------------------------------------

}  // namespace wind_farm_plugin
