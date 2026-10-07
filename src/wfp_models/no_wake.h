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

#include "eckit/config/Configuration.h"

#include "wfp_model.h"


namespace wind_farm_plugin {


/**
 * @brief Wind farm model with no wake effect.
 *
 * All behaviour is WFPModel's own default (local wind at each turbine, farm-average wind at
 * arbitrary points, no coupling) — this class exists only so that wind_farm_model.name: no_wake
 * still resolves to a concrete, registered type via the factory.
 */
class NoWake final : public WFPModel {

public:
    NoWake(const eckit::Configuration& conf);

    constexpr static const char* type() { return "no_wake"; }
};


}  // namespace wind_farm_plugin
