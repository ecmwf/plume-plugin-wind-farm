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

#include "eckit/config/Configuration.h"
#include "eckit/config/LocalConfiguration.h"

#include "point.h"


namespace wind_farm_plugin {


/**
 * @brief Wind turbine class
 *
 */
class WindTurbine : public LatLonPoint {

public:
    WindTurbine(int id, const eckit::Configuration& conf, size_t nearestPointID, double minDistanceLocal,
                size_t minRankGlob, const eckit::Configuration& defaults = eckit::LocalConfiguration());

    WindTurbine(int id, double lat, double lon, double hubHeight, double radius, double Cp, double Ct, double cutoffMax,
                double cutoffMin, double rhoHub, size_t nearestPointID, double minDistanceLocal, size_t minRankGlob);

    /**
     * @brief wind turbine power output
     *
     * @param windMag
     * @return double
     */
    double computePower(const double windMag) const;

    /**
     * @brief wind turbine power output
     *
     * @param windU
     * @param windV
     * @return double
     */
    double computePower(const double windU, const double windV) const;


    // getters
    int ID() const { return ID_; }
    double hubHeight() const { return hubHeight_; }
    double radius() const { return radius_; }
    double Cp() const { return Cp_; }
    double Ct() const { return Ct_; }
    double cutoffMax() const { return cutoffMax_; }
    double cutoffMin() const { return cutoffMin_; }
    double rhoHub() const { return rhoHub_; }
    size_t nearestPointID() const { return nearestPointID_; }
    double minDistanceLocal() const { return minDistanceLocal_; }
    size_t minRankGlob() const { return minRankGlob_; }

    friend std::ostream& operator<<(std::ostream& os, const WindTurbine& wt);

private:
    // WT ID
    int ID_;

    // Wind turbine parameters
    double hubHeight_;
    double radius_;
    double Cp_;
    double Ct_;
    double cutoffMax_;
    double cutoffMin_;

    // Atmospheric params
    double rhoHub_;

    // Nearest Grid Point info
    size_t nearestPointID_;
    double minDistanceLocal_;
    size_t minRankGlob_;
};

}  // namespace wind_farm_plugin