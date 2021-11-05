#pragma once

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>

#include <xtensor/xtensor.hpp>
#include <xtensor/xfixed.hpp>

#include <fmt/format.h>
#include <fusion.h>
#include <scs.h>
#include <nlohmann/json.hpp>

#include "../container.hpp"
#include "../objects/methods/closest_points.hpp"
#include "../objects/methods/write_objects.hpp"
#include "../objects/neighbor.hpp"
#include "../quaternion.hpp"

#include <nanoflann.hpp>

using namespace mosek::fusion;
using namespace monty;
namespace nl = nlohmann;

using namespace xt::placeholders;


namespace scopi
{
  ///////////////////////
  // KdTree definition //
  ///////////////////////
  template<std::size_t dim>
  class KdTree
  {
    public:
      KdTree(scopi::scopi_container<dim> &p, std::size_t actptr) : _p{p}, _actptr{actptr} {}
      inline std::size_t kdtree_get_point_count() const
      {
        //std::cout << "KDTREE _p.size() = "<< _p.size() <<std::endl;
        return _p.size();
      }
      inline double kdtree_get_pt(std::size_t idx, const std::size_t d) const
      {
        //std::cout << "KDTREE _p["<< _actptr+idx << "][" << d << "] = " << _p.pos()(_actptr+idx)[d] << std::endl;
        return _p.pos()(_actptr+idx)(d); //_p[idx]->pos()[d];
        // return _p.pos()(idx)[d];
      }
      template<class BBOX>
      bool kdtree_get_bbox(BBOX & /* bb */) const
      {
        return false;
      }
    private:
      scopi::scopi_container<dim> &_p;
      std::size_t _actptr;
  };

  struct useMosekSolver{};
  struct useScsSolver{};
  // https://stackoverflow.com/questions/46939699/how-can-i-use-c-template-argument-to-decide-which-type-of-member-is-in-a-class
  template <typename SolverType>
      struct matrixType_selector {};
  template <>
      struct matrixType_selector<useMosekSolver>
      {
          using matrixType = Matrix::t;
      };
  template <>
      struct matrixType_selector<useScsSolver>
      {
          using matrixType = ScsMatrix*;
      };
  template <typename SolverType>
      using matrixType = typename matrixType_selector<SolverType>::matrixType;

  template<std::size_t dim, typename SolverType>
      class MosekSolver
      {
          public:
              MosekSolver(scopi::scopi_container<dim>& particles, double dt, std::size_t active_ptr);
              void solve(std::size_t total_it);

          private:
              void displacementObstacles();
              std::vector<scopi::neighbor<dim>> createListContactsAndSort();
              void writeOutputFiles(std::vector<scopi::neighbor<dim>>& contacts, std::size_t nite);
              void moveActiveParticles(std::vector<double> uw);

              xt::xtensor<double, 1> createVectorDistances(std::vector<scopi::neighbor<dim>>& contacts);
              xt::xtensor<double, 1> createVectorC();

              xt::xtensor<double, 1> createVectorC_mosek();
              xt::xtensor<double, 1> createVectorC_scs();

              void createMatrixA_coo(std::vector<scopi::neighbor<dim>>& contacts, std::vector<int>& A_rows, std::vector<int>& A_cols, std::vector<double>& A_values, std::size_t firstCol);
              matrixType<SolverType> createMatrixA_mosek(std::vector<scopi::neighbor<dim>>& contacts);
              matrixType<SolverType> createMatrixAz_mosek();
              template<typename ptrType>
                  void createMatrixA_cscStorage(std::vector<scopi::neighbor<dim>>& contacts, std::vector<ptrType>& col, std::vector<ptrType>& row, std::vector<double>& val);
              matrixType<SolverType> createMatrixA_scs(std::vector<scopi::neighbor<dim>>& contacts);
              template<typename ptrType>
                  void createMatrixP_cscStorage(std::vector<ptrType>& col, std::vector<ptrType>& row, std::vector<double>& val);
              matrixType<SolverType> createMatrixP_scs();

