#!/usr/bin/env python3
"""Render a minimal rough-metal scene with the microscopy plugins."""

from __future__ import annotations

import argparse
from pathlib import Path

import mitsuba as mi


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="telecentric_kohler.exr")
    parser.add_argument("--variant", default="scalar_rgb")
    parser.add_argument("--spp", type=int, default=256)
    return parser.parse_args()


def scene_dict(spp: int) -> dict:
    return {
        "type": "scene",
        "integrator": {
            "type": "path",
            "max_depth": 4,
            "rr_depth": 3,
        },
        "sensor": {
            "type": "telecentric_microscope",
            "na_det": 0.30,
            "n_det_medium": 1.0,
            "det_focus_z": 0.0,
            "sensor_z": 10.0,
            "fov_mm": 1.0,
            "fov_y_mm": 1.0,
            "field_stop_diameter_mm": 0.9,
            "sampler": {
                "type": "independent",
                "sample_count": spp,
            },
            "film": {
                "type": "hdrfilm",
                "width": 256,
                "height": 256,
                "pixel_format": "rgb",
                "rfilter": {"type": "box"},
            },
        },
        "illumination": {
            "type": "kohler",
            "na": 0.30,
            "n_medium": 1.0,
            "condenser_z": 2.0,
            "illum_focus_z": 0.0,
            "field_diameter_mm": 0.8,
            "radiance": 1.0,
        },
        "sample": {
            "type": "rectangle",
            "to_world": mi.ScalarTransform4f().translate([0.0, 0.0, -0.05]),
            "bsdf": {
                "type": "roughconductor",
                "distribution": "ggx",
                "alpha": 0.08,
                "eta": {"type": "rgb", "value": [2.9, 2.6, 2.3]},
                "k": {"type": "rgb", "value": [4.6, 4.1, 3.6]},
            },
        },
    }


def main() -> None:
    args = parse_args()
    mi.set_variant(args.variant)
    scene = mi.load_dict(scene_dict(args.spp))
    image = mi.render(scene)
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    mi.Bitmap(image).write(str(output))
    print(output)


if __name__ == "__main__":
    main()
