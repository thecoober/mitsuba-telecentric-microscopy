#include <algorithm>

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT class KohlerEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_needs_sample_2, m_needs_sample_3)
    MI_IMPORT_TYPES(Texture)

    KohlerEmitter(const Properties &props) : Base(props) {
        m_na = props.get<ScalarFloat>("na", .25f);
        m_n_medium = props.get<ScalarFloat>("n_medium", 1.f);
        m_condenser_z = props.get<ScalarFloat>("condenser_z", 1.f);
        m_illum_focus_z = props.get<ScalarFloat>("illum_focus_z", 0.f);
        m_focus_epsilon_mm =
            props.get<ScalarFloat>("focus_epsilon_mm", 1e-7f);
        m_intersection_area_epsilon_mm2 =
            props.get<ScalarFloat>("intersection_area_epsilon_mm2", 1e-7f);
        m_intersection_segments =
            props.get<uint32_t>("intersection_segments", 24);
        m_intersection_sampling =
            props.get<std::string>("intersection_sampling", "polygon");

        ScalarFloat field_diameter =
            props.has_property("field_diameter_mm")
                ? props.get<ScalarFloat>("field_diameter_mm")
                : props.get<ScalarFloat>("source_diameter_mm", 1.f);
        m_field_radius = .5f * field_diameter;

        m_radiance = props.has_property("radiance")
                         ? props.get_emissive_texture<Texture>("radiance", 1.f)
                         : props.get_emissive_texture<Texture>("total_radiance", 1.f);

        if (m_radiance->is_spatially_varying())
            Throw("KohlerEmitter: radiance must not be spatially varying.");
        if (m_na <= 0.f || m_n_medium <= 0.f || m_na >= m_n_medium)
            Throw("KohlerEmitter: require 0 < na < n_medium.");
        if (m_field_radius <= 0.f)
            Throw("KohlerEmitter: field/source diameter must be > 0.");
        if (m_condenser_z <= m_illum_focus_z)
            Throw("KohlerEmitter: condenser_z must be > illum_focus_z.");
        if (m_focus_epsilon_mm < 0.f)
            Throw("KohlerEmitter: focus_epsilon_mm must be >= 0.");
        if (m_intersection_area_epsilon_mm2 <= 0.f)
            Throw("KohlerEmitter: intersection_area_epsilon_mm2 must be > 0.");
        if (m_intersection_segments < 8)
            Throw("KohlerEmitter: intersection_segments must be >= 8.");
        if (m_intersection_sampling != "polar" &&
            m_intersection_sampling != "polygon")
            Throw("KohlerEmitter: intersection_sampling must be 'polar' or 'polygon'.");

        m_sin_theta_max = m_na / m_n_medium;
        m_sin_theta_max_2 = m_sin_theta_max * m_sin_theta_max;
        m_pupil_pdf_area = 1.f / (dr::Pi<ScalarFloat> * m_sin_theta_max_2);
        m_irradiance_to_radiance =
            1.f / (dr::Pi<ScalarFloat> * m_sin_theta_max_2);
        m_field_area = dr::Pi<ScalarFloat> * m_field_radius * m_field_radius;
        m_position_pdf = 1.f / m_field_area;
        m_tan_theta_max =
            m_sin_theta_max / std::sqrt(1.f - m_sin_theta_max_2);
        m_defocus_distance_threshold_mm =
            std::sqrt(m_intersection_area_epsilon_mm2 /
                      dr::Pi<ScalarFloat>) /
            m_tan_theta_max;
        m_focus_switch_distance_mm =
            std::max(m_focus_epsilon_mm, m_defocus_distance_threshold_mm);

        ScalarFloat dz = m_condenser_z - m_illum_focus_z;
        m_condenser_radius = m_field_radius + dz * m_tan_theta_max;

        m_flags = +EmitterFlags::Infinite |
                  +EmitterFlags::ReferenceDependent;
        m_needs_sample_2 = true;
        m_needs_sample_3 = true;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("radiance", m_radiance, ParamFlags::Differentiable);
    }

    Vector3f emitted_direction(const Point2f &sample) const {
        Point2f pupil = sample_uniform_disk(sample);
        Float sx = m_sin_theta_max * pupil.x();
        Float sy = m_sin_theta_max * pupil.y();
        Float sz = -dr::sqrt(dr::maximum(1.f - sx * sx - sy * sy,
                                         Float(0.f)));
        return Vector3f(sx, sy, sz);
    }

    Float direction_pdf(const Vector3f &emitted_dir) const {
        return dr::abs(emitted_dir.z()) * m_pupil_pdf_area;
    }

    Point3f point_on_z_plane(const Point3f &p, const Vector3f &emitted_dir,
                             ScalarFloat z) const {
        Float t = (p.z() - z) / emitted_dir.z();
        return p - emitted_dir * t;
    }

    Point3f origin_on_condenser(const Point3f &focus_p,
                                const Vector3f &emitted_dir) const {
        Float t = (m_illum_focus_z - m_condenser_z) / emitted_dir.z();
        return focus_p - emitted_dir * t;
    }

    Vector3f focus_direction(const Point3f &p, const Point3f &focus_p) const {
        Vector3f to_lower_z = dr::select(m_illum_focus_z > p.z(),
                                         p - focus_p, focus_p - p);
        return dr::normalize(to_lower_z);
    }

    std::tuple<Float, Float> intersection_fan_center(const Point3f &p,
                                                     const Float &blur_radius) const {
        Float d = dr::sqrt(p.x() * p.x() + p.y() * p.y());
        Float inv_d = dr::select(d > 1e-7f, dr::rcp(d), Float(0.f));
        Float ux = p.x() * inv_d;
        Float uy = p.y() * inv_d;

        Mask footprint_inside_field = d + blur_radius <= m_field_radius;
        Mask field_inside_footprint = d + m_field_radius <= blur_radius;

        Float chord_x = (d * d + m_field_radius * m_field_radius -
                         blur_radius * blur_radius) /
                        dr::maximum(2.f * d, Float(1e-7f));
        Float center_x = dr::select(
            footprint_inside_field, p.x(),
            dr::select(field_inside_footprint, Float(0.f), ux * chord_x));
        Float center_y = dr::select(
            footprint_inside_field, p.y(),
            dr::select(field_inside_footprint, Float(0.f), uy * chord_x));
        return { center_x, center_y };
    }

    Float disk_ray_extent(const Float &cx, const Float &cy,
                          const Float &dx, const Float &dy, const Float &radius,
                          const Float &ux, const Float &uy) const {
        Float qx = cx - dx;
        Float qy = cy - dy;
        Float b = qx * ux + qy * uy;
        Float c = qx * qx + qy * qy;
        return -b + dr::sqrt(dr::maximum(b * b + radius * radius - c,
                                         Float(0.f)));
    }

    std::vector<std::pair<Float, Float>>
    intersection_fan_vertices(const Float &center_x, const Float &center_y,
                              const Point3f &p,
                              const Float &blur_radius) const {
        std::vector<std::pair<Float, Float>> vertices;
        vertices.reserve(m_intersection_segments);
        for (uint32_t i = 0; i < m_intersection_segments; ++i) {
            ScalarFloat phi = 2.f * dr::Pi<ScalarFloat> *
                              i / (ScalarFloat) m_intersection_segments;
            Float ux = std::cos(phi);
            Float uy = std::sin(phi);
            Float t_field =
                disk_ray_extent(center_x, center_y, Float(0.f), Float(0.f),
                                Float(m_field_radius), ux, uy);
            Float t_footprint =
                disk_ray_extent(center_x, center_y, p.x(), p.y(),
                                blur_radius, ux, uy);
            Float t = dr::minimum(t_field, t_footprint);
            vertices.push_back({ center_x + t * ux, center_y + t * uy });
        }
        return vertices;
    }

    Float fan_polygon_area(const std::vector<std::pair<Float, Float>> &vertices,
                           const Float &center_x, const Float &center_y) const {
        Float total(0.f);
        for (uint32_t i = 0; i < m_intersection_segments; ++i) {
            const auto &[x0, y0] = vertices[i];
            const auto &[x1, y1] = vertices[(i + 1) % m_intersection_segments];
            Float ax = x0 - center_x, ay = y0 - center_y;
            Float bx = x1 - center_x, by = y1 - center_y;
            total += .5f * dr::maximum(ax * by - ay * bx, Float(0.f));
        }
        return total;
    }

    Float intersection_polygon_area(const Point3f &p,
                                    const Float &blur_radius) const {
        auto [center_x, center_y] = intersection_fan_center(p, blur_radius);
        auto vertices = intersection_fan_vertices(center_x, center_y, p,
                                                  blur_radius);
        return fan_polygon_area(vertices, center_x, center_y);
    }

    Float polar_extent(const Float &center_x, const Float &center_y,
                       const Point3f &p, const Float &blur_radius,
                       const Float &ux, const Float &uy) const {
        Float t_field =
            disk_ray_extent(center_x, center_y, Float(0.f), Float(0.f),
                            Float(m_field_radius), ux, uy);
        Float t_footprint =
            disk_ray_extent(center_x, center_y, p.x(), p.y(), blur_radius,
                            ux, uy);
        return dr::minimum(t_field, t_footprint);
    }

    Float disk_overlap_area(ScalarFloat r0, const Float &r1,
                            const Float &d) const {
        Mask no_overlap = d >= r0 + r1;
        Mask r0_contains_r1 = d + r1 <= r0;
        Mask r1_contains_r0 = d + r0 <= r1;

        Float d_safe = dr::maximum(d, Float(1e-7f));
        Float r1_safe = dr::maximum(r1, Float(1e-7f));
        Float c0 = (d_safe * d_safe + r0 * r0 - r1_safe * r1_safe) /
                   (2.f * d_safe * r0);
        Float c1 = (d_safe * d_safe + r1_safe * r1_safe - r0 * r0) /
                   (2.f * d_safe * r1_safe);
        c0 = dr::clip(c0, -1.f, 1.f);
        c1 = dr::clip(c1, -1.f, 1.f);

        Float lens =
            r0 * r0 * dr::acos(c0) +
            r1_safe * r1_safe * dr::acos(c1) -
            .5f * dr::sqrt(dr::maximum(
                (-d_safe + r0 + r1_safe) * (d_safe + r0 - r1_safe) *
                    (d_safe - r0 + r1_safe) * (d_safe + r0 + r1_safe),
                Float(0.f)));
        Float area = dr::select(no_overlap, Float(0.f), lens);
        area = dr::select(r0_contains_r1, dr::Pi<Float> * r1 * r1, area);
        area = dr::select(r1_contains_r0,
                          dr::Pi<Float> * r0 * r0, area);
        return area;
    }

    std::tuple<Point3f, Float, Mask>
    sample_focus_intersection_polar(const Point3f &p, const Point2f &sample,
                                    Mask active) const {
        Float dz = dr::abs(m_illum_focus_z - p.z());
        Float blur_radius = dz * m_tan_theta_max;
        Float d = dr::sqrt(p.x() * p.x() + p.y() * p.y());
        Float exact_area = disk_overlap_area(m_field_radius, blur_radius, d);
        Mask valid = active &&
                     (exact_area > m_intersection_area_epsilon_mm2);

        auto [center_x, center_y] = intersection_fan_center(p, blur_radius);
        Float phi = 2.f * dr::Pi<Float> * sample.x();
        Float ux = dr::cos(phi), uy = dr::sin(phi);
        Float extent = polar_extent(center_x, center_y, p, blur_radius, ux, uy);
        Float radius = dr::sqrt(sample.y()) * extent;
        Point3f focus_p(center_x + radius * ux,
                        center_y + radius * uy,
                        m_illum_focus_z);
        Float area_pdf = dr::rcp(dr::maximum(dr::Pi<Float> * extent * extent,
                                             Float(m_intersection_area_epsilon_mm2)));
        return { focus_p, area_pdf, valid && (extent > 1e-7f) };
    }

    std::tuple<Point3f, Float, Mask>
    sample_focus_intersection_polygon(const Point3f &p, const Point2f &sample,
                                      Mask active) const {
        Float dz = dr::abs(m_illum_focus_z - p.z());
        Float blur_radius = dz * m_tan_theta_max;
        Float d = dr::sqrt(p.x() * p.x() + p.y() * p.y());
        Float exact_area = disk_overlap_area(m_field_radius, blur_radius, d);
        Mask valid = active &&
                     (exact_area > m_intersection_area_epsilon_mm2);

        auto [center_x, center_y] = intersection_fan_center(p, blur_radius);
        auto vertices = intersection_fan_vertices(center_x, center_y, p,
                                                  blur_radius);

        std::vector<Float> areas;
        areas.reserve(m_intersection_segments);
        Float total_area(0.f);
        for (uint32_t i = 0; i < m_intersection_segments; ++i) {
            const auto &[x0, y0] = vertices[i];
            const auto &[x1, y1] = vertices[(i + 1) % m_intersection_segments];
            Float ax = x0 - center_x, ay = y0 - center_y;
            Float bx = x1 - center_x, by = y1 - center_y;
            Float area = .5f * dr::maximum(ax * by - ay * bx, Float(0.f));
            areas.push_back(area);
            total_area += area;
        }

        Float target = sample.x() * total_area;
        Float prefix(0.f), local_x(0.f);
        Float tri_x0(center_x), tri_y0(center_y);
        Float tri_x1(center_x), tri_y1(center_y);
        Mask chosen(false);
        for (uint32_t i = 0; i < m_intersection_segments; ++i) {
            Mask positive_area = areas[i] > 0.f;
            Mask hit = valid && !chosen && positive_area &&
                       (target <= prefix + areas[i]);
            Float lx = (target - prefix) /
                       dr::maximum(areas[i], Float(1e-20f));
            const auto &[x0, y0] = vertices[i];
            const auto &[x1, y1] = vertices[(i + 1) % m_intersection_segments];
            local_x = dr::select(hit, dr::clip(lx, 0.f, 1.f), local_x);
            tri_x0 = dr::select(hit, x0, tri_x0);
            tri_y0 = dr::select(hit, y0, tri_y0);
            tri_x1 = dr::select(hit, x1, tri_x1);
            tri_y1 = dr::select(hit, y1, tri_y1);
            chosen |= hit;
            prefix += areas[i];
        }

        Float su = dr::sqrt(local_x);
        Float b1 = su * (1.f - sample.y());
        Float b2 = su * sample.y();
        Point3f focus_p(center_x + b1 * (tri_x0 - center_x) +
                            b2 * (tri_x1 - center_x),
                        center_y + b1 * (tri_y0 - center_y) +
                            b2 * (tri_y1 - center_y),
                        m_illum_focus_z);
        Float area_pdf = dr::rcp(dr::maximum(
            total_area, Float(m_intersection_area_epsilon_mm2)));
        return { focus_p, area_pdf, valid && chosen };
    }

    std::tuple<Point3f, Float, Mask>
    sample_focus_intersection(const Point3f &p, const Point2f &sample,
                              Mask active) const {
        if (m_intersection_sampling == "polar")
            return sample_focus_intersection_polar(p, sample, active);
        return sample_focus_intersection_polygon(p, sample, active);
    }

    Float focus_area_pdf(const Point3f &p, const Point3f &focus_p,
                         Mask active) const {
        Float dz = dr::abs(m_illum_focus_z - p.z());
        Float blur_radius = dz * m_tan_theta_max;
        Float d = dr::sqrt(p.x() * p.x() + p.y() * p.y());
        Float exact_area = disk_overlap_area(m_field_radius, blur_radius, d);

        if (m_intersection_sampling == "polygon")
            return dr::select(
                active && (exact_area > m_intersection_area_epsilon_mm2),
                              dr::rcp(dr::maximum(
                                  intersection_polygon_area(p, blur_radius),
                                  Float(m_intersection_area_epsilon_mm2))),
                              Float(0.f));

        auto [center_x, center_y] = intersection_fan_center(p, blur_radius);
        Float vx = focus_p.x() - center_x;
        Float vy = focus_p.y() - center_y;
        Float r = dr::sqrt(vx * vx + vy * vy);
        Float ux = vx / dr::maximum(r, Float(1e-7f));
        Float uy = vy / dr::maximum(r, Float(1e-7f));
        Float extent = polar_extent(center_x, center_y, p, blur_radius, ux, uy);
        Mask valid = active &&
                     (exact_area > m_intersection_area_epsilon_mm2) &&
                     (r <= extent + 1e-6f);
        return dr::select(valid,
                          dr::rcp(dr::maximum(dr::Pi<Float> * extent * extent,
                                              Float(m_intersection_area_epsilon_mm2))),
                          Float(0.f));
    }

    std::tuple<Vector3f, Point3f, Float, Mask>
    focused_direction_sample(const Point3f &p, const Point2f &sample,
                             Mask active) const {
        Vector3f emitted_dir = emitted_direction(sample);
        Point3f focus_p = point_on_z_plane(p, emitted_dir, m_illum_focus_z);
        Point3f origin = origin_on_condenser(focus_p, emitted_dir);
        Mask valid = active && inside_disk(focus_p, m_field_radius);
        return { emitted_dir, origin, direction_pdf(emitted_dir), valid };
    }

    std::tuple<Vector3f, Point3f, Float, Mask>
    defocused_direction_sample(const Point3f &p, const Point2f &sample,
                               Mask active) const {
        Point3f focus_p;
        Float area_pdf;
        Mask valid;
        std::tie(focus_p, area_pdf, valid) =
            sample_focus_intersection(p, sample, active);
        Vector3f emitted_dir = focus_direction(p, focus_p);
        Point3f origin = origin_on_condenser(focus_p, emitted_dir);
        Float dist = dr::norm(focus_p - p);
        Float dz = dr::abs(m_illum_focus_z - p.z());
        Float pdf = area_pdf * dist * dist * dist /
                    dr::maximum(dz, Float(1e-7f));
        return { emitted_dir, origin, pdf, valid };
    }

    std::tuple<Vector3f, Point3f, Float, Mask>
    direct_sample(const Point3f &p, const Point2f &sample, Mask active) const {
        Mask focused = is_focused(p);
        auto [dir_f, origin_f, pdf_f, valid_f] =
            focused_direction_sample(p, sample, active);
        auto [dir_d, origin_d, pdf_d, valid_d] =
            defocused_direction_sample(p, sample, active && !focused);
        return {
            dr::select(focused, dir_f, dir_d),
            dr::select(focused, origin_f, origin_d),
            dr::select(focused, pdf_f, pdf_d),
            dr::select(focused, valid_f, valid_d)
        };
    }

    Float direct_pdf(const Point3f &p, const Vector3f &emitted_dir,
                     Mask active) const {
        Float sin2 = emitted_dir.x() * emitted_dir.x() +
                     emitted_dir.y() * emitted_dir.y();
        Mask in_na = (emitted_dir.z() < 0.f) && (sin2 <= m_sin_theta_max_2);
        Mask focused = is_focused(p);

        Point3f focus_p = point_on_z_plane(p, emitted_dir, m_illum_focus_z);
        Mask in_field = inside_disk(focus_p, m_field_radius);
        Float dir_pdf = direction_pdf(emitted_dir);

        Float dz = dr::abs(m_illum_focus_z - p.z());
        Float blur_radius = dz * m_tan_theta_max;
        Float d = dr::sqrt(p.x() * p.x() + p.y() * p.y());
        Float exact_area = disk_overlap_area(m_field_radius, blur_radius, d);
        Float dist = dr::norm(focus_p - p);
        Float area_pdf = focus_area_pdf(p, focus_p, active);
        Float focus_pdf = area_pdf * dist * dist * dist /
                          dr::maximum(dz, Float(1e-7f));

        Mask valid_focused = active && focused && in_field && in_na;
        Mask valid_defocused =
            active && !focused && in_na && in_field &&
            (exact_area > m_intersection_area_epsilon_mm2);
        Float pdf = dr::select(focused, dir_pdf, focus_pdf);
        return dr::select(valid_focused || valid_defocused, pdf, Float(0.f));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2,
                                          const Point2f &sample3,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample, active);

        Point2f disk = sample_uniform_disk(sample2);
        Point3f field_p(m_field_radius * disk.x(),
                        m_field_radius * disk.y(),
                        m_illum_focus_z);
        Vector3f dir = emitted_direction(sample3);
        Point3f origin = origin_on_condenser(field_p, dir);
        Ray3f ray(origin, dir, time, wavelengths);

        Float pdf = m_position_pdf * direction_pdf(dir);
        return { ray, depolarizer<Spectrum>(wav_weight / pdf) };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Vector3f emitted_dir;
        Point3f origin;
        Float pdf;
        Mask valid;
        std::tie(emitted_dir, origin, pdf, valid) =
            direct_sample(it.p, sample, active);

        DirectionSample3f ds;
        ds.p = origin;
        ds.n = Normal3f(0.f, 0.f, -1.f);
        ds.uv = Point2f(0.f);
        ds.time = it.time;
        ds.delta = false;
        ds.emitter = this;
        ds.d = dr::normalize(origin - it.p);
        ds.dist = dr::norm(origin - it.p);
        ds.pdf = pdf;

        valid = valid && (ds.dist > 0.f) && (pdf > 0.f);
        ds.pdf = dr::select(valid, ds.pdf, Float(0.f));

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;
        UnpolarizedSpectrum radiance =
            m_irradiance_to_radiance * m_radiance->eval(si, active);
        return { ds, depolarizer<Spectrum>(
                         dr::select(valid, radiance / pdf,
                                    UnpolarizedSpectrum(0.f))) };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        return direct_pdf(it.p, -ds.d, active);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        Float pdf = direct_pdf(it.p, -ds.d, active);
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;
        UnpolarizedSpectrum radiance =
            m_irradiance_to_radiance * m_radiance->eval(si, active);
        return depolarizer<Spectrum>(
            dr::select(pdf > 0.f, radiance, UnpolarizedSpectrum(0.f)));
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        Point2f disk = sample_uniform_disk(sample);
        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p = Point3f(m_field_radius * disk.x(),
                       m_field_radius * disk.y(),
                       m_illum_focus_z);
        ps.n = Normal3f(0.f, 0.f, 1.f);
        ps.uv = sample;
        ps.time = time;
        ps.delta = false;
        ps.pdf = dr::select(active, Float(m_position_pdf), Float(0.f));
        return { ps, dr::select(active, dr::rcp(Float(m_position_pdf)), Float(0.f)) };
    }

    Float pdf_position(const PositionSample3f &ps, Mask active) const override {
        Mask valid = active &&
                     (dr::abs(ps.p.z() - m_illum_focus_z) < 1e-6f) &&
                     inside_disk(ps.p, m_field_radius);
        return dr::select(valid, Float(m_position_pdf), Float(0.f));
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        auto [wavelengths, weight] = m_radiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
        return { wavelengths, m_irradiance_to_radiance * weight };
    }

    Spectrum eval(const SurfaceInteraction3f &, Mask) const override {
        return Spectrum(0.f);
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarFloat r = m_condenser_radius;
        return ScalarBoundingBox3f(ScalarPoint3f(-r, -r, m_condenser_z),
                                   ScalarPoint3f( r,  r, m_condenser_z));
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "KohlerEmitter["
            << "na=" << m_na
            << ", n_medium=" << m_n_medium
            << ", focal_irradiance_to_radiance=" << m_irradiance_to_radiance
            << ", field_radius=" << m_field_radius
            << ", condenser_z=" << m_condenser_z
            << ", illum_focus_z=" << m_illum_focus_z
            << ", focus_distance_threshold_mm=" << m_focus_epsilon_mm
            << ", defocus_distance_threshold_mm="
            << m_defocus_distance_threshold_mm
            << ", focus_switch_distance_mm=" << m_focus_switch_distance_mm
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(KohlerEmitter)

private:
    Mask is_focused(const Point3f &p) const {
        return dr::abs(m_illum_focus_z - p.z()) <=
               m_focus_switch_distance_mm;
    }

    Point2f sample_uniform_disk(const Point2f &sample) const {
        Float r = dr::sqrt(sample.x());
        Float phi = 2.f * dr::Pi<Float> * sample.y();
        return Point2f(r * dr::cos(phi), r * dr::sin(phi));
    }

    Mask inside_disk(const Point3f &p, ScalarFloat radius) const {
        return p.x() * p.x() + p.y() * p.y() <= radius * radius;
    }

    ref<Texture> m_radiance;
    ScalarFloat m_na, m_n_medium, m_condenser_z, m_illum_focus_z;
    ScalarFloat m_focus_epsilon_mm, m_intersection_area_epsilon_mm2;
    ScalarFloat m_defocus_distance_threshold_mm, m_focus_switch_distance_mm;
    ScalarFloat m_field_radius;
    ScalarFloat m_sin_theta_max, m_sin_theta_max_2, m_pupil_pdf_area;
    ScalarFloat m_irradiance_to_radiance;
    ScalarFloat m_field_area, m_position_pdf, m_tan_theta_max;
    ScalarFloat m_condenser_radius;
    uint32_t m_intersection_segments;
    std::string m_intersection_sampling;

    MI_TRAVERSE_CB(Base, m_radiance)
};

MI_EXPORT_PLUGIN(KohlerEmitter)
NAMESPACE_END(mitsuba)
