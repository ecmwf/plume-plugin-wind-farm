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

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "eckit/config/Configuration.h"

#include "../wind_map.h"
#include "../wind_turbine.h"
#include "wfp_model.h"


namespace wind_farm_plugin {

/**
 * @brief Wind-farm implicit roughness-length parameterisation, with two-way write-back to the host model's z0m.
 *
 * Only the roughness length for momentum (z0m) is touched here, deliberately — not z0h (heat/moisture). The two
 * are physically distinct and a wind farm's drag effect is a momentum-sink effect first; folding z0h in too, if
 * ever wanted, is a separate extension, not a naming detail.
 *
 *   lambda = sum_i( Ct_i(v)/2 * A_rotor_i ) / x^2
 *   wf_roughness = h_hub_bar * exp(-kappa / sqrt(lambda))
 *
 * blended with the host's current background z0m by fractional coverage:
 *
 *   z0m[cell] = wf_fraction[cell] * wf_roughness[cell] + (1 - wf_fraction[cell]) * z0m[cell]
 *
 * wf_fraction depends on how many turbines share a grid cell, each one's rotor radius, and the cell's area; not turbine
 * positions/spacing/layout/footprint at all (a turbine's location only decides *which* cell it's grouped into, upstream
 * of this formula). All time-invariant for the run, so cached once by initialiseCoupling();
 *
 * wf_roughness is recomputed every applyCoupling() call, but whether it actually *varies* with the current wind
 * depends on each turbine's configured thrust curve, not on this class: Ct(v) genuinely depends on v only for
 * perf_curves::TabularWindCurve (linear interpolation). perf_curves::ConstWindCurve ("constant") ignores v
 * entirely except for optional cut-in/cut-out thresholding to 0 — with that curve type, wf_roughness only
 * changes when v crosses a cut-in/cut-out boundary, not continuously with wind speed.
 *
 * Optional config key wf_roughness_constant replaces the dynamic wf_roughness formula above with a single fixed
 * value (still blended by wf_fraction the same way), so past studies using the implicit approach with an imposed
 * constant (Keith et al. 2004; Kirk-Davidoff & Keith 2008; Wang & Prinn 2010) can be compared against.
 * A single-turbine farm is valid in this mode.
 *
 * Does not override computeWindAtTurbines()/computeWindAtPoints() — inherits WFPModel's defaults (read local wind
 * directly), which is exactly correct for this model's power computation: by the time that path runs, the host's
 * own diffusion scheme has already applied the farm's effect via the blended z0m this model wrote earlier in the
 * same timestep, so no further analytic correction belongs here.
 *
 * @todo First approach: *implicit* wind farm representation with surface roughness length, using formula from
 * Frandsen 1992, J. Wind Eng. Ind. Aerod. 39, 251-265 to confirm with DTWO WP7 partners:
 *  - Frandsen (1992) Eq. (24): c'_t = (1/2)*C_T*A_r/x^2 is exactly our lambda's per-turbine term. The
 *    exp(-kappa/sqrt(...)) shape is also the footnote to Fig. 5, and Eq. (31), which additionally folds in a
 *    background terrain roughness.
 *  - h_hub_bar prefactor: not literally written in Frandsen's footnote/Eq. (31) but derivable from the equations.
 *    The standard log law solved for z0 at reference height h with friction velocity u_*2 gives
 *    z0,2 = h*exp(-k*u_h/u_*2) in general. Eq. (25), u_*2^2 = u_*1^2 + u_h^2*c'_t, gives u_h/u_*2 ~= 1/sqrt(c'_t),
 *    so z0,2 ~= h*exp(-k/sqrt(c'_t)) — matching the written footnote exactly, except for the h.
 *  - NOT IN FRANDSEN AT ALL — our own extensions layered on top of the single-turbine-type, infinite-regular-array
 *    derivation, not literature-derived:
 *     1. Ct(v) evaluated live from each turbine's thrust curve at the current wind. Frandsen uses a fixed C_T
 *        (e.g. 0.88) as an input for a given case, never a function of instantaneous wind speed.
 *     2. Multiple turbines per grid cell, combined via a drag-weighted hub-height average. His derivation is for
 *        one hub height, one C_T, evenly spaced — not a heterogeneous mix sharing one cell.
 *     3. The entire fractional-coverage blend with a background z0m — Frandsen's z0 describes an (idealised,
 *        infinite) wind farm's own roughness outright, to be used as a boundary condition on its own.
 *        The blend assumes the farm is spread uniformly across whatever surface types make up the background z0m (sea,
 *        land, forest, ...), and A_cell (see gridCellArea()) is the *entire* cell area rather than just the tile the
 *        farm actually sits on. Is it a reasonable assumption, should we move away from using a blend at all, can
 *        Plume expose cell tile makeup and farm type (onshore/offshore).
 */
class SurfaceRoughness final : public WFPModel {

public:
    SurfaceRoughness(const eckit::Configuration& conf);

