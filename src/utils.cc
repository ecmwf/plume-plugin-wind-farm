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

#include <sstream>
#include <unordered_map>

#include "eckit/exception/Exceptions.h"
#include "eckit/utils/StringTools.h"

#include "utils.h"


namespace wind_farm_plugin {

// auxiliary functions, not to be exposed in header
namespace {

constexpr double DEG2RAD = M_PI / 180.0;
constexpr double RAD2DEG = 180.0 / M_PI;

// WGS84 ellipsoid parameters
constexpr double WGS84_A  = 6378137.0; // semi-major axis [m] at equator
constexpr double WGS84_F  = 1.0 / 298.257223563; // flattening
constexpr double WGS84_E2 = WGS84_F * (2.0 - WGS84_F); // eccentricity squared
const     double WGS84_E  = std::sqrt(WGS84_E2); // eccentricity

// mercator projection of latitude (in radians) to y coordinate
double mercatorY(double latRad) {
    const double s = std::sin(latRad);
    return WGS84_A * (
        std::log(std::tan(M_PI / 4.0 + latRad / 2.0)) - // spherical Mercator
        0.5 * WGS84_E * std::log((1.0 + WGS84_E * s) / (1.0 - WGS84_E * s)) // ellipsoidal correction
    );
}

std::pair<double, double> projectMercWGS84(double lonDeg, double latDeg) {
    const double lon = lonDeg * DEG2RAD;
    const double lat = latDeg * DEG2RAD;

    const double x = WGS84_A * lon;
    const double y = mercatorY(lat);

    return {x, y};
}

std::unordered_map<std::string, std::string> parseCrsParams(const std::string& crs) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream stream(crs);
    std::string token;

    while (stream >> token) {
        if (token.empty()) {
            continue;
        }
        if (token.front() == '+') {
            token.erase(0, 1);
        }
        auto pos = token.find('=');
        if (pos == std::string::npos) {
            params[token] = "";
        }
        else {
            params[token.substr(0, pos)] = token.substr(pos + 1);
        }
    }

    return params;
}

double getParamAsDouble(const std::unordered_map<std::string, std::string>& params,
                        const std::string& key,
                        double defaultValue) {
    auto it = params.find(key);
    if (it == params.end() || it->second.empty()) {
        return defaultValue;
    }
    return std::stod(it->second);
}

} // namespace


double earthDistance(double lat1, double lon1, double lat2, double lon2) {
    double earthRad = 6371229.0;  // earth radius [m]
    double dLat     = (lat2 - lat1) * M_PI / 180.0;
    double dLon     = (lon2 - lon1) * M_PI / 180.0;
    double a        = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) * std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return earthRad * c;
}


std::pair<double, double> lonLat2xy(double lon1, double lat1,
                                    double lon2, double lat2) {
    auto p1 = projectMercWGS84(lon1, lat1);
    auto p2 = projectMercWGS84(lon2, lat2);

    return {p1.first - p2.first, p1.second - p2.second};
}

std::pair<double, double> xyToLonLat(double x, double y, const std::string& crs) {
    auto params = parseCrsParams(crs);

    auto projIt = params.find("proj");
    if (projIt == params.end() || projIt->second != "merc") {
        throw eckit::BadParameter("Unsupported CRS for xyToLonLat: " + crs, Here());
    }

    const double lon0 = getParamAsDouble(params, "lon_0", 0.0) * DEG2RAD;
    const double k0   = getParamAsDouble(params, "k", 1.0);
    const double x0   = getParamAsDouble(params, "x_0", 0.0);
    const double y0   = getParamAsDouble(params, "y_0", 0.0);

    const double lon = lon0 + (x - x0) / (k0 * WGS84_A);

    const double t = std::exp(-(y - y0) / (k0 * WGS84_A));

    // Initial spherical Mercator inverse
    double lat = M_PI / 2.0 - 2.0 * std::atan(t);

    // Iterative ellipsoidal inverse
    for (int i = 0; i < 10; ++i) {
        const double s = std::sin(lat);
        const double factor = std::pow(
            (1.0 - WGS84_E * s) / (1.0 + WGS84_E * s),
            WGS84_E / 2.0
        );

        const double newLat = M_PI / 2.0 - 2.0 * std::atan(t * factor);

        if (std::abs(newLat - lat) < 1e-14) {
            lat = newLat;
            break;
        }

        lat = newLat;
    }

    return {lon * RAD2DEG, lat * RAD2DEG};
}

