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

#include <functional>
#include <memory>
#include <unordered_map>


#include "eckit/config/Configuration.h"
#include "eckit/exception/Exceptions.h"

#include "curve_factory.h"


namespace wind_farm_plugin {

void CurveFactory::registerBuilder(const std::string& name, Builder builder) {
    builders_[name] = builder;
}

std::unique_ptr<Curve> CurveFactory::build(const eckit::Configuration& config) const {

    // assert that the configuration has a "type" key
    if (!config.has("type")) {
        throw eckit::BadParameter("The configuration must have a 'type' key to specify the coefficient curve type", Here());
    }
    auto typeName = config.getString("type");

    auto it = builders_.find(typeName);
    if (it == builders_.end()) {
        throw eckit::BadParameter("No builder registered for coefficient curve type: " + typeName, Here());
    }
    return it->second(config);
}

CurveFactory& CurveFactory::instance() {
    static CurveFactory instance;
    return instance;
}

} // namespace wind_farm_plugin