    bool supportsCoupling() const override { return true; }

    void initialiseCoupling(const WindFarm& windFarm, const WindMap& wMap) override;

    void applyCoupling(const WindMap& wMap, const WindFarm& windFarm,
                       plume::data::ModelDataView& modelData) const override;

    const std::string& couplingTargetParam() const override { return targetParam_; }

    static const char* type() { return "roughness"; }

private:
    /**
     * @brief Per-grid-cell quantities cached once by initialiseCoupling() — time-invariant for the run.
     *
     * @todo `area` is a mock average until we have the Atlas utility, not real.
     *
     * @todo `fraction` is clamped to [0,1] (see initialiseCoupling()) because its numerator (rotor swept area, a
     * vertical cross-section) and denominator (cell area, a horizontal footprint) are not the same kind of
     * quantity — their ratio isn't inherently bounded like a real ground-coverage fraction is, and exceeds 1 for
     * large rotors and/or many turbines sharing a coarse cell. The clamp keeps the blend well-defined, but
     * doesn't resolve whether rotor swept area is the right numerator at all.
     */
    struct CellData {
        double area               = 0.0;  ///< A_cell (host grid), m^2 — used only for `fraction`.
        double turbineSpacingArea = 0.0;  ///< Frandsen's x^2 (farm-internal spacing), m^2. Unused when constant mode.
        double fraction           = 0.0;  ///< wf_fraction = min(1, sum(A_rotor,i) / A_cell).
    };

    std::string targetParam_;  ///< Writable model parameter written back to, default "z0m".

    double kappa_ = 0.4;  ///< Von Karman constant.

    std::optional<double> wfRoughnessConstant_;  ///< Replaces the dynamic formula when set.

    /**
     * @brief Keyed by nearestPointID; every grid point with >=1 local turbine.
     *
     * @warning The dynamic (non-constant) mode only does not support a single-turbine wind farm because Frandsen's
     *          array framework doesn't extend to N=1.
     */
    std::map<size_t, CellData> cellData_;

    /// Same keys as cellData_: turbines grouped by grid point. Cached once by initialiseCoupling() and reused
    /// by applyCoupling() — turbine-to-point assignment is invariant for the run.
    std::map<size_t, std::vector<const WindTurbine*>> turbinesByPoint_;

    /**
     * @brief Horizontal area of the grid cell a point belongs to, in square metres.
     *
     * Currently a MOCK (see averageCellArea() in the .cc) — the same domain-averaged area for every point, not
     * each point's true cell area. Temporary stand-in for an Atlas utility in development that will return true
     * per-point areas; tracked via ATLAS-XX and remove averageCellArea() once it lands.
     *
     * @param pointID Unused today; kept for the future Atlas-backed implementation, which will index into a
     * per-point area field by it.
     */
    double gridCellArea(const WindMap& wMap, size_t pointID) const;
};


}  // namespace wind_farm_plugin
