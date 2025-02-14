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

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/runtime/Log.h"
#include "atlas/grid/Distribution.h"


namespace test {


/**
 * @brief Create a uniform 2D field with the specified name and value.
 *
 * @param name The name of the field.
 * @param value The value to fill the field with.
 * @return An atlas::Field object filled with the specified value.
 */
atlas::Field createUniform2DField(std::string name, double value);


}  // namespace test