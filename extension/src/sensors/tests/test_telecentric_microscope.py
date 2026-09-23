import drjit as dr
import mitsuba as mi
import pytest


def create_sensor(field_stop_diameter_mm):
    return mi.load_dict(
        {
            "type": "telecentric_microscope",
            "na_det": 0.3,
            "n_det_medium": 1.0,
            "det_focus_z": 0.0,
            "sensor_z": 22.0,
            "fov_mm": 1.0,
            "fov_y_mm": 1.0,
            "field_stop_diameter_mm": field_stop_diameter_mm,
            "film": {
                "type": "hdrfilm",
                "width": 16,
                "height": 16,
            },
        }
    )


def sample_weight(sensor, position):
    _ray, weight = sensor.sample_ray(
        0.0,
        0.0,
        position,
        [0.5, 0.5],
    )
    return mi.unpolarized_spectrum(weight)


def test01_object_conjugate_field_stop(variant_scalar_rgb):
    sensor = create_sensor(1.1)

    assert dr.all(sample_weight(sensor, [0.5, 0.5]) > 0.0)
    assert dr.all(sample_weight(sensor, [0.0, 0.5]) > 0.0)
    assert dr.allclose(sample_weight(sensor, [0.0, 0.0]), 0.0)


def test02_zero_diameter_disables_field_stop(variant_scalar_rgb):
    sensor = create_sensor(0.0)
    assert dr.all(sample_weight(sensor, [0.0, 0.0]) > 0.0)


def test03_negative_diameter_rejected(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="field_stop_diameter_mm"):
        create_sensor(-1.0)
