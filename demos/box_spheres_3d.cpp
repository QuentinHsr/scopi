#include <cstddef>
#include <vector>
#include <xtensor/xmath.hpp>
#include <scopi/objects/types/sphere.hpp>
#include <scopi/objects/types/plan.hpp>
#include <scopi/solver.hpp>
#include <scopi/property.hpp>

#include <scopi/solvers/OptimProjectedGradient.hpp>
#include <scopi/solvers/gradient/nesterov_dynrho_restart.hpp>
#include <scopi/solvers/OptimMosek.hpp>
#include <scopi/vap/vap_fpd.hpp>
#include <scopi/contact/contact_brute_force.hpp>

int main()
{
    plog::init(plog::info, "box_spheres_3d_new_friction_small_01_convexe.log");

    constexpr std::size_t dim = 3;
    double PI = xt::numeric_constants<double>::PI;

    std::size_t total_it = 500;
    double width_box = 10.;
    std::size_t n = 8; // n^3 spheres
    double g = 1.;

    double r0 = width_box/n/2.;
    double dt = 0.2*(r0-0.1)/(2.*g);
    double rho = 0.2/(dt*dt);
    std::cout << "dt = " << dt << "  rho = " << rho << std::endl;

    // scopi::Params<scopi::OptimProjectedGradient<scopi::DryWithoutFriction, scopi::nesterov_dynrho_restart<>>, scopi::DryWithoutFriction, scopi::contact_brute_force, scopi::vap_fpd> params;
    // params.scopi_params.output_frequency = 2000;
    // params.optim_params.tol_l = 1e-3;
    // params.optim_params.rho = rho;

    scopi::Params<scopi::OptimMosek<scopi::DryWithFriction>, scopi::DryWithFriction, scopi::contact_brute_force, scopi::vap_fpd> params;
    params.optim_params.change_default_tol_mosek = false;
    params.problem_params.mu = 0.1;
    params.contacts_params.dmax = 4.*(r0-0.1);

    scopi::scopi_container<dim> particles;
    auto prop = scopi::property<dim>().force({{0., -g, 0.}});

    const xt::xtensor_fixed<double, xt::xshape<dim>> axes_y({0., 1., 0.});
    const xt::xtensor_fixed<double, xt::xshape<dim>> axes_z({0., 0., 1.});

    scopi::plan<dim> p_left({{0., 0., 0.}}, {scopi::quaternion(0., axes_z)});
    particles.push_back(p_left, scopi::property<dim>().deactivate());
    scopi::plan<dim> p_right({{width_box+2*r0, 0., 0.}}, {scopi::quaternion(0., axes_z)});
    particles.push_back(p_right, scopi::property<dim>().deactivate());
    scopi::plan<dim> p_horizontal({{0., 0., 0.}}, {scopi::quaternion(PI/2., axes_z)});
    particles.push_back(p_horizontal, scopi::property<dim>().deactivate());
    scopi::plan<dim> p_front({{0., 0., 0.}}, {scopi::quaternion(PI/2., axes_y)});
    particles.push_back(p_front, scopi::property<dim>().deactivate());
    scopi::plan<dim> p_back({{0., 0., width_box+2*r0}}, {scopi::quaternion(PI/2., axes_y)});
    particles.push_back(p_back, scopi::property<dim>().deactivate());

    std::default_random_engine generator;
    std::uniform_real_distribution<double> distrib_r(r0-0.2, r0-0.1);
    std::uniform_real_distribution<double> distrib_m(1., 2.);
    std::uniform_real_distribution<double> distrib_mouve_center(-0.05, 0.05);

    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = 0; j < n; ++j)
        {
            for (std::size_t k = 0; k < n; ++k)
            {
                double r = distrib_r(generator);
                double m = distrib_m(generator);
                double dx = distrib_mouve_center(generator);
                double dy = distrib_mouve_center(generator);
                double dz = distrib_mouve_center(generator);
                scopi::sphere<dim> s({{r0+0.1 + i*2.*r0 + dx, 1.1*r0 + 2.1*r0*j + dy, r0+0.1 + k*2.*r0 + dz}}, r);
                particles.push_back(s, prop.mass(m).moment_inertia({m*r*r/2., m*r*r/2., m*r*r/2.}));
            }
        }
    }

    scopi::ScopiSolver<dim, scopi::OptimMosek<scopi::DryWithFriction>, scopi::contact_brute_force, scopi::vap_fpd> solver(particles, dt, params);
    // scopi::ScopiSolver<dim, scopi::OptimProjectedGradient<scopi::DryWithoutFriction, scopi::nesterov_dynrho_restart<>>, scopi::contact_brute_force, scopi::vap_fpd> solver(particles, dt, params);
    solver.solve(total_it);

    return 0;
}
