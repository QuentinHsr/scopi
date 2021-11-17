#pragma once

#include "mkl_service.h"
#include "mkl_spblas.h"
#include <stdio.h>

namespace scopi{
    template<std::size_t dim>
        class UzawaSolver : public OptimizationSolver<dim>
    {
        public:
            UzawaSolver(scopi::scopi_container<dim>& particles, double dt, std::size_t Nactive, std::size_t active_ptr);
            void createMatrixConstraint(std::vector<scopi::neighbor<dim>>& contacts);
            void createMatrixMass();
            int solveOptimizationProbelm(std::vector<scopi::neighbor<dim>>& contacts);
            auto getUadapt();
            auto getWadapt();
            void allocateMemory(std::size_t nc);
            void freeMemory();

        private:
            int testMkl();

            const double _tol;
            const std::size_t _maxiter;
            const double _rho;

            sparse_status_t _status;
            sparse_matrix_t _invP;
            struct matrix_descr _descrInvP;
            sparse_matrix_t _A;
            struct matrix_descr _descrA;
            sparse_matrix_t _matMatProd;

    };

    template<std::size_t dim>
        UzawaSolver<dim>::UzawaSolver(scopi::scopi_container<dim>& particles, double dt, std::size_t Nactive, std::size_t active_ptr) : 
            OptimizationSolver<dim>(particles, dt, Nactive, active_ptr, 2*3*Nactive + 2*3*Nactive, 0),
            _tol(1.0e-2), _maxiter(40000), _rho(0.2)
    {
    }

    template<std::size_t dim>
        void UzawaSolver<dim>::createMatrixConstraint(std::vector<scopi::neighbor<dim>>& contacts)
        {
            std::vector<MKL_INT> coo_rows;
            std::vector<MKL_INT> coo_cols;
            std::vector<double> coo_vals;
            OptimizationSolver<dim>::createMatrixConstraint(contacts, coo_rows, coo_cols, coo_vals, 0);

            std::vector<MKL_INT> csr_row;
            std::vector<MKL_INT> csr_col;
            std::vector<double> csr_val;
            OptimizationSolver<dim>::cooToCsr(coo_rows, coo_cols, coo_vals, csr_row, csr_col, csr_val);

            _status = mkl_sparse_d_create_csc( &_A,
                    SPARSE_INDEX_BASE_ZERO,
                    contacts.size(),    // number of rows
                    6*this->_Nactive,    // number of cols
                    csr_row.data(),
                    csr_row.data()+1,
                    csr_col.data(),
                    csr_val.data() );
            if (_status != SPARSE_STATUS_SUCCESS)
            {
                printf(" Error in mkl_sparse_d_create_csc for matrix A: %d \n", _status);
            }

            _descrA.type = SPARSE_MATRIX_TYPE_GENERAL;
            _descrA.diag = SPARSE_DIAG_NON_UNIT;
        }

    template<std::size_t dim>
        void UzawaSolver<dim>::createMatrixMass()
        {
            // !!!!!!!
            // auto invM = xt::ones<double>({3*p.size(), });
            // !!!!!!!
            std::vector<MKL_INT> col(6*this->_Nactive+1);
            std::vector<MKL_INT> row(6*this->_Nactive);
            std::vector<double> val(6*this->_Nactive);

            for (std::size_t i=0; i<this->_Nactive; ++i)
            {
                for (std::size_t d=0; d<3; ++d)
                {
                    row.push_back(3*i + d);
                    col.push_back(3*i + d);
                    val.push_back(1./this->_mass); // TODO: add mass into particles
                }
            }
            for (std::size_t i=0; i<this->_Nactive; ++i)
            {
                for (std::size_t d=0; d<3; ++d)
                {
                    row.push_back(3*this->_Nactive + 3*i + d);
                    col.push_back(3*this->_Nactive + 3*i + d);
                    val.push_back(1./this->_moment);
                }
            }
            col.push_back(6*this->_Nactive);

            _status = mkl_sparse_d_create_csc( &_invP,
                    SPARSE_INDEX_BASE_ZERO,
                    6*this->_Nactive,    // number of rows
                    6*this->_Nactive,    // number of cols
                    col.data(),
                    col.data()+1,
                    row.data(),
                    val.data() );
            if (_status != SPARSE_STATUS_SUCCESS)
            {
                printf(" Error in mkl_sparse_d_create_csc for matrix P^-1: %d \n", _status);
            }

            _descrInvP.type = SPARSE_MATRIX_TYPE_DIAGONAL;
        }

