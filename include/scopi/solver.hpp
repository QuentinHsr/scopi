#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <fstream>
#include <vector>

#include <xtensor/xtensor.hpp>
#include <xtensor/xfixed.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"

#include "container.hpp"
#include "objects/methods/closest_points.hpp"
#include "objects/methods/write_objects.hpp"
#include "objects/neighbor.hpp"
#include "quaternion.hpp"

#include "solvers/OptimUzawaMatrixFreeOmp.hpp"
#include "solvers/OptimMosek.hpp"
#include "problems/DryWithoutFriction.hpp"
#include "contact/contact_kdtree.hpp"
#include "vap/vap_fixed.hpp"
#include "vap/vap_projection.hpp"
#include "params.hpp"

namespace nl = nlohmann;

using namespace xt::placeholders;

namespace scopi
{
    template<std::size_t dim>
    void update_velocity_omega(scopi_container<dim>& particles, std::size_t i, const xt::xtensor<double, 2>& wadapt);

    template<std::size_t dim,
             class optim_solver_t = OptimUzawaMatrixFreeOmp<DryWithoutFriction>,
             class contact_t = contact_kdtree,
             class vap_t = vap_fixed
             >
    class ScopiSolver : public optim_solver_t
                      , public vap_t
                      , public contact_t
    {
    private:
        using problem_t = typename optim_solver_t::problem_type;
    public:
        using params_t = Params<optim_solver_t, problem_t, contact_t, vap_t>;
        ScopiSolver(scopi_container<dim>& particles,
                    double dt,
                    const Params<optim_solver_t, problem_t, contact_t, vap_t>& params = Params<optim_solver_t, problem_t, contact_t, vap_t>());
        void solve(std::size_t total_it, std::size_t initial_iter = 0);

    private:
        void displacement_obstacles();
        std::vector<neighbor<dim>> compute_contacts();
        std::vector<neighbor<dim>> compute_contacts_worms();
        void write_output_files(const std::vector<neighbor<dim>>& contacts, std::size_t nite);
        void move_active_particles();
        void update_velocity();

        ScopiParams m_params;
        scopi_container<dim>& m_particles;
        double m_dt;
    };

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::ScopiSolver(scopi_container<dim>& particles,
                                                                    double dt,
                                                                    const Params<optim_solver_t, problem_t, contact_t, vap_t>& params)
    : optim_solver_t(particles.nb_active(), dt, particles, params.optim_params, params.problem_params)
    , vap_t(particles.nb_active(), particles.nb_inactive(), particles.size(), dt, params.vap_params)
    , contact_t(params.contacts_params)
    , m_params(params.scopi_params)
    , m_particles(particles)
    , m_dt(dt)
    {}

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    void ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::solve(std::size_t total_it, std::size_t initial_iter)
    {
        // Time Loop
        for (std::size_t nite = initial_iter; nite < total_it; ++nite)
        {
            PLOG_INFO << "\n\n------------------- Time iteration ----------------> " << nite;

            displacement_obstacles();
            auto contacts = compute_contacts();
            auto contacts_worms = compute_contacts_worms();
            if (nite % m_params.output_frequency == 0 || m_params.output_frequency == std::size_t(-1))
                write_output_files(contacts, nite);
            this->set_a_priori_velocity(m_particles, contacts, contacts_worms);
            this->extra_steps_before_solve(contacts);
            while (this->should_solve_optimization_problem())
            {
                optim_solver_t::run(m_particles, contacts, contacts_worms, nite);
                // TODO get_constraint computes a matrix-vector product, do it only if needed
                this->extra_steps_after_solve(contacts, this->get_lagrange_multiplier(contacts, contacts_worms), this->get_constraint(contacts));
            }
            move_active_particles();
            update_velocity();
        }
    }

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    void ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::displacement_obstacles()
    {
        tic();
        for (std::size_t i = 0; i < m_particles.nb_inactive(); ++i)
        {

            auto  w = get_omega(m_particles.desired_omega()(i));
            double normw = xt::linalg::norm(w);
            if (normw == 0)
            {
                normw = 1;
            }
            type::quaternion_t expw;
            auto expw_adapt = xt::adapt(expw);
            expw_adapt(0) = std::cos(0.5*normw*m_dt);
            xt::view(expw_adapt, xt::range(1, _)) = std::sin(0.5*normw*m_dt)/normw*w;

            for (std::size_t d = 0; d < dim; ++d)
            {
                m_particles.pos()(i)(d) += m_dt*m_particles.vd()(i)(d);
            }
            m_particles.q()(i) = mult_quaternion(m_particles.q()(i), expw);
            normalize(m_particles.q()(i));
            // std::cout << "obstacle " << i << ": " << m_particles.pos()(0) << " " << m_particles.q()(0) << std::endl;
        }
        auto duration = toc();
        PLOG_INFO << "----> CPUTIME : obstacles = " << duration;
    }

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    std::vector<neighbor<dim>> ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::compute_contacts()
    {
        auto contacts = contact_t::run(m_particles, m_particles.nb_inactive());
        PLOG_INFO << "contacts.size() = " << contacts.size() << std::endl;
        return contacts;
    }

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    std::vector<neighbor<dim>> ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::compute_contacts_worms()
    {
        std::vector<neighbor<dim>> contacts;
        #pragma omp parallel for
        for (std::size_t i = 0; i < m_particles.size(); ++i)
        {
            for (std::size_t j = 0; j < m_particles[i]->size()-1; ++j)
            {
                auto neigh = closest_points_dispatcher<dim>::dispatch(*select_object_dispatcher<dim>::dispatch(*m_particles[i], index(j  )),
                                                                      *select_object_dispatcher<dim>::dispatch(*m_particles[i], index(j+1)));
                neigh.i = m_particles.offset(i) + j;
                neigh.j = m_particles.offset(i) + j + 1;
                #pragma omp critical
                contacts.emplace_back(std::move(neigh));
            }
        }
        return contacts;
    }

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    void ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::write_output_files(const std::vector<neighbor<dim>>& contacts, std::size_t nite)
    {
        tic();
        nl::json json_output;

        std::ofstream file(fmt::format("{}{:04d}.json", m_params.filename, nite));

        json_output["objects"] = {};

        for (std::size_t i = 0; i < m_particles.size(); ++i)
        {
            json_output["objects"].push_back(write_objects_dispatcher<dim>::dispatch(*m_particles[i]));
        }

        if (m_params.write_velocity)
        {
            for (std::size_t i = 0; i < m_particles.size(); ++i)
            {
                json_output["objects"][i]["velocity"] = m_particles.v()(i);
            }
        }

        json_output["contacts"] = {};

        for (std::size_t i = 0; i < contacts.size(); ++i)
        {
            nl::json contact;

            contact["pi"] = contacts[i].pi;
            contact["pj"] = contacts[i].pj;
            contact["nij"] = contacts[i].nij;

            json_output["contacts"].push_back(contact);

        }

        file << std::setw(4) << json_output;
        file.close();

        auto duration = toc();
        PLOG_INFO << "----> CPUTIME : write output files = " << duration;
    }

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    void ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::move_active_particles()
    {
        tic();
        std::size_t active_offset = m_particles.nb_inactive();
        auto uadapt = this->get_uadapt();
        auto wadapt = this->get_wadapt();

        for (std::size_t i = 0; i < m_particles.nb_active(); ++i)
        {
            xt::xtensor_fixed<double, xt::xshape<3>> w({0, 0, wadapt(i, 2)});
            double normw = xt::linalg::norm(w);
            if (normw == 0)
            {
                normw = 1;
            }
            type::quaternion_t expw;
            auto expw_adapt = xt::adapt(expw);
            expw_adapt(0) = std::cos(0.5*normw*m_dt);
            xt::view(expw_adapt, xt::range(1, _)) = std::sin(0.5*normw*m_dt)/normw*w;
            for (std::size_t d = 0; d < dim; ++d)
            {
                m_particles.pos()(i + active_offset)(d) += m_dt*uadapt(i, d);
            }

            m_particles.q()(i + active_offset) = mult_quaternion(m_particles.q()(i + active_offset), expw);
            normalize(m_particles.q()(i + active_offset));
        }

        auto duration = toc();
        PLOG_INFO << "----> CPUTIME : move active particles = " << duration;
    }

