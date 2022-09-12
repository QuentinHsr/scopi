#pragma once

#include "base.hpp"
#include "../../quaternion.hpp"

namespace scopi
{
    ///////////////////////
    // sphere definition //
    ///////////////////////
    /**
     * @brief Sphere.
     *
     * @tparam dim Dimension (2 or 3).
     * @tparam owner
     */
    template<std::size_t dim, bool owner=true>
    class sphere: public object<dim, owner>
    {
    public:

        /**
         * @brief Alias for the base class \c object.
         */
        using base_type = object<dim, owner>;
        /**
         * @brief Alias for position type.
         */
        using position_type = typename base_type::position_type;
        /**
         * @brief Alias for quaternion type.
         */
        using quaternion_type = typename base_type::quaternion_type;

        /**
         * @brief Constructor with default rotation.
         *
         * @param pos [in] Position of the center of the sphere.
         * @param radius [in] Radius of the sphere.
         */
        sphere(position_type pos, double radius);
        /**
         * @brief Constructor with given rotation.
         *
         * @param pos [in] Position of the center of the sphere.
         * @param q [in] Quaternion describing the rotation of the sphere.
         * @param radius [in] Radius of the sphere.
         */
        sphere(position_type pos, quaternion_type q, double radius);

        // sphere(const sphere&) = default;
        // sphere& operator=(const sphere&) = default;

        /**
         * @brief Get the radius of the sphere.
         */
        double radius() const;
        /**
         * @brief 
         *
         * TODO
         *
         * @return 
         */
        virtual std::unique_ptr<base_constructor<dim>> construct() const override;
        /**
         * @brief Print the elements of the sphere on standard output.
         */
        virtual void print() const override;
        /**
         * @brief Get the hash of the sphere.
         */
        virtual std::size_t hash() const override;
        /**
         * @brief Get the rotation matrix of the sphere.
         */
        auto rotation() const;
        /**
         * @brief Get the coordinates of the point at the surface of the sphere in 2D.
         *
         * \todo Add drawing.
         *
         * @param b [in] Angle of the point.
         *
         * @return (x, y) coordinates of the point.
         */
        auto point(const double b) const;
        /**
         * @brief Get the coordinates of the point at the surface of the sphere in 3D.
         *
         * \todo Add drawing.
         *
         * @param a [in] Angle of the point.
         * @param b [in] Angle of the point.
         *
         * @return (x, y, z) coordinates of the point.
         */
        auto point(const double a, const double b) const;
        /**
         * @brief Get the outer normal of the sphere in 2D.
         *
         * \todo Add drawing.
         *
         * @param b [in] Angle of the point to compute the normal.
         *
         * @return (x, y) coordinates of the normal.
         */
        auto normal(const double b) const;
        /**
         * @brief Get the outer normal of the sphere in 3D.
         *
         * @param a [in] Angle of the point to compute the normal.
         * @param b [in] Angle of the point to compute the normal.
         *
         * @return  (x, y, z) coordinates of the normal.
         */
        auto normal(const double a, const double b) const;

    private:

        /**
         * @brief Create the hash of the spheres.
         *
         * Two spheres with the same dimension and the same radius have the same hash. 
         */
        void create_hash();

        /**
         * @brief Radius of the sphere.
         */
        double m_radius;
        /**
         * @brief Hash of the sphere.
         */
        std::size_t m_hash;
    };

    ///////////////////////////
    // sphere implementation //
    ///////////////////////////
    template<std::size_t dim, bool owner>
    sphere<dim, owner>::sphere(position_type pos, double radius)
    : base_type(pos, {quaternion()}, 1)
    , m_radius(radius)
    {
        create_hash();
    }

    template<std::size_t dim, bool owner>
    sphere<dim, owner>::sphere(position_type pos, quaternion_type q, double radius)
    : base_type(pos, q, 1)
    , m_radius(radius)
    {
        create_hash();
    }

    template<std::size_t dim, bool owner>
    std::unique_ptr<base_constructor<dim>> sphere<dim, owner>::construct() const
    {
        return make_object_constructor<sphere<dim, false>>(m_radius);
    }

    template<std::size_t dim, bool owner>
    double sphere<dim, owner>::radius() const
    {
        return m_radius;
    }

    template<std::size_t dim, bool owner>
    void sphere<dim, owner>::print() const
    {
        std::cout << "sphere<" << dim << ">(" << m_radius << ")\n";
    }

    template<std::size_t dim, bool owner>
    std::size_t sphere<dim, owner>::hash() const
    {
        return m_hash;
    }

    template<std::size_t dim, bool owner>
    void sphere<dim, owner>::create_hash()
    {
        std::stringstream ss;
        ss << "sphere<" << dim << ">(" << m_radius << ")";
        m_hash = std::hash<std::string>{}(ss.str());
    }

    template<std::size_t dim, bool owner>
    auto sphere<dim, owner>::rotation() const
    {
        return rotation_matrix<dim>(this->q());
    }

    template<std::size_t dim, bool owner>
    auto sphere<dim, owner>::point(const double b) const
    {
        xt::xtensor_fixed<double, xt::xshape<dim>> pt;
        pt(0) = m_radius * std::cos(b);
        pt(1) = m_radius * std::sin(b);
        return xt::flatten(xt::eval(xt::linalg::dot(rotation_matrix<dim>(this->q()),pt) + this->pos()));
    }

    template<std::size_t dim, bool owner>
    auto sphere<dim, owner>::point(const double a, const double b) const
    {
        xt::xtensor_fixed<double, xt::xshape<dim>> pt;
        pt(0) = m_radius * std::cos(a) * std::cos(b);
        pt(1) = m_radius * std::cos(a) * std::sin(b);
        pt(2) = m_radius * std::sin(a);
        return xt::flatten(xt::eval(xt::linalg::dot(rotation_matrix<dim>(this->q()),pt) + this->pos()));
    }

    template<std::size_t dim, bool owner>
    auto sphere<dim, owner>::normal(const double b) const
    {
        xt::xtensor_fixed<double, xt::xshape<dim>> n;
        n(0) = m_radius * std::cos(b);
        n(1) = m_radius * std::sin(b);
        n = xt::flatten(xt::linalg::dot(rotation_matrix<dim>(this->q()),n));
        n /= xt::linalg::norm(n, 2);
        return n;
    }

    template<std::size_t dim, bool owner>
    auto sphere<dim, owner>::normal(const double a, const double b) const
    {
        xt::xtensor_fixed<double, xt::xshape<dim>> n;
        n(0) =  std::cos(a) * std::cos(b);
        n(1) =  std::cos(a) * std::sin(b);
        n(2) =  std::sin(a);
        n = xt::flatten(xt::linalg::dot(rotation_matrix<dim>(this->q()),n));
        n /= xt::linalg::norm(n, 2);
        return n;
    }


}
