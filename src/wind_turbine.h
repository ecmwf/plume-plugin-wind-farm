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

#include "eckit/config/Configuration.h"
#include "eckit/config/LocalConfiguration.h"

#include "point.h"
#include "perf_curves/curve.h"
#include "perf_curves/curve_factory.h"


namespace wind_farm_plugin {


/**
 * @brief Wind turbine performance class
 * A quantity described by a performance curve (e.g. Power or Thrust)
 */
class WindTurbinePerf {
public:
    WindTurbinePerf(const eckit::Configuration& config) : config_(config) {
        Curve_ = CurveFactory::instance().build(config_);
    };
    virtual ~WindTurbinePerf() = default;

    // Calculate the performance of the wind turbine (e.g. Power or Thrust)
    virtual double calculate(double windSpeed, double rho, double WindTurbineRadius) const = 0;
    
    // Calculate the performance coefficient of the wind turbine (e.g. Power Coeff or Thrust Coeff)
    virtual double calculate_coeff(double windSpeed, double rho, double WindTurbineRadius) const = 0;

protected:
    eckit::LocalConfiguration config_;
    std::unique_ptr<Curve> Curve_;
};


/** 
 * @brief Wind turbine power class
 * This class calculates the power output of a wind turbine based on the wind speed and other parameters.
 */
class WindTurbinePower : public WindTurbinePerf {
public:
    WindTurbinePower(const eckit::Configuration& config) : WindTurbinePerf(config) {};

    double calculate(double windSpeed, double rho, double WindTurbineRadius) const override {
        double p_value = Curve_->value(windSpeed);
        if (config_.getBool("is_coefficient")) {
            double diskArea = M_PI * WindTurbineRadius * WindTurbineRadius;        
            return  p_value * 0.5 * rho * diskArea * pow(windSpeed, 3);
        } else {
            return p_value;
        }
    }

    double calculate_coeff(double windSpeed, double rho, double WindTurbineRadius) const override {
        double p_value = Curve_->value(windSpeed);
        if (config_.getBool("is_coefficient")) {
            return p_value;
        } else {
            
            if (windSpeed < 1e-6) {
                return 0.0; // Avoid division by zero for very low wind speeds
            }
            
            // Power coefficient = P / (0.5 * rho * A * V^3)
            double area = M_PI * WindTurbineRadius * WindTurbineRadius;
            return p_value / (0.5 * rho * area * pow(windSpeed, 3));
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const WindTurbinePower& power) {
        os << "WindTurbinePower with config: " << power.config_;
        return os;
    }

};


/**
 * @brief Wind turbine thrust class
 * This class calculates the thrust force of a wind turbine based on the wind speed and other parameters.
 */
class WindTurbineThrust : public WindTurbinePerf {
public:
    WindTurbineThrust(const eckit::Configuration& config) : WindTurbinePerf(config) {};
    
    // Calculate the thrust force of the wind turbine
    double calculate(double windSpeed, double rho, double WindTurbineRadius) const {
        double t_value = Curve_->value(windSpeed);
        double diskArea = M_PI * WindTurbineRadius * WindTurbineRadius;
        if (config_.getBool("is_coefficient")) {
            return  t_value * 0.5 * rho * diskArea * pow(windSpeed, 2);
        } else {
            return t_value;
        }
    }

    // Calculate the thrust coefficient of the wind turbine
    double calculate_coeff(double windSpeed, double rho, double WindTurbineRadius) const {
        double t_value = Curve_->value(windSpeed);
        if (config_.getBool("is_coefficient")) {
            return t_value;
        } else {
            if (windSpeed < 1e-6) {
                return 0.0; // Avoid division by zero for very low wind speeds
            }
            double area = M_PI * WindTurbineRadius * WindTurbineRadius;
            return t_value / (0.5 * rho * area * pow(windSpeed, 2));
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const WindTurbineThrust& thrust) {
        os << "WindTurbineThrust with config: " << thrust.config_;
        return os;
    }

};



/**
 * @brief Wind turbine class
 *
 */
class WindTurbine : public LatLonPoint {

public:
    WindTurbine(int id, const eckit::Configuration& conf, size_t nearestPointID, double minDistanceLocal,
                size_t minRankGlob, const eckit::Configuration& defaults = eckit::LocalConfiguration());

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

    double Ct(double vmag) const;

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

    // std::unique_ptr<Curve> CtCurve_;
    std::unique_ptr<WindTurbinePower> power_;
    std::unique_ptr<WindTurbineThrust> thrust_;

    // Atmospheric params
    double rhoHub_;

    // Nearest Grid Point info
    size_t nearestPointID_;
    double minDistanceLocal_;
    size_t minRankGlob_;
};


}  // namespace wind_farm_plugin