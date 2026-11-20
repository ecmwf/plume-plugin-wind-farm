# Plugin Configuration Example

This directory contains examples of the plugin configuration file.

```yaml
plugins:
  - name: WindFarmPlugin
    lib: wind_farm_plugin
    parameters:
      -
        - name: "u"
          type: "ATLAS_FIELD"
          height: &wind_field_height 70
        - name: "v"
          type: "ATLAS_FIELD"
          height: *wind_field_height
    core-config:
      wind_farm_model:
        name: jensen
        kw: 0.04
      wind_field_height: *wind_field_height
      compute_power: true
      export_wind_box: true
      export_wind_turbine_power: false
      wind_farm_box:
        lon_min: 6.900
        lat_min: 54.900
        lon_max: 7.100
        lat_max: 55.100
        n_lon: 100
        n_lat: 100
      wind_turbines_filename: "<wind-turbines-configuration-filename>"
```

The high level plugin configuration defines which wind farm model to run and what output is desired (power output and wind speed in a lat/lon box around the wind farm). The core configuration of the plugin has the following top-level keys:

|         Parameter          |                Description                 |
|----------------------------|--------------------------------------------|
| wind_farm_model            | Wind farm model specific parameters        |
| wind_field_height          | Height of the wind data from the model     |
| compute_power              | Flag to compute power output               |
| export_wind_box            | Flag to export wind speed in the box       |
| export_wind_turbine_power  | Flag to export per-turbine power in CSV    |
| wind_farm_box              | Lat/Lon box where wind values are exported |
| wind_turbines_filename     | Name of the wind turbine config file       |
| config_format              | Wind turbine configuration format ('native' or 'windio') |

The example uses a YAML anchor to define the wind field height once and reuse it for both parameters. The anchor must be defined before any alias that references it.

The key ```wind_turbines_filename``` is the name of a separate YAML file that defines wind turbine parameters (example below). The example provided contains the coordinates of a dummy wind farm with a matrix of 10x10 wind turbines around a lat/lon point at approximately [55.0&deg;, 7.0&deg;]

If `config_format` is set to "windio", the wind-farm and wind turbine files can be written in WindIO format. Note that the support for windIO format is currently limited to single wind-farm and single wind-turbine type (see the config examples under the tests directory).

> [!NOTE]
> The hub height defined in the wind turbines file is not used to select the wind field height. 
> The wind field height exposed by Plume comes from the plugin configuration (the `height` values in `parameters`).
> It is the user's responsibility to make sure those heights are set to a sensible value.

## Wind farm model

`wind_farm_model.name` selects the model:

|    name     | Description                                                            |
|-------------|-------------------------------------------------------------------------|
| `jensen`    | Analytic wake model (wake deficit + superposition). One-way only.       |
| `no_wake`   | No wake effect; power from local wind only. One-way only.               |
| `roughness` | Frandsen (1992) effective-roughness model, or user-provided constant. Two-way: writes back `z0m`.  |

`roughness`, or any two-way-coupling-enabled engineering models, also take `target_param` (e.g., default `z0m` for roughness) which is the model parameter it writes back into.

`roughness` also optionally takes `wf_roughness_constant`: a fixed roughness length (m) that replaces the dynamic Frandsen-based estimate, so a literature constant can be used directly, reproducing studies that impose one outright (e.g. Keith et al. 2004; Kirk-Davidoff & Keith 2008; Wang & Prinn 2010), or simply comparing against one. Unlike the dynamic estimate, this needs no inter-turbine spacing, so it also works for a single-turbine farm.

## Two-way coupling

The plugin coupling mode is inferred from two signals in the Plume configuration, either sufficient: `wind_farm_model.target_param` being configured at all, or that parameter already present, negotiated as writable, in the plugin's data. Both need to actually line up: `target_param` set on a model that doesn't support coupling, or a coupling-capable model whose target param wasn't granted write access, both throw a clear error at setup rather than silently falling back to one-way behaviour.

Granting write access needs:
- A top-level `write-back-policy` (e.g. `single-writer`) in the Plume manager config, disabled by default.
- The target param declared `writable: true`, in the *same* `parameters` group as `u`/`v` if power or wind export are also enabled for the run.

