#pragma once

#include <cmath>
#include <cstddef>
#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"
#include <vector>
#include <xtensor/xtensor.hpp>
#include <xtensor/xfixed.hpp>

#include "../types.hpp"
#include "../container.hpp"
#include "../quaternion.hpp"
#include "../objects/neighbor.hpp"
#include "../utils.hpp"
#include "../params/ProblemParams.hpp"
#include "ProblemBase.hpp"

namespace scopi
{
    class DryWithFrictionFixedPoint;

    template<>
    struct ProblemParams<DryWithFrictionFixedPoint>
    {
        ProblemParams();
        ProblemParams(const ProblemParams<DryWithFrictionFixedPoint>& params);

        double mu;
        double tol_fixed_point;
        std::size_t max_iter_fixed_point;
    };

    class DryWithFrictionFixedPoint : protected ProblemBase
    {
    protected:
        DryWithFrictionFixedPoint(std::size_t nparticles, double dt, const ProblemParams<DryWithFrictionFixedPoint>& problem_params);

        template <std::size_t dim>
        void create_matrix_constraint_coo(const scopi_container<dim>& particles,
                                          const std::vector<neighbor<dim>>& contacts,
                                          const std::vector<neighbor<dim>>& contacts_worms,
                                          std::size_t firstCol);
        template <std::size_t dim>
        std::size_t number_row_matrix(const std::vector<neighbor<dim>>& contact,
                                      const std::vector<neighbor<dim>>& contacts_worms);
        template<std::size_t dim>
        void create_vector_distances(const std::vector<neighbor<dim>>& contacts, const std::vector<neighbor<dim>>& contacts_worms);

        template<std::size_t dim>
        void extra_setps_before_solve(const std::vector<neighbor<dim>>& contacts);
        template<std::size_t dim>
        void extra_setps_after_solve(const std::vector<neighbor<dim>>& contacts,
                                     const xt::xtensor<double, 1>& lambda,
                                     const xt::xtensor<double, 2>& u_tilde);
        bool should_solve_optimization_problem();

    private:
        ProblemParams<DryWithFrictionFixedPoint> m_params;
        xt::xtensor<double, 1> m_s;
        xt::xtensor<double, 1> m_s_old;
        std::size_t m_nb_iter;
    };

