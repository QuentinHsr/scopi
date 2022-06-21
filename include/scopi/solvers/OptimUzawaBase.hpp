#pragma once

#include "OptimBase.hpp"

#include <omp.h>
#include "tbb/tbb.h"

#include <xtensor/xadapt.hpp>
#include <xtensor/xview.hpp>
#include <xtensor/xnoalias.hpp>
#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"

#include "../params/OptimParams.hpp"
#include "../problems/DryWithoutFriction.hpp"

namespace scopi{
    template<class problem_t>
    class OptimParamsUzawaBase
    {
    public:
        OptimParamsUzawaBase();
        OptimParamsUzawaBase(const OptimParamsUzawaBase& params);

        ProblemParams<problem_t> m_problem_params;
        double m_tol;
        std::size_t m_max_iter;
        double m_rho;
    };

    template<class Derived, class problem_t = DryWithoutFriction>
    class OptimUzawaBase: public OptimBase<Derived>
    {
    public:
        using base_type = OptimBase<Derived>;
        using problem_type = problem_t; 
        OptimUzawaBase(std::size_t nparts, double dt, const OptimParams<Derived>& optim_params);

        template <std::size_t dim>
        int solve_optimization_problem_impl(scopi_container<dim>& particles,
                                            const std::vector<neighbor<dim>>& contacts,
                                            problem_t& problem);

        auto uadapt_data();
        auto wadapt_data();
        auto lagrange_multiplier_data();

        int get_nb_active_contacts_impl() const;

    protected:
        template <std::size_t dim>
        void gemv_inv_P(const scopi_container<dim>& particles, problem_t& problem);

        template <std::size_t dim>
        void gemv_A(const scopi_container<dim>& particles,
                    const std::vector<neighbor<dim>>& contacts, 
                    problem_t& problem);

        template <std::size_t dim>
        void gemv_transpose_A(const scopi_container<dim>& particles,
                              const std::vector<neighbor<dim>>& contacts,
                              problem_t& problem);

        template <std::size_t dim>
        void init_uzawa(scopi_container<dim>& particles,
                        const std::vector<neighbor<dim>>& contacts, 
                        problem_t& problem);
        void finalize_uzawa();

        const double m_dmin;
        xt::xtensor<double, 1> m_U;
        xt::xtensor<double, 1> m_L;
        xt::xtensor<double, 1> m_R;
    };

    template<class Derived, class problem_t>
    OptimUzawaBase<Derived, problem_t>::OptimUzawaBase(std::size_t nparts, double dt, const OptimParams<Derived>& optim_params)
    : base_type(nparts, dt, 2*3*nparts, 0, optim_params)
    , m_dmin(0.)
    , m_U(xt::zeros<double>({6*nparts}))
    {}

    template<class Derived, class problem_t>
    template <std::size_t dim>
    int OptimUzawaBase<Derived, problem_t>::solve_optimization_problem_impl(scopi_container<dim>& particles,
                                                                            const std::vector<neighbor<dim>>& contacts,
                                                                            problem_t& problem)
    {
        tic();
        init_uzawa(particles, contacts, problem);
        auto duration = toc();
        m_L = xt::zeros<double>({problem.number_row_matrix(contacts, particles)});
        m_R = xt::zeros<double>({problem.number_row_matrix(contacts, particles)});
        PLOG_INFO << "----> CPUTIME : Uzawa matrix = " << duration;

        double time_assign_u = 0.;
        double time_gemv_transpose_A = 0.;
        double time_gemv_inv_P = 0.;
        double time_assign_r = 0.;
        double time_gemv_A = 0.;
        double time_assign_l = 0.;
        double time_compute_cmax = 0.;
        double time_solve = 0.;

        std::size_t cc = 0;
        double cmax = -1000.0;
        while ( (cmax<=-this->m_params.m_tol) && (cc <= this->m_params.m_max_iter) )
        {
            tic();
            xt::noalias(m_U) = this->m_c;
            auto duration = toc();
            time_assign_u += duration;
            time_solve += duration;

            tic();
            gemv_transpose_A(particles, contacts, problem); // U = A^T * L + U
            duration = toc();
            time_gemv_transpose_A += duration;
            time_solve += duration;

            tic();
            gemv_inv_P(particles, problem);  // U = - P^-1 * U
            duration = toc();
            time_gemv_inv_P += duration;
            time_solve += duration;

            tic();
            xt::noalias(m_R) = problem.m_distances - m_dmin;
            duration = toc();
            time_assign_r += duration;
            time_solve += duration;

            tic();
            gemv_A(particles, contacts, problem); // R = - A * U + R
            duration = toc();
            time_gemv_A += duration;
            time_solve += duration;

            tic();
            xt::noalias(m_L) = xt::maximum( m_L-this->m_params.m_rho*m_R, 0);
            duration = toc();
            time_assign_l += duration;
            time_solve += duration;

            tic();
            cmax = double((xt::amin(m_R))(0));
            duration = toc();
            time_compute_cmax += duration;
            time_solve += duration;
            cc += 1;

            // std::cout << m_U << std::endl;

            PLOG_VERBOSE << std::setw(24) << std::scientific << "-- C++ -- Projection : minimal constraint : " << cc << '\t' << cmax;
        }

        PLOG_ERROR_IF(cc >= this->m_params.m_max_iter) << "Uzawa does not converge";

        PLOG_INFO << "----> CPUTIME : solve (total) = " << time_solve;
        PLOG_INFO << "----> CPUTIME : solve (U = c) = " << time_assign_u;
        PLOG_INFO << "----> CPUTIME : solve (U = A^T*L+U) = " << time_gemv_transpose_A;
        PLOG_INFO << "----> CPUTIME : solve (U = -P^-1*U) = " << time_gemv_inv_P;
        PLOG_INFO << "----> CPUTIME : solve (R = d) = " << time_assign_r;
        PLOG_INFO << "----> CPUTIME : solve (R = -A*U+R) = " << time_gemv_A;
        PLOG_INFO << "----> CPUTIME : solve (L = max(L-rho*R, 0)) = " << time_assign_l;
        PLOG_INFO << "----> CPUTIME : solve (cmax = min(R)) = " << time_compute_cmax;

        finalize_uzawa();

        return cc;
    }

