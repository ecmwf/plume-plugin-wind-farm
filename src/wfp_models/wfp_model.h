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

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "eckit/config/Configuration.h"

#include "../wind_farm.h"
#include "../wind_map.h"


namespace wind_farm_plugin {

class WFPModel {

public:
    WFPModel(const eckit::Configuration& conf);

    virtual ~WFPModel() = default;

    /**
     * @brief wind farm parametrization model to compute power output
     *
     * @param wMap
     * @param windFarm
     * @return double
     */
    virtual double computePower(const WindMap& wMap, const WindFarm& windFarm) const = 0;

    /**
     * @brief wind farm parametrization model to compute power output per wind turbine
     *
     * @param wMap
     * @param windFarm
     * @return std::vector<LatLonValue>
     */
    virtual std::vector<LatLonValue> computePowerByTurbine(const WindMap& wMap,
                                                            const WindFarm& windFarm) const = 0;


    /**
     * @brief Computes wind speed at given points
     *
     * @param avgWind
     * @param windFarm
     * @param points
     * @return std::vector<WindPoint>
     */
    virtual std::vector<WindPoint> computeWindAtPoints(
        const WindMap& wMap, const WindFarm& windFarm,
        const std::vector<std::unique_ptr<LatLonPoint>>& points) const = 0;
};


// fwd declaration
class WFPModelBuilderBase;


// factory (registers/deregisters builders and calls "build")
class WFPModelFactory {

public:  // methods
    static WFPModelFactory& instance();

    void enregister(const std::string& name, const WFPModelBuilderBase& builder);
    void deregister(const std::string& name);
    std::vector<std::string> list_registered();

    WFPModel* build(const std::string& name, const eckit::Configuration& config) const;

private:  // methods
    // Only one instance can be built, inside instance()
    WFPModelFactory();
    ~WFPModelFactory();

private:  // members
    mutable std::mutex mutex_;

    std::map<std::string, std::reference_wrapper<const WFPModelBuilderBase>> builders_;
};


// base builder
class WFPModelBuilderBase {
public:  // methods
    // Only instantiate from subclasses
    WFPModelBuilderBase(const std::string& name);
    virtual ~WFPModelBuilderBase();

    virtual WFPModel* make(const eckit::Configuration& config) const = 0;

public:  // members
    std::string name_;
};


// a concrete builder for a specific WFPModel type
template <typename T>
class WFPModelBuilder : public WFPModelBuilderBase {
public:  // methods
    // The name of the builder is taken from the type of the built object
    WFPModelBuilder() : WFPModelBuilderBase(T::type()) {}

    ~WFPModelBuilder() override {}

    WFPModel* make(const eckit::Configuration& config) const override { return new T(config); }
};


}  // namespace wind_farm_plugin