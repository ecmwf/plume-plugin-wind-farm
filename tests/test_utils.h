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

#include <string>

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/grid/Distribution.h"
#include "atlas/runtime/Log.h"

#include "eckit/config/LocalConfiguration.h"

#include "wind_map.h"


namespace test {


/**
 * @brief Create a uniform 2D field with the specified name and value.
 *
 * @param name The name of the field.
 * @param value The value to fill the field with.
 * @return An atlas::Field object filled with the specified value.
 */
atlas::Field createUniform2DField(std::string name, double value);

// Env vars asserted non-null internally, so callers never need to check them at the call site.
const char* getTestConfigPath();
const char* getCloseTestConfigPath();
const char* getSingleTurbineFilePath();

// Extracts the wind farm plugin's core-config sub-section from a full plume test config file.
eckit::LocalConfiguration loadCoreConfig(const char* configPathEnv);

// Returns a copy of coreConfig with wind_farm_model.name overridden to modelName.
eckit::LocalConfiguration overrideModel(const eckit::LocalConfiguration& coreConfig, const std::string& modelName);

// Shared (u=10, v=5) wind fixture used by most CASEs across the suite that don't need their own specific
// wind — built once on first use and reused, rather than re-allocating the underlying L200x101 grid per
// CASE/file.
atlas::Field& sharedUField();
atlas::Field& sharedVField();
const wind_farm_plugin::WindMap& sharedWindMap();


}  // namespace test