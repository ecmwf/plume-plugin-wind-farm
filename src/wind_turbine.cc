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

#include <math.h>
#include <iostream>

#include "wind_turbine.h"

namespace wind_farm_plugin {


// ---- wind turbine
WindTurbine::WindTurbine(int id, const eckit::Configuration& conf, size_t nearestPointID, double minDistanceLocal,
                         size_t minRankGlob, const eckit::Configuration& defaults) :
    LatLonPoint(conf.getDouble("lat"), conf.getDouble("lon")) {

    // WT name
    ID_ = id;

    hubHeight_ = conf.getDouble("hub_height", defaults.getDouble("hub_height"));
    radius_ = conf.getDouble("radius", defaults.getDouble("radius"));
    Cp_ = conf.getDouble("power_coeff", defaults.getDouble("power_coeff"));
    Ct_ = conf.getDouble("thrust_coeff", defaults.getDouble("thrust_coeff"));
    cutoffMax_ = conf.getDouble("cutoff_max", defaults.getDouble("cutoff_max"));
    cutoffMin_ = conf.getDouble("cutoff_min", defaults.getDouble("cutoff_min"));
    rhoHub_ = conf.getDouble("rho_hub", defaults.getDouble("rho_hub"));

    // Nearest Grid Point
    nearestPointID_   = nearestPointID;
    minDistanceLocal_ = minDistanceLocal;
    minRankGlob_      = minRankGlob;
}


WindTurbine::WindTurbine(int id, double lat, double lon, double hubHeight, double radius, double Cp, double Ct,
                         double cutoffMax, double cutoffMin, double rhoHub, size_t nearestPointID,
                         double minDistanceLocal, size_t minRankGlob) :
    ID_{id},
    hubHeight_{hubHeight},
    radius_{radius},
    Cp_{Cp},
    Ct_{Ct},
    cutoffMax_{cutoffMax},
    cutoffMin_{cutoffMin},
    rhoHub_{rhoHub},
    nearestPointID_{nearestPointID},
    minDistanceLocal_{minDistanceLocal},
    minRankGlob_{minRankGlob},
    LatLonPoint(lat, lon) {}


double WindTurbine::computePower(const double windMag) const {

    // calc power output
    double powerWatts;

    // min/max cutoff speed
    if (windMag < cutoffMin_) {
        powerWatts = 0.0;
    }
    else if (windMag > cutoffMax_) {
        powerWatts = 0.0;
    }
    else {
        double diskArea = M_PI * radius_ * radius_;
        powerWatts      = 0.5 * rhoHub_ * diskArea * pow(windMag, 3) * Cp_;
    }
    return powerWatts;
}


double WindTurbine::computePower(const double windU, const double windV) const {
    double umag = sqrt(windU * windU + windV * windV);
    return computePower(umag);
}


/**
 * @brief pretty print the wind turbine
 *
 * @param os
 * @param wt
 * @return std::ostream&
 */
std::ostream& operator<<(std::ostream& os, const WindTurbine& wt) {
    os << "Wind Turbine ID: " << wt.ID_ << std::endl;
    os << "    Coordinates: " << wt.lat() << ", " << wt.lon() << std::endl;
    os << "    Hub Height: " << wt.hubHeight_ << std::endl;
    os << "    Rotor Radius: " << wt.radius_ << std::endl;
    os << "    Power Coefficient: " << wt.Cp_ << std::endl;
    os << "    Thrust Coefficient: " << wt.Ct_ << std::endl;
    os << "    Cutoff Max: " << wt.cutoffMax_ << std::endl;
    os << "    Cutoff Min: " << wt.cutoffMin_ << std::endl;
    os << "    Rho Hub: " << wt.rhoHub_ << std::endl;
    os << "    Nearest Point ID: " << wt.nearestPointID_ << std::endl;
    os << "    Min Distance Local: " << wt.minDistanceLocal_ << std::endl;
    os << "    Min Rank Glob: " << wt.minRankGlob_ << std::endl;
    return os;
}

}  // namespace wind_farm_plugin