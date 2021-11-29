#pragma once

#include "OptimizationSolver.hpp"
#include <scs.h>

namespace scopi{
    template<std::size_t dim>
        class ScsSolver : public OptimizationSolver<dim>
        {
            public:
                ScsSolver(scopi::scopi_container<dim>& particles, double dt, std::size_t Nactive, std::size_t active_ptr);
                ~ScsSolver();
                void createMatrixConstraint(std::vector<scopi::neighbor<dim>>& contacts);
                void createMatrixMass();
                int solveOptimizationProbelm(std::vector<scopi::neighbor<dim>>& contacts, std::vector<double>& solOut);
            private:
                ScsMatrix* _P = NULL;
                ScsMatrix* _A = NULL;
                ScsSolver(const ScsSolver &);
                ScsSolver & operator=(const ScsSolver &);
        };

    template<std::size_t dim>
        ScsSolver<dim>::ScsSolver(scopi::scopi_container<dim>& particles, double dt, std::size_t Nactive, std::size_t active_ptr) : 
            OptimizationSolver<dim>(particles, dt, Nactive, active_ptr, 2*3*Nactive, 0)
    {
            _P = new ScsMatrix;
            _A = new ScsMatrix;
    }

    template<std::size_t dim>
        ScsSolver<dim>::~ScsSolver()
        {
            delete _P;
            delete _A;
        }

    template<std::size_t dim>
        void ScsSolver<dim>::createMatrixConstraint(std::vector<scopi::neighbor<dim>>& contacts)
        {

            // COO storage to CSR storage is easy to write, e.g.
            // https://www-users.cse.umn.edu/~saad/software/SPARSKIT/
            // The CSC storage of A is the CSR storage of A^T
            // reverse the role of row and column pointers to have the transpose
            std::vector<int> coo_rows;
            std::vector<int> coo_cols;
            std::vector<double> coo_values;
            OptimizationSolver<dim>::createMatrixConstraint(contacts, coo_rows, coo_cols, coo_values, 0);

            std::size_t nrow = 6*this->_Nactive;
            std::size_t nnz = coo_values.size();
            std::vector<int> csc_col(nrow+1, 0);
            std::vector<int> csc_row(nnz);
            std::vector<double> csc_val(nnz);

            // determine row-lengths.
            for(std::size_t k = 0; k < nnz; ++k)
            {
                csc_col[coo_cols[k]]++;
            }

            // starting position of each row..
            {
                int k = 0;
                for(std::size_t j = 0; j < nrow+1; ++j)
                {
                    int k0 = csc_col[j];
                    csc_col[j] = k;
                    k += k0;
                }
            }

            // go through the structure  once more. Fill in output matrix.
            for(std::size_t k = 0; k < nnz; ++k)
            {
                int i = coo_cols[k];
                int j = coo_rows[k];
                double x = coo_values[k];
                int iad = csc_col[i];
                csc_val[iad] = x;
                csc_row[iad] = j;
                csc_col[i] = iad+1;
            }

            // shift back iao
            for(std::size_t j = nrow; j >= 1; --j)
            {
                csc_col[j] = csc_col[j-1];
            }
            csc_col[0] = 0;

            _A->x = new double[csc_val.size()];
            _A->i = new scs_int[csc_row.size()];
            _A->p = new scs_int[csc_col.size()];
            for(std::size_t i = 0; i < csc_val.size(); ++i)
                _A->x[i] = csc_val[i];
            for(std::size_t i = 0; i < csc_row.size(); ++i)
                _A->i[i] = csc_row[i];
            for(std::size_t i = 0; i < csc_col.size(); ++i)
                _A->p[i] = csc_col[i];
            _A->m = contacts.size();
            _A->n = 6*this->_Nactive;
        }

