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

#include "wind_farm_config.h"

namespace wind_farm_plugin {

const std::vector<eckit::LocalConfiguration>& WindFarmConfig::turbines() const {
    return turbines_;
}

const eckit::LocalConfiguration& WindFarmConfig::turbineDefaults() const {
    return turbineDefaults_;
}

void WindFarmConfig::clear() {
    turbines_.clear();
    turbineDefaults_ = eckit::LocalConfiguration();
}

void WindFarmConfig::reserveTurbines(size_t size) {
    turbines_.reserve(size);
}

void WindFarmConfig::addTurbine(const eckit::LocalConfiguration& turbine) {
    turbines_.emplace_back(turbine);
}

void WindFarmConfig::setTurbineDefaults(const eckit::LocalConfiguration& defaults) {
    turbineDefaults_ = defaults;
}

}  // namespace wind_farm_plugin
