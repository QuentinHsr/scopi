#pragma once

#include "OptimBase.hpp"
#include <omp.h>

#include <xtensor/xadapt.hpp>
#include <xtensor/xview.hpp>

namespace scopi{
    template<std::size_t dim>
        class OptimUzawaMatrixFreeOmp : public OptimBase<OptimUzawaMatrixFreeOmp<dim>, dim>
    {
        public:
            using base_type = OptimBase<OptimUzawaMatrixFreeOmp<dim>, dim>;

            OptimUzawaMatrixFreeOmp(scopi::scopi_container<dim>& particles, double dt, std::size_t Nactive, std::size_t active_ptr);
            void createMatrixConstraint_impl(const std::vector<scopi::neighbor<dim>>& contacts);
            void createMatrixMass_impl();
            int solveOptimizationProblem_impl(const std::vector<scopi::neighbor<dim>>& contacts);
            auto getUadapt_impl();
            auto getWadapt_impl();
            void allocateMemory_impl(const std::size_t nc);
            void freeMemory_impl();
            int getNbActiveContacts_impl();

        private:
            void gemv_invP();
            void gemv_A(const std::vector<scopi::neighbor<dim>>& contacts);
            void gemv_transposeA(const std::vector<scopi::neighbor<dim>>& contacts);

            const double _tol;
            const std::size_t _maxiter;
            const double _rho;
            const double _dmin;
            xt::xtensor<double, 1> _U;
            xt::xtensor<double, 1> _L;
            xt::xtensor<double, 1> _R;
            int _nbActiveContacts = 0;
    };

    template<std::size_t dim>
        OptimUzawaMatrixFreeOmp<dim>::OptimUzawaMatrixFreeOmp(scopi::scopi_container<dim>& particles, double dt, std::size_t Nactive, std::size_t active_ptr) : 
            OptimBase<OptimUzawaMatrixFreeOmp<dim>, dim>(particles, dt, Nactive, active_ptr, 2*3*Nactive, 0),
            _tol(1.0e-6), _maxiter(40000), _rho(2000.), _dmin(0.),
            _U(xt::zeros<double>({6*Nactive}))
            {
            }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::createMatrixConstraint_impl(const std::vector<scopi::neighbor<dim>>& contacts)
        {
            std::ignore = contacts;
        }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::createMatrixMass_impl()
        {
        }

    template<std::size_t dim>
        int OptimUzawaMatrixFreeOmp<dim>::solveOptimizationProblem_impl(const std::vector<scopi::neighbor<dim>>& contacts)
        {
            _L = xt::zeros_like(this->_distances);
            _R = xt::zeros_like(this->_distances);

            double timeAssignU = 0.;
            double timeGemvTransposeA = 0.;
            double timeGemvInvP = 0.;
            double timeAssignR = 0.;
            double timeGemvA = 0.;
            double timeAssignL = 0.;
            double timeComputeCmax = 0.;

            std::size_t cc = 0;
            double cmax = -1000.0;
            while ( (cmax<=-_tol)&&(cc <= _maxiter) )
            {
                tic();
                _U = this->_c;
                timeAssignU += toc();

                tic();
                gemv_transposeA(contacts); // U = A^T * L + U
                timeGemvTransposeA += toc();

                tic();
                gemv_invP();  // U = - P^-1 * U
                timeGemvInvP += toc();

                tic();
                _R = this->_distances - _dmin;
                timeAssignR += toc();

                tic();
                gemv_A(contacts); // R = - A * U + R
                timeGemvA += toc();

                tic();
                _L = xt::maximum( _L-_rho*_R, 0);
                timeAssignL += toc();

                tic();
                cmax = double((xt::amin(_R))(0));
                timeComputeCmax += toc();
                cc += 1;
                // std::cout << "-- C++ -- Projection : minimal constraint : " << cmax << std::endl;
            }

            if (cc>=_maxiter)
            {
                std::cout<<"\n-- C++ -- Projection : ********************** WARNING **********************"<<std::endl;
                std::cout<<  "-- C++ -- Projection : *************** Uzawa does not converge ***************"<<std::endl;
                std::cout<<  "-- C++ -- Projection : ********************** WARNING **********************\n"<<std::endl;
            }

            std::cout << "----> CPUTIME : solve (U = c) = " << timeAssignU << std::endl;
            std::cout << "----> CPUTIME : solve (U = A^T*L+U) = " << timeGemvTransposeA << std::endl;
            std::cout << "----> CPUTIME : solve (U = -P^-1*U) = " << timeGemvInvP << std::endl;
            std::cout << "----> CPUTIME : solve (R = d) = " << timeAssignR << std::endl;
            std::cout << "----> CPUTIME : solve (R = -A*U+R) = " << timeGemvA << std::endl;
            std::cout << "----> CPUTIME : solve (L = max(L-rho*R, 0)) = " << timeAssignL << std::endl;
            std::cout << "----> CPUTIME : solve (cmax = min(R)) = " << timeComputeCmax << std::endl;

            return cc;
        }

