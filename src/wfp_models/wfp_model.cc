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
#include "eckit/mpi/Comm.h"

#include "wfp_model.h"

namespace wind_farm_plugin {


WFPModel::WFPModel(const eckit::Configuration& conf) {
    // do nothing
}


// ---------------------------------------------------------
WFPModelFactory::WFPModelFactory() {}

WFPModelFactory::~WFPModelFactory() = default;

WFPModelFactory& WFPModelFactory::instance() {
    static WFPModelFactory theinstance;
    return theinstance;
}

void WFPModelFactory::enregister(const std::string& name, const WFPModelBuilderBase& builder) {
    std::lock_guard<std::mutex> lock(mutex_);
    ASSERT(builders_.find(name) == builders_.end());
    builders_.emplace(std::make_pair(name, std::ref(builder)));
}

void WFPModelFactory::deregister(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = builders_.find(name);
    ASSERT(it != builders_.end());
    builders_.erase(it);
}

std::vector<std::string> WFPModelFactory::list_registered() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> reg_;
    for (const auto& b : builders_) {
        reg_.push_back(b.first);
    }
    return reg_;
}

WFPModel* WFPModelFactory::build(const std::string& name, const eckit::Configuration& config) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = builders_.find(name);
    if (it == builders_.end()) {
        throw eckit::SeriousBug("Builder not found for backend " + name, Here());
    }

    return it->second.get().make(config);
}

WFPModelBuilderBase::WFPModelBuilderBase(const std::string& name) : name_(name) {
    WFPModelFactory::instance().enregister(name, *this);
}

WFPModelBuilderBase::~WFPModelBuilderBase() {
    WFPModelFactory::instance().deregister(name_);
}


}  // namespace wind_farm_plugin