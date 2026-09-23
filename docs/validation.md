# Validation protocol

The release includes focused unit tests, but scientific use should additionally
validate the complete optical configuration.

## Sensor checks

- `na_det = 0` produces parallel chief rays.
- Focus-plane coordinates preserve object-space magnification.
- The circular field stop accepts center and edge samples and rejects corners.
- Sampled direction angles remain within the configured NA.
- Ray differentials remain finite at crop boundaries.

## Emitter checks

- `sample_direction`, `pdf_direction`, and `eval_direction` agree.
- Primary camera rays do not directly see the infinite Kohler emitter.
- BSDF sampling can connect to the reference-dependent emitter.
- Focused and defocused overlap branches remain continuous near the switch.
- Field-edge irradiance converges as `intersection_segments` increases.

## Integrator checks

- `direct`, `path`, `volpath`, and `volpathmis` preserve ordinary emitter
  behavior.
- Differentiable integrators run without changing primal radiance.
- CPU scalar and CUDA JIT variants agree within Monte Carlo uncertainty.

## Scientific validation

Record the Mitsuba commit, active variant, NA values, focus planes, field sizes,
scene units, BSDF, sample count, and seeds. Compare synthetic and measured
focal stacks using linear radiance where possible. Separate geometric defocus,
diffraction filtering, denoising, and display gamma into named output stages.
