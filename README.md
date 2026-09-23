# Mitsuba Telecentric Microscopy

Physics-based telecentric microscopy extensions for Mitsuba 3. The initial
release provides a numerical-aperture-limited telecentric microscope sensor,
finite-field Kohler illumination, and the renderer integration needed by
reference-dependent emitters.

Version `0.1.1` is a research release based on Mitsuba `v3.8.0` at commit
`2ba361481801f6e158bbab2b64345f6778bd2865`.

## Scope

The repository implements:

- a `telecentric_microscope` sensor with independent object-space field size,
  detection NA, focus plane, sensor plane, field stop, and optional first-order
  telecentric magnification error;
- a `kohler` emitter with independent illumination field and angular aperture;
- focused and defocused illumination sampling using the overlap between the
  field stop and the NA footprint;
- a `ReferenceDependent` emitter contract for radiance that depends on both
  the preceding interaction and sampled direction;
- compatibility updates for primal and differentiable Mitsuba integrators;
- scalar regression tests for field-stop behavior, emitter sampling/PDF
  consistency, BSDF-sampled connections, and GGX boundary stability.

The implementation is geometric-optics based. It does not simulate coherent
wave propagation. Diffraction filtering, camera response, z-stack scheduling,
and application-specific geometry generation belong in the calling project.

## Repository layout

```text
extension/                 New Mitsuba headers, plugins, and tests
patches/mitsuba-3.8.0/     Patches to existing Mitsuba source files
examples/                  Minimal executable scene
scripts/                   Bootstrap, patch, build, and test helpers
docs/                      Model, parameter, and validation notes
upstream/                  Pinned upstream version information
```

The complete Mitsuba source tree is intentionally not vendored. The bootstrap
script clones the official repository at the pinned commit, copies the new
plugins, and applies the compatibility patches.

## Quick start

Requirements follow Mitsuba 3: a C++ toolchain, CMake, Ninja, Python, and the
platform dependencies documented by the upstream project.

```bash
git clone https://github.com/thecoober/mitsuba-telecentric-microscopy.git
cd mitsuba-telecentric-microscopy

./scripts/bootstrap.sh
./scripts/build.sh
./scripts/test.sh
```

The default source checkout is created under
`.deps/mitsuba3-v3.8.0`, and the build directory under
`build/mitsuba3-v3.8.0`.

To apply the extension to an existing clean checkout of the pinned commit:

```bash
./scripts/apply_patches.sh /path/to/mitsuba3
```

The script refuses an incompatible upstream commit by default. Set
`MITSUBA_TELECENTRIC_ALLOW_UNPINNED=1` only when deliberately porting the code.

## Minimal scene

After building Mitsuba and activating its generated `setpath.sh`, run:

```bash
python examples/minimal_scene.py \
    --output telecentric_kohler_checkerboard.exr
```

The scene uses an inclined checkerboard whose center intersects the shared
detection and illumination focal plane. The center strip remains sharp while
the left and right regions become progressively defocused. Its circular
illumination boundary is produced by the finite Kohler field rather than by a
detector field stop. Use `--tilt-deg`, `--focus-z`, and `--illum-field-mm` to
change these effects. Application scenes can replace the board with a measured
or procedurally generated microscopic surface.

![Inclined checkerboard rendered with telecentric detection and finite-field
Kohler illumination](docs/images/telecentric_kohler_checkerboard.png)

## Patches

The release contains two patches:

1. `0001-reference-dependent-kohler-integration.patch` adds the general emitter
   contract, plugin registration, bindings, generated docstrings, and
   integrator compatibility needed by spatially conditioned illumination.
2. `0002-ggx-visible-normal-boundary-stability.patch` maps measure-zero QMC
   boundary samples to the open unit square and guards against non-finite GGX
   visible-normal slopes.

The second patch is separated because it is a general numerical robustness fix
rather than part of the microscope model itself.

## Reproducibility

The pinned upstream tag and commit are recorded in
`upstream/mitsuba-version.txt`. Tests should be run before changing the
upstream version. Results obtained with modified patches should record the
Mitsuba commit, active variant, optical parameters, and random seeds.

## Status

This is a research release. It has not yet been merged into the official
Mitsuba repository, and compatibility with versions other than the pinned
Mitsuba `v3.8.0` is not claimed.

## License and attribution

The project is distributed under the BSD 3-Clause License. See `LICENSE` and
`NOTICE`. Files derived from Mitsuba retain the upstream copyright and license
terms. Please cite both this software release and Mitsuba 3 in academic work.
