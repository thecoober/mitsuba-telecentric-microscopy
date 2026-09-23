#!/usr/bin/env python3
"""Render an inclined checkerboard with telecentric detection and Kohler illumination.

The checkerboard center intersects the detection and illumination focal plane.
Tilting the board therefore produces an in-focus strip near the image center and
increasing defocus toward the left and right image boundaries.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import mitsuba as mi


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output", default="telecentric_kohler_checkerboard.exr")
    parser.add_argument("--variant", default="scalar_rgb")
    parser.add_argument("--spp", type=int, default=256)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--tilt-deg", type=float, default=10.0)
    parser.add_argument("--focus-z", type=float, default=0.0)
    parser.add_argument("--illum-field-mm", type=float, default=0.78)
    return parser.parse_args()


def scene_dict(
    spp: int,
    tilt_deg: float,
    focus_z: float,
    illum_field_mm: float,
) -> dict:
    if spp <= 0:
        raise ValueError("spp must be positive")
    if not 0.0 < illum_field_mm <= 1.5:
        raise ValueError("illum_field_mm must be in (0, 1.5]")

    board_to_world = (
        mi.ScalarTransform4f().translate([0.0, 0.0, focus_z])
        @ mi.ScalarTransform4f().rotate([0.0, 1.0, 0.0], tilt_deg)
        @ mi.ScalarTransform4f().scale([0.68, 0.68, 1.0])
    )

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
            "det_focus_z": focus_z,
            "sensor_z": focus_z + 10.0,
            "fov_mm": 1.0,
            "fov_y_mm": 1.0,
            # Disable detector clipping in this example so that the visible
            # circular boundary comes only from the Kohler illumination field.
            "field_stop_diameter_mm": 0.0,
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
            "condenser_z": focus_z + 2.0,
            "illum_focus_z": focus_z,
            "field_diameter_mm": illum_field_mm,
            "radiance": 1.0,
        },
        "inclined_checkerboard": {
            "type": "rectangle",
            "to_world": board_to_world,
            "bsdf": {
                "type": "diffuse",
                "reflectance": {
                    "type": "checkerboard",
                    "color0": {"type": "rgb", "value": [0.04, 0.04, 0.04]},
                    "color1": {"type": "rgb", "value": [0.75, 0.75, 0.75]},
                    "to_uv": mi.ScalarTransform4f().scale([18.0, 18.0, 1.0]),
                },
            },
        },
    }


def main() -> None:
    args = parse_args()
    mi.set_variant(args.variant)
    scene = mi.load_dict(
        scene_dict(
            spp=args.spp,
            tilt_deg=args.tilt_deg,
            focus_z=args.focus_z,
            illum_field_mm=args.illum_field_mm,
        )
    )
    image = mi.render(scene, seed=args.seed)
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    mi.Bitmap(image).write(str(output))
    print(
        f"{output}\n"
        f"tilt={args.tilt_deg:.3f} deg, focus_z={args.focus_z:.6f} mm, "
        f"illumination_field={args.illum_field_mm:.3f} mm"
    )


if __name__ == "__main__":
    main()
