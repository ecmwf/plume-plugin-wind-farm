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
#include "atlas/domain/Domain.h"
#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/StructuredColumns.h"

#include "plume/data/FieldAccess.h"

#include "point.h"


namespace wind_farm_plugin {

class WindMap {

public:
    WindMap(plume::data::FieldView fieldU, plume::data::FieldView fieldV) : fieldU_{fieldU}, fieldV_{fieldV} {
        lonLatField_ = fieldU_.functionspace().lonlat();
    };

    ~WindMap() = default;

    atlas::array::ArrayView<const double, 2> arrayU() const {
        return atlas::array::make_view<const double, 2>(fieldU_);
    }

    atlas::array::ArrayView<const double, 2> arrayV() const {
        return atlas::array::make_view<const double, 2>(fieldV_);
    }

    atlas::Field lonlat() const { return lonLatField_; }

    const atlas::FunctionSpace& functionspace() const { return fieldU_.functionspace(); }

    /**
     * @brief This rank's own locally-owned wind, at whichever of its grid points fall inside a lat/lon box.
     *
     * A box spanning multiple ranks' subdomains needs each rank's result collected separately as done in the wind box
     * export in two way coupled runs.
     */
    std::vector<WindSample> windInBox(double latMin, double latMax, double lonMin, double lonMax) const {
        atlas::RectangularLonLatDomain box(latMax, lonMin, latMin, lonMax);  // north, west, south, east

        auto lonlatView = atlas::array::make_view<double, 2>(lonLatField_);
        auto u          = arrayU();
        auto v          = arrayV();

        std::vector<WindSample> samples;
        for (size_t i = 0; i < lonlatView.shape(0); i++) {
            double lon = lonlatView(i, 0);
            double lat = lonlatView(i, 1);
            if (box.contains(lon, lat)) {
                samples.push_back(WindSample{lat, lon, u(i, 0), v(i, 0)});
            }
        }
        return samples;
    }

private:
    plume::data::FieldView fieldU_;
    plume::data::FieldView fieldV_;
    atlas::Field lonLatField_;
};


}  // namespace wind_farm_plugin