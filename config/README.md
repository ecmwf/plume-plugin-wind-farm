# Plugin Configuration Example

This directory contains an example of the plugin configuration file.

```json
{
    "name": "WindFarmPlugin",
    "lib": "wind_farm_plugin",
    "core-config": {
        "wind_farm_model": {
            "name": "jensen",
            "kw": 0.04
        },
        "compute_power": true,
        "export_wind_box": true,
        "wind_turbines_filename": "<wind-turbines-configuration-filename>"
    }
}
```

The high level plugin configuration defines which wind farm model to run and what output is desired (power output and wind speed in a lat/lon box around the wind farm). The core configuration of the plugin has the following top-level keys:

|         Parameter        |                Description               |
|--------------------------|------------------------------------------|
| wind_farm_model          | Wind farm model specific parameters      |
| compute_power            | Flag to compute power output             |
| export_wind_box          | Flag to export wind speed in the box     |
| wind_turbines_filename   | Name of the wind turbine config file     |

The key ```wind_turbines_filename``` is the name of a separate JSON file that defines wind turbine parameters (example below). The example provided contains the coordinates of a dummy wind farm with a matrix of 10x10 wind turbines around a lat/lon point at approximately [55.0&deg;, 7.0&deg;]

```json
{
    "wind_farm_box": {
        "lon_min": 6.900,
        "lat_min": 54.900,
        "lon_max": 7.100,
        "lat_max": 55.100,
        "n_lon": 100,
        "n_lat": 100
    },
    "wind_turbine_defaults" : {
        "hub_height": 70.0,
        "radius": 40.0,
        "power": {
            "type": "constant",
            "is_coefficient": true,
            "value": 0.38,
            "cutin_wind_speed": 0.2,
            "cutout_wind_speed": 25.0
        },
        "thrust": {
            "type": "constant",
            "is_coefficient": true,
            "value": 0.5,
            "cutin_wind_speed": 0.2,
            "cutout_wind_speed": 25.0
        },
        "rho_hub": 1.25
    },
    "wind_turbines": [
        {"lat": 54.9550, "lon": 6.9550},
        {"lat": 54.9550, "lon": 6.9650},
        {"lat": 54.9550, "lon": 6.9750}
    ]
}
```

The top level parameters are described in the following table. Note that in principle, each turbine can override any default wind turbine parameter, to be able to simulate cases of wind farms that include wind turbines of different type. 

|         Parameter        |                  Description                |
|--------------------------|---------------------------------------------|
| wind_farm_box            | Lat/Lon box where wind values are exported  |
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