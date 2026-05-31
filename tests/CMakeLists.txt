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
        ../src/utils.h
        ../src/utils.cc
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
        PLUME_WIND_FARM_TEST_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/data/plume-config-native.yml
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

ecbuild_add_test (
    TARGET plume_plugin_wind_farm_test_utils_functions
    SOURCES
        test_utils_functions.cc
    INCLUDES
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
    LIBS
        eckit
        atlas
        wind_farm_plugin
)

ecbuild_add_test (
    TARGET plume_plugin_wind_farm_test_windio
    SOURCES
        test_wind_farm_windio.cc
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
        PLUME_WIND_FARM_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/data
        PLUME_WIND_FARM_TEST_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/data/plume-config-windio.yml
)

ecbuild_add_test (
    TARGET plume_plugin_wind_farm_test_user_formats
    SOURCES
        test_user_formats.cc
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
        PLUME_WIND_FARM_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/data
        PLUME_WIND_FARM_TEST_CONFIG_NATIVE=${CMAKE_CURRENT_BINARY_DIR}/data/plume-config-native.yml
        PLUME_WIND_FARM_TEST_CONFIG_WINDIO=${CMAKE_CURRENT_BINARY_DIR}/data/plume-config-windio.yml
)

# test the wind-farm plugin with the emulator
if (TARGET nwp_emulator_run_dp.x)
    message(WARNING "nwp_emulator_run_dp.x found => adding emulator tests")

    ecbuild_add_test(
        TARGET plume_plugin_wind_farm_with_emulator
        COMMAND
            $<TARGET_FILE:nwp_emulator_run_dp.x>
        ENVIRONMENT
            LD_LIBRARY_PATH=$<TARGET_FILE_DIR:wind_farm_plugin>:$ENV{LD_LIBRARY_PATH}
        ARGS --config-src=${CMAKE_CURRENT_SOURCE_DIR}/data/emulator-config.yml
            --plume-cfg=${CMAKE_CURRENT_BINARY_DIR}/data/plume-config-native.yml
    )

    ecbuild_add_test(
        TARGET plume_plugin_wind_farm_with_emulator_windio
        COMMAND
            $<TARGET_FILE:nwp_emulator_run_dp.x>
        ENVIRONMENT
            LD_LIBRARY_PATH=$<TARGET_FILE_DIR:wind_farm_plugin>:$ENV{LD_LIBRARY_PATH}
        ARGS --config-src=${CMAKE_CURRENT_SOURCE_DIR}/data/emulator-config.yml
            --plume-cfg=${CMAKE_CURRENT_BINARY_DIR}/data/plume-config-windio.yml
    )
else()
    message(WARNING "nwp_emulator_run_dp.x not found => skipping emulator test")
endif()