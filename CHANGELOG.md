# Changelog

## 0.1.0 - 2026-09-23

- Add the `telecentric_microscope` sensor.
- Add finite-field, finite-NA `kohler` illumination.
- Add object-conjugate detection field-stop support.
- Add reference-dependent emitter evaluation for BSDF-sampled paths.
- Update primal and differentiable integrators for the new emitter contract.
- Add scalar regression tests for the sensor and emitter.
- Add a separated GGX visible-normal boundary stability patch and test.
- Pin the reproducible upstream baseline to Mitsuba `v3.8.0`.
