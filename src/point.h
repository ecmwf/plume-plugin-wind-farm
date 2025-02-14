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

#include <math.h>
#include <iostream>

namespace wind_farm_plugin {

/**
 * @brief LatLonPoint class
 *
 */
class LatLonPoint {

public:
    LatLonPoint(double lat, double lon) : lat_{lat}, lon_{lon} {}
    virtual ~LatLonPoint() = default;

    double lat() const { return lat_; }
    double lon() const { return lon_; }

    friend std::ostream& operator<<(std::ostream& os, const LatLonPoint& wt);

private:
    double lat_;
    double lon_;
};


/**
 * @brief Associates a lat/lon point to wind velocity.
 * It is used to store the wind components (e.g. computed by the wind farm 
 * model) at specific locations and is not necessarily related to the grid points 
 * of the atlas wind fields.
 *
 */
class WindPoint {

public:
    WindPoint(const LatLonPoint& point, double wind_u, double wind_v) :
        point_{point}, wind_u_{wind_u}, wind_v_{wind_v} {}

    const LatLonPoint& point() const { return point_; }

    double wind_mag() const { return std::sqrt(wind_u_ * wind_u_ + wind_v_ * wind_v_); }

    double wind_u() const { return wind_u_; }

    double wind_v() const { return wind_v_; }

    friend std::ostream& operator<<(std::ostream& os, const WindPoint& wt);

private:
    const LatLonPoint& point_;
    double wind_u_;
    double wind_v_;
};

}  // namespace wind_farm_plugin