    template<std::size_t dim>
        int UzawaSolver<dim>::solveOptimizationProbelm(std::vector<scopi::neighbor<dim>>& contacts)
        {
            // Uzawa algorithm

            // while (( dt*R.max()>tol*2*people[:,2].min()) and (k<nb_iter_max)):
            //    U[:] = V[:] - dt M^{-1} B.transpose()@L[:]
            //    R[:] = dt B@U[:] - (D[:]-dmin)
            //    L[:] = sp.maximum(L[:] + rho*R[:], 0)
            //    k += 1

            // The mkl_sparse_?_mv routine computes a sparse matrix-vector product defined as
            //              y := alpha*op(A)*x + beta*y
            // sparse_status_t mkl_sparse_d_mv (
            //    sparse_operation_t operation,
            //    double alpha,
            //    const sparse_matrix_t A,
            //    struct matrix_descr descr,
            //    const double *x,
            //    double beta,
            //    double *y
            // );

            auto U = xt::adapt(reinterpret_cast<double*>(this->_particles.v().data()), {3*this->_particles.size(), });
            auto V = xt::adapt(reinterpret_cast<double*>(this->_particles.vd().data()), {3*this->_particles.size(), });
            auto L = xt::zeros_like(this->_distances);
            auto R = xt::zeros_like(this->_distances);

            std::size_t cc = 0;
            double cmax = -1000.0;
            while ( (cmax<=-_tol)&&(cc <= _maxiter) )
            {
                _status = mkl_sparse_spmm(SPARSE_OPERATION_TRANSPOSE, _A, _invP, &_matMatProd); // matMatProd = A^T * P^-1
                if (_status != SPARSE_STATUS_SUCCESS && _status != SPARSE_STATUS_NOT_SUPPORTED)
                {
                    printf(" Error in mkl_sparse_spmm: %d \n", _status);
                    return -1;
                }

                _status = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, 1., _matMatProd, _descrA, &L[0], 0., &U[0]); // U = (A^T * P^-1) * L
                if (_status != SPARSE_STATUS_SUCCESS && _status != SPARSE_STATUS_NOT_SUPPORTED)
                {
                    printf(" Error in mkl_sparse_d_mv U = (P^-1 A) L: %d \n", _status);
                    return -1;
                }

                _status = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, 1., _invP, _descrInvP, &this->_c[0], 1., &U[0]); // U = P^-1 * c + U
                if (_status != SPARSE_STATUS_SUCCESS && _status != SPARSE_STATUS_NOT_SUPPORTED)
                {
                    printf(" Error in mkl_sparse_d_mv U = P^-1 c + U: %d \n", _status);
                    return -1;
                }