The host calls Plume at various entry points in the timestep, two are relevant for the two-way coupled plugin: once with the target param updated (e.g., at surface exchange the plugin with the roughness model on blends the farm's effect in, then returns), once with `u`/`v` updated post-diffusion (the plugin's usual power/wind-box outputs, now reflecting the coupled wind).

`export_wind_box` adapts automatically: one-way runs get `wind_farm_box`'s `n_lon`x`n_lat` idealised wake-model evaluation, as before. Two-way runs instead sample the host's real, already-coupled wind at whichever native grid points fall inside `wind_farm_box`'s bounds — one CSV per MPI rank per step (`..._step_NNNNNN_rank_RRRR.csv`), skipped for ranks with nothing in the box.

`n_lon`/`n_lat` are only meaningful for the one-way evaluation, so under two-way coupling just omit them, bounds alone are enough. Setup throws if they're present, rather than silently ignoring dead config.

### Two-way coupling example

```yaml
write-back-policy: single-writer
plugins:
  - name: WindFarmPlugin
    lib: wind_farm_plugin
    parameters:
      -
        - name: "u"
          type: "ATLAS_FIELD"
          height: &wind_field_height 70
        - name: "v"
          type: "ATLAS_FIELD"
          height: *wind_field_height
        - name: "z0m"
          type: "ATLAS_FIELD"
          writable: true
    core-config:
      wind_farm_model:
        name: roughness
        target_param: z0m
      wind_field_height: *wind_field_height
      compute_power: true
      export_wind_box: true
      wind_farm_box:
        lon_min: 6.900
        lat_min: 54.900
        lon_max: 7.100
        lat_max: 55.100
      wind_turbines_filename: "<wind-turbines-configuration-filename>"
```

## Wind Farm Configuration

Plugin-native example configuration (see [WindIO documentation](https://ieawindsystems.github.io/windIO/main/index.html) for WindIO examples):
```yaml
wind_turbine_defaults:
  hub_height: 70.0
  radius: 40.0
  power:
    type: constant
    is_coefficient: true
    value: 0.38
    cutin_wind_speed: 0.2
    cutout_wind_speed: 25.0
  thrust:
    type: constant
    is_coefficient: true
    value: 0.5
    cutin_wind_speed: 0.2
    cutout_wind_speed: 25.0
  rho_hub: 1.25
wind_turbines:
  - lat: 54.9550
    lon: 6.9550
  - lat: 54.9550
    lon: 6.9650
  - lat: 54.9550
    lon: 6.9750
```

The top level parameters are described in the following table. Note that in principle, each turbine can override any default wind turbine parameter, to be able to simulate cases of wind farms that include wind turbines of different type. 

|         Parameter        |                  Description                |
|--------------------------|---------------------------------------------|
| wind_turbine_defaults    | Default wind turbine parameters             |
| wind_turbines            | Coordinates of wind turbines                |


Note that wind turbine power and thrust can be defined as either constant values or user-provided curves. The tables below shows the available options:

- Configuration for constant value thrust and/or power:

|         Parameter        |                        Description                       |
|--------------------------|----------------------------------------------------------|
| type                     | "constant"                                               |
| is_coefficient           | whether the value provided is a thrust/power coefficient |
| value                    | value of thrust or power (coefficient, if so defined)    |
| cutin_wind_speed         | minimum wind speed for wind turbine operation            |
| cutout_wind_speed        | maximum wind speed for wind turbine operation            |

- Configuration for tabular values for thrust/power curves

|         Parameter        |                         Description                            |
|--------------------------|----------------------------------------------------------------|
| type                     | "tabular_wind_curve"                                           |
| is_coefficient           | whether the value provided is a thrust/power coefficient       |
| wind_speeds              | Array of wind speed points where the curve values are provided |
| values                   | Array of thrust/power values                                   |
| cutin_wind_speed         | minimum wind speed for wind turbine operation                  |
| cutout_wind_speed        | maximum wind speed for wind turbine operation                  |