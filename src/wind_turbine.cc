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

#include "eckit/config/Configuration.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"

#include "utils.h"
#include "wind_turbine.h"

namespace wind_farm_plugin {

namespace {
const eckit::Configuration& selectConfig(const eckit::Configuration& conf, const eckit::Configuration& defaults,
                                         const std::string& key) {
    if (conf.has(key)) {
        return conf;
    }
    if (defaults.has(key)) {
        return defaults;
    }
    throw eckit::BadParameter("Wind turbine configuration must have '" + key + "' defined.", Here());
}
}  // namespace

// ---- wind turbine
WindTurbine::WindTurbine(int id, const eckit::Configuration& conf, size_t nearestPointID, double minDistanceLocal,
                         size_t minRankGlob, const eckit::Configuration& defaults) :
    LatLonPoint(conf.getDouble("lat"), conf.getDouble("lon")) {

    // WT name
    ID_ = id;

    hubHeight_ = selectConfig(conf, defaults, "hub_height").getDouble("hub_height");
    radius_    = selectConfig(conf, defaults, "radius").getDouble("radius");
    rhoHub_    = selectConfig(conf, defaults, "rho_hub").getDouble("rho_hub");

    power_  = std::make_unique<WindTurbinePower>(selectConfig(conf, defaults, "power").getSubConfiguration("power"));
    thrust_ = std::make_unique<WindTurbineThrust>(selectConfig(conf, defaults, "thrust").getSubConfiguration("thrust"));

    // Nearest Grid Point
    nearestPointID_   = nearestPointID;
    minDistanceLocal_ = minDistanceLocal;
    minRankGlob_      = minRankGlob;
}


double WindTurbine::computePower(const double windMag) const {
    return power_->calculate(windMag, rhoHub_, radius_);
}


double WindTurbine::Ct(double vmag) const {
    return thrust_->calculate_coeff(vmag, rhoHub_, radius_);
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
    os << "    Power Curve: " << *(wt.power_) << std::endl;
    os << "    Thrust Curve: " << *(wt.thrust_) << std::endl;
    os << "    Rho Hub: " << wt.rhoHub_ << std::endl;
    os << "    Nearest Point ID: " << wt.nearestPointID_ << std::endl;
    os << "    Min Distance Local: " << wt.minDistanceLocal_ << std::endl;
    os << "    Min Rank Glob: " << wt.minRankGlob_ << std::endl;
    return os;
}



// -------------------------------------------------------------------------------------

}  // namespace wind_farm_plugin