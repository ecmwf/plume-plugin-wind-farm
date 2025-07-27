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


#include <functional>
#include <memory>
#include <unordered_map>


#include "eckit/config/Configuration.h"
#include "curve.h"


namespace wind_farm_plugin {

class CurveFactory {    
    
public:

    using Builder = std::function<std::unique_ptr<Curve>(const eckit::Configuration&)>;

    static CurveFactory& instance();

    /**
     * @brief Register a builder for a specific curve type.
     *
     * @param name The name of the curve type.
     * @param builder The function that builds the curve.
     */
    void registerBuilder(const std::string& name, Builder builder);

    /**
     * @brief Build a curve of the specified type.
     *
     * @param name The name of the curve type.
     * @param config The configuration for the curve.
     * @return std::unique_ptr<Curve> A unique pointer to the created curve.
     */
    std::unique_ptr<Curve> build(const eckit::Configuration& config) const;

    // singletons should not be cloneable or assignable
    CurveFactory(const CurveFactory&) = delete;
    CurveFactory& operator=(const CurveFactory&) = delete;
    CurveFactory(CurveFactory&&) = delete;
    CurveFactory& operator=(CurveFactory&&) = delete;

private:
    CurveFactory() = default;
    std::unordered_map<std::string, Builder> builders_;

};

} // namespace wind_farm_plugin