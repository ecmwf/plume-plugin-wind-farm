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


namespace wind_farm_plugin {

class WindMap {

public:
    WindMap(const atlas::Field& fieldU, const atlas::Field& fieldV) : fieldU_{fieldU}, fieldV_{fieldV} {
        lonLatField_ = fieldU_.functionspace().lonlat();
    };

    ~WindMap() = default;

    atlas::Field fieldU() const { return fieldU_; }
    atlas::Field fieldV() const { return fieldV_; }

    atlas::array::ArrayView<const double, 2> arrayU() const {
        return atlas::array::make_view<const double, 2>(fieldU_);
    }

    atlas::array::ArrayView<const double, 2> arrayV() const {
        return atlas::array::make_view<const double, 2>(fieldV_);
    }

    atlas::Field lonlat() const { return lonLatField_; }


private:
    atlas::Field fieldU_;
    atlas::Field fieldV_;
    atlas::Field lonLatField_;
};


}  // namespace wind_farm_plugin