                _status = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, 1., _A, _descrA, &U[0], 0., &R[0]); // R = A * U
                if (_status != SPARSE_STATUS_SUCCESS && _status != SPARSE_STATUS_NOT_SUPPORTED)
                {
                    printf(" Error in mkl_sparse_d_mv R = A U: %d \n", _status);
                    return -1;
                }

                R = R - this->_distances;
                L = xt::maximum( L-_rho*R, 0);

                cmax = double((xt::amin(R))(0));
                cc += 1;
            }

            if (cc>=_maxiter)
            {
                std::cout<<"\n-- C++ -- Projection : ********************** WARNING **********************"<<std::endl;
                std::cout<<  "-- C++ -- Projection : *************** Uzawa does not converge ***************"<<std::endl;
                std::cout<<  "-- C++ -- Projection : ********************** WARNING **********************\n"<<std::endl;
            }
            return cc;
        }

    template<std::size_t dim>
        int UzawaSolver<dim>::testMkl()
        {
#ifdef MKL_ILP64
#define INT_PRINT_FORMAT "%lld"
#else
#define INT_PRINT_FORMAT "%d"
#endif
            //*******************************************************************************
            //     Definition arrays for sparse representation of the matrix A in
            //     the coordinate format:
            //*******************************************************************************
#define M 5    /* nrows = ncols = M */
#define NNZ 13
            MKL_INT m = M;

            MKL_INT colIndex[M+1] = {   0,               3,              6,              9,        11,       13};
            MKL_INT rows[NNZ]     = {   0,   1,    3,    0,   1,   4,    0,   2,   3,    2,   3,    2,   4};
            double values[NNZ]    = { 1.0 -2.0, -4.0, -1.0, 5.0, 8.0, -3.0, 4.0, 2.0,  6.0, 7.0, 4.0, -5.0};

            //*******************************************************************************
            //    Declaration of local variables :
            //*******************************************************************************


            double      sol_vec[M]  = {1.0, 1.0, 1.0, 1.0, 1.0};
            double      rhs_vec[M]  = {0.0, 0.0, 0.0, 0.0, 0.0};

            double      alpha = 1.0, beta = 0.0;
            MKL_INT     i;
            struct matrix_descr descrA;
            sparse_matrix_t cscA;

            sparse_status_t status;
            int exit_status = 0;

            printf( "\n EXAMPLE PROGRAM FOR CSC format routines from IE Sparse BLAS\n" );
            printf( "-------------------------------------------------------\n" );

            //*******************************************************************************
            //   Create CSC sparse matrix handle and analyze step
            //*******************************************************************************

            status = mkl_sparse_d_create_csc( &cscA,
                    SPARSE_INDEX_BASE_ZERO,
                    m,    // number of rows
                    m,    // number of cols
                    colIndex,
                    colIndex+1,
                    rows,
                    values );

            if (status != SPARSE_STATUS_SUCCESS) {
                printf(" Error in mkl_sparse_d_create_csc: %d \n", status);
                exit_status = 1;
                goto exit;
            }

            //*******************************************************************************
            // First we set hints for the different operations before calling the
            // mkl_sparse_optimize() api which actually does the analyze step.  Not all
            // configurations have optimized steps, so the hint apis may return status
            // MKL_SPARSE_STATUS_NOT_SUPPORTED (=6) if no analysis stage is actually available
            // for that configuration.
            //*******************************************************************************

            //*******************************************************************************
            // Set hints for Task 2: Upper triangular transpose MV and SV solve
            // with unit diagonal
            //*******************************************************************************
            descrA.type = SPARSE_MATRIX_TYPE_GENERAL;

            status = mkl_sparse_set_mv_hint(cscA, SPARSE_OPERATION_NON_TRANSPOSE, descrA, 1 );
            if (status != SPARSE_STATUS_SUCCESS && status != SPARSE_STATUS_NOT_SUPPORTED) {
                printf(" Error in set hints for Task 2: mkl_sparse_set_mv_hint: %d \n", status);
                exit_status = 1;
                goto exit;
            }

            status = mkl_sparse_set_sv_hint(cscA, SPARSE_OPERATION_NON_TRANSPOSE, descrA, 1 );
            if (status != SPARSE_STATUS_SUCCESS && status != SPARSE_STATUS_NOT_SUPPORTED) {
                printf(" Error in set hints for Task 2: mkl_sparse_set_sv_hint: %d \n", status);
                exit_status = 1;
                goto exit;
            }
            //*******************************************************************************
            // Analyze sparse matrix; choose proper kernels and workload balancing strategy
            //*******************************************************************************
            status = mkl_sparse_optimize ( cscA );
            if (status != SPARSE_STATUS_SUCCESS) {
                printf(" Error in mkl_sparse_optimize: %d \n", status);
                exit_status = 1;
                goto exit;
            }
            //*******************************************************************************
            //    Task 2.    Obtain Triangular matrix-vector multiply (U+I) *sol --> rhs
            //    and solve triangular system   (U+I) *tmp = rhs with single right hand sides
            //    Array tmp must be equal to the array sol
            //*******************************************************************************
            printf("                                     \n");
            printf("   TASK 2:                           \n");
            printf("   INPUT DATA FOR mkl_sparse_d_mv    \n");
            printf("   WITH UNIT UPPER TRIANGULAR MATRIX \n");
            printf("     ALPHA = %4.1f  BETA = %4.1f     \n", alpha, beta);
            printf("     SPARSE_OPERATION_NON_TRANSPOSE  \n" );
            printf("   Input vector                      \n");
            for (i = 0; i < m; i++) {
                printf("%7.1f\n", sol_vec[i]);
            };

            descrA.type = SPARSE_MATRIX_TYPE_GENERAL;

            status = mkl_sparse_d_mv( SPARSE_OPERATION_NON_TRANSPOSE, alpha, cscA, descrA, sol_vec, beta, rhs_vec);
            if (status != SPARSE_STATUS_SUCCESS) {
                printf(" Error in Task 2 mkl_sparse_d_mv: %d \n", status);
                exit_status = 1;
                goto exit;
            }

            printf("                                   \n");
            printf("   OUTPUT DATA FOR mkl_sparse_d_mv \n");
            printf("   WITH TRIANGULAR MATRIX          \n");
            for (i = 0; i < m; i++) {
                printf("%7.1f\n", rhs_vec[i]);
            };


exit:
            // Release matrix handle and deallocate matrix
            mkl_sparse_destroy ( cscA );

            return exit_status;

        }

    template<std::size_t dim>
        auto UzawaSolver<dim>::getUadapt()
        {
            std::vector<double> v(this->_Nactive * 3UL, 0.);
            return xt::adapt(v, {this->_Nactive, 3UL});
        }

    template<std::size_t dim>
        auto UzawaSolver<dim>::getWadapt()
        {
            std::vector<double> v(this->_Nactive * 3UL, 0.);
            return xt::adapt(v, {this->_Nactive, 3UL});
        }

    template<std::size_t dim>
        void UzawaSolver<dim>::allocateMemory(std::size_t nc)
        {
            std::ignore = nc;
        }

    template<std::size_t dim>
        void UzawaSolver<dim>::freeMemory()
        {
            mkl_sparse_destroy ( _invP );
            mkl_sparse_destroy ( _A );
            mkl_sparse_destroy ( _matMatProd );
        }
}
