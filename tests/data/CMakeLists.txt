# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation
# nor does it submit to any jurisdiction.


set(test_wind_turbine_json ${CMAKE_CURRENT_SOURCE_DIR}/test_wind_turbines.json)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test-plume-config.json.in 
               ${CMAKE_CURRENT_BINARY_DIR}/test-plume-config.json @ONLY)