    template<std::size_t dim>
        auto OptimUzawaMatrixFreeOmp<dim>::getUadapt_impl()
        {
            return xt::adapt(reinterpret_cast<double*>(_U.data()), {this->_Nactive, 3UL});
        }

    template<std::size_t dim>
        auto OptimUzawaMatrixFreeOmp<dim>::getWadapt_impl()
        {
            return xt::adapt(reinterpret_cast<double*>(_U.data()+3*this->_Nactive), {this->_Nactive, 3UL});
        }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::allocateMemory_impl(const std::size_t nc)
        {
            std::ignore = nc;
        }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::freeMemory_impl()
        {
        }

    template<std::size_t dim>
        int OptimUzawaMatrixFreeOmp<dim>::getNbActiveContacts_impl()
        {
            return xt::sum(xt::where(_L > 0., xt::ones_like(_L), xt::zeros_like(_L)))();
        }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::gemv_invP()
        {
            // for loops instead of xtensor functions to control exactly the parallelism
#pragma omp parallel for
            for(std::size_t i = 0; i < this->_Nactive; ++i)
            {
                for (std::size_t d=0; d<3; ++d)
                {
                    _U(3*i + d) /= (-1. * this->_mass); // TODO: add mass into particles
                    _U(3*this->_Nactive + 3*i + d) /= (-1. * this->_moment);
                }
            }
        }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::gemv_A(const std::vector<scopi::neighbor<dim>>& contacts)
        {
#pragma omp parallel for
            for(std::size_t ic = 0; ic < contacts.size(); ++ic)
            {
                auto &c = contacts[ic];
                for (std::size_t d=0; d<3; ++d)
                {
                    if (c.i >= this->_active_ptr)
                    {
                        _R(ic) -= (-this->_dt*c.nij[d]) * _U((c.i - this->_active_ptr)*3 + d);
                    }
                    if (c.j >= this->_active_ptr)
                    {
                        _R(ic) -= (this->_dt*c.nij[d]) * _U((c.j - this->_active_ptr)*3 + d);
                    }
                }

                auto r_i = c.pi - this->_particles.pos()(c.i);
                auto r_j = c.pj - this->_particles.pos()(c.j);

                xt::xtensor_fixed<double, xt::xshape<3, 3>> ri_cross, rj_cross;

                if (dim == 2)
                {
                    ri_cross = {{      0,      0, r_i(1)},
                        {      0,      0, -r_i(0)},
                        {-r_i(1), r_i(0),       0}};

                    rj_cross = {{      0,      0,  r_j(1)},
                        {      0,      0, -r_j(0)},
                        {-r_j(1), r_j(0),       0}};
                }
                else
                {
                    ri_cross = {{      0, -r_i(2),  r_i(1)},
                        { r_i(2),       0, -r_i(0)},
                        {-r_i(1),  r_i(0),       0}};

                    rj_cross = {{      0, -r_j(2),  r_j(1)},
                        { r_j(2),       0, -r_j(0)},
                        {-r_j(1),  r_j(0),       0}};
                }

                auto Ri = scopi::rotation_matrix<3>(this->_particles.q()(c.i));
                auto Rj = scopi::rotation_matrix<3>(this->_particles.q()(c.j));

                if (c.i >= this->_active_ptr)
                {
                    std::size_t ind_part = c.i - this->_active_ptr;
                    auto dot = xt::eval(xt::linalg::dot(ri_cross, Ri));
                    for (std::size_t ip=0; ip<3; ++ip)
                    {
                        _R(ic) -= (this->_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip))) * _U(3*this->_Nactive + 3*ind_part + ip);
                    }
                }