    template<std::size_t dim, class optim_solver_t, class contact_t, class vap_t>
    void ScopiSolver<dim, optim_solver_t, contact_t, vap_t>::update_velocity()
    {
        tic();
        std::size_t active_offset = m_particles.nb_inactive();
        auto uadapt = this->get_uadapt();
        auto wadapt = this->get_wadapt();

        for (std::size_t i = 0; i < m_particles.nb_active(); ++i)
        {
            for (std::size_t d = 0; d < dim; ++d)
            {
                m_particles.v()(i + active_offset)(d) = uadapt(i, d);
            }
            update_velocity_omega(m_particles, i, wadapt);
        }
        auto duration = toc();
        PLOG_INFO << "----> CPUTIME : update velocity = " << duration;
    }

    template<>
    void update_velocity_omega(scopi_container<2>& particles, std::size_t i, const xt::xtensor<double, 2>& wadapt)
    {
        particles.omega()(i + particles.nb_inactive()) = wadapt(i, 2);
    }

    void update_velocity_omega(scopi_container<3>& particles, std::size_t i, const xt::xtensor<double, 2>& wadapt)
    {
        for (std::size_t d = 0; d < 3; ++d)
        {
            particles.omega()(i + particles.nb_inactive())(d) = wadapt(i, d);
        }
    }

    
}

