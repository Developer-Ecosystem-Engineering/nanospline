#pragma once

#include <nanospline/PatchBase.h>

namespace nanospline {

template <typename _Scalar, int _dim>
class RevolutionPatch final : public PatchBase<_Scalar, _dim>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    static_assert(_dim == 3, "Surface of revolution is 3D only.");

    using Base = PatchBase<_Scalar, _dim>;
    using ThisType = RevolutionPatch<_Scalar, _dim>;
    using Scalar = typename Base::Scalar;
    using Point = typename Base::Point;
    using UVPoint = typename Base::UVPoint;
    using ProfileType = CurveBase<_Scalar, _dim>;

public:
    RevolutionPatch()
    {
        m_location.setZero();
        m_axis << 0, 0, 1;
        Base::set_degree_u(2);
        Base::set_degree_v(2);
    }

    std::unique_ptr<Base> clone() const override {
        auto patch = std::make_unique<ThisType>();
        patch->set_location(get_location());
        patch->set_axis(get_axis());
        patch->set_profile(get_profile());
        patch->set_u_lower_bound(get_u_lower_bound());
        patch->set_u_upper_bound(get_u_upper_bound());
        patch->set_v_lower_bound(get_v_lower_bound());
        patch->set_v_upper_bound(get_v_upper_bound());
        patch->initialize();
        return patch;
    }
    PatchEnum get_patch_type() const override { return PatchEnum::REVOLUTION; }

    const Point& get_location() const { return m_location; }
    void set_location(const Point& p) { m_location = p; }

    const Point& get_axis() const { return m_axis; }
    void set_axis(const Point& d) { m_axis = d; }

    const ProfileType* get_profile() const { return m_profile.get(); }
    void set_profile(const ProfileType* profile) {
        if (profile != nullptr) {
            m_profile = profile->clone();
        } else {
            m_profile = nullptr;
        }
    }
    void set_profile(std::unique_ptr<ProfileType>&& profile) { m_profile = std::move(profile); }

public:
    Point evaluate(Scalar u, Scalar v) const override
    {
        assert_valid_profile();
        const auto p = m_profile->evaluate(v);
        Eigen::AngleAxis<Scalar> R(u, m_axis);
        return m_location + (R * (p - m_location).transpose()).transpose();
    }

    Point evaluate_derivative_u(Scalar u, Scalar v) const override
    {
        assert_valid_profile();
        const auto p = evaluate(u, v);
        Point s = p - m_location;
        Point d = m_axis.cross(s);
        constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 10;
        if (d.norm() > TOL) {
            const Scalar r = (s - s.dot(m_axis) * m_axis).norm();
            d = d.normalized() * r;
        }
        return d;
    }

    Point evaluate_derivative_v(Scalar u, Scalar v) const override
    {
        assert_valid_profile();
        const auto d = m_profile->evaluate_derivative(v);
        Eigen::AngleAxis<Scalar> R(u, m_axis);
        return (R * d.transpose()).transpose();
    }

    Point evaluate_2nd_derivative_uu(Scalar u, Scalar v) const override
    {
        assert_valid_profile();
        const auto p = evaluate(u, v);
        Point d = p - m_location;
        return -d + d.dot(m_axis) * m_axis;
    }

    Point evaluate_2nd_derivative_vv(Scalar u, Scalar v) const override
    {
        assert_valid_profile();
        const auto d = m_profile->evaluate_2nd_derivative(v);
        Eigen::AngleAxis<Scalar> R(u, m_axis);
        return (R * d.transpose()).transpose();
    }

    Point evaluate_2nd_derivative_uv(Scalar u, Scalar v) const override
    {
        assert_valid_profile();
        Point dv = evaluate_derivative_v(u, v);
        Point duv = m_axis.cross(dv);

        constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 10;
        if (duv.norm() > TOL) {
            const Scalar r = (duv - duv.dot(m_axis) * m_axis).norm();
            duv = duv.normalized() * r;
        }
        return duv;
    }

    UVPoint inverse_evaluate(const Point& p,
        const Scalar min_u,
        const Scalar max_u,
        const Scalar min_v,
        const Scalar max_v) const override
    {
        assert_valid_profile();
        constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 100;
        const int num_samples = std::max(m_profile->get_num_control_points(), 7) + 1;
        UVPoint uv = Base::approximate_inverse_evaluate(p, num_samples, min_u, max_u, min_v, max_v, 10);
        uv = Base::newton_raphson(p, uv, 20, TOL, min_u, max_u, min_v, max_v);
        assert(uv[0] >= min_u && uv[0] <= max_u);
        assert(uv[1] >= min_v && uv[1] <= max_v);
        return uv;
    }

    void initialize() override
    {
        constexpr Scalar TOL = std::numeric_limits<Scalar>::epsilon() * 10;
        assert_valid_profile();
        assert(std::abs(m_axis.squaredNorm() - 1) < TOL);
        assert(m_u_upper >= m_u_lower);
        assert(m_v_upper >= m_v_lower);

        Base::set_periodic_u(m_profile->get_periodic());
        if (m_u_upper - m_u_lower > 2 * M_PI - TOL) {
            Base::set_periodic_u(true);
        } else {
            Base::set_periodic_u(false);
        }
    }

    Scalar get_u_lower_bound() const override { return m_u_lower; }
    Scalar get_u_upper_bound() const override { return m_u_upper; }
    Scalar get_v_lower_bound() const override { return m_v_lower; }
    Scalar get_v_upper_bound() const override { return m_v_upper; }

    void set_u_lower_bound(Scalar t) { m_u_lower = t; }
    void set_u_upper_bound(Scalar t) { m_u_upper = t; }
    void set_v_lower_bound(Scalar t) { m_v_lower = t; }
    void set_v_upper_bound(Scalar t) { m_v_upper = t; }

public:
    virtual int get_num_weights_u() const override { return 0; }
    virtual int get_num_weights_v() const override { return 0; }
    virtual Scalar get_weight(int i, int j) const override
    {
        throw not_implemented_error("RevolutionPatch patch does not support weight");
    }
    virtual void set_weight(int i, int j, Scalar val) override
    {
        throw not_implemented_error("RevolutionPatch patch does not support weight");
    }

    virtual int get_num_knots_u() const override { return 0; }
    virtual Scalar get_knot_u(int i) const override
    {
        throw not_implemented_error("RevolutionPatch patch does not support knots");
    }
    virtual void set_knot_u(int i, Scalar val) override
    {
        throw not_implemented_error("RevolutionPatch patch does not support knots");
    }

    virtual int get_num_knots_v() const override { return 0; }
    virtual Scalar get_knot_v(int i) const override
    {
        throw not_implemented_error("RevolutionPatch patch does not support knots");
    }
    virtual void set_knot_v(int i, Scalar val) override
    {
        throw not_implemented_error("RevolutionPatch patch does not support knots");
    }

public:
    int num_control_points_u() const override { return 0; }

    int num_control_points_v() const override { return 0; }

    UVPoint get_control_point_preimage(int i, int j) const override
    {
        throw not_implemented_error("RevolutionPatch does not need control points.");
    }

private:
    void assert_valid_profile() const
    {
        if (m_profile == nullptr) {
            throw invalid_setting_error("Profile not set!");
        }
    }

private:
    Point m_location;
    Point m_axis;
    std::unique_ptr<ProfileType> m_profile = nullptr;
    Scalar m_u_lower = 0;
    Scalar m_u_upper = 1;
    Scalar m_v_lower = 0;
    Scalar m_v_upper = 2 * M_PI;
};

} // namespace nanospline
