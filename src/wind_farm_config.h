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

#include <vector>

#include "eckit/config/LocalConfiguration.h"

namespace wind_farm_plugin {

class WindFarmConfig {
public:
    const std::vector<eckit::LocalConfiguration>& turbines() const;

    const eckit::LocalConfiguration& turbineDefaults() const;

    void clear();

    void reserveTurbines(size_t size);

    void addTurbine(const eckit::LocalConfiguration& turbine);

    void setTurbineDefaults(const eckit::LocalConfiguration& defaults);

private:
    std::vector<eckit::LocalConfiguration> turbines_{};
    eckit::LocalConfiguration turbineDefaults_{};
};

}  // namespace wind_farm_plugin