    template<std::size_t dim>
    void DryWithFrictionFixedPoint::create_matrix_constraint_coo(const scopi_container<dim>& particles,
                                                                 const std::vector<neighbor<dim>>& contacts,
                                                                 const std::vector<neighbor<dim>>& contacts_worms,
                                                                 std::size_t firstCol)
    {
        std::size_t index = matrix_positive_distance(particles, contacts, firstCol, number_row_matrix(contacts, contacts_worms), 4);
        std::size_t active_offset = particles.nb_inactive();
        std::size_t ic = 0;
        for (auto &c: contacts)
        {
            if (c.i >= active_offset)
            {
                for (std::size_t ind_row = 0; ind_row < 3; ++ind_row)
                {
                    for (std::size_t ind_col = 0; ind_col < 3; ++ind_col)
                    {
                        this->m_A_rows[index] = 4*ic + 1 + ind_row;
                        this->m_A_cols[index] = firstCol + (c.i - active_offset)*3 + ind_col;
                        this->m_A_values[index] = -this->m_dt*m_params.mu*c.nij[ind_row]*c.nij[ind_col];
                        if(ind_row == ind_col)
                        {
                            this->m_A_values[index] += this->m_dt*m_params.mu;
                        }
                        index++;
                    }
                }
            }

            if (c.j >= active_offset)
            {
                for (std::size_t ind_row = 0; ind_row < 3; ++ind_row)
                {
                    for (std::size_t ind_col = 0; ind_col < 3; ++ind_col)
                    {
                        this->m_A_rows[index] = 4*ic + 1 + ind_row;
                        this->m_A_cols[index] = firstCol + (c.j - active_offset)*3 + ind_col;
                        this->m_A_values[index] = this->m_dt*m_params.mu*c.nij[ind_row]*c.nij[ind_col];
                        if(ind_row == ind_col)
                        {
                            this->m_A_values[index] -= this->m_dt*m_params.mu;
                        }
                        index++;
                    }
                }
            }

            auto ri_cross = cross_product<dim>(c.pi - particles.pos()(c.i));
            auto rj_cross = cross_product<dim>(c.pj - particles.pos()(c.j));
            auto Ri = rotation_matrix<3>(particles.q()(c.i));
            auto Rj = rotation_matrix<3>(particles.q()(c.j));

            if (c.i >= active_offset)
            {
                std::size_t ind_part = c.i - active_offset;
                auto dot = xt::eval(xt::linalg::dot(ri_cross, Ri));
                for (std::size_t ind_row = 0; ind_row < 3; ++ind_row)
                {
                    for (std::size_t ind_col = 0; ind_col < 3; ++ind_col)
                    {
                        this->m_A_rows[index] = 4*ic + 1 + ind_row;
                        this->m_A_cols[index] = firstCol + 3*particles.nb_active() + 3*ind_part + ind_col;
                        this->m_A_values[index] = -m_params.mu*this->m_dt*dot(ind_row, ind_col) + m_params.mu*this->m_dt*(c.nij[0]*dot(0, ind_col)+c.nij[1]*dot(1, ind_col)+c.nij[2]*dot(2, ind_col));
                        index++;
                    }
                }
            }

            if (c.j >= active_offset)
            {
                std::size_t ind_part = c.j - active_offset;
                auto dot = xt::eval(xt::linalg::dot(rj_cross, Rj));
                for (std::size_t ind_row = 0; ind_row < 3; ++ind_row)
                {
                    for (std::size_t ind_col = 0; ind_col < 3; ++ind_col)
                    {
                        this->m_A_rows[index] = 4*ic + 1 + ind_row;
                        this->m_A_cols[index] = firstCol + 3*particles.nb_active() + 3*ind_part + ind_col;
                        this->m_A_values[index] = m_params.mu*this->m_dt*dot(ind_row, ind_col) - m_params.mu*this->m_dt*(c.nij[0]*dot(0, ind_col)+c.nij[1]*dot(1, ind_col)+c.nij[2]*dot(2, ind_col));
                        index++;
                    }
                }
            }

            ++ic;
        }
    }

    template <std::size_t dim>
    std::size_t DryWithFrictionFixedPoint::number_row_matrix(const std::vector<neighbor<dim>>& contacts,
                                                             const std::vector<neighbor<dim>>&)
    {
        return 4*contacts.size();
    }

    template<std::size_t dim>
    void DryWithFrictionFixedPoint::create_vector_distances(const std::vector<neighbor<dim>>& contacts, const std::vector<neighbor<dim>>&)
    {
        this->m_distances = xt::zeros<double>({4*contacts.size()});
        for (std::size_t i = 0; i < contacts.size(); ++i)
        {
            this->m_distances[4*i] = contacts[i].dij + m_params.mu*this->m_dt*m_s(i);
        }
    }

    template<std::size_t dim>
    void DryWithFrictionFixedPoint::extra_setps_before_solve(const std::vector<neighbor<dim>>& contacts)
    {
        m_nb_iter = 0;
        m_s = xt::ones<double>({contacts.size()});
        m_s_old = 2.*xt::ones<double>({contacts.size()});
    }

    template<std::size_t dim>
    void DryWithFrictionFixedPoint::extra_setps_after_solve(const std::vector<neighbor<dim>>& contacts,
                                                            const xt::xtensor<double, 1>&,
                                                            const xt::xtensor<double, 2>& u_tilde)
    {
        m_nb_iter++;
        m_s_old = m_s;
        // TODO use xtensor functions to avoid loop
        for (std::size_t i = 0; i < contacts.size(); ++i)
        {
            m_s(i) = xt::linalg::norm(xt::view(u_tilde, i, xt::range(1, _)));
        }
    }
  
}