              std::vector<double> createMatricesAndSolve(std::vector<scopi::neighbor<dim>>& contacts, std::size_t nite, useMosekSolver);
              std::vector<double> createMatricesAndSolve(std::vector<scopi::neighbor<dim>>& contacts, std::size_t nite, useScsSolver);


              scopi::scopi_container<dim>& _particles;
              double _dt;
              std::size_t _active_ptr;
              std::size_t _Nactive;
              double _mass = 1.;
              double _moment = .1;
              SolverType _solverType;

      };

  template<std::size_t dim, typename SolverType>
      MosekSolver<dim, SolverType>::MosekSolver(scopi::scopi_container<dim>& particles, double dt, std::size_t active_ptr) : _particles(particles), _dt(dt), _active_ptr(active_ptr)
  {
      _Nactive = _particles.size() - _active_ptr;
  }

  template<std::size_t dim, typename SolverType>
      void MosekSolver<dim, SolverType>::solve(std::size_t total_it)
      {
          // Time Loop
          for (std::size_t nite=0; nite<total_it; ++nite)
          {
              std::cout << "\n\n------------------- Time iteration ----------------> " << nite << std::endl;

              //displacement of obstacles
              displacementObstacles();

              // create list of contacts
              std::cout << "----> create list of contacts " << nite << std::endl;
              auto contacts = createListContactsAndSort();

              // output files
              std::cout << "----> json output files " << nite << std::endl;
              writeOutputFiles(contacts, nite);

              // for (std::size_t i=0; i<_Nactive; ++i)
              // {
              //     for (std::size_t d=0; d<dim; ++d)
              //     {
              //         particles.pos()(i + active_ptr)(d) += dt*particles.vd()(i + active_ptr)(d);
              //     }
              // }


              // Create and solve Mosek optimization problem
              auto sol = createMatricesAndSolve(contacts, nite, _solverType);

              // move the active particles
              moveActiveParticles(sol);
          }
      }

  template<std::size_t dim, typename SolverType>
      void MosekSolver<dim, SolverType>::displacementObstacles()
      {
          for (std::size_t i=0; i<_active_ptr; ++i)
          {
              xt::xtensor_fixed<double, xt::xshape<3>> w({0, 0, _particles.desired_omega()(i)});
              double normw = xt::linalg::norm(w);
              if (normw == 0)
              {
                  normw = 1;
              }
              scopi::type::quaternion expw;
              expw(0) = std::cos(0.5*normw*_dt);
              xt::view(expw, xt::range(1, _)) = std::sin(0.5*normw*_dt)/normw*w;

              for (std::size_t d=0; d<dim; ++d)
              {
                  _particles.pos()(i)(d) += _dt*_particles.vd()(i)(d);
              }
              _particles.q()(i) = scopi::mult_quaternion(_particles.q()(i), expw);

              std::cout << "obstacle " << i << ": " << _particles.pos()(0) << " " << _particles.q()(0) << std::endl;
          }
      }

