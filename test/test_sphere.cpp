#include <gtest/gtest.h>
#include "utils.hpp"

#include <scopi/objects/types/sphere.hpp>
#include <scopi/container.hpp>
#include <scopi/solvers/mosek.hpp>

namespace scopi
{
    class Sphere2dTest  : public ::testing::Test {
        protected:
            Sphere2dTest()
                : m_s({{-0.2, 0.0}}, 0.1)
                {}
            sphere<2> m_s;
    };

    class Sphere2dConstTest  : public ::testing::Test {
        protected:
            Sphere2dConstTest()
                : m_s({{-0.2, 0.0}}, 0.1)
                {}
            const sphere<2> m_s;
    };

    class Sphere2dRotationTest  : public ::testing::Test {
        protected:
            Sphere2dRotationTest()
                : m_s({{-0.2, 0.0}}, {quaternion(PI/3)}, 0.1)
                {}
            sphere<2> m_s;
    };

    class Sphere3dTest  : public ::testing::Test {
        protected:
            Sphere3dTest()
                : m_s({{-0.2, 0.0, 0.1}}, 0.1)
                {}
            sphere<3> m_s;
    };

    class Sphere3dConstTest  : public ::testing::Test {
        protected:
            Sphere3dConstTest()
                : m_s({{-0.2, 0.0, 0.1}}, 0.1)
                {}
            const sphere<3> m_s;
    };

    class Sphere3dRotationTest  : public ::testing::Test {
        protected:
            Sphere3dRotationTest()
                : m_s({{-0.2, 0.0, 0.1}}, {quaternion(PI/3)}, 0.1)
                {}
            sphere<3> m_s;
    };

    // pos
    TEST_F(Sphere2dTest, pos_2d)
    {
        EXPECT_EQ(m_s.pos()(0), -0.2);
        EXPECT_EQ(m_s.pos()(1), 0.);
    }

    TEST_F(Sphere3dTest, pos_3d)
    {
        EXPECT_EQ(m_s.pos()(0), -0.2);
        EXPECT_EQ(m_s.pos()(1), 0.);
        EXPECT_EQ(m_s.pos()(2), 0.1);
    }

    TEST_F(Sphere2dConstTest, pos_2d_const)
    {
        EXPECT_EQ(m_s.pos()(0), -0.2);
        EXPECT_EQ(m_s.pos()(1), 0.);
    }

    TEST_F(Sphere3dConstTest, pos_3d_const)
    {
        EXPECT_EQ(m_s.pos()(0), -0.2);
        EXPECT_EQ(m_s.pos()(1), 0.);
        EXPECT_EQ(m_s.pos()(2), 0.1);
    }

    TEST_F(Sphere2dTest, pos_2d_index)
    {
        EXPECT_EQ(m_s.pos(0)(), -0.2);
        EXPECT_EQ(m_s.pos(1)(), 0.);
    }

    TEST_F(Sphere3dTest, pos_3d_index)
    {
        EXPECT_EQ(m_s.pos(0)(), -0.2);
        EXPECT_EQ(m_s.pos(1)(), 0.);
        EXPECT_EQ(m_s.pos(2)(), 0.1);
    }

    TEST_F(Sphere2dConstTest, pos_2d_index_const)
    {
        EXPECT_EQ(m_s.pos(0)(), -0.2);
        EXPECT_EQ(m_s.pos(1)(), 0.);
    }

    TEST_F(Sphere3dConstTest, pos_3d_index_const)
    {
        EXPECT_EQ(m_s.pos(0)(), -0.2);
        EXPECT_EQ(m_s.pos(1)(), 0.);
        EXPECT_EQ(m_s.pos(2)(), 0.1);
    }

    TEST_F(Sphere2dTest, pos_2d_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->pos()(0), -0.2);
        EXPECT_EQ(particles[0]->pos()(1), 0.);
    }

    TEST_F(Sphere3dTest, pos_3d_container)
    {
        constexpr std::size_t dim = 3;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0, 0}}, {{0.25, 0, 0}}, 0, 0, {{0, 0, 0}});

        EXPECT_EQ(particles[0]->pos()(0), -0.2);
        EXPECT_EQ(particles[0]->pos()(1), 0.);
        EXPECT_EQ(particles[0]->pos()(2), 0.1);
    }

    TEST_F(Sphere2dConstTest, pos_2d_const_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->pos()(0), -0.2);
        EXPECT_EQ(particles[0]->pos()(1), 0.);
    }

    TEST_F(Sphere3dConstTest, pos_3d_const_container)
    {
        constexpr std::size_t dim = 3;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0, 0}}, {{0.25, 0, 0}}, 0, 0, {{0, 0, 0}});

        EXPECT_EQ(particles[0]->pos()(0), -0.2);
        EXPECT_EQ(particles[0]->pos()(1), 0.);
        EXPECT_EQ(particles[0]->pos()(2), 0.1);
    }

    TEST_F(Sphere2dTest, pos_2d_index_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->pos(0)(), -0.2);
        EXPECT_EQ(particles[0]->pos(1)(), 0.);
    }

    TEST_F(Sphere3dTest, pos_3d_index_container)
    {
        constexpr std::size_t dim = 3;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0, 0}}, {{0.25, 0, 0}}, 0, 0, {{0, 0, 0}});

        EXPECT_EQ(particles[0]->pos(0)(), -0.2);
        EXPECT_EQ(particles[0]->pos(1)(), 0.);
        EXPECT_EQ(particles[0]->pos(2)(), 0.1);
    }

    TEST_F(Sphere2dConstTest, pos_2d_index_const_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->pos(0)(), -0.2);
        EXPECT_EQ(particles[0]->pos(1)(), 0.);
    }

    TEST_F(Sphere3dConstTest, pos_3d_index_const_container)
    {
        constexpr std::size_t dim = 3;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0, 0}}, {{0.25, 0, 0}}, 0, 0, {{0, 0, 0}});

        EXPECT_EQ(particles[0]->pos(0)(), -0.2);
        EXPECT_EQ(particles[0]->pos(1)(), 0.);
        EXPECT_EQ(particles[0]->pos(2)(), 0.1);
    }

    // q
    TEST_F(Sphere2dTest, q)
    {
        EXPECT_EQ(m_s.q()(0), 1.);
        EXPECT_EQ(m_s.q()(1), 0.);
        EXPECT_EQ(m_s.q()(2), 0.);
        EXPECT_EQ(m_s.q()(3), 0.);
    }

    TEST_F(Sphere2dConstTest, q_const)
    {
        EXPECT_EQ(m_s.q()(0), 1.);
        EXPECT_EQ(m_s.q()(1), 0.);
        EXPECT_EQ(m_s.q()(2), 0.);
        EXPECT_EQ(m_s.q()(3), 0.);
    }

    TEST_F(Sphere2dTest, q_index)
    {
        EXPECT_EQ(m_s.q(0)(), 1.);
        EXPECT_EQ(m_s.q(1)(), 0.);
        EXPECT_EQ(m_s.q(2)(), 0.);
        EXPECT_EQ(m_s.q(3)(), 0.);
    }

    TEST_F(Sphere2dConstTest, q_index_const)
    {
        EXPECT_EQ(m_s.q(0)(), 1.);
        EXPECT_EQ(m_s.q(1)(), 0.);
        EXPECT_EQ(m_s.q(2)(), 0.);
        EXPECT_EQ(m_s.q(3)(), 0.);
    }

    TEST_F(Sphere2dTest, q_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->q()(0), 1.);
        EXPECT_EQ(particles[0]->q()(1), 0.);
        EXPECT_EQ(particles[0]->q()(2), 0.);
        EXPECT_EQ(particles[0]->q()(3), 0.);
    }

    TEST_F(Sphere2dConstTest, q_const_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->q()(0), 1.);
        EXPECT_EQ(particles[0]->q()(1), 0.);
        EXPECT_EQ(particles[0]->q()(2), 0.);
        EXPECT_EQ(particles[0]->q()(3), 0.);
    }

    TEST_F(Sphere2dTest, q_index_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->q(0)(), 1.);
        EXPECT_EQ(particles[0]->q(1)(), 0.);
        EXPECT_EQ(particles[0]->q(2)(), 0.);
        EXPECT_EQ(particles[0]->q(3)(), 0.);
    }

    TEST_F(Sphere2dConstTest, q_index_const_container)
    {
        constexpr std::size_t dim = 2;
        scopi_container<dim> particles;
        particles.push_back(m_s, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});

        EXPECT_EQ(particles[0]->q(0)(), 1.);
        EXPECT_EQ(particles[0]->q(1)(), 0.);
        EXPECT_EQ(particles[0]->q(2)(), 0.);
        EXPECT_EQ(particles[0]->q(3)(), 0.);
    }

    // radius
    TEST_F(Sphere2dTest, radius)
    {
        EXPECT_EQ(m_s.radius(), 0.1);
    }

    // rotation
    TEST_F(Sphere2dRotationTest, rotation_2d)
    {
        auto rotation_matrix = m_s.rotation();
        EXPECT_DOUBLE_EQ(rotation_matrix(0, 0), 1./2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(0, 1), -std::sqrt(3.)/2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(1, 0), std::sqrt(3.)/2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(1, 1), 1./2.);
    }
    

    TEST_F(Sphere3dRotationTest, rotation_3d)
    {
        auto rotation_matrix = m_s.rotation();
        EXPECT_DOUBLE_EQ(rotation_matrix(0, 0), 1./2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(0, 1), -std::sqrt(3.)/2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(0, 2), 0.);
        EXPECT_DOUBLE_EQ(rotation_matrix(1, 0), std::sqrt(3.)/2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(1, 1), 1./2.);
        EXPECT_DOUBLE_EQ(rotation_matrix(1, 2), 0.);
        EXPECT_DOUBLE_EQ(rotation_matrix(2, 0), 0.);
        EXPECT_DOUBLE_EQ(rotation_matrix(2, 1), 0.);
        EXPECT_DOUBLE_EQ(rotation_matrix(2, 2), 1.);
    }
    //

    // point
    TEST_F(Sphere2dTest, point_2d)
    {
        auto point = m_s.point(0.);
        EXPECT_EQ(point(0), -0.1);
        EXPECT_EQ(point(1), 0.);
    }

    TEST_F(Sphere3dTest, point_3d)
    {
        auto point = m_s.point(0., 0.);
        EXPECT_EQ(point(0), -0.1);
        EXPECT_EQ(point(1), 0.);
        EXPECT_EQ(point(2), 0.1);
    }
    //

    // normal
    TEST_F(Sphere2dTest, normal_2d)
    {
        auto normal = m_s.normal(0.);
        EXPECT_EQ(normal(0), 1.);
        EXPECT_EQ(normal(1), 0.);
    }

    TEST_F(Sphere3dTest, normal_3d)
    {
        auto normal = m_s.normal(0., 0.);
        EXPECT_EQ(normal(0), 1.);
        EXPECT_EQ(normal(1), 0.);
        EXPECT_EQ(normal(2), 0.);
    }

    // two_spheres
    class TestTwoSpheresAsymmetrical  : public ::testing::Test {
        protected:
            void SetUp() override {
                sphere<2> s1({{-0.2, -0.05}}, 0.1);
                sphere<2> s2({{ 0.2,  0.05}}, 0.1);
                m_particles.push_back(s1, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});
                m_particles.push_back(s2, {{0, 0}}, {{-0.25, 0}}, 0, 0, {{0, 0}});
            }

            double m_dt = .005;
            std::size_t m_total_it = 1000;
            scopi_container<2> m_particles;
            std::size_t m_active_ptr = 0; // without obstacles
    };

    class TestTwoSpheresSymmetrical  : public ::testing::Test {
        protected:
            void SetUp() override {
                sphere<2> s1({{-0.2, 0.}}, 0.1);
                sphere<2> s2({{ 0.2, 0.}}, 0.1);
                m_particles.push_back(s1, {{0, 0}}, {{0.25, 0}}, 0, 0, {{0, 0}});
                m_particles.push_back(s2, {{0, 0}}, {{-0.25, 0}}, 0, 0, {{0, 0}});
            }

            double m_dt = .005;
            std::size_t m_total_it = 1000;
            scopi_container<2> m_particles;
            std::size_t m_active_ptr = 0; // without obstacles
    };

    TEST_F(TestTwoSpheresAsymmetrical, two_spheres_asymmetrical)
    {
        // TODO set the optimization solver (Mosek, Uzawa, ...) here and duplicate this test for all solver 
        constexpr std::size_t dim = 2;
        ScopiSolver<dim> solver(m_particles, m_dt, m_active_ptr);
        solver.solve(m_total_it);

        std::string filenameRef;
        if(solver.getOptimSolverName() == "OptimMosek")
            filenameRef = "../test/two_spheres_asymmetrical_mosek.json"; 
        else if(solver.getOptimSolverName() == "OptimUzawaMkl")
            filenameRef = "../test/two_spheres_asymmetrical_uzawaMkl.json"; 
        else if(solver.getOptimSolverName() == "OptimScs")
            filenameRef = "../test/two_spheres_asymmetrical_scs.json"; 
        else if(solver.getOptimSolverName() == "OptimUzawaMatrixFreeTbb")
            filenameRef = "../test/two_spheres_asymmetrical_uzawaMatrixFreeTbb.json"; 
        else if(solver.getOptimSolverName() == "OptimUzawaMatrixFreeOmp")
            filenameRef = "../test/two_spheres_asymmetrical_uzawaMatrixFreeOmp.json"; 

        EXPECT_PRED2(diffFile, "./Results/scopi_objects_0999.json", filenameRef);
    }

    TEST_F(TestTwoSpheresSymmetrical, two_spheres_symmetrical)
    {
        // TODO set the optimization solver (Mosek, Uzawa, ...) here and duplicate this test for all solver 
        constexpr std::size_t dim = 2;
        ScopiSolver<dim> solver(m_particles, m_dt, m_active_ptr);
        solver.solve(m_total_it);

        std::string filenameRef;
        if(solver.getOptimSolverName() == "OptimMosek")
            filenameRef = "../test/two_spheres_symmetrical_mosek.json"; 
        else if(solver.getOptimSolverName() == "OptimUzawaMkl")
            filenameRef = "../test/two_spheres_symmetrical_uzawaMkl.json"; 
        else if(solver.getOptimSolverName() == "OptimScs")
            filenameRef = "../test/two_spheres_symmetrical_scs.json"; 
        else if(solver.getOptimSolverName() == "OptimUzawaMatrixFreeTbb")
            filenameRef = "../test/two_spheres_symmetrical_uzawaMatrixFreeTbb.json"; 
        else if(solver.getOptimSolverName() == "OptimUzawaMatrixFreeOmp")
            filenameRef = "../test/two_spheres_symmetrical_uzawaMatrixFreeOmp.json"; 

        EXPECT_PRED2(diffFile, "./Results/scopi_objects_0999.json", filenameRef);
    }

}
