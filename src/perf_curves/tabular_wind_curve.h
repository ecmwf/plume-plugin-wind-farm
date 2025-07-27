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
#include <vector>
#include <optional>

#include "curve.h"
#include "curve_factory.h"

#include "eckit/config/LocalConfiguration.h"

namespace wind_farm_plugin {

/**
 * @brief Curve from tabular data.
 * This class implement a curve defined by tabular data (linearly interpolated)
 * It expects a configuration of type:
 * {
 *     "type": "tabular_wind_curve",
 *     "wind_speeds": [0, 1, 2, 3, 4, 5], // Wind speeds for which coefficients are defined
 *     "values": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6] // Corresponding coefficients for the wind speeds
 * }

 */
class TabularWindCurve : public Curve {

public:
    TabularWindCurve(const eckit::Configuration& config);

    double value(double x_value) const override;

private:
    static const std::string type_;

    std::vector<double> x_values_;
    std::vector<double> y_values_;

    // optional parameter for cut-in wind speed
    std::optional<double> cutInWindSpeed_;
    std::optional<double> cutOutWindSpeed_;

    // helper function for self registration into factory
    static struct Registrar {
        Registrar() {
            CurveFactory::instance().registerBuilder(
                type_, [](const eckit::Configuration& config) -> std::unique_ptr<Curve> {
                    return std::make_unique<TabularWindCurve>(config);
                });
        }
    } registrar;

};

} // namespace wind_farm_plugin {
