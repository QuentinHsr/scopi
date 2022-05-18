#include "scopi/problems/MatrixOptimSolverFriction.hpp"
#include <utility>

namespace scopi
{
    std::pair<type::position_t<2>, double> analytical_solution_sphere_plan(double alpha, double mu, double t, double r, double g, double y0)
    {
        // TODO replace omega by theta
        double x_normal, omega;
        double t_impact = std::sqrt(2*(y0-r)/(g*std::cos(alpha)));
        if (t > t_impact)
        {
            double v_t_m = g*t_impact*std::sin(alpha);
            double v_n_m = -g*t_impact*std::cos(alpha);
            double t2 = (t - t_impact );
            double x_impact = g*std::sin(alpha)*t_impact*t_impact/2.;
            if(std::tan(alpha) <= 3*mu)
            {
                x_normal = g*std::sin(alpha)*t2*t2/3. + 2.*v_t_m*t2/3. + x_impact;
                omega = -2.*g*std::sin(alpha)*t2/(3.*r) - 2*v_t_m/(3.*r);
            }
            else
            {
                x_normal = g*(std::sin(alpha) - mu*std::cos(alpha))*t2*t2/2. + (v_t_m + mu*v_n_m)*t2 + x_impact;
                omega = -2.*mu*g*std::cos(alpha)*t2/r + 2*mu*v_n_m/r;
            }
            xt::xtensor<double, 1> x = xt::xtensor<double, 1>({x_normal*std::cos(alpha) + r*std::sin(alpha), -x_normal*std::sin(alpha) + r*std::cos(alpha)});
            return std::make_pair(x, omega);
        }
        else
        {
            xt::xtensor<double, 1> x = xt::xtensor<double, 1>({y0*std::sin(alpha), y0*std::cos(alpha) - g*t*t/2.});
            return std::make_pair(x, 0.);
        }
    }

    std::pair<type::position_t<2>, double> analytical_solution_sphere_plan_velocity(double alpha, double mu, double t, double r, double g, double y0)
    {
        double v_normal, omega;
        double t_impact = std::sqrt(2*(y0-r)/(g*std::cos(alpha)));
        if (t > t_impact)
        {
            double v_t_m = g*t_impact*std::sin(alpha);
            double v_n_m = -g*t_impact*std::cos(alpha);
            double t2 = (t - t_impact );
            if(std::tan(alpha) <= 3*mu)
            {
                v_normal = 2.*g*std::sin(alpha)*t2*t2/3. + 2.*v_t_m/3.;
                omega = -2.*g*std::sin(alpha)*t2/(3.*r) - 2*v_t_m/(3.*r);
            }
            else
            {
                v_normal = g*(std::sin(alpha) - mu*std::cos(alpha))*t2 + (v_t_m + mu*v_n_m);
                omega = -2.*mu*g*std::cos(alpha)*t2/r + 2*mu*v_n_m/r;
            }
            xt::xtensor<double, 1> x = xt::xtensor<double, 1>({v_normal*std::cos(alpha), -v_normal*std::sin(alpha)});
            return std::make_pair(x, omega);
        }
        else
        {
            xt::xtensor<double, 1> x = xt::xtensor<double, 1>({0., -g*t});
            return std::make_pair(x, 0.);
        }
    }


    MatrixOptimSolverFriction::MatrixOptimSolverFriction(std::size_t nparticles, double dt)
    : m_nparticles(nparticles)
    , m_dt(dt)
    , m_mu(0.)
    {}

    void MatrixOptimSolverFriction::set_coeff_friction(double mu)
    {
        m_mu = mu;
    }

    std::size_t MatrixOptimSolverFriction::get_nb_gamma_neg()
    {
        return 0;
    }

    std::size_t MatrixOptimSolverFriction::get_nb_gamma_min()
    {
        return 0;
    }
}
