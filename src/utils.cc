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

#include "eckit/exception/Exceptions.h"

#include "utils.h"


namespace wind_farm_plugin {


double earthDistance(double lat1, double lon1, double lat2, double lon2) {
    double earthRad = 6371229.0;  // earth radius [m]
    double dLat     = (lat2 - lat1) * M_PI / 180.0;
    double dLon     = (lon2 - lon1) * M_PI / 180.0;
    double a        = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) * std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return earthRad * c;
}


std::pair<double, double> lonLat2xy(double lon1, double lat1, double lon2, double lat2) {

    double earthRad = 6371229.0;  // earth radius [m]
    double mPerDeg  = M_PI * earthRad / 180.0;

    double y = (lat1 - lat2) * mPerDeg;
    double x = (lon1 - lon2) * mPerDeg * std::cos(lat1 * M_PI / 180.0);

    return std::make_pair(x, y);
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