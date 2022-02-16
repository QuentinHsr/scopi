#pragma once

#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"
#include <xtensor/xtensor.hpp>

#ifdef SCOPI_USE_MOSEK
#include <fusion.h>
#endif

#include "../container.hpp"
#include "../quaternion.hpp"
#include "../objects/neighbor.hpp"
#include "../utils.hpp"

namespace scopi
{
    class MatrixOptimSolver
    {
    protected:
        MatrixOptimSolver(std::size_t nparts, double dt, double mu = 0.);

        template <std::size_t dim>
        void create_matrix_constraint_coo(const scopi_container<dim>& particles,
                                          const std::vector<neighbor<dim>>& contacts,
                                          std::size_t firstCol);

#ifdef SCOPI_USE_MOSEK
        std::shared_ptr<monty::ndarray<double, 1>> distances_to_mosek_vector(xt::xtensor<double, 1> distances) const;
        template <std::size_t dim>
        std::size_t number_row_matrix_mosek(const std::vector<neighbor<dim>>& contacts) const;
        std::size_t number_col_matrix_mosek() const;
        std::size_t matrix_first_col_index_mosek() const;
        template <std::size_t dim>
        mosek::fusion::Constraint::t constraint_mosek(std::shared_ptr<monty::ndarray<double, 1>> D,
                                       mosek::fusion::Matrix::t A,
                                       mosek::fusion::Variable::t X,
                                       mosek::fusion::Model::t model,
                                       const std::vector<neighbor<dim>>& contacts) const;
#endif

        std::size_t m_nparticles;
        double m_dt;

        std::vector<int> m_A_rows;
        std::vector<int> m_A_cols;
        std::vector<double> m_A_values;
    };

    template<std::size_t dim>
    void MatrixOptimSolver::create_matrix_constraint_coo(const scopi_container<dim>& particles,
                                                              const std::vector<neighbor<dim>>& contacts,
                                                              std::size_t firstCol)
    {
        std::size_t active_offset = particles.nb_inactive();
        std::size_t u_size = 3*contacts.size()*2;
        std::size_t w_size = 3*contacts.size()*2;
        m_A_rows.resize(u_size + w_size);
        m_A_cols.resize(u_size + w_size);
        m_A_values.resize(u_size + w_size);

        std::size_t ic = 0;
        std::size_t index = 0;
        for (auto &c: contacts)
        {
            if (c.i >= active_offset)
            {
                for (std::size_t d = 0; d < 3; ++d)
                {
                    m_A_rows[index] = ic;
                    m_A_cols[index] = firstCol + (c.i - active_offset)*3 + d;
                    m_A_values[index] = -m_dt*c.nij[d];
                    index++;
                }
            }

            if (c.j >= active_offset)
            {
                for (std::size_t d = 0; d < 3; ++d)
                {
                    m_A_rows[index] = ic;
                    m_A_cols[index] = firstCol + (c.j - active_offset)*3 + d;
                    m_A_values[index] = m_dt*c.nij[d];
                    index++;
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
                for (std::size_t ip = 0; ip < 3; ++ip)
                {
                    m_A_rows[index] = ic;
                    m_A_cols[index] = firstCol + 3*particles.nb_active() + 3*ind_part + ip;
                    m_A_values[index] = m_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip));
                    index++;
                }
            }

            if (c.j >= active_offset)
            {
                std::size_t ind_part = c.j - active_offset;
                auto dot = xt::eval(xt::linalg::dot(rj_cross, Rj));
                for (std::size_t ip = 0; ip < 3; ++ip)
                {
                    m_A_rows[index] = ic;
                    m_A_cols[index] = firstCol + 3*particles.nb_active() + 3*ind_part + ip;
                    m_A_values[index] = -m_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip));
                    index++;
                }
            }

            ++ic;
        }
        m_A_rows.resize(index);
        m_A_cols.resize(index);
        m_A_values.resize(index);
    }

#ifdef SCOPI_USE_MOSEK
    template <std::size_t dim>
    std::size_t MatrixOptimSolver::number_row_matrix_mosek(const std::vector<neighbor<dim>>& contacts) const
    {
        return contacts.size();
    }

    template <std::size_t dim>
    mosek::fusion::Constraint::t MatrixOptimSolver::constraint_mosek(std::shared_ptr<monty::ndarray<double, 1>> D,
                                   mosek::fusion::Matrix::t A,
                                   mosek::fusion::Variable::t X,
                                   mosek::fusion::Model::t model,
                                   const std::vector<neighbor<dim>>&) const
    {
        using namespace mosek::fusion;
        return model->constraint("qc1", Expr::mul(A, X), Domain::lessThan(D));
    }
#endif

}

