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
 * @brief A point with an associated value
 *
 */
class LatLonValue : public LatLonPoint {

public:
    LatLonValue(double lat = 0.0, double lon = 0.0, double valueIn = 0.0) :
        LatLonPoint(lat, lon), value_{valueIn} {}

    double value() const { return value_; }
    void setValue(double valueIn) { value_ = valueIn; }

private:
    double value_;
};


/**
 * @brief Associates a lat/lon point to wind velocity.
 * It is used to store the wind components (e.g. computed by the wind farm
 * model) at specific locations and is not necessarily related to the grid points
 * of the atlas wind fields.
 *
 * @par Lifetime: point_ is a reference by design since this class's first commit, passes a LatLonPoint/WindTurbine
 * already owned elsewhere for the call's duration, so it has never dangled. Trades a small size saving
 * (ref + 2 doubles vs. 4) for requiring producers to have an owned point to reference — which WindSample below doesn't.
 *
 * @todo To discuss with reviewers: worth collapsing into one value-owning type now that WindSample exists?
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


/**
 * @brief A grid point's real (not wind-farm-model-derived) wind, as actually held by the host.
 *
 * Owns its values, unlike WindPoint: WindMap::windInBox() reads lat/lon/u/v straight off an atlas ArrayView,
 * with no owned LatLonPoint to reference. See WindPoint's @todo re: collapsing the two.
 */
struct WindSample {
    double lat = 0.0;
    double lon = 0.0;
    double u   = 0.0;
    double v   = 0.0;
};

}  // namespace wind_farm_plugin