    template<std::size_t dim>
        void ScsSolver<dim>::createMatrixMass()
        {
            std::vector<scs_int> col;
            std::vector<scs_int> row;
            std::vector<scs_float> val;
            row.reserve(6*this->_Nactive);
            col.reserve(6*this->_Nactive+1);
            val.reserve(6*this->_Nactive);

            for (std::size_t i=0; i<this->_Nactive; ++i)
            {
                for (std::size_t d=0; d<3; ++d)
                {
                    row.push_back(3*i + d);
                    col.push_back(3*i + d);
                    val.push_back(this->_mass); // TODO: add mass into particles
                }
            }
            for (std::size_t i=0; i<this->_Nactive; ++i)
            {
                for (std::size_t d=0; d<3; ++d)
                {
                    row.push_back(3*this->_Nactive + 3*i + d);
                    col.push_back(3*this->_Nactive + 3*i + d);
                    val.push_back(this->_moment);
                }
            }
            col.push_back(6*this->_Nactive);

            // TODO allocation in constructor
            // There is a segfault if the memory is allocated in the constructor
            _P->x = new scs_float[6*this->_Nactive];
            _P->i = new scs_int[6*this->_Nactive];
            _P->p = new scs_int[6*this->_Nactive+1];
            for(std::size_t i = 0; i < val.size(); ++i)
                _P->x[i] = val[i];
            for(std::size_t i = 0; i < row.size(); ++i)
                _P->i[i] = row[i];
            for(std::size_t i = 0; i < col.size(); ++i)
                _P->p[i] = col[i];
            _P->m = 6*this->_Nactive;
            _P->n = 6*this->_Nactive;
        }

    template<std::size_t dim>
        int ScsSolver<dim>::solveOptimizationProbelm(std::vector<scopi::neighbor<dim>>& contacts, std::vector<double>& solOut)
        {
            ScsData d;
            d.m = contacts.size();
            d.n = 6*this->_Nactive;
            d.A = _A;
            d.P = _P;
            d.b = this->_distances.data();
            d.c = this->_c.data();

            ScsCone k;
            k.z = 0; // 0 linear equality constraints
            k.l = contacts.size(); // s >= 0
            k.bu = NULL; 
            k.bl = NULL; 
            k.bsize = 0;
            k.q = NULL;
            k.qsize = 0;
            k.s = NULL;
            k.ssize = 0;
            k.ep = 0;
            k.ed = 0;
            k.p = NULL;
            k.psize = 0;

            ScsSolution sol;
            sol.x = new double[d.n];
            sol.y = new double[d.m];
            sol.s = new double[d.m];
            ScsInfo info;

            ScsSettings stgs;
            // default values not set
            // use values given by
            // https://www.cvxgrp.org/scs/api/settings.html#settings
            stgs.normalize = 1;
            stgs.scale = 0.1;
            stgs.adaptive_scale = 1;
            stgs.rho_x = 1e-6;
            stgs.max_iters = 1e5;
            stgs.eps_abs = 1e-4;
            stgs.eps_rel = 1e-4;
            stgs.eps_infeas = 1e-7;
            stgs.alpha = 1.5;
            stgs.time_limit_secs = 0.;
            stgs.verbose = 1;
            stgs.warm_start = 0;
            stgs.acceleration_lookback = 0;
            stgs.acceleration_interval = 1;
            stgs.write_data_filename = NULL;
            stgs.log_csv_filename = NULL;

            scs(&d, &k, &stgs, &sol, &info);

            // if(info.iter == -1)
            //     std::abort();

            solOut = std::vector<double> (sol.x, sol.x + 6*this->_Nactive);
            auto nbIter = info.iter;
            int nbActiveContatcs = 0;
            for(std::size_t i = 0; i < contacts.size(); ++i)
            {
                if(sol.y[i] > 0.)
                {
                    nbActiveContatcs++;
                }
            }
            std::cout << "Contacts: " << contacts.size() << "  active contacts " << nbActiveContatcs << std::endl;

            // free the memory
            delete[] _A->x;
            delete[] _A->i;
            delete[] _A->p;
            delete[] _P->x;
            delete[] _P->i;
            delete[] _P->p;
            delete[] sol.x;
            delete[] sol.y;
            delete[] sol.s;

            return nbIter;
        }

}
