#pragma once

#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class TelecentricMicroscopeSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, m_needs_sample_2, m_needs_sample_3,
                   sample_wavelengths)
    MI_IMPORT_TYPES()

    TelecentricMicroscopeSensor(const Properties &props);

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &position_sample,
                                          const Point2f &aperture_sample,
                                          Mask active) const override;

    std::pair<RayDifferential3f, Spectrum> sample_ray_differential(
        Float time, Float wavelength_sample, const Point2f &position_sample,
        const Point2f &aperture_sample, Mask active) const override;

    ScalarBoundingBox3f bbox() const override;
    std::string to_string() const override;

    bool is_telecentric_microscope() const override { return true; }
    ScalarFloat microscope_na_det() const override { return m_na_det; }
    ScalarFloat microscope_n_det_medium() const override { return m_n_det_medium; }
    ScalarFloat microscope_det_focus_z() const override { return m_det_focus_z; }
    ScalarFloat microscope_sensor_z() const override { return m_sensor_z; }
    ScalarFloat microscope_fov_mm() const override { return m_fov_mm; }
    ScalarFloat microscope_fov_y_mm() const override { return m_fov_y_mm; }
    ScalarFloat microscope_telecentric_mag_error_per_mm() const override {
        return m_telecentric_mag_error_per_mm;
    }

    MI_DECLARE_CLASS(TelecentricMicroscopeSensor)

private:
    Point2f sample_uniform_disk(const Point2f &sample) const;
    Point3f focus_point(const Point2f &sample) const;
    Mask inside_field_stop(const Point3f &focus_p) const;
    std::tuple<Vector3f, Vector3f, Vector3f>
    chief_ray_basis_at(const Point3f &focus_p) const;
    Vector3f sample_detection_direction(const Point3f &focus_p,
                                        const Point2f &sample) const;
    Ray3f ray_from_focus(const Point3f &focus_p, const Vector3f &view_dir,
                         Float time) const;
    Ray3f sample_ray(Float time, const Point2f &sample2,
                     const Point2f &sample3) const;

    ScalarFloat m_na_det, m_n_det_medium, m_det_focus_z, m_sensor_z;
    ScalarFloat m_fov_mm, m_fov_y_mm, m_telecentric_mag_error_per_mm;
    ScalarFloat m_field_stop_diameter_mm, m_field_stop_radius_mm;
    ScalarFloat m_sin_theta_max;
};

MI_EXTERN_CLASS(TelecentricMicroscopeSensor)

NAMESPACE_END(mitsuba)
