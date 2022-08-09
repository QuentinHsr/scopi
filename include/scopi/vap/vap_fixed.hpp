#pragma once

#include "base.hpp"
#include <vector>

namespace scopi
{
    class vap_fixed;
     
    template<>
    struct VapParams<vap_fixed>
    {
    };

    class vap_fixed: public vap_base<vap_fixed>
    {
    public:
        using base_type = vap_base<vap_fixed>;
        template <std::size_t dim>
        void set_a_priori_velocity_impl(scopi_container<dim>& particles, std::vector<neighbor<dim>>& contacts_worms);

        template <std::size_t dim>
        void update_velocity_impl(scopi_container<dim>& particles, const xt::xtensor<double, 2>& uadapt, const xt::xtensor<double, 2>& wadapt);

        vap_fixed(std::size_t Nactive, std::size_t active_ptr, double dt, const VapParams<vap_fixed>& params);

    };

    template <std::size_t dim>
    void vap_fixed::set_a_priori_velocity_impl(scopi_container<dim>&, std::vector<neighbor<dim>>&)
    {}

    template <std::size_t dim>
    void vap_fixed::update_velocity_impl(scopi_container<dim>&, const xt::xtensor<double, 2>&, const xt::xtensor<double, 2>&)
    {}
}
