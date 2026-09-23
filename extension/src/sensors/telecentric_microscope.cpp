#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/telecentric_microscope.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
TelecentricMicroscopeSensor<Float, Spectrum>::TelecentricMicroscopeSensor(
    const Properties &props) : Base(props) {
    m_na_det = props.get<ScalarFloat>("na_det", .25f);
    m_n_det_medium = props.get<ScalarFloat>("n_det_medium", 1.f);
    m_det_focus_z = props.get<ScalarFloat>("det_focus_z", 0.f);
    m_sensor_z = props.get<ScalarFloat>("sensor_z", 10.f);
    m_fov_mm = props.get<ScalarFloat>("fov_mm", 1.f);
    m_fov_y_mm = props.get<ScalarFloat>("fov_y_mm", m_fov_mm);
    m_telecentric_mag_error_per_mm =
        props.get<ScalarFloat>("telecentric_mag_error_per_mm", 0.f);
    m_field_stop_diameter_mm =
        props.get<ScalarFloat>("field_stop_diameter_mm", 0.f);

    if (m_n_det_medium <= 0.f || m_na_det < 0.f || m_na_det >= m_n_det_medium)
        Throw("TelecentricMicroscopeSensor: require 0 <= na_det < n_det_medium.");
    if (m_sensor_z <= m_det_focus_z)
        Throw("TelecentricMicroscopeSensor: sensor_z must be > det_focus_z.");
    if (m_fov_mm <= 0.f || m_fov_y_mm <= 0.f)
        Throw("TelecentricMicroscopeSensor: fov must be > 0.");
    if (m_field_stop_diameter_mm < 0.f)
        Throw("TelecentricMicroscopeSensor: field_stop_diameter_mm must be >= 0.");

    m_field_stop_radius_mm = .5f * m_field_stop_diameter_mm;
    m_sin_theta_max = m_na_det / m_n_det_medium;
    m_needs_sample_2 = true;
    m_needs_sample_3 = m_na_det > 0.f;
}

MI_VARIANT std::pair<typename TelecentricMicroscopeSensor<Float, Spectrum>::Ray3f,
                     Spectrum>
TelecentricMicroscopeSensor<Float, Spectrum>::sample_ray(
    Float time, Float wavelength_sample, const Point2f &position_sample,
    const Point2f &aperture_sample, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

    auto [wavelengths, wav_weight] =
        sample_wavelengths(dr::zeros<SurfaceInteraction3f>(), wavelength_sample,
                           active);

    Point3f focus_p = focus_point(position_sample);
    Mask field_active = active && inside_field_stop(focus_p);
    Ray3f ray = sample_ray(time, position_sample, aperture_sample);
    ray.wavelengths = wavelengths;
    return { ray, dr::select(field_active, wav_weight, dr::zeros<Spectrum>()) };
}

MI_VARIANT std::pair<typename TelecentricMicroscopeSensor<Float, Spectrum>::RayDifferential3f,
                     Spectrum>
