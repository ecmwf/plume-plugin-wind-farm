# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation
# nor does it submit to any jurisdiction.

# plume-config and wind-farm layout file (native format)
# set(plume_config_native_yml ${CMAKE_CURRENT_SOURCE_DIR}/plume-config-native.yml)
set(native_dummy_wind_farm_yml ${CMAKE_CURRENT_SOURCE_DIR}/native_dummy_windfarm.yml)

# close-packed variant (turbines ~2.5km apart, in line with the wind direction used by
# the regression test) so Jensen's wake model actually produces inter-turbine deficit
set(native_dummy_windfarm_close_yml ${CMAKE_CURRENT_SOURCE_DIR}/native_dummy_windfarm_close.yml)

# files for windio format
# set(plume_config_windio_yml ${CMAKE_CURRENT_SOURCE_DIR}/plume-config-windio.yml)
set(windio_dummy_wind_farm_yml ${CMAKE_CURRENT_BINARY_DIR}/windio_dummy_windfarm.yml)
set(windio_dummy_turbine_yml ${CMAKE_CURRENT_SOURCE_DIR}/windio_dummy_turbine.yml)

# configure files for tests
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plume-config-native.yml.in
               ${CMAKE_CURRENT_BINARY_DIR}/plume-config-native.yml @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plume-config-windio.yml.in
               ${CMAKE_CURRENT_BINARY_DIR}/plume-config-windio.yml @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plume-config-native-close.yml.in
               ${CMAKE_CURRENT_BINARY_DIR}/plume-config-native-close.yml @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/windio_dummy_windfarm.yml.in
               ${CMAKE_CURRENT_BINARY_DIR}/windio_dummy_windfarm.yml @ONLY)

