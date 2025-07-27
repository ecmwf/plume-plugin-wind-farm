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

#include "const_wind_curve.h"
#include "eckit/exception/Exceptions.h"


namespace wind_farm_plugin {

const std::string ConstWindCurve::type_ = "constant";

ConstWindCurve::ConstWindCurve(const eckit::Configuration& config) : Curve(config) {
    if (!config.has("value")) {
        throw eckit::BadParameter("The 'value' parameter is required for ConstWindCurve", Here());
    }
    y_value_ = config.getDouble("value");

    // Optional parameters for cut-in and cut-out wind speeds
    cutInWindSpeed_ = std::nullopt;
    if (config.has("cutin_wind_speed")) {
        cutInWindSpeed_ = config.getDouble("cutin_wind_speed");
    }

    cutOutWindSpeed_ = std::nullopt;
    if (config.has("cutout_wind_speed")) {
        cutOutWindSpeed_ = config.getDouble("cutout_wind_speed");
    }

}

double ConstWindCurve::value(double x_value) const {

    // If cut-in or cut-out wind speeds are defined, apply them
    if (cutInWindSpeed_ && x_value < *cutInWindSpeed_) {
        return 0.0; // Below cut-in wind speed, coefficient is zero
    }
    if (cutOutWindSpeed_ && x_value > *cutOutWindSpeed_) {
        return 0.0; // Above cut-out wind speed, coefficient is zero
    }
    
    return y_value_;
}

// Register the constant coefficient curve in the factory
ConstWindCurve::Registrar ConstWindCurve::registrar;

}