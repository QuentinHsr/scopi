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
    template<class projection_t = projection_max>
    class nesterov_dynrho: public projection_t
    {
    protected:
        nesterov_dynrho(std::size_t max_iter, double rho, double tol_dg, double tol_l, bool verbose);
        std::size_t projection(const sparse_matrix_t& A, const struct matrix_descr& descr, const xt::xtensor<double, 1>& c, xt::xtensor<double, 1>& l);
    private:
        std::size_t m_max_iter;
        double m_rho;
        double m_tol_dg;
        double m_tol_l;
        bool m_verbose;
        double m_rho_init;

        sparse_status_t m_status;
        xt::xtensor<double, 1> m_dg;
        xt::xtensor<double, 1> m_uu;
        xt::xtensor<double, 1> m_y;
        xt::xtensor<double, 1> m_l_old;
        xt::xtensor<double, 1> m_tmp;
        xt::xtensor<double, 1> m_lambda_prev;
    };

    template<class projection_t>
    nesterov_dynrho<projection_t>::nesterov_dynrho(std::size_t max_iter, double rho, double tol_dg, double tol_l, bool verbose)
    : projection_t()
    , m_max_iter(max_iter)
    , m_rho(rho)
    , m_tol_dg(tol_dg)
    , m_tol_l(tol_l)
    , m_verbose(verbose)
    , m_rho_init(rho)
    {}

    template<class projection_t>
    std::size_t nesterov_dynrho<projection_t>::projection(const sparse_matrix_t& A, const struct matrix_descr& descr, const xt::xtensor<double, 1>& c, xt::xtensor<double, 1>& l)
    {
        std::size_t iter = 0;
        double theta_old = 1.;
        m_y = l;
        m_l_old = l;
        m_rho = m_rho_init;
        double lipsch = 1./m_rho;
        m_rho = 1./lipsch;
        m_tmp.resize({l.size()});
        while (iter < m_max_iter)
        {
            xt::noalias(m_lambda_prev) = l;
            // dg = A*y+c
            xt::noalias(m_dg) = c;
            m_status = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, 1., A, descr, m_y.data(), 1., m_dg.data());
            PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_mv for dg = A*y+dg: " << m_status;

            xt::noalias(l) = this->projection_cone(m_y - m_rho * m_dg);

            std::size_t cc = 0;
            // lAl = l^T*A*l
            double lAl;
            m_status = mkl_sparse_d_dotmv(SPARSE_OPERATION_NON_TRANSPOSE, 1., A, descr, l.data(), 0., m_tmp.data(), &lAl);
            PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_dotmv for lAl = l^T*A*l: " << m_status;
            double cl = xt::linalg::dot(c, l)(0);
            // yAy = y^T*A*y
            double yAy;
            m_status = mkl_sparse_d_dotmv(SPARSE_OPERATION_NON_TRANSPOSE, 1., A, descr, m_y.data(), 0., m_tmp.data(), &yAy);
            PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_dotmv for yAy = y^T*A*y: " << m_status;
            double cy = xt::linalg::dot(c, m_y)(0);
            double dgly = xt::linalg::dot(m_dg, l - m_y)(0);
            double lyly = xt::linalg::dot(l - m_y, l - m_y)(0);

            while ((0.5*lAl + cl > 0.5*yAy + cy + dgly + lipsch/2.*lyly) && (cc < 10))
            {
                lipsch *= 2.;
                m_rho = 1./lipsch;
                xt::noalias(l) = this->projection_cone(m_y - m_rho * m_dg);

                cc++;
                // lAl = l^T*A*l
                m_status = mkl_sparse_d_dotmv(SPARSE_OPERATION_NON_TRANSPOSE, 1., A, descr, l.data(), 0., m_tmp.data(), &lAl);
                PLOG_ERROR_IF(m_status != SPARSE_STATUS_SUCCESS) << "Error in mkl_sparse_d_dotmv for lAl = l^TA*l: " << m_status;
                cl = xt::linalg::dot(c, l)(0);
                dgly = xt::linalg::dot(m_dg, l - m_y)(0);
                lyly = xt::linalg::dot(l - m_y, l - m_y)(0);
            }

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

            if (diff_lambda < m_tol_l)
            // if (norm_dg < m_tol_dg || norm_l < m_tol_l || cmax > -m_tol_dg)
            {
                return iter+1;
            }

            m_l_old = l;
            theta_old = theta;
            lipsch *= 0.97; // 0.9 in the paper
            m_rho = 1./lipsch;
            iter++;
        }
        PLOG_ERROR << "Uzawa does not converge";
        return iter;
    }
}
#endif
