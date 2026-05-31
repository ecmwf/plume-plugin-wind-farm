# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation
# nor does it submit to any jurisdiction.


configure_file(git_sha1.h.in git_sha1.h @ONLY)
configure_file(version.h.in version.h @ONLY)

# WF plugin
ecbuild_add_library(
    TARGET wind_farm_plugin

    SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/git_sha1.h
        ${CMAKE_CURRENT_BINARY_DIR}/version.h
        point.h
        point.cc
        utils.cc
        utils.h
        plugin.cc
        plugin.h
        config_parser.cc
        config_parser.h
        config_parser_native.cc
        config_parser_native.h
        config_parser_windio.cc
        config_parser_windio.h
        wind_farm_config.cc
        wind_farm_config.h
        wind_farm.cc
        wind_farm.h
        wind_map.h
        wind_turbine.cc
        wind_turbine.h
        wfp_models/wfp_model.cc
        wfp_models/wfp_model.h
        wfp_models/no_wake.cc
        wfp_models/no_wake.h
        wfp_models/jensen.cc
        wfp_models/jensen.h
        perf_curves/const_wind_curve.cc
        perf_curves/const_wind_curve.h
        perf_curves/curve_factory.cc
        perf_curves/curve_factory.h
        perf_curves/curve.cc
        perf_curves/curve.h
        perf_curves/tabular_wind_curve.cc
        perf_curves/tabular_wind_curve.h

    PUBLIC_INCLUDES
       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
       $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>

    PRIVATE_INCLUDES
        "${eckit_INCLUDE_DIRS}"
        "${CMAKE_SOURCE_DIR}"

    PRIVATE_LIBS
        eckit
        eckit_mpi
        atlas
        plume_plugin
)
