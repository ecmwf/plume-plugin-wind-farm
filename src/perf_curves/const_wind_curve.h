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

#include <functional>
#include <memory>
#include <string>
#include <optional>


#include "curve.h"
#include "curve_factory.h"

#include "eckit/config/LocalConfiguration.h"

namespace wind_farm_plugin {

/** * @brief Constant value curve.
 * This class implements a constant value curve
 * It expects a configuration of type:
 * {
 *     "type": "constant",
 *     "value": 0.5
 * }
 */
class ConstWindCurve : public Curve {

public:
    ConstWindCurve(const eckit::Configuration& config);

    double value(double x_value) const override;

private:
    static const std::string type_;

    double y_value_;  ///< The constant value

    // optional parameter for cut-in / cut-out wind speed
    std::optional<double> cutInWindSpeed_;
    std::optional<double> cutOutWindSpeed_;

    // helper function for self registration into factory
    static struct Registrar {
        Registrar() {
            CurveFactory::instance().registerBuilder(
                type_, [](const eckit::Configuration& config) -> std::unique_ptr<Curve> {
                    return std::make_unique<ConstWindCurve>(config);
                });
        }
    } registrar;

};
    
} // namespace wind_farm_plugin {
