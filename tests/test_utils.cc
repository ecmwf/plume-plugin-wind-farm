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


#include "test_utils.h"


namespace test {

atlas::Field createUniform2DField(std::string name, double value) {

    long n_procs = atlas::mpi::comm().size();

    atlas::StructuredGrid grid = atlas::Grid("L200x101");
    atlas::functionspace::StructuredColumns fs;

    if (n_procs > 1) {
        atlas::grid::Distribution distribution(grid, atlas::util::Config("type", "checkerboard") | 
            atlas::util::Config("bands", n_procs));
        fs = atlas::functionspace::StructuredColumns(grid, distribution);        
    } else {
        fs = atlas::functionspace::StructuredColumns(grid);
    }

    atlas::Field field = fs.createField<double>(atlas::option::name(name) | atlas::option::levels(1));
    auto buff = atlas::array::make_view<double, 2>(field);

    for (atlas::idx_t i_pt = 0; i_pt < fs.size(); i_pt++) {
        buff(i_pt, 0) = value;
    }

    return field;
}

}  // namespace test