    template<class Derived, class problem_t>
    auto OptimUzawaBase<Derived, problem_t>::uadapt_data()
    {
        return m_U.data();
    }

    template<class Derived, class problem_t>
    auto OptimUzawaBase<Derived, problem_t>::wadapt_data()
    {
        return m_U.data() + 3*this->m_nparts;
    }

    template<class Derived, class problem_t>
    auto OptimUzawaBase<Derived, problem_t>::lagrange_multiplier_data()
    {
        return m_L.data();
    }

    template<class Derived, class problem_t>
    int OptimUzawaBase<Derived, problem_t>::get_nb_active_contacts_impl() const
    {
        return xt::sum(xt::where(m_L > 0., xt::ones_like(m_L), xt::zeros_like(m_L)))();
    }

    template<class Derived, class problem_t>
    template <std::size_t dim>
    void OptimUzawaBase<Derived, problem_t>::gemv_inv_P(const scopi_container<dim>& particles, problem_t& problem)
    {
        static_cast<Derived&>(*this).gemv_inv_P_impl(particles, problem);
    }

    template<class Derived, class problem_t>
    template <std::size_t dim>
    void OptimUzawaBase<Derived, problem_t>::gemv_A(const scopi_container<dim>& particles,
                                                    const std::vector<neighbor<dim>>& contacts,
                                                    problem_t& problem)
    {
        static_cast<Derived&>(*this).gemv_A_impl(particles, contacts, problem);
    }

    template<class Derived, class problem_t>
    template <std::size_t dim>
    void OptimUzawaBase<Derived, problem_t>::gemv_transpose_A(const scopi_container<dim>& particles,
                                                              const std::vector<neighbor<dim>>& contacts,
                                                              problem_t& problem)
    {
        static_cast<Derived&>(*this).gemv_transpose_A_impl(particles, contacts, problem);
    }

    template<class Derived, class problem_t>
    template <std::size_t dim>
    void OptimUzawaBase<Derived, problem_t>::init_uzawa(scopi_container<dim>& particles,
                                                        const std::vector<neighbor<dim>>& contacts,
                                                        problem_t& problem)
    {
        static_cast<Derived&>(*this).init_uzawa_impl(particles, contacts, problem);
    }

    template<class Derived, class problem_t>
    void OptimUzawaBase<Derived, problem_t>::finalize_uzawa()
    {
        static_cast<Derived&>(*this).finalize_uzawa_impl();
    }

    template<class problem_t>
    OptimParamsUzawaBase<problem_t>::OptimParamsUzawaBase(const OptimParamsUzawaBase& params)
    : m_problem_params(params.m_problem_params)
    , m_tol(params.m_tol)
    , m_max_iter(params.m_max_iter)
    , m_rho(params.m_rho)
    {}

    template<class problem_t>
    OptimParamsUzawaBase<problem_t>::OptimParamsUzawaBase()
    : m_problem_params()
    , m_tol(1e-9)
    , m_max_iter(40000)
    , m_rho(2000.)
    {}

}