std::string stripIncludePrefix(const std::string& rawValue) {
    auto trimmed = eckit::StringTools::trim(rawValue);
    const std::string includeTag = "!include";

    if (trimmed.rfind(includeTag, 0) == 0) {
        trimmed = eckit::StringTools::trim(trimmed.substr(includeTag.size()));
    }

    if (!trimmed.empty()){

        // trim double quotes "
        if (trimmed.front() == '"' and trimmed.back() == '"') {
            trimmed = eckit::StringTools::front_trim(trimmed, "\"");
            trimmed = eckit::StringTools::back_trim(trimmed, "\"");
        }

        // trim single quotes '
        if (trimmed.front() == '\'' and trimmed.back() == '\'') {
            trimmed = eckit::StringTools::front_trim(trimmed, "'");
            trimmed = eckit::StringTools::back_trim(trimmed, "'");
        }

    }

    return trimmed;
}


void exportWindPoints(const std::vector<WindPoint>& points, const std::string& filename) {

    Log::info() << "Exporting wind points to " << filename << ", size: " << points.size() << std::endl;

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw eckit::CantOpenFile( "Error opening file " + filename + " for writing", Here());
    }

    file << "lon,lat,vel" << std::endl;
    for (const auto& p : points) {
        file << p.point().lon() << "," << p.point().lat() << "," << p.wind_mag() << std::endl;
    }


    file.close();
    if (file.fail()) {
        throw eckit::CloseError("Error closing file " + filename, Here());
    }
}



double linearInterpolate(double x, const std::vector<double>& x_vals, const std::vector<double>& y_vals) {
    size_t n = x_vals.size();
    if (n != y_vals.size()) {
        throw eckit::BadValue("x_vals and y_vals must have the same size");
    }

    // if x < x_vals[0], then return y_vals[0]
    if (x <= x_vals[0]) {
        return y_vals[0];
    }

    // if x > x_vals[n-1], then return y_vals[n-1]
    if (x >= x_vals[n - 1]) {
        return y_vals[n - 1];
    }

    // Find the interval [x_i, x_{i+1}] such that x_i <= x <= x_{i+1}
    for (size_t i = 0; i < n - 1; ++i) {
        if (x_vals[i] <= x && x <= x_vals[i + 1]) {
            double x0 = x_vals[i];
            double x1 = x_vals[i + 1];
            double y0 = y_vals[i];
            double y1 = y_vals[i + 1];

            // Linear interpolation formula
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        }
    }

}

// export wind turbine powers to CSV file
void exportWindTurbinePowers(const std::vector<LatLonValue>& powers, const std::string& filename,
                             std::optional<int> step) {

    Log::info() << "Exporting wind turbine powers to " << filename << ", size: " << powers.size()
                << (step ? " (step " + std::to_string(*step) + ", append mode)" : "") << std::endl;

    const bool appendMode = step.has_value();

    // When appending, only write the header if the file does not already exist
    bool writeHeader = true;
    if (appendMode) {
        std::ifstream existing(filename);
        writeHeader = !existing.good();
    }

    std::ofstream file(filename, appendMode ? std::ios::app : std::ios::out);
    if (!file.is_open()) {
        throw eckit::CantOpenFile( "Error opening file " + filename + " for writing", Here());
    }

    if (writeHeader) {
        if (appendMode) {
            file << "step,lat,lon,power" << std::endl;
        }
        else {
            file << "lat,lon,power" << std::endl;
        }
    }

    for (const auto& turbinePower : powers) {
        if (appendMode) {
            file << *step << ",";
        }
        file << turbinePower.lat() << "," << turbinePower.lon() << "," << turbinePower.value() << std::endl;
    }

    file.close();
    if (file.fail()) {
        throw eckit::CloseError("Error closing file " + filename, Here());
    }
}



}  // namespace wind_farm_plugin