#include <cstddef>
#include <xtensor/xmath.hpp>
#include <scopi/objects/types/globule.hpp>
#include <scopi/solver.hpp>
#include <scopi/property.hpp>
#include <scopi/solvers/OptimMosek.hpp>
#include <scopi/problems/ViscousGlobule.hpp>

int main()
{
    plog::init(plog::error, "two_globules.log");

    constexpr std::size_t dim = 2;
    double dt = .005;
    std::size_t total_it = 1;
    scopi::scopi_container<dim> particles;
    auto prop = scopi::property<dim>().mass(1.).moment_inertia(0.1);

    scopi::globule<dim> g1({{1., 1.}, {3., 1.}, {5., 1.}, {7., 1.}, {9., 1.}, {11., 1.}}, 1.);
    scopi::globule<dim> g2({{-1., -1.}, {-3., -1.}, {-5., -1.}, {-7., -1.}, {-9., -1.}, {-11., -1.}}, 1.);
    particles.push_back(g1, prop.desired_velocity({-1., 0.}));
    particles.push_back(g2, prop.desired_velocity({1., 0.}));

    scopi::OptimParams<scopi::OptimMosek> optim_params;
    scopi::ProblemParams<scopi::ViscousGlobule> problem_params;
    scopi::ScopiSolver<dim, scopi::ViscousGlobule, scopi::OptimMosek> solver(particles, dt, optim_params, problem_params);
    solver.solve(total_it);

    return 0;
}
