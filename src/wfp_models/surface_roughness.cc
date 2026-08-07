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

        // The host may write this field back one chunk at a time (e.g. from an OpenMP-parallelised loop),
        // flagging each call's bounds via 1-based, inclusive "chunk_start"/"chunk_end" field metadata. Absent
        // metadata means the whole field is valid, so a non-chunked host is unaffected.
        const auto& metadata     = z0mWriter.metadata();
        const bool hasChunkStart = metadata.has("chunk_start");
        const bool hasChunkEnd   = metadata.has("chunk_end");

        if (hasChunkStart != hasChunkEnd) {
            throw eckit::BadValue("SurfaceRoughness: incomplete chunk bounds in field metadata", Here());
        }

        size_t chunkStartIdx = 0;                                      // 0-based, inclusive
        size_t chunkEndIdx   = static_cast<size_t>(z0m.shape(0)) - 1;  // 0-based, inclusive

        if (hasChunkStart) {
            const long chunkStart = metadata.getLong("chunk_start");
            const long chunkEnd   = metadata.getLong("chunk_end");

            if (chunkStart < 1 || chunkEnd < chunkStart || chunkEnd > z0m.shape(0)) {
                throw eckit::BadValue("SurfaceRoughness: invalid chunk bounds in field metadata", Here());
            }

            chunkStartIdx = static_cast<size_t>(chunkStart - 1);
            chunkEndIdx   = static_cast<size_t>(chunkEnd - 1);
        }

        for (const auto& entry : cellData_) {
            size_t pointID       = entry.first;
            const CellData& cell = entry.second;

            if (pointID < chunkStartIdx || pointID > chunkEndIdx) {
                continue;  // this turbine's grid point isn't in the chunk this call is writing
            }

            double z0mBackground = z0m(pointID, 0);
            // Should we handle missing values ?
            if (z0mBackground <= 0.0) {
                throw eckit::BadValue(
                    "SurfaceRoughness: non-physical z0m background (<= 0) at a turbine-affected point", Here());
            }

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

                if (weightSum > 0.0) {
                    double hubHeightBar = weightedHubSum / weightSum;
                    // Frandsen 1992 Eq. (31): lambda -> 0 reduces exactly to z0mBackground.
                    double backgroundTerm = kappa_ / std::log(hubHeightBar / z0mBackground);
                    wfRoughness =
                        hubHeightBar * std::exp(-kappa_ / std::sqrt(backgroundTerm * backgroundTerm + lambda));
                }
                else {
                    wfRoughness = z0mBackground;  // every local turbine idle, no farm effect.
                }
            }

            // @todo NAIVE — blends against the whole-cell background regardless of which surface tile(s) make it
            // up; see the class-level @todo in surface_roughness.h for why that's not yet right for mixed cells.
            z0m(pointID, 0) = cell.fraction * wfRoughness + (1.0 - cell.fraction) * z0mBackground;
        }
    });
}


}  // namespace wind_farm_plugin
