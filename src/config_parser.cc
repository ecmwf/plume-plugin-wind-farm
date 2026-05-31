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

#include <string>

#include "eckit/exception/Exceptions.h"

#include "config_parser.h"
#include "config_parser_native.h"
#include "config_parser_windio.h"

namespace wind_farm_plugin {

std::unique_ptr<ConfigParser> ConfigParser::build(const std::string& format) {
    if (format == "native") {
        return std::make_unique<ConfigParserNative>();
    }
    if (format == "windio") {
        return std::make_unique<ConfigParserWindIO>();
    }

    throw eckit::BadParameter("Invalid config_format. Supported values are 'native' and 'windio'", Here());
}

}  // namespace wind_farm_plugin
