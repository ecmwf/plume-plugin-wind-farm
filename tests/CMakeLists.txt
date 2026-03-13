# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation
# nor does it submit to any jurisdiction.

add_subdirectory(data)

ecbuild_add_test (
    TARGET plume_plugin_wind_farm_test_wind_farm
    SOURCES
        test_utils.h
        test_utils.cc
        test_wind_farm.cc
    INCLUDES
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/tests>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
    LIBS
        eckit
        atlas
        wind_farm_plugin
    ENVIRONMENT
        PLUME_WIND_FARM_TEST_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/data/test-plume-config.yml
)

ecbuild_add_test (
    TARGET plume_plugin_wind_farm_test_points
    SOURCES
        test_points.cc
    INCLUDES
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
    LIBS
        eckit
        wind_farm_plugin
)

ecbuild_add_test (
    TARGET plume_plugin_wind_farm_test_wind_turbine
    SOURCES
        test_wind_turbine.cc
    INCLUDES
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
    LIBS
        eckit
        wind_farm_plugin
)