#pragma once

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>

#include <xtensor/xtensor.hpp>
#include <xtensor/xfixed.hpp>

#include <fmt/format.h>
#include <fusion.h>
#include <nlohmann/json.hpp>

#include "../container.hpp"
#include "../objects/methods/closest_points.hpp"
#include "../objects/methods/write_objects.hpp"
#include "../objects/neighbor.hpp"
#include "../quaternion.hpp"

#include <scopi/contact/contact_kdtree.hpp>
// #include <scopi/contact/contact_brute_force.hpp>

#include <nanoflann.hpp>

using namespace mosek::fusion;
using namespace monty;
namespace nl = nlohmann;

using namespace xt::placeholders;

namespace scopi
{

    template<std::size_t dim>
    void mosek_solver(scopi::scopi_container<dim>& particles, double dt, std::size_t total_it, std::size_t active_ptr)
    {
        std::size_t Nactive = particles.size() - active_ptr;
        // Time Loop
        for (std::size_t nite=0; nite<total_it; ++nite)
        {
        std::cout << "\n\n------------------- Time iteration ----------------> " << nite << std::endl;

            //displacement of obstacles
            for (std::size_t i=0; i<active_ptr; ++i)
            {
                xt::xtensor_fixed<double, xt::xshape<3>> w({0, 0, particles.desired_omega()(i)});
                double normw = xt::linalg::norm(w);
                if (normw == 0)
                {
                    normw = 1;
                }
                scopi::type::quaternion expw;
                expw(0) = std::cos(0.5*normw*dt);
                xt::view(expw, xt::range(1, _)) = std::sin(0.5*normw*dt)/normw*w;

                for (std::size_t d=0; d<dim; ++d)
                {
                    particles.pos()(i)(d) += dt*particles.vd()(i)(d);
                }
                particles.q()(i) = scopi::mult_quaternion(particles.q()(i), expw);

                std::cout << "obstacle " << i << ": " << particles.pos()(0) << " " << particles.q()(0) << std::endl;
            }

            // create list of contacts
            std::cout << "----> create list of contacts " << nite << std::endl;
            // // scopi::contact_brute_force cont(2);
            scopi::contact_kdtree cont(2, 10);
            auto contacts = cont.run(particles, active_ptr);
            std::cout << "----> MOSEK : contacts.size() = " << contacts.size() << std::endl;

            // output files
            std::cout << "----> json output files " << nite << std::endl;

            nl::json json_output;

            std::ofstream file(fmt::format("./Results/scopi_objects_{:04d}.json", nite));

            json_output["objects"] = {};

            for(std::size_t i = 0; i < particles.size(); ++i)
            {
                json_output["objects"].push_back(scopi::write_objects_dispatcher<dim>::dispatch(*particles[i]));
            }

            json_output["contacts"] = {};

            for(std::size_t i=0; i<contacts.size(); ++i)
            {
                nl::json contact;

                contact["pi"] = contacts[i].pi;
                contact["pj"] = contacts[i].pj;
                contact["nij"] = contacts[i].nij;

                json_output["contacts"].push_back(contact);

            }

            file << std::setw(4) << json_output;
            file.close();

            // for (std::size_t i=0; i<Nactive; ++i)
            // {
            //     for (std::size_t d=0; d<dim; ++d)
            //     {
            //         particles.pos()(i + active_ptr)(d) += dt*particles.vd()(i + active_ptr)(d);
            //     }
            // }


            // create mass and inertia matrices
            std::cout << "----> create mass and inertia matrices " << nite << std::endl;
            tic();
            double mass = 1.;
            double moment = .1;

            xt::xtensor<double, 1> c = xt::zeros<double>({1 + 2*3*Nactive + 2*3*Nactive});
            c(0) = 1;
            std::size_t Mdec = 1;
            std::size_t Jdec = Mdec + 3*Nactive;
            for (std::size_t i=0; i<Nactive; ++i)
            {
                for (std::size_t d=0; d<dim; ++d)
                {
                    c(Mdec + 3*i + d) = -mass*particles.vd()(active_ptr + i)[d]; // TODO: add mass into particles
                }
                c(Jdec + 3*i + 2) = -moment*particles.desired_omega()(active_ptr + i);
            }

            // fill vector with distances
            xt::xtensor<double, 1> distances = xt::zeros<double>({contacts.size()});
            for(std::size_t i=0; i<contacts.size(); ++i)
            {
                distances[i] = contacts[i].dij;
            }
            // std::cout << "distances " << distances << std::endl;

            // Preallocate
            std::vector<int> A_rows;
            std::vector<int> A_cols;
            std::vector<double> A_values;

            std::size_t u_size = 3*contacts.size()*2;
            std::size_t w_size = 3*contacts.size()*2;
            A_rows.reserve(u_size + w_size);
            A_cols.reserve(u_size + w_size);
            A_values.reserve(u_size + w_size);

            std::size_t ic = 0;
            for (auto &c: contacts)
            {

                for (std::size_t d=0; d<3; ++d)
                {
                    if (c.i >= active_ptr)
                    {
                        A_rows.push_back(ic);
                        A_cols.push_back(1 + (c.i - active_ptr)*3 + d);
                        A_values.push_back(-dt*c.nij[d]);
                    }
                    if (c.j >= active_ptr)
                    {
                        A_rows.push_back(ic);
                        A_cols.push_back(1 + (c.j - active_ptr)*3 + d);
                        A_values.push_back(dt*c.nij[d]);
                    }
                }

                auto r_i = c.pi - particles.pos()(c.i);
                auto r_j = c.pj - particles.pos()(c.j);

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

                auto Ri = scopi::rotation_matrix<3>(particles.q()(c.i));
                auto Rj = scopi::rotation_matrix<3>(particles.q()(c.j));

                if (c.i >= active_ptr)
                {
                    std::size_t ind_part = c.i - active_ptr;
                    auto dot = xt::eval(xt::linalg::dot(ri_cross, Ri));
                    for (std::size_t ip=0; ip<3; ++ip)
                    {
                        A_rows.push_back(ic);
                        A_cols.push_back(1 + 3*Nactive + 3*ind_part + ip);
                        A_values.push_back(dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip)));
                    }
                }

                if (c.j >= active_ptr)
                {
                    std::size_t ind_part = c.j - active_ptr;
                    auto dot = xt::eval(xt::linalg::dot(rj_cross, Rj));
                    for (std::size_t ip=0; ip<3; ++ip)
                    {
                        A_rows.push_back(ic);
                        A_cols.push_back(1 + 3*Nactive + 3*ind_part + ip);
                        A_values.push_back(-dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip)));
                    }
                }

                ++ic;
            }

            auto A = Matrix::sparse(contacts.size(), 1 + 6*Nactive + 6*Nactive,
                                    std::make_shared<ndarray<int, 1>>(A_rows.data(), shape_t<1>({A_rows.size()})),
                                    std::make_shared<ndarray<int, 1>>(A_cols.data(), shape_t<1>({A_cols.size()})),
                                    std::make_shared<ndarray<double, 1>>(A_values.data(), shape_t<1>({A_values.size()})));

            std::vector<int> Az_rows;
            std::vector<int> Az_cols;
            std::vector<double> Az_values;

            Az_rows.reserve(6*Nactive*2);
            Az_cols.reserve(6*Nactive*2);
            Az_values.reserve(6*Nactive*2);

            for (std::size_t i=0; i<Nactive; ++i)
            {
                for (std::size_t d=0; d<2; ++d)
                {
                    Az_rows.push_back(3*i + d);
                    Az_cols.push_back(1 + 3*i + d);
                    Az_values.push_back(std::sqrt(mass)); // TODO: add mass into particles
                // }
                // for (std::size_t d=0; d<3; ++d)
                // {
                    Az_rows.push_back(3*i + d);
                    Az_cols.push_back(1 + 6*Nactive + 3*i + d);
                    Az_values.push_back(-1.);
                }

                // for (std::size_t d=0; d<3; ++d)
                // {
                //     Az_rows.push_back(3*Nactive + 3*i + d);
                //     Az_cols.push_back(1 + 3*Nactive + 3*i + d);
                //     Az_values.push_back(std::sqrt(moment));
                // }
                Az_rows.push_back(3*Nactive + 3*i + 2);
                Az_cols.push_back(1 + 3*Nactive + 3*i + 2);
                Az_values.push_back(std::sqrt(moment));

                // for (std::size_t d=0; d<3; ++d)
                // {
                //     Az_rows.push_back(3*Nactive + 3*i + d);
                //     Az_cols.push_back( 1 + 6*Nactive + 3*Nactive + 3*i + d);
                //     Az_values.push_back(-1);
                // }
                Az_rows.push_back(3*Nactive + 3*i + 2);
                Az_cols.push_back( 1 + 6*Nactive + 3*Nactive + 3*i + 2);
                Az_values.push_back(-1);
            }

            auto Az = Matrix::sparse(6*Nactive, 1 + 6*Nactive + 6*Nactive,
                                    std::make_shared<ndarray<int, 1>>(Az_rows.data(), shape_t<1>({Az_rows.size()})),
                                    std::make_shared<ndarray<int, 1>>(Az_cols.data(), shape_t<1>({Az_cols.size()})),
                                    std::make_shared<ndarray<double, 1>>(Az_values.data(), shape_t<1>({Az_values.size()})));

            auto duration4 = toc();
            std::cout << "----> CPUTIME : matrices = " << duration4 << std::endl;

            // Create Mosek optimization problem
            std::cout << "----> Create Mosek optimization problem " << nite << std::endl;
            tic();
            Model::t model = new Model("contact"); auto _M = finally([&]() { model->dispose(); });
            // variables
            Variable::t X = model->variable("X", 1 + 6*Nactive + 6*Nactive);

            // functional to minimize
            auto c_mosek = std::make_shared<ndarray<double, 1>>(c.data(), shape_t<1>({c.shape(0)}));
            model->objective("minvar", ObjectiveSense::Minimize, Expr::dot(c_mosek, X));

            // constraints
            auto D_mosek = std::make_shared<ndarray<double, 1>>(distances.data(), shape_t<1>({distances.shape(0)}));

            Constraint::t qc1 = model->constraint("qc1", Expr::mul(A, X), Domain::lessThan(D_mosek));
            Constraint::t qc2 = model->constraint("qc2", Expr::mul(Az, X), Domain::equalsTo(0.));
            Constraint::t qc3 = model->constraint("qc3", Expr::vstack(1, X->index(0), X->slice(1 + 6*Nactive, 1 + 6*Nactive + 6*Nactive)), Domain::inRotatedQCone());
            // model->setSolverParam("intpntCoTolPfeas", 1e-10);
            // model->setSolverParam("intpntTolPfeas", 1.e-10);

            // model->setSolverParam("intpntCoTolDfeas", 1e-6);
            // model->setLogHandler([](const std::string & msg) { std::cout << msg << std::flush; } );
            //solve
            model->solve();

            auto duration5 = toc();
            std::cout << "----> CPUTIME : mosek = " << duration5 << std::endl;
            std::cout << "Mosek iterations : " << model->getSolverIntInfo("intpntIter") << std::endl;

            // move the active particles
            ndarray<double, 1> Xlvl   = *(X->level());

            auto uadapt = xt::adapt(reinterpret_cast<double*>(Xlvl.raw()+1), {Nactive, 3UL});
            auto wadapt = xt::adapt(reinterpret_cast<double*>(Xlvl.raw()+1+3*Nactive), {Nactive, 3UL});

            for (std::size_t i=0; i<Nactive; ++i)
            {
                xt::xtensor_fixed<double, xt::xshape<3>> w({0, 0, wadapt(i, 2)});
                double normw = xt::linalg::norm(w);
                if (normw == 0)
                {
                    normw = 1;
                }
                scopi::type::quaternion expw;
                expw(0) = std::cos(0.5*normw*dt);
                xt::view(expw, xt::range(1, _)) = std::sin(0.5*normw*dt)/normw*w;
                for (std::size_t d=0; d<dim; ++d)
                {
                    particles.pos()(i + active_ptr)(d) += dt*uadapt(i, d);
                }
                // xt::view(particles.pos(), i) += dt*xt::view(uadapt, i);

                // particles.q()(i) = scopi::quaternion(theta(i));
                // std::cout << expw << " " << particles.q()(i) << std::endl;
                particles.q()(i + active_ptr) = scopi::mult_quaternion(particles.q()(i + active_ptr), expw);
                normalize(particles.q()(i + active_ptr));
                // std::cout << "position" << particles.pos()(i) << std::endl << std::endl;
                // std::cout << "quaternion " << particles.q()(i) << std::endl << std::endl;

            }
        }
    }
}