                if (c.j >= this->_active_ptr)
                {
                    std::size_t ind_part = c.j - this->_active_ptr;
                    auto dot = xt::eval(xt::linalg::dot(rj_cross, Rj));
                    for (std::size_t ip=0; ip<3; ++ip)
                    {
                        _R(ic) -= (-this->_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip))) * _U(3*this->_Nactive + 3*ind_part + ip);
                    }
                }

            }
        }

    template<std::size_t dim>
        void OptimUzawaMatrixFreeOmp<dim>::gemv_transposeA(const std::vector<scopi::neighbor<dim>>& contacts)
        {
#pragma omp parallel for
            for(std::size_t ic = 0; ic < contacts.size(); ++ic)
            {
                auto &c = contacts[ic];

                for (std::size_t d=0; d<3; ++d)
                {
                    if (c.i >= this->_active_ptr)
                    {
#pragma omp atomic
                         _U((c.i - this->_active_ptr)*3 + d) += _L(ic) * (-this->_dt*c.nij[d]);
                    }
                    if (c.j >= this->_active_ptr)
                    {
#pragma omp atomic
                        _U((c.j - this->_active_ptr)*3 + d) += _L(ic) * (this->_dt*c.nij[d]);
                    }
                }

                auto r_i = c.pi - this->_particles.pos()(c.i);
                auto r_j = c.pj - this->_particles.pos()(c.j);

                xt::xtensor_fixed<double, xt::xshape<3, 3>> ri_cross, rj_cross;

                if (dim == 2)
                {
                    ri_cross = {{      0,      0, r_i(1)},
                        {      0,      0, -r_i(0)},
                        {-r_i(1), r_i(0),       0}};

                    rj_cross = {{      0,      0,  r_j(1)},
                        {      0,      0, -r_j(0)},
                        {-r_j(1), r_j(0),       0}};
                }
                else
                {
                    ri_cross = {{      0, -r_i(2),  r_i(1)},
                        { r_i(2),       0, -r_i(0)},
                        {-r_i(1),  r_i(0),       0}};

                    rj_cross = {{      0, -r_j(2),  r_j(1)},
                        { r_j(2),       0, -r_j(0)},
                        {-r_j(1),  r_j(0),       0}};
                }

                auto Ri = scopi::rotation_matrix<3>(this->_particles.q()(c.i));
                auto Rj = scopi::rotation_matrix<3>(this->_particles.q()(c.j));

                if (c.i >= this->_active_ptr)
                {
                    std::size_t ind_part = c.i - this->_active_ptr;
                    auto dot = xt::eval(xt::linalg::dot(ri_cross, Ri));
                    for (std::size_t ip=0; ip<3; ++ip)
                    {
#pragma omp atomic
                        _U(3*this->_Nactive + 3*ind_part + ip) += _L(ic) * (this->_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip)));
                    }
                }

                if (c.j >= this->_active_ptr)
                {
                    std::size_t ind_part = c.j - this->_active_ptr;
                    auto dot = xt::eval(xt::linalg::dot(rj_cross, Rj));
                    for (std::size_t ip=0; ip<3; ++ip)
                    {
#pragma omp atomic
                        _U(3*this->_Nactive + 3*ind_part + ip) += _L(ic) * (-this->_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip)));
                    }
                }
            }
        }
}
