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


// ---- wind turbine
WindTurbine::WindTurbine(int id, const eckit::Configuration& conf, size_t nearestPointID, double minDistanceLocal,
                         size_t minRankGlob, const eckit::Configuration& defaults) :
    LatLonPoint(conf.getDouble("lat"), conf.getDouble("lon")) {

    // WT name
    ID_ = id;

    hubHeight_ = conf.getDouble("hub_height", defaults.getDouble("hub_height"));
    radius_ = conf.getDouble("radius", defaults.getDouble("radius"));
    rhoHub_ = conf.getDouble("rho_hub", defaults.getDouble("rho_hub"));

    // if the wt config contains either power, use it
    if (conf.has("power")) {
        power_ = std::make_unique<WindTurbinePower>(conf.getSubConfiguration("power"));
    } else if (defaults.has("power")) {
        power_ = std::make_unique<WindTurbinePower>(defaults.getSubConfiguration("power"));
    } else {
        throw eckit::BadParameter("Wind turbine configuration must have 'power' defined.", Here());
    }

    // if the wt config contains either thrust, use it
    if (conf.has("thrust")) {
        thrust_ = std::make_unique<WindTurbineThrust>(conf.getSubConfiguration("thrust"));
    } else if (defaults.has("thrust")) {
        thrust_ = std::make_unique<WindTurbineThrust>(defaults.getSubConfiguration("thrust"));
    } else {
        throw eckit::BadParameter("Wind turbine configuration must have 'thrust' defined.", Here());
    }

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