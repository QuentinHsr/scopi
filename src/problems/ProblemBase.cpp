#include "scopi/problems/ProblemBase.hpp"
#include <cstddef>

namespace scopi
{
    ProblemBase::ProblemBase(std::size_t nparts, double dt)
    : m_nparticles(nparts)
    , m_dt(dt)
    {}

    void ProblemBase::matrix_free_gemv_inv_P_moment(const scopi_container<2>& particles,
                                                    xt::xtensor<double, 1>& U,
                                                    std::size_t active_offset,
                                                    std::size_t row)
    {
        auto nparticles = particles.nb_active();
        U(3*nparticles + 3*row + 2) /= (-1.*particles.j()(active_offset + row));
    }

    void ProblemBase::matrix_free_gemv_inv_P_moment(const scopi_container<3>& particles,
                                                    xt::xtensor<double, 1>& U,
                                                    std::size_t active_offset,
                                                    std::size_t row)
    {
        auto nparticles = particles.nb_active();
        for (std::size_t d = 0; d < 3; ++d)
        {
            U(3*nparticles + 3*row + d) /= (-1.*particles.j()(active_offset + row)(d));
        }
    }
}

