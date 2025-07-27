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

#include "utils.h"
#include "tabular_wind_curve.h"
#include "eckit/exception/Exceptions.h"


namespace wind_farm_plugin {

const std::string TabularWindCurve::type_ = "tabular_wind_curve";

TabularWindCurve::TabularWindCurve(const eckit::Configuration& config) : Curve(config) {

    // get vector of wind speeds
    if (!config.has("wind_speeds")) {
        throw eckit::BadParameter("The 'wind_speed' parameter is required for TabularWindCurve", Here());
    }
    x_values_ = config.getDoubleVector("wind_speeds");

    // get vector of coefficients
    if (!config.has("values")) {
        throw eckit::BadParameter("The 'values' parameter is required for TabularWindCurve", Here());
    }
    y_values_ = config.getDoubleVector("values");

    // Ensure that wind speeds and coefficients are not empty
    if (x_values_.empty() || y_values_.empty()) {
        throw eckit::BadParameter("The 'wind_speeds' and 'values' cannot be empty", Here());
    }

    // wind_speed and values must have the same size
    if (x_values_.size() != y_values_.size()) {
        throw eckit::BadParameter("The 'wind_speeds' and 'values' must have the same size", Here());
    }

    // Ensure that there are at least two points for interpolation
    if (x_values_.size() < 2) {
        throw eckit::BadParameter("The 'wind_speeds' and 'values' be of size >= 2", Here());
    }    

    // Ensure wind speeds are sorted
    if (!std::is_sorted(x_values_.begin(), x_values_.end())) {
        throw eckit::BadParameter("The 'wind_speeds' must be sorted in ascending order", Here());
    }


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

double TabularWindCurve::value(double x_value) const {
    
    // linear interpolation of coefficients based on wind speed

    // Check if x_value is below cut-in wind speed
    if (cutInWindSpeed_ && x_value < *cutInWindSpeed_) {
        return 0.0; // Return 0 for wind speeds below cut-in speed
    }

    // Check if x_value is above cut-out wind speed
    if (cutOutWindSpeed_ && x_value > *cutOutWindSpeed_) {
        return 0.0; // Return 0 for wind speeds above cut-out speed
    }

    return linearInterpolate(x_value, x_values_, y_values_);
}

// Register the constant coefficient curve in the factory
TabularWindCurve::Registrar TabularWindCurve::registrar;

}