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
        Log::error() << "Error opening file!" << std::endl;
        return;
    }

    file << "lon,lat,vel" << std::endl;
    for (const auto& p : points) {
        file << p.point().lon() << "," << p.point().lat() << "," << p.wind_mag() << std::endl;
    }

    file.close();
}


}  // namespace wind_farm_plugin