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

#include "eckit/config/LocalConfiguration.h"


namespace wind_farm_plugin {

/**
 * @brief Base class for performance curves (thrust, power curves as a function of wind speed)
 */
class Curve {

public:
    Curve(const eckit::Configuration& config = eckit::LocalConfiguration());

    virtual ~Curve() = default;

    /**
     * @brief Calculate the value of the curve at a given x_value
     *
     * @param x_value The input value for which to calculate the curve value
     * @return double The calculated value of the curve
     */
    virtual double value(double x_value) const = 0;

    friend std::ostream& operator<<(std::ostream& os, const Curve& curve) {
        os << "Curve with config: " << curve.config_;
        return os;
    }

protected:
    eckit::LocalConfiguration config_;
};


} // namespace wind_farm_plugin