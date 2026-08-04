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

#include <algorithm>
#include <cmath>
#include <optional>

#include "eckit/exception/Exceptions.h"

#include "atlas/array.h"
#include "atlas/field/FieldBuilder.h"
#include "atlas/functionspace/StructuredColumns.h"

#include "plume/data/ModelDataView.h"

#include "surface_roughness.h"


namespace wind_farm_plugin {

static WFPModelBuilder<SurfaceRoughness> SurfaceRoughnessBuilder;

SurfaceRoughness::SurfaceRoughness(const eckit::Configuration& conf) :
    WFPModel{conf}, targetParam_{conf.getString("target_param", "z0m")} {
    if (conf.has("wf_roughness_constant")) {
        wfRoughnessConstant_ = conf.getDouble("wf_roughness_constant");
    }
}


void SurfaceRoughness::initialiseCoupling(const WindFarm& windFarm, const WindMap& wMap) {

    turbinesByPoint_ = windFarm.turbinesByGridPoint();
    cellData_.clear();

    if (turbinesByPoint_.empty()) {
        return;  // no local turbines on this rank
    }

    // turbine spacing is undefined for every point at once exactly when the farm has fewer than two turbines GLOBALLY.
    if (!wfRoughnessConstant_ && !windFarm.turbineSpacing(turbinesByPoint_.begin()->first)) {
        throw eckit::UserError(
            "SurfaceRoughness: the farm has a single turbine. Frandsen's array framework doesn't apply to a"
            "single-turbine farm. This model cannot produce a meaningful result for this configuration.",
            Here());
    }

    // Atlas utility to get the area of each grid cell in m^2 for structured columns functionspaces.
    // Defaults to Earth's IFS radius, so not providing a radius override here.
    atlas::functionspace::StructuredColumns fs(wMap.functionspace());
    atlas::Field areaField = atlas::field::FieldBuilder("grid-box-area", fs)();
    auto area              = atlas::array::make_view<const double, 1>(areaField);

    for (const auto& entry : turbinesByPoint_) {
        size_t pointID                             = entry.first;
        const std::vector<const WindTurbine*>& wts = entry.second;

        double rotorAreaSum = 0.0;
        for (const auto* wt : wts) {
            rotorAreaSum += M_PI * wt->radius() * wt->radius();
        }

        CellData cell;
        cell.area = area(pointID);

        // Clamped: rotorAreaSum is a vertical cross-section (the rotor disc), cell.area a horizontal ground
        // footprint — dividing one by the other isn't bounded to [0,1] the way a real ground-coverage fraction
        // is, and can exceed it (large rotors and/or many turbines sharing a coarse cell). See class-level @todo.
        cell.fraction = std::min(1.0, rotorAreaSum / cell.area);

        if (!wfRoughnessConstant_) {
            // The spacing VALUE itself is per-point (local turbine density can vary across the farm).
            double spacing          = *windFarm.turbineSpacing(pointID);
            cell.turbineSpacingArea = spacing * spacing;
        }

        cellData_[pointID] = cell;
    }
}


void SurfaceRoughness::applyCoupling(const WindMap& wMap, const WindFarm& windFarm,
                                     plume::data::ModelDataView& modelData) const {

    if (cellData_.empty()) {
        return;  // no local turbines on this rank — nothing to blend
    }

    auto arrayU = wMap.arrayU();
    auto arrayV = wMap.arrayV();

    modelData.writeParam(targetParam_, [&](plume::data::FieldWriter& z0mWriter) {
        auto z0m = atlas::array::make_view<double, 2>(z0mWriter);

        for (const auto& entry : cellData_) {
            size_t pointID       = entry.first;
            const CellData& cell = entry.second;

            double wfRoughness;
            if (wfRoughnessConstant_) {
                wfRoughness = *wfRoughnessConstant_;
            }
            else {
                const auto& turbines = turbinesByPoint_.at(pointID);

                double u    = arrayU(pointID, 0);
                double v    = arrayV(pointID, 0);
                double vmag = std::sqrt(u * u + v * v);

                // lambda and the drag-weighted hub height share the same per-turbine weight: Ct_i(v)/2 * A_rotor_i.
                double lambda         = 0.0;
                double weightedHubSum = 0.0;
                double weightSum      = 0.0;
                for (const auto* wt : turbines) {
                    double rotorArea = M_PI * wt->radius() * wt->radius();
                    double weight    = (wt->Ct(vmag) / 2.0) * rotorArea;  // Ct(v)/2 * A_rotor
                    lambda += weight / cell.turbineSpacingArea;           // Frandsen's x^2
                    weightedHubSum += wt->hubHeight() * weight;
                    weightSum += weight;
                }

                double hubHeightBar = (weightSum > 0.0) ? weightedHubSum / weightSum : 0.0;
                wfRoughness         = (lambda > 0.0) ? hubHeightBar * std::exp(-kappa_ / std::sqrt(lambda)) : 0.0;
            }

            // @todo NAIVE — blends against the whole-cell background regardless of which surface tile(s) make it
            // up; see the class-level @todo in surface_roughness.h for why that's not yet right for mixed cells.
            double z0mBackground = z0m(pointID, 0);
            z0m(pointID, 0)      = cell.fraction * wfRoughness + (1.0 - cell.fraction) * z0mBackground;
        }
    });
}


}  // namespace wind_farm_plugin
