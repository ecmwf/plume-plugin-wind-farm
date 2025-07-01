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

#include <cmath>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#include "atlas/runtime/Log.h"
#include "point.h"


using atlas::Log;


namespace wind_farm_plugin {

/**
 * @brief Generic class to handle basic vector algebra
 *
 * @tparam T
 */
template <typename T>
class Vector3D {

public:
    Vector3D(T x = 0, T y = 0, T z = 0) : x(x), y(y), z(z) {}

    // Scalar (dot) product
    T dot(const Vector3D& v) const { return x * v.x + y * v.y + z * v.z; }

    // Cross product
    Vector3D cross(const Vector3D& v) const {
        return Vector3D(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }

    // Magnitude (length of vector)
    T magnitude() const { return std::sqrt(x * x + y * y + z * z); }

    // Normalise vector (unit vector)
    Vector3D normalise() const {
        T mag = magnitude();
        return (mag > 0) ? Vector3D(x / mag, y / mag, z / mag) : Vector3D(0, 0, 0);
    }

    Vector3D rotateAroundAxis(const Vector3D& axis, T angle) const {
        Vector3D this_par  = axis.normalise() * this->dot(axis.normalise());
        Vector3D this_perp = *this - this_par;
        Vector3D cr        = this_par.cross(this_perp).normalise();

        T cosA = std::cos(angle);
        T sinA = std::sin(angle);

        Vector3D this_perp_cos = this_perp * cosA;
        Vector3D this_perp_sin = cr * this_perp.magnitude() * sinA;

        return this_perp_cos + this_perp_sin + this_par;
    }

    Vector3D operator*(T scalar) const { return Vector3D(x * scalar, y * scalar, z * scalar); }

    Vector3D operator+(const Vector3D& v) const { return Vector3D(x + v.x, y + v.y, z + v.z); }

    Vector3D operator-(const Vector3D& v) const { return Vector3D(x - v.x, y - v.y, z - v.z); }

    // get x component
    T getX() const { return x; }

    // get y component
    T getY() const { return y; }

    // get z component
    T getZ() const { return z; }

    // Print vector
    void print() const { std::cout << "(" << x << ", " << y << ", " << z << ")" << std::endl; }

    // operator <<
    friend std::ostream& operator<<(std::ostream& os, const Vector3D& v) {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return os;
    }

private:
    T x, y, z;
};

using Vec3  = Vector3D<double>;
using Vec3F = Vector3D<float>;


/**
 * @brief Distance [m] between two points on Earth surface (greater circle)
 *
 * @param lat1
 * @param lon1
 * @param lat2
 * @param lon2
 * @return double
 */
double earthDistance(double lat1, double lon1, double lat2, double lon2);


/**
 * @brief from lon/lat to x/y (approx)
 *
 * @param lon1
 * @param lat1
 * @param lon2
 * @param lat2
 * @return std::pair<double,double>
 */
std::pair<double, double> lonLat2xy(double lon1, double lat1, double lon2, double lat2);


/**
 * @brief export wind points to CSV file
 *
 * @param points
 * @param filename
 */
void exportWindPoints(const std::vector<WindPoint>& points, const std::string& filename);


}  // namespace wind_farm_plugin