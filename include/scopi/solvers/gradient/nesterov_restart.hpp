#pragma once

#ifdef SCOPI_USE_MKL
#include <mkl_spblas.h>

#include <xtensor/xadapt.hpp>
#include <xtensor/xview.hpp>
#include <xtensor/xnoalias.hpp>
#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"
#include "projection_max.hpp"

namespace scopi{
    /**
     * @brief Accelerated Projected Gradient Descent algorithm with Adaptive Restart.
     *
     * See OptimProjectedGradient for the notations.
     * The algorithm is
     *  - \f$ \indexUzawa = 0 \f$;
     *  - \f$ \l^{\indexUzawa} = 0 \f$;
     *  - \f$ \y^{\indexUzawa} = 0 \f$;
     *  - \f$ \theta^{\indexUzawa} = 1 \f$.
     *  - While (\f$ \convergenceCriterion \f$)
     *      - \f$ \mathbf{dg}^{\indexUzawa} = \A \y^{\indexUzawa} + \e \f$;
     *      - \f$ \l^{\indexUzawa+1} = \max \left (\y^{\indexUzawa} - \rho \mathbf{dg}^{\indexUzawa}, 0 \right) \f$;
     *      - \f$ \theta^{\indexUzawa+1} = \frac{1}{2} \theta^{\indexUzawa} \sqrt{4 + \left( \theta^{\indexUzawa} \right)^2} - \left( \theta^{\indexUzawa} \right)^2 \f$;
     *      - \f$ \beta^{\indexUzawa+1} = \theta^{\indexUzawa} \frac{1 - \theta^{\indexUzawa}}{\left( \theta^{\indexUzawa} \right)^2 + \theta^{\indexUzawa+1}} \f$;
     *      - \f$ \y^{\indexUzawa+1} = \l^{\indexUzawa+1} + \beta^{\indexUzawa+1} \left( \l^{\indexUzawa+1} - \l^{\indexUzawa} \right) \f$;
     *      - If (\f$ \mathbf{dg}^{\indexUzawa} \cdot \left( \l^{\indexUzawa+1} - \l^{\indexUzawa} \right) > 0 \f$ )
     *          - \f$ \y^{\indexUzawa+1} = \l^{\indexUzawa+1} \f$;
     *          - \f$ \theta^{\indexUzawa+1} = 1 \f$;
     *
     *      - \f$ \indexUzawa++ \f$.
     *
     * @tparam projection_t Projection on admissible velocities.
     */
    template<class projection_t = projection_max>
    class nesterov_restart: public projection_t
    {
    protected:
        /**
         * @brief Constructor.
         *
         * @param max_iter [in] Maximal number of iterations.
         * @param rho [in] Step for the gradient descent.
         * @param tol_dg [in] Tolerance for \f$ \mathbf{dg} \f$ criterion.
         * @param tol_l [in] Tolerance for \f$ \l \f$ criterion.
         * @param verbose [in] Whether to compute and print the function cost.
         */
        nesterov_restart(std::size_t max_iter, double rho, double tol_dg, double tol_l, bool verbose);
        /**
         * @brief Gradient descent algorithm.
         *
         * @param A [in] Matrix \f$ \A \f$.
         * @param descr [in] Structure specifying \f$ \A \f$ properties. 
         * @param c [in] Vector \f$ \e \f$.
         * @param l [out] vector \f$ \l \f$.
         *
         * @return Number of iterations the algorithm needed to converge.
         */
        std::size_t projection(const sparse_matrix_t& A, const struct matrix_descr& descr, const xt::xtensor<double, 1>& c, xt::xtensor<double, 1>& l);
    private:
        /**
         * @brief Maximal number of iterations.
         */
        std::size_t m_max_iter;
        /**
         * @brief Step for the gradient descent.
         */
        double m_rho;
        /**
         * @brief Tolerance for \f$ \mathbf{dg} \f$ criterion (unused).
         */
        double m_tol_dg;
        /**
         * @brief Tolerance for \f$ \l \f$ criterion.
         */
        double m_tol_l;
        /**
         * @brief Whether to compute and print the function cost.
         */
        bool m_verbose;