TelecentricMicroscopeSensor<Float, Spectrum>::sample_ray_differential(
    Float time, Float wavelength_sample, const Point2f &position_sample,
    const Point2f &aperture_sample, Mask active) const {
    auto [ray_base, weight] =
        sample_ray(time, wavelength_sample, position_sample, aperture_sample,
                   active);
    RayDifferential3f ray(ray_base);

    Vector2f inv_size = dr::rcp(Vector2f(m_film->crop_size()));
    Ray3f ray_x = sample_ray(time,
                             position_sample + Point2f(inv_size.x(), 0.f),
                             aperture_sample);
    Ray3f ray_y = sample_ray(time,
                             position_sample + Point2f(0.f, inv_size.y()),
                             aperture_sample);
    ray.o_x = ray_x.o;
    ray.o_y = ray_y.o;
    ray.d_x = ray_x.d;
    ray.d_y = ray_y.d;
    ray.has_differentials = true;
    return { ray, weight };
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::ScalarBoundingBox3f
TelecentricMicroscopeSensor<Float, Spectrum>::bbox() const {
    return ScalarBoundingBox3f(
        ScalarPoint3f(-.5f * m_fov_mm, -.5f * m_fov_y_mm, m_det_focus_z),
        ScalarPoint3f( .5f * m_fov_mm,  .5f * m_fov_y_mm, m_sensor_z));
}

MI_VARIANT std::string
TelecentricMicroscopeSensor<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "TelecentricMicroscopeSensor["
        << "na_det=" << m_na_det
        << ", n_det_medium=" << m_n_det_medium
        << ", det_focus_z=" << m_det_focus_z
        << ", sensor_z=" << m_sensor_z
        << ", fov_mm=" << m_fov_mm
        << ", fov_y_mm=" << m_fov_y_mm
        << ", field_stop_diameter_mm=" << m_field_stop_diameter_mm
        << "]";
    return oss.str();
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::Point2f
TelecentricMicroscopeSensor<Float, Spectrum>::sample_uniform_disk(
    const Point2f &sample) const {
    Float r = dr::sqrt(sample.x());
    Float phi = 2.f * dr::Pi<Float> * sample.y();
    return Point2f(r * dr::cos(phi), r * dr::sin(phi));
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::Point3f
TelecentricMicroscopeSensor<Float, Spectrum>::focus_point(
    const Point2f &sample) const {
    return Point3f((sample.x() - .5f) * m_fov_mm,
                   (sample.y() - .5f) * m_fov_y_mm,
                   m_det_focus_z);
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::Mask
TelecentricMicroscopeSensor<Float, Spectrum>::inside_field_stop(
    const Point3f &focus_p) const {
    if (m_field_stop_diameter_mm <= 0.f)
        return true;
    Float radius_squared = focus_p.x() * focus_p.x() +
                           focus_p.y() * focus_p.y();
    return radius_squared <= dr::square(Float(m_field_stop_radius_mm));
}

MI_VARIANT std::tuple<typename TelecentricMicroscopeSensor<Float, Spectrum>::Vector3f,
                      typename TelecentricMicroscopeSensor<Float, Spectrum>::Vector3f,
                      typename TelecentricMicroscopeSensor<Float, Spectrum>::Vector3f>
TelecentricMicroscopeSensor<Float, Spectrum>::chief_ray_basis_at(
    const Point3f &focus_p) const {
    Vector3f chief = dr::normalize(Vector3f(
        m_telecentric_mag_error_per_mm * focus_p.x(),
        m_telecentric_mag_error_per_mm * focus_p.y(), -1.f));
    Vector3f reference(0.f, 1.f, 0.f);
    reference = dr::select(dr::abs(chief.y()) > .95f,
                           Vector3f(1.f, 0.f, 0.f), reference);
    Vector3f basis_u = dr::normalize(dr::cross(reference, chief));
    Vector3f basis_v = dr::cross(chief, basis_u);
    return { chief, basis_u, basis_v };
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::Vector3f
TelecentricMicroscopeSensor<Float, Spectrum>::sample_detection_direction(
    const Point3f &focus_p, const Point2f &sample) const {
    auto [chief, basis_u, basis_v] = chief_ray_basis_at(focus_p);
    if (m_na_det <= 0.f)
        return chief;

    Point2f pupil = sample_uniform_disk(sample);
    Float sx = m_sin_theta_max * pupil.x();
    Float sy = m_sin_theta_max * pupil.y();
    Float sz = dr::sqrt(dr::maximum(1.f - sx * sx - sy * sy, Float(1e-12f)));
    return dr::normalize(basis_u * sx + basis_v * sy + chief * sz);
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::Ray3f
TelecentricMicroscopeSensor<Float, Spectrum>::ray_from_focus(
    const Point3f &focus_p, const Vector3f &view_dir, Float time) const {
    Float t = (m_sensor_z - m_det_focus_z) / (-view_dir.z());
    Ray3f ray;
    ray.o = focus_p - view_dir * t;
    ray.d = view_dir;
    ray.time = time;
    return ray;
}

MI_VARIANT typename TelecentricMicroscopeSensor<Float, Spectrum>::Ray3f
TelecentricMicroscopeSensor<Float, Spectrum>::sample_ray(
    Float time, const Point2f &sample2, const Point2f &sample3) const {
    Point3f fp = focus_point(sample2);
    Vector3f view_dir = sample_detection_direction(fp, sample3);
    return ray_from_focus(fp, view_dir, time);
}

MI_INSTANTIATE_CLASS(TelecentricMicroscopeSensor)
MI_EXPORT_PLUGIN(TelecentricMicroscopeSensor)
NAMESPACE_END(mitsuba)