  template<std::size_t dim, typename SolverType>
      std::vector<scopi::neighbor<dim>> MosekSolver<dim, SolverType>::createListContactsAndSort()
      {
          double dmax = 2;
          std::vector<scopi::neighbor<dim>> contacts;

          // utilisation de kdtree pour ne rechercher les contacts que pour les particules proches
          tic();
          using my_kd_tree_t = typename nanoflann::KDTreeSingleIndexAdaptor<
              nanoflann::L2_Simple_Adaptor<double, KdTree<dim>>, KdTree<dim>, dim >;
          KdTree<dim> kd(_particles,_active_ptr);
          my_kd_tree_t index(
                  dim, kd,
                  nanoflann::KDTreeSingleIndexAdaptorParams(10 /* max leaf */)
                  );
          index.buildIndex();
          auto duration = toc();
          std::cout << "----> CPUTIME : build kdtree index = " << duration << std::endl;

          tic();
#pragma omp parallel for num_threads(8)

          for(std::size_t i = 0; i < _particles.size() - 1; ++i)
          {

              // for(std::size_t j = i + 1; j < particles.size(); ++j)
              // {
              //     auto neigh = scopi::closest_points_dispatcher<dim>::dispatch(*particles[i], *particles[j]);
              //     if (neigh.dij < dmax)
              //     {
              //         contacts.emplace_back(std::move(neigh));
              //         contacts.back().i = i;
              //         contacts.back().j = j;
              //     }
              // }

              double query_pt[dim];
              for (std::size_t d=0; d<dim; ++d)
              {
                  query_pt[d] = _particles.pos()(_active_ptr + i)(d);
                  // query_pt[d] = particles.pos()(i)(d);
              }
              //std::cout << "i = " << i << " query_pt = " << query_pt[0] << " " << query_pt[1] << std::endl;

              std::vector<std::pair<size_t, double>> indices_dists;
              double radius = 4;

              nanoflann::RadiusResultSet<double, std::size_t> resultSet(
                      radius, indices_dists);

              std::vector<std::pair<unsigned long, double>> ret_matches;

              const std::size_t nMatches = index.radiusSearch(query_pt, radius, ret_matches,
                      nanoflann::SearchParams());

              //std::cout << i << " nMatches = " << nMatches << std::endl;

              for (std::size_t ic = 0; ic < nMatches; ++ic) {

                  std::size_t j = ret_matches[ic].first;
                  //double dist = ret_matches[ic].second;
                  if (i != j) {
                      auto neigh = scopi::closest_points_dispatcher<dim>::dispatch(*_particles[i], *_particles[j]);
                      if (neigh.dij < dmax) {
                          neigh.i = i;
                          neigh.j = j;
#pragma omp critical
                          contacts.emplace_back(std::move(neigh));
                          // contacts.back().i = i;
                          // contacts.back().j = j;
                      }
                  }

              }

          }
          duration = toc();
          std::cout << "----> CPUTIME : compute " << contacts.size() << " contacts = " << duration << std::endl;

          tic();
          std::sort(contacts.begin(), contacts.end(), [](auto& a, auto& b )
                  {
                  return a.i < b.i && a.j < b.j;
                  });
          duration = toc();
          std::cout << "----> CPUTIME : sort " << contacts.size() << " contacts = " << duration << std::endl;
          //exit(0);

          return contacts;
      }