        /**
         * @brief Value indicating whether the operation was successful or not, and why.
         */
        sparse_status_t m_status;
        /**
         * @brief Vector \f$ \mathbf{dg}^{\indexUzawa} \f$.
         */
        xt::xtensor<double, 1> m_dg;
        /**
         * @brief Vector \f$ \A \l^{\indexUzawa+1} + \e \f$.
         */
        xt::xtensor<double, 1> m_uu;
        /**
         * @brief Vector \f$ \y^{\indexUzawa+1} \f$.
         */
        xt::xtensor<double, 1> m_y;
        /**
         * @brief Vector \f$ \l^{\indexUzawa} \f$.
         */
        xt::xtensor<double, 1> m_l_old;
        /**
         * @brief Vector \f$ \l^{\indexUzawa} \f$.
         */
        xt::xtensor<double, 1> m_lambda_prev;
    };

    template<class projection_t>
    nesterov_restart<projection_t>::nesterov_restart(std::size_t max_iter, double rho, double tol_dg, double tol_l, bool verbose)
    : projection_t()
    , m_max_iter(max_iter)
    , m_rho(rho)
    , m_tol_dg(tol_dg)
    , m_tol_l(tol_l)
    , m_verbose(verbose)
    {}

    template<class projection_t>
    std::size_t nesterov_restart<projection_t>::projection(const sparse_matrix_t& A, const struct matrix_descr& descr, const xt::xtensor<double, 1>& c, xt::xtensor<double, 1>& l)
    {
        PLOG_INFO << "Projection: Nesterov with restart";
        std::size_t iter = 0;
        double theta_old = 1.;
        m_y = l;
        m_l_old = l;
        while (iter < m_max_iter)
        {
            xt::noalias(m_lambda_prev) = l;
            // dg = A*y+c
            xt::noalias(m_dg) = c;
            m_status = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, 1., A, descr, m_y.data(), 1., m_dg.data());
            PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_mv for dg = A*y+dg: " << m_status;

            xt::noalias(l) = this->projection_cone(m_y - m_rho * m_dg);
            double theta = 0.5*(theta_old*std::sqrt(4.+theta_old*theta_old) - theta_old*theta_old);
            double beta = theta_old*(1. - theta_old)/(theta_old*theta_old + theta);
            m_y = l + beta*(l - m_l_old);
            // double norm_dg = xt::amax(xt::abs(m_dg))(0);
            // double norm_l = xt::amax(xt::abs(l))(0);
            // double cmax = double((xt::amin(m_dg))(0));
            double diff_lambda = xt::amax(xt::abs(l - m_lambda_prev))(0) / (xt::amax(xt::abs(m_lambda_prev))(0) + 1.);

            if (m_verbose)
            {
                // uu = A*l + c
                xt::noalias(m_uu) = c;
                m_status = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, 1., A, descr, l.data(), 1., m_uu.data());
                PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_mv for uu = A*l+c: " << m_status;
                double constraint = double((xt::amin(m_uu))(0));
                // cout = 1./2.*l^T*A*l
                double cout;
                m_status = mkl_sparse_d_dotmv(SPARSE_OPERATION_NON_TRANSPOSE, 1./2., A, descr, l.data(), 0., m_uu.data(), &cout);
                PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_dotmv for cout = 1/2*l^T*A*l: " << m_status;
                PLOG_VERBOSE << constraint << "  " << cout + xt::linalg::dot(c, l)(0);
            }

            // if (norm_dg < m_tol_dg || norm_l < m_tol_l || cmax > -m_tol_dg)
            if (diff_lambda < m_tol_l)
            {
                return iter+1;
            }

            if (xt::linalg::dot(m_dg, l - m_l_old)(0) > 0.)
            {
                m_y = l;
                theta = 1.;
            }

            m_l_old = l;
            theta_old = theta;
            iter++;
        }
        PLOG_ERROR << "Uzawa does not converge";
        return iter;
    }
}
#endif
