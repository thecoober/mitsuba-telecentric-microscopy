# Design

## Separation of field and aperture

The illumination field stop and condenser aperture are independent. The field
diameter limits points on the illumination focus plane; `na` limits the angular
cone. Away from that plane, the valid focus-plane region is the intersection of
the field disk and the point's NA footprint.

The detector follows the same separation. Film coordinates map to object-space
focus points, while `na_det` samples a finite direction cone around the chief
ray. A circular object-conjugate field stop optionally masks focus points.

## Reference-dependent emission

Ordinary infinite emitters can be evaluated from a ray miss. Kohler illumination
cannot: its incident radiance is conditioned on the receiving surface point and
the sampled direction. `EmitterFlags::ReferenceDependent` marks this contract.
`Scene::eval_emitter_connection()` evaluates an ordinary endpoint emitter or
calls `eval_direction()` with the preceding interaction as appropriate.

Primal and differentiable integrators use the same scene method for
BSDF-sampled emitter connections. Next-event estimation continues to use the
normal `sample_emitter_direction()` path.

## Geometric-optics boundary

The sensor and emitter operate in geometric optics. They model finite numerical
aperture, field stops, focus planes, visibility, and radiance transport. They do
not propagate phase or generate diffraction patterns. Diffraction and camera
response should be applied explicitly by the application when required.