  template<std::size_t dim, typename SolverType>
      void MosekSolver<dim, SolverType>::writeOutputFiles(std::vector<scopi::neighbor<dim>>& contacts, std::size_t nite)
      {
          nl::json json_output;

          std::ofstream file(fmt::format("./Results/scopi_objects_{:04d}.json", nite));

          json_output["objects"] = {};

          for(std::size_t i = 0; i < _particles.size(); ++i)
          {
              json_output["objects"].push_back(scopi::write_objects_dispatcher<dim>::dispatch(*_particles[i]));
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
      }

  template<std::size_t dim, typename SolverType>
      xt::xtensor<double, 1> MosekSolver<dim, SolverType>::createVectorC()
      {
          xt::xtensor<double, 1> c = xt::zeros<double>({2*3*_Nactive});
          std::size_t Mdec = 0;
          std::size_t Jdec = Mdec + 3*_Nactive;
          for (std::size_t i=0; i<_Nactive; ++i)
          {
              for (std::size_t d=0; d<dim; ++d)
              {
                  c(Mdec + 3*i + d) = -_mass*_particles.vd()(_active_ptr + i)[d]; // TODO: add mass into particles
              }
              c(Jdec + 3*i + 2) = -_moment*_particles.desired_omega()(_active_ptr + i);
          }
          return c;
      }

  template<std::size_t dim, typename SolverType>
      xt::xtensor<double, 1> MosekSolver<dim, SolverType>::createVectorC_mosek()
      {
          xt::xtensor<double, 1> c = xt::zeros<double>({1 + 2*3*_Nactive + 2*3*_Nactive});
          c(0) = 1;
          // TODO use xt functions
          auto tmp = createVectorC();
          for(std::size_t i = 0; i < 6*_Nactive; ++i)
          {
              c(1+i) = tmp(i);
          }
          return c;
      }

  template<std::size_t dim, typename SolverType>
      xt::xtensor<double, 1> MosekSolver<dim, SolverType>::createVectorC_scs()
      {
          return createVectorC();
      }

  template<std::size_t dim, typename SolverType>
      xt::xtensor<double, 1> MosekSolver<dim, SolverType>::createVectorDistances(std::vector<scopi::neighbor<dim>>& contacts)
      {
          // fill vector with distances
          xt::xtensor<double, 1> distances = xt::zeros<double>({contacts.size()});
          for(std::size_t i=0; i<contacts.size(); ++i)
          {
              distances[i] = contacts[i].dij;
          }
          // std::cout << "distances " << distances << std::endl;
          return distances;
      }

  template<std::size_t dim, typename SolverType>
      matrixType<SolverType> MosekSolver<dim, SolverType>::createMatrixA_mosek(std::vector<scopi::neighbor<dim>>& contacts)
      {
          // Preallocate
          std::vector<int> A_rows;
          std::vector<int> A_cols;
          std::vector<double> A_values;

          createMatrixA_coo(contacts, A_rows, A_cols, A_values, 1);

          return Matrix::sparse(contacts.size(), 1 + 6*_Nactive + 6*_Nactive,
                  std::make_shared<ndarray<int, 1>>(A_rows.data(), shape_t<1>({A_rows.size()})),
                  std::make_shared<ndarray<int, 1>>(A_cols.data(), shape_t<1>({A_cols.size()})),
                  std::make_shared<ndarray<double, 1>>(A_values.data(), shape_t<1>({A_values.size()})));
      }

  template<std::size_t dim, typename SolverType> template<typename ptrType>
      void MosekSolver<dim, SolverType>::createMatrixA_cscStorage(std::vector<scopi::neighbor<dim>>& contacts, std::vector<ptrType>& csc_col, std::vector<ptrType>& csc_row, std::vector<double>& csc_val)
      {
          // COO storage to CSR storage is easy to write, e.g.
          // https://www-users.cse.umn.edu/~saad/software/SPARSKIT/
          // The CSC storage of A is the CSR storage of A^T
          // reverse the role of row and column pointers to have the transpose
          std::vector<int> coo_rows;
          std::vector<int> coo_cols;
          std::vector<double> coo_values;
          createMatrixA_coo(contacts, coo_rows, coo_cols, coo_values, 0);

          std::size_t nrow = 6*_Nactive;
          std::size_t nnz = coo_values.size();
          csc_col.resize(nrow+1);
          std::fill(csc_col.begin(), csc_col.end(), 0.);
          csc_row.resize(nnz);
          csc_val.resize(nnz);

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
      }

  template<std::size_t dim, typename SolverType>
      matrixType<SolverType> MosekSolver<dim, SolverType>::createMatrixA_scs(std::vector<scopi::neighbor<dim>>& contacts)
      {
          std::vector<int> csc_rows;
          std::vector<int> csc_cols;
          std::vector<double> csc_values;

          createMatrixA_cscStorage(contacts, csc_cols, csc_rows, csc_values);

          ScsMatrix* A = new ScsMatrix;
          A->x = new double[csc_values.size()];
          A->i = new scs_int[csc_rows.size()];
          A->p = new scs_int[csc_cols.size()];
          for(std::size_t i = 0; i < csc_values.size(); ++i)
              A->x[i] = csc_values[i];
          for(std::size_t i = 0; i < csc_rows.size(); ++i)
              A->i[i] = csc_rows[i];
          for(std::size_t i = 0; i < csc_cols.size(); ++i)
              A->p[i] = csc_cols[i];
          A->m = contacts.size();
          A->n = 6*_Nactive;
          return A;
      }


  template<std::size_t dim, typename SolverType>
      void MosekSolver<dim, SolverType>::createMatrixA_coo(std::vector<scopi::neighbor<dim>>& contacts, std::vector<int>& A_rows, std::vector<int>& A_cols, std::vector<double>& A_values, std::size_t firstCol)
      {
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
                  if (c.i >= _active_ptr)
                  {
                      A_rows.push_back(ic);
                      A_cols.push_back(firstCol + (c.i - _active_ptr)*3 + d);
                      A_values.push_back(-_dt*c.nij[d]);
                  }
                  if (c.j >= _active_ptr)
                  {
                      A_rows.push_back(ic);
                      A_cols.push_back(firstCol + (c.j - _active_ptr)*3 + d);
                      A_values.push_back(_dt*c.nij[d]);
                  }
              }

              auto r_i = c.pi - _particles.pos()(c.i);
              auto r_j = c.pj - _particles.pos()(c.j);

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

              auto Ri = scopi::rotation_matrix<3>(_particles.q()(c.i));
              auto Rj = scopi::rotation_matrix<3>(_particles.q()(c.j));

              if (c.i >= _active_ptr)
              {
                  std::size_t ind_part = c.i - _active_ptr;
                  auto dot = xt::eval(xt::linalg::dot(ri_cross, Ri));
                  for (std::size_t ip=0; ip<3; ++ip)
                  {
                      A_rows.push_back(ic);
                      A_cols.push_back(firstCol + 3*_Nactive + 3*ind_part + ip);
                      A_values.push_back(_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip)));
                  }
              }

              if (c.j >= _active_ptr)
              {
                  std::size_t ind_part = c.j - _active_ptr;
                  auto dot = xt::eval(xt::linalg::dot(rj_cross, Rj));
                  for (std::size_t ip=0; ip<3; ++ip)
                  {
                      A_rows.push_back(ic);
                      A_cols.push_back(firstCol + 3*_Nactive + 3*ind_part + ip);
                      A_values.push_back(-_dt*(c.nij[0]*dot(0, ip)+c.nij[1]*dot(1, ip)+c.nij[2]*dot(2, ip)));
                  }
              }

              ++ic;
          }
      }

  template<std::size_t dim, typename SolverType>
      matrixType<SolverType> MosekSolver<dim, SolverType>::createMatrixAz_mosek()
      {
          std::vector<int> Az_rows;
          std::vector<int> Az_cols;
          std::vector<double> Az_values;

          Az_rows.reserve(6*_Nactive*2);
          Az_cols.reserve(6*_Nactive*2);
          Az_values.reserve(6*_Nactive*2);

          for (std::size_t i=0; i<_Nactive; ++i)
          {
              for (std::size_t d=0; d<2; ++d)
              {
                  Az_rows.push_back(3*i + d);
                  Az_cols.push_back(1 + 3*i + d);
                  Az_values.push_back(std::sqrt(_mass)); // TODO: add mass into particles
                  // }
                  // for (std::size_t d=0; d<3; ++d)
                  // {
                  Az_rows.push_back(3*i + d);
                  Az_cols.push_back(1 + 6*_Nactive + 3*i + d);
                  Az_values.push_back(-1.);
          }

          // for (std::size_t d=0; d<3; ++d)
          // {
          //     Az_rows.push_back(3*_Nactive + 3*i + d);
          //     Az_cols.push_back(1 + 3*_Nactive + 3*i + d);
          //     Az_values.push_back(std::sqrt(moment));
          // }
          Az_rows.push_back(3*_Nactive + 3*i + 2);
          Az_cols.push_back(1 + 3*_Nactive + 3*i + 2);
          Az_values.push_back(std::sqrt(_moment));

          // for (std::size_t d=0; d<3; ++d)
          // {
          //     Az_rows.push_back(3*_Nactive + 3*i + d);
          //     Az_cols.push_back( 1 + 6*_Nactive + 3*_Nactive + 3*i + d);
          //     Az_values.push_back(-1);
          // }
          Az_rows.push_back(3*_Nactive + 3*i + 2);
          Az_cols.push_back( 1 + 6*_Nactive + 3*_Nactive + 3*i + 2);
          Az_values.push_back(-1);
          }

          return Matrix::sparse(6*_Nactive, 1 + 6*_Nactive + 6*_Nactive,
                  std::make_shared<ndarray<int, 1>>(Az_rows.data(), shape_t<1>({Az_rows.size()})),
                  std::make_shared<ndarray<int, 1>>(Az_cols.data(), shape_t<1>({Az_cols.size()})),
                  std::make_shared<ndarray<double, 1>>(Az_values.data(), shape_t<1>({Az_values.size()})));
      }

  template<std::size_t dim, typename SolverType> template<typename ptrType>
      void MosekSolver<dim, SolverType>::createMatrixP_cscStorage(std::vector<ptrType>& col, std::vector<ptrType>& row, std::vector<double>& val)
      {
          row.reserve(6*_Nactive);
          col.reserve(6*_Nactive+1);
          val.reserve(6*_Nactive);

          for (std::size_t i=0; i<_Nactive; ++i)
          {
              for (std::size_t d=0; d<3; ++d)
              {
                  row.push_back(3*i + d);
                  col.push_back(3*i + d);
                  val.push_back(_mass); // TODO: add mass into particles
              }
          }
          for (std::size_t i=0; i<_Nactive; ++i)
          {
              for (std::size_t d=0; d<3; ++d)
              {
                  row.push_back(3*_Nactive + 3*i + d);
                  col.push_back(3*_Nactive + 3*i + d);
                  val.push_back(_moment);
              }
          }
          col.push_back(6*_Nactive);
      }

  template<std::size_t dim, typename SolverType>
      matrixType<SolverType> MosekSolver<dim, SolverType>::createMatrixP_scs()
      {
          std::vector<scs_int> col;
          std::vector<scs_int> row;
          std::vector<scs_float> val;
          createMatrixP_cscStorage(col, row, val);

          ScsMatrix* P = new ScsMatrix;
          P->x = new double[val.size()];
          P->i = new scs_int[row.size()];
          P->p = new scs_int[col.size()];
          for(std::size_t i = 0; i < val.size(); ++i)
              P->x[i] = val[i];
          for(std::size_t i = 0; i < row.size(); ++i)
              P->i[i] = row[i];
          for(std::size_t i = 0; i < col.size(); ++i)
              P->p[i] = col[i];
          P->m = 6*_Nactive;
          P->n = 6*_Nactive;
          return P;
      }

  template<std::size_t dim, typename SolverType>
      std::vector<double> MosekSolver<dim, SolverType>::createMatricesAndSolve(std::vector<scopi::neighbor<dim>>& contacts, std::size_t nite, useMosekSolver)
      {
          // create mass and inertia matrices
          std::cout << "----> create mass and inertia matrices " << nite << std::endl;
          tic();
          auto c = createVectorC_mosek();
          auto distances = createVectorDistances(contacts);
          auto A = createMatrixA_mosek(contacts);
          auto Az = createMatrixAz_mosek();

          auto duration4 = toc();
          std::cout << "----> CPUTIME : matrices = " << duration4 << std::endl;

          std::cout << "----> Create Mosek optimization problem " << nite << std::endl;
          tic();
          Model::t model = new Model("contact"); auto _M = finally([&]() { model->dispose(); });
          // variables
          Variable::t X = model->variable("X", 1 + 6*_Nactive + 6*_Nactive);

          // functional to minimize
          auto c_mosek = std::make_shared<ndarray<double, 1>>(c.data(), shape_t<1>({c.shape(0)}));
          model->objective("minvar", ObjectiveSense::Minimize, Expr::dot(c_mosek, X));

          // constraints
          auto D_mosek = std::make_shared<ndarray<double, 1>>(distances.data(), shape_t<1>({distances.shape(0)}));

          Constraint::t qc1 = model->constraint("qc1", Expr::mul(A, X), Domain::lessThan(D_mosek));
          Constraint::t qc2 = model->constraint("qc2", Expr::mul(Az, X), Domain::equalsTo(0.));
          Constraint::t qc3 = model->constraint("qc3", Expr::vstack(1, X->index(0), X->slice(1 + 6*_Nactive, 1 + 6*_Nactive + 6*_Nactive)), Domain::inRotatedQCone());
          // model->setSolverParam("intpntCoTolPfeas", 1e-10);
          // model->setSolverParam("intpntTolPfeas", 1.e-10);

          // model->setSolverParam("intpntCoTolDfeas", 1e-6);
          // model->setLogHandler([](const std::string & msg) { std::cout << msg << std::flush; } );
          //solve
          model->solve();

          auto duration5 = toc();
          std::cout << "----> CPUTIME : mosek = " << duration5 << std::endl;
          std::cout << "Mosek iterations : " << model->getSolverIntInfo("intpntIter") << std::endl;

          auto Xlvl = *(X->level());

          std::vector<double> uw(Xlvl.raw()+1,Xlvl.raw()+1 + 6*_Nactive);

          auto dual = *(qc1->dual());
          int nbActiveContatcs = 0;
          for(auto x : dual) 
          {
              if(std::abs(x) > 1e-3)
                  nbActiveContatcs++;
          }
          std::cout << "Contacts: " << contacts.size() << "  active contacts " << nbActiveContatcs << std::endl;

          return uw;

      }

  template<std::size_t dim, typename SolverType>
      std::vector<double> MosekSolver<dim, SolverType>::createMatricesAndSolve(std::vector<scopi::neighbor<dim>>& contacts, std::size_t nite, useScsSolver)
      {
          // create mass and inertia matrices
          std::cout << "----> create mass and inertia matrices " << nite << std::endl;
          tic();
          auto A = createMatrixA_scs(contacts);
          auto P = createMatrixP_scs();
          auto c = createVectorC_scs();
          auto distances = createVectorDistances(contacts);
          xt::xtensor<double, 1> b = xt::zeros<double>({6*_Nactive});

          auto duration4 = toc();
          std::cout << "----> CPUTIME : matrices = " << duration4 << std::endl;

          std::cout << "----> Create Mosek optimization problem " << nite << std::endl;
          tic();
          ScsData d;
          d.m = contacts.size();
          d.n = 6*_Nactive;
          d.A = A;
          d.P = P;
          d.b = distances.data();
          d.c = c.data();

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
          
          auto duration5 = toc();
          std::cout << "----> CPUTIME : SCS = " << duration5 << std::endl;
          std::cout << "SCS iterations : " << info.iter << std::endl;
          // if(info.iter == -1)
          //     std::abort();

          std::vector<double> uw (sol.x, sol.x + 6*_Nactive);
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
          delete[] d.A->x;
          delete[] d.A->i;
          delete[] d.A->p;
          delete d.A;
          delete[] d.P->x;
          delete[] d.P->i;
          delete[] d.P->p;
          delete d.P;
          delete[] sol.x;
          delete[] sol.y;
          delete[] sol.s;

          return uw;
      }

  template<std::size_t dim, typename SolverType>
      void MosekSolver<dim, SolverType>::moveActiveParticles(std::vector<double> uw)
      {

          auto uadapt = xt::adapt(uw.data(), {_Nactive, 3UL});
          auto wadapt = xt::adapt(uw.data()+3*_Nactive, {_Nactive, 3UL});

          for (std::size_t i=0; i<_Nactive; ++i)
          {
              xt::xtensor_fixed<double, xt::xshape<3>> w({0, 0, wadapt(i, 2)});
              double normw = xt::linalg::norm(w);
              if (normw == 0)
              {
                  normw = 1;
              }
              scopi::type::quaternion expw;
              expw(0) = std::cos(0.5*normw*_dt);
              xt::view(expw, xt::range(1, _)) = std::sin(0.5*normw*_dt)/normw*w;
              for (std::size_t d=0; d<dim; ++d)
              {
                  _particles.pos()(i + _active_ptr)(d) += _dt*uadapt(i, d);
              }
              // xt::view(particles.pos(), i) += dt*xt::view(uadapt, i);

              // particles.q()(i) = scopi::quaternion(theta(i));
              // std::cout << expw << " " << particles.q()(i) << std::endl;
              _particles.q()(i + _active_ptr) = scopi::mult_quaternion(_particles.q()(i + _active_ptr), expw);
              normalize(_particles.q()(i + _active_ptr));
              // std::cout << "position" << particles.pos()(i) << std::endl << std::endl;
              // std::cout << "quaternion " << particles.q()(i) << std::endl << std::endl;

          }
      }
}

