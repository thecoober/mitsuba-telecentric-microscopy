# Plugin parameters

## `telecentric_microscope`

| Parameter | Default | Meaning |
| --- | ---: | --- |
| `na_det` | `0.25` | Detection numerical aperture |
| `n_det_medium` | `1.0` | Detection-side refractive index |
| `det_focus_z` | `0.0` | Object-space detection focus plane in scene units |
| `sensor_z` | `10.0` | Sensor reference plane; must exceed `det_focus_z` |
| `fov_mm` | `1.0` | Object-space horizontal film extent |
| `fov_y_mm` | `fov_mm` | Object-space vertical film extent |
| `field_stop_diameter_mm` | `0.0` | Circular object-conjugate stop; zero disables it |
| `telecentric_mag_error_per_mm` | `0.0` | First-order chief-ray tilt coefficient |

The implementation assumes scene coordinates are millimeters when the `_mm`
parameters are used as named.

## `kohler`

| Parameter | Default | Meaning |
| --- | ---: | --- |
| `na` | `0.25` | Illumination numerical aperture |
| `n_medium` | `1.0` | Illumination-side refractive index |
| `condenser_z` | `1.0` | Condenser reference plane |
| `illum_focus_z` | `0.0` | Illumination focus plane |
| `field_diameter_mm` | `1.0` | Circular illumination field diameter |
| `radiance` | `1.0` | Incident radiance scale or emissive texture |
| `focus_epsilon_mm` | `1e-7` | Focus/defocus numerical switch tolerance |
| `intersection_area_epsilon_mm2` | `1e-7` | Minimum valid overlap area |
| `intersection_segments` | `24` | Polygonal overlap approximation resolution |
| `intersection_sampling` | `polygon` | `polygon` or `polar` overlap sampling |

The emitter requires `0 < na < n_medium` and
`condenser_z > illum_focus_z`.
