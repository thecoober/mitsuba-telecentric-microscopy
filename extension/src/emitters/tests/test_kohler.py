import drjit as dr
import mitsuba as mi


def make_kohler():
    return mi.load_dict({
        'type': 'kohler',
        'na': 0.2,
        'n_medium': 1.0,
        'condenser_z': 2.0,
        'illum_focus_z': 0.0,
        'field_diameter_mm': 1.0,
        'radiance': 1.0,
    })


def make_sensor():
    return {
        'type': 'orthographic',
        'near_clip': 0.01,
        'far_clip': 10.0,
        'to_world': (
            mi.ScalarTransform4f().look_at(
                origin=[0, 0, 1],
                target=[0, 0, 0],
                up=[0, 1, 0],
            )
            @ mi.ScalarTransform4f().scale([0.2, 0.2, 1])
        ),
        'sampler': {
            'type': 'independent',
            'sample_count': 4,
        },
        'film': {
            'type': 'hdrfilm',
            'width': 1,
            'height': 1,
            'pixel_format': 'rgb',
            'rfilter': {'type': 'box'},
        },
    }


def make_reflection_scene(integrator):
    return mi.load_dict({
        'type': 'scene',
        'integrator': integrator,
        'sensor': make_sensor(),
        'mirror': {
            'type': 'rectangle',
            'bsdf': {'type': 'conductor'},
        },
        'emitter': {
            'type': 'kohler',
            'na': 0.2,
            'n_medium': 1.0,
            'condenser_z': 2.0,
            'illum_focus_z': 0.0,
            'field_diameter_mm': 1.0,
            'radiance': 1.0,
        },
    })


def test_reference_dependent_contract(variant_scalar_rgb):
    emitter = make_kohler()
    assert mi.has_flag(
        emitter.flags(), mi.EmitterFlags.ReferenceDependent)

    si = mi.SurfaceInteraction3f()
    assert dr.allclose(emitter.eval(si), 0.0)

    ref = mi.Interaction3f()
    ref.p = [0, 0, 0]
    ds = mi.DirectionSample3f()
    ds.d = [0, 0, 1]
    ds.emitter = emitter
    assert dr.all(emitter.eval_direction(ref, ds) > 0.0)

    ds.d = [0, 0, -1]
    assert dr.allclose(emitter.eval_direction(ref, ds), 0.0)


def test_direction_sampling_matches_pdf_and_evaluation(variant_scalar_rgb):
    emitter = make_kohler()

    for position in ([0, 0, 0], [0.1, -0.05, -0.25]):
        ref = mi.Interaction3f()
        ref.p = position

        for sample in ([0.1, 0.2], [0.45, 0.8], [0.9, 0.35]):
            ds, weight = emitter.sample_direction(ref, sample)
            assert ds.pdf > 0.0
            assert dr.allclose(
                emitter.pdf_direction(ref, ds), ds.pdf, rtol=1e-5)
            assert dr.allclose(
                weight * ds.pdf,
                emitter.eval_direction(ref, ds),
                rtol=1e-5,
            )


def test_primary_ray_does_not_see_kohler(variant_scalar_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'integrator': {
            'type': 'path',
            'max_depth': 1,
        },
        'sensor': make_sensor(),
        'emitter': {
            'type': 'kohler',
            'na': 0.2,
            'n_medium': 1.0,
            'condenser_z': 2.0,
            'illum_focus_z': 0.0,
            'field_diameter_mm': 1.0,
            'radiance': 1.0,
        },
    })

    image = mi.render(scene)
    assert dr.allclose(image.array, 0.0)


def test_bsdf_sample_connects_to_kohler(variant_scalar_rgb):
    scene = make_reflection_scene(
        {
            'type': 'path',
            'max_depth': 2,
        }
    )

    image = mi.render(scene)
    assert dr.all(image.array > 0.0)


def test_direct_integrator_bsdf_sample_connects_to_kohler(
        variant_scalar_rgb):
    scene = make_reflection_scene(
        {
            'type': 'direct',
            'emitter_samples': 0,
            'bsdf_samples': 1,
        }
    )

    image = mi.render(scene)
    assert dr.all(image.array > 0.0)
