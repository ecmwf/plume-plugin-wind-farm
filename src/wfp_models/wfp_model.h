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
#include "eckit/exception/Exceptions.h"

#include "../wind_farm.h"
#include "../wind_map.h"


namespace plume {
namespace data {
class ModelDataView;  // forward declaration — only two-way coupling models need the full type
}
}  // namespace plume


namespace wind_farm_plugin {

class WFPModel {

public:
    WFPModel(const eckit::Configuration& conf);

    virtual ~WFPModel() = default;

    /**
     * @brief wind farm parametrization model to compute power output
     *
     * Shared across every model — not virtual, not overridable. Power always comes from the same turbine power curve
     * applied to whatever computeWindAtTurbines() returns; only that hook varies between models.
     *
     * Sums locally then does a single scalar MPI reduce — deliberately independent of computePowerByTurbine(), which
     * needs a full per-turbine, global-sized reduce that this method has no use for.
     *
     * @param wMap
     * @param windFarm
     * @return double
     */
    double computePower(const WindMap& wMap, const WindFarm& windFarm) const;

    /**
     * @brief wind farm parametrization model to compute power output per wind turbine
     *
     * @param wMap
     * @param windFarm
     * @return std::vector<LatLonValue>
     */
    std::vector<LatLonValue> computePowerByTurbine(const WindMap& wMap, const WindFarm& windFarm) const;

    /**
     * @brief Computes wind speed at given points.
     *
     * Default: the farm-average wind speed at every point (today's NoWake behaviour).
     *
     * @todo naming, open discussion: this method's only caller is WindFarm::computeWindBox(), an optional diagnostic
     * export of the model's implied wind field over an arbitrary grid; unrelated to power computation. A name like
     * sampleWindField() would say that directly, instead of reading as a sibling of computeWindAtTurbines() (which
     * this method has no relationship with).
     *
     * @param wMap
     * @param windFarm
     * @param points
     * @return std::vector<WindPoint>
     */
    virtual std::vector<WindPoint> computeWindAtPoints(const WindMap& wMap, const WindFarm& windFarm,
                                                       const std::vector<std::unique_ptr<LatLonPoint>>& points) const;

    // ---- Two-way coupling: optional, default unsupported. SurfaceRoughness opts in. ----

    /// Whether this model supports writing a parameter back to the host model.
    virtual bool supportsCoupling() const { return false; }

    /// Caches whatever per-grid-point quantities a coupling model needs that are time-invariant.
    virtual void initialiseCoupling(const WindFarm&, const WindMap&) {}

    /// Called during run only when the host marks the target param updated. Writes back farm impact on model fields.
    virtual void applyCoupling(const WindMap&, const WindFarm&, plume::data::ModelDataView&) const {
        throw eckit::NotImplemented("This wind farm model does not support two-way coupling", Here());
    }

    /// Name of the writable model parameter this model writes back to, when supportsCoupling().
    virtual const std::string& couplingTargetParam() const {
        throw eckit::NotImplemented("This wind farm model does not support two-way coupling", Here());
    }

protected:
    /**
     * @brief Wind at each of this rank's local turbines, for power computation.
     *
     * Default: read each turbine's own local wind directly from the model's wind field, with no correction (today's
     * NoWake behaviour). Correct for a model with no wake effect, and also correct when two-way coupling is active:
     * the deficit is then already realised by the host model rather than computed analytically here. Jensen overrides
     * this with the wake-deficit calculation.
     *
     * Only ever invoked polymorphically from computePowerByTurbine() — no external caller needs it.
     *
     * @todo naming, open discussion: this method's only path is the power computation (computePower() /
     * computePowerByTurbine()). A name like turbineInflow() would say that directly; the current name reads as a
     * sibling of computeWindAtPoints(), but the two methods are unrelated; one feeds power, the other feeds an optional
     * diagnostic export.
     */
    virtual std::vector<WindPoint> computeWindAtTurbines(const WindMap& wMap, const WindFarm& windFarm) const;
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
