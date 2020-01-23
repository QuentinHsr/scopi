#define FORCE_IMPORT_ARRAY                // numpy C api loading
#include "xtensor-python/pyarray.hpp"     // Numpy bindings
#include "xtensor-python/pytensor.hpp"    // Numpy bindings

#include "fm/fm.hpp"

double eikonal(
  std::size_t i,
  std::size_t j,
  std::size_t k,
  double h,
  double infval,
  xt::pyarray<double>& img
) {
  //  | grad(T) | = 1/F where F is the speed (1 by default)
  //
  //      B   N
  //       \  |
  //        \ |
  //  W ----- M ----- E
  //          | \
  //          |  \
  //          S   F
  //
  //   max( (T_M-T_W)/h, (T_M-T_E)/h, 0 )^2
  // + max( (T_M-T_B)/h, (T_M-T_F)/h, 0 )^2
  // + max( (T_M-T_S)/h, (T_M-T_N)/h, 0 )^2
  // = ( 1/F_M )^2

  std::size_t nx = img.shape()[0];
  std::size_t ny = img.shape()[1];
  std::size_t nz = img.shape()[2];

  // M = (i,j,z) supposed into the domain
  //double F_M = 1; // all speeds = 1...
  //double F2_M = F_M*F_M;
  // double T_M = img(i,j,k);
  double T_W, T_E, T_S, T_N, T_B, T_F;
  i>0  ? T_W = img(i-1,j,k) : T_W = infval;
  i<nx-1 ? T_E = img(i+1,j,k) : T_E = infval;
  j>0  ? T_B = img(i,j-1,k) : T_B = infval;
  j<ny-1 ? T_F = img(i,j+1,k) : T_F = infval;
  k>0  ? T_S = img(i,j,k-1) : T_S = infval;
  k<nz-1 ? T_N = img(i,j,k+1) : T_N = infval;
  // std::cout<<"nx="<<nx<<" ny="<<ny<<" nz="<<nz<<std::endl;
  // std::cout<<"i="<<i<<" j="<<j<<" k="<<k<<std::endl;
  // std::cout<<"     ----> ijk = "<<i<<","<<j<<","<<k<<" => T_M = "<<T_M<<" T_W = "<<T_W<<" T_E = "<<T_E
  //          <<" T_B = "<<T_B<<" T_F = "<<T_F<<" T_S = "<<T_S
  //          <<" T_N = "<<T_N<<std::endl;
  double T_WE = std::min(T_W,T_E);
  double T_BF = std::min(T_B,T_F);
  double T_SN = std::min(T_S,T_N);
  // std::cout<<"T_WE="<<T_WE<<" T_BF="<<T_BF<<" T_SN="<<T_SN<<std::endl;
  if (T_WE>=infval){
    if (T_BF>=infval){
      // if (T_SN>=infval){ // USEFUL FOR DEBUG BUT NEVER OCCUR IN PRODUCTION :-)
      //   std::cout<<"-- C++ -- eikonal : case that should never happen..."<<std::endl; // T_WE = infval   T_BF = infval   T_SN = infval
      //   exit(EXIT_FAILURE);
      // }
      // else{
      // T_WE = infval   T_BF = infval   T_SN = X
      //return h/F_M+T_SN;
      return h+T_SN;
      // }
    }
    else{
      if (T_SN>=infval){
        // T_WE = infval   T_BF = X   T_SN = infval
        // return h/F_M+T_BF;
        return h+T_BF;
      }
      else{
        // T_WE = infval   T_BF = X   T_SN = X
        // return 0.5*( T_SN+T_BF+std::sqrt(2*h2/F2_M-(T_SN-T_BF)*(T_SN-T_BF)) );
        return 0.5*( T_SN+T_BF+std::sqrt(2*h*h-(T_SN-T_BF)*(T_SN-T_BF)) );
      }
    }
  }
  else{
    if (T_BF>=infval){
      if (T_SN>=infval){
        // T_WE = X   T_BF = infval   T_SN = infval
        // return h/F_M+T_WE;
        return h+T_WE;
      }
      else{
        // T_WE = X   T_BF = infval   T_SN = X
        // return 0.5*( T_WE+T_SN+std::sqrt(2*h2/F2_M-(T_WE-T_SN)*(T_WE-T_SN)) );
        return 0.5*( T_WE+T_SN+std::sqrt(2*h*h-(T_WE-T_SN)*(T_WE-T_SN)) );
      }
    }
    else{
      if (T_SN>=infval){
        // T_WE = X   T_BF = X   T_SN = infval
        // return 0.5*( T_WE+T_BF+std::sqrt(2*h2/F2_M-(T_WE-T_BF)*(T_WE-T_BF)) );
        return 0.5*( T_WE+T_BF+std::sqrt(2*h*h-(T_WE-T_BF)*(T_WE-T_BF)) );
      }
      else{
        // T_WE = X   T_BF = X   T_SN = X
        // a*T_M^2+b*T_M+c=0
        // where a=3 b=-2*(T_WE+T_BF+T_SN) c=T_WE*T_WE+T_BF*T_BF+T_SN*T_SN-h*h/(F_M*F_M)
        double b = -(T_WE+T_BF+T_SN);
        // double c = T_WE*T_WE+T_BF*T_BF+T_SN*T_SN-h2/F2_M;
        double c = T_WE*T_WE+T_BF*T_BF+T_SN*T_SN-h*h;
        return (-b+std::sqrt(b*b-3*c))/3;
      }
    }
  }
}

xt::pyarray<double> compute_distance(
  double h,
  double infval,
  xt::pyarray<double>& img,
  xt::pyarray<std::size_t>& narrow_band
) {

  std::cout<<"-- C++ -- FM : img.shape = "<<img.shape()[0]<<","<<img.shape()[1]<<","<<img.shape()[2]<<std::endl;
  std::cout<<"-- C++ -- FM : narrow_band.shape = "<<narrow_band.shape()[0]<<","<<narrow_band.shape()[1]<<std::endl;

  std::size_t nx = img.shape()[0];
  std::size_t ny = img.shape()[1];
  std::size_t nz = img.shape()[2];

  // Create and initialize the heap with cells which contain 0
  Heap* myheap = new Heap();

  for (std::size_t ic = 0 ; ic < narrow_band.shape()[0] ; ic++){
    std::size_t i = narrow_band(ic,0);
    std::size_t j = narrow_band(ic,1);
    std::size_t k = narrow_band(ic,2);
    // std::cout<<"INIT point IN narrow band : "<<i<<","<<j<<","<<k<<" ic = "<<ic<<" distance = "<<img(i, j, k)<<std::endl;
    if (i>0){
      if (img(i-1, j, k)==infval) {
        double d = eikonal(i-1, j, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i-1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i-1, j, k );
        img(i-1, j, k) = 10*infval; // pour ne pas le mettre 2 fois dans le tas...
      }
    }
    if (i<nx-1){
      if (img(i+1, j, k)==infval) {
        double d = eikonal(i+1, j, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i+1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i+1, j, k );
        img(i+1, j, k) = 10*infval;
      }
    }
    if (j>0){
      if (img(i, j-1, k)==infval) {
        double d = eikonal(i, j-1, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j-1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j-1, k );
        img(i, j-1, k) = 10*infval;
      }
    }
    if (j<ny-1){
      if (img(i, j+1, k)==infval) {
        double d = eikonal(i, j+1, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j+1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j+1, k );
        img(i, j+1, k) = 10*infval;
      }
    }
    if (k>0){
      if (img(i, j, k-1)==infval) {
        double d = eikonal(i, j, k-1, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j<<","<<k-1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k-1 );
        img(i, j, k-1) = 10*infval;
      }
    }
    if (k<nz-1){
      if (img(i, j, k+1)==infval) {
        double d = eikonal(i, j, k+1, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j<<","<<k+1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k+1 );
        img(i, j, k+1) = 10*infval;
      }
    }
  }

  // std::cout<<"print heap : "<<std::endl;
  // myheap->print();

  std::size_t heap_size = 1000;
  while (heap_size > 0){
    // std::cout<<"heap_size : "<<heap_size<<std::endl;
    addist record=myheap->deletemin();
    std::size_t i = record.i;
    std::size_t j = record.j;
    std::size_t k = record.k;
    img(i,j,k) = record.dist;
    // std::cout<<"Computed distance = "<<record.dist<<" ijk = "<<i<<","<<j<<","<<k<<std::endl;
    if (i>0){
      if (img(i-1, j, k)==infval) {
        double d = eikonal(i-1, j, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i-1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i-1, j, k );
        img(i-1, j, k) = 10*infval; // pour ne pas le mettre 2 fois dans le tas...
      }
    }
    if (i<nx-1){
      if (img(i+1, j, k)==infval) {
        double d = eikonal(i+1, j, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i+1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i+1, j, k );
        img(i+1, j, k) = 10*infval;
      }
    }
    if (j>0){
      if (img(i, j-1, k)==infval) {
        double d = eikonal(i, j-1, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j-1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j-1, k );
        img(i, j-1, k) = 10*infval;
      }
    }
    if (j<ny-1){
      if (img(i, j+1, k)==infval) {
        double d = eikonal(i, j+1, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j+1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j+1, k );
        img(i, j+1, k) = 10*infval;
      }
    }
    if (k>0){
      if (img(i, j, k-1)==infval) {
        double d = eikonal(i, j, k-1, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j<<","<<k-1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k-1 );
        img(i, j, k-1) = 10*infval;
      }
    }
    if (k<nz-1){
      if (img(i, j, k+1)==infval) {
        double d = eikonal(i, j, k+1, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j<<","<<k+1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k+1 );
        img(i, j, k+1) = 10*infval;
      }
    }

    heap_size = myheap -> size();
    //std::cout<<"heap_size = "<<heap_size<<std::endl;
    //exit(EXIT_FAILURE);
  }

  return img;
}


double xt_eikonal(
  std::size_t i,
  std::size_t j,
  std::size_t k,
  double h,
  double infval,
  xt::xarray<double>& img
) {
  //  | grad(T) | = 1/F where F is the speed (1 by default)
  //
  //      B   N
  //       \  |
  //        \ |
  //  W ----- M ----- E
  //          | \
  //          |  \
  //          S   F
  //
  //   max( (T_M-T_W)/h, (T_M-T_E)/h, 0 )^2
  // + max( (T_M-T_B)/h, (T_M-T_F)/h, 0 )^2
  // + max( (T_M-T_S)/h, (T_M-T_N)/h, 0 )^2
  // = ( 1/F_M )^2

  std::size_t nx = img.shape()[0];
  std::size_t ny = img.shape()[1];
  std::size_t nz = img.shape()[2];

  // M = (i,j,z) supposed into the domain
  //double F_M = 1; // all speeds = 1...
  //double F2_M = F_M*F_M;
  // double T_M = img(i,j,k);
  double T_W, T_E, T_S, T_N, T_B, T_F;
  i>0  ? T_W = img(i-1,j,k) : T_W = infval;
  i<nx-1 ? T_E = img(i+1,j,k) : T_E = infval;
  j>0  ? T_B = img(i,j-1,k) : T_B = infval;
  j<ny-1 ? T_F = img(i,j+1,k) : T_F = infval;
  k>0  ? T_S = img(i,j,k-1) : T_S = infval;
  k<nz-1 ? T_N = img(i,j,k+1) : T_N = infval;
  // std::cout<<"nx="<<nx<<" ny="<<ny<<" nz="<<nz<<std::endl;
  // std::cout<<"i="<<i<<" j="<<j<<" k="<<k<<std::endl;
  // std::cout<<"     ----> ijk = "<<i<<","<<j<<","<<k<<" => T_M = "<<T_M<<" T_W = "<<T_W<<" T_E = "<<T_E
  //          <<" T_B = "<<T_B<<" T_F = "<<T_F<<" T_S = "<<T_S
  //          <<" T_N = "<<T_N<<std::endl;
  double T_WE = std::min(T_W,T_E);
  double T_BF = std::min(T_B,T_F);
  double T_SN = std::min(T_S,T_N);
  // std::cout<<"T_WE="<<T_WE<<" T_BF="<<T_BF<<" T_SN="<<T_SN<<std::endl;
  if (T_WE>=infval){
    if (T_BF>=infval){
      // if (T_SN>=infval){ // USEFUL FOR DEBUG BUT NEVER OCCUR IN PRODUCTION :-)
      //   std::cout<<"-- C++ -- eikonal : case that should never happen..."<<std::endl; // T_WE = infval   T_BF = infval   T_SN = infval
      //   exit(EXIT_FAILURE);
      // }
      // else{
      // T_WE = infval   T_BF = infval   T_SN = X
      //return h/F_M+T_SN;
      return h+T_SN;
      // }
    }
    else{
      if (T_SN>=infval){
        // T_WE = infval   T_BF = X   T_SN = infval
        // return h/F_M+T_BF;
        return h+T_BF;
      }
      else{
        // T_WE = infval   T_BF = X   T_SN = X
        // return 0.5*( T_SN+T_BF+std::sqrt(2*h2/F2_M-(T_SN-T_BF)*(T_SN-T_BF)) );
        return 0.5*( T_SN+T_BF+std::sqrt(2*h*h-(T_SN-T_BF)*(T_SN-T_BF)) );
      }
    }
  }
  else{
    if (T_BF>=infval){
      if (T_SN>=infval){
        // T_WE = X   T_BF = infval   T_SN = infval
        // return h/F_M+T_WE;
        return h+T_WE;
      }
      else{
        // T_WE = X   T_BF = infval   T_SN = X
        // return 0.5*( T_WE+T_SN+std::sqrt(2*h2/F2_M-(T_WE-T_SN)*(T_WE-T_SN)) );
        return 0.5*( T_WE+T_SN+std::sqrt(2*h*h-(T_WE-T_SN)*(T_WE-T_SN)) );
      }
    }
    else{
      if (T_SN>=infval){
        // T_WE = X   T_BF = X   T_SN = infval
        // return 0.5*( T_WE+T_BF+std::sqrt(2*h2/F2_M-(T_WE-T_BF)*(T_WE-T_BF)) );
        return 0.5*( T_WE+T_BF+std::sqrt(2*h*h-(T_WE-T_BF)*(T_WE-T_BF)) );
      }
      else{
        // T_WE = X   T_BF = X   T_SN = X
        // a*T_M^2+b*T_M+c=0
        // where a=3 b=-2*(T_WE+T_BF+T_SN) c=T_WE*T_WE+T_BF*T_BF+T_SN*T_SN-h*h/(F_M*F_M)
        double b = -(T_WE+T_BF+T_SN);
        // double c = T_WE*T_WE+T_BF*T_BF+T_SN*T_SN-h2/F2_M;
        double c = T_WE*T_WE+T_BF*T_BF+T_SN*T_SN-h*h;
        return (-b+std::sqrt(b*b-3*c))/3;
      }
    }
  }
}

xt::xarray<double> xt_compute_distance(
  double h, double infval,
  xt::xarray<double>& img,
  xt::xarray<std::size_t>& narrow_band
) {

  std::cout<<"-- C++ -- FM : img.shape = "<<img.shape()[0]<<","<<img.shape()[1]<<","<<img.shape()[2]<<std::endl;
  std::cout<<"-- C++ -- FM : narrow_band.shape = "<<narrow_band.shape()[0]<<","<<narrow_band.shape()[1]<<std::endl;

  std::size_t nx = img.shape()[0];
  std::size_t ny = img.shape()[1];
  std::size_t nz = img.shape()[2];


  // Create and initialize the heap with cells which contain 0
  Heap* myheap = new Heap();

  for (std::size_t ic = 0 ; ic < narrow_band.shape()[0] ; ic++){
    std::size_t i = narrow_band(ic,0);
    std::size_t j = narrow_band(ic,1);
    std::size_t k = narrow_band(ic,2);
    // std::cout<<"INIT point IN narrow band : "<<i<<","<<j<<","<<k<<" ic = "<<ic<<" distance = "<<img(i, j, k)<<std::endl;
    if (i>0){
      if (img(i-1, j, k)==infval) {
        double d = xt_eikonal(i-1, j, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i-1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i-1, j, k );
        img(i-1, j, k) = 10*infval; // pour ne pas le mettre 2 fois dans le tas...
      }
    }
    if (i<nx-1){
      if (img(i+1, j, k)==infval) {
        double d = xt_eikonal(i+1, j, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i+1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i+1, j, k );
        img(i+1, j, k) = 10*infval;
      }
    }
    if (j>0){
      if (img(i, j-1, k)==infval) {
        double d = xt_eikonal(i, j-1, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j-1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j-1, k );
        img(i, j-1, k) = 10*infval;
      }
    }
    if (j<ny-1){
      if (img(i, j+1, k)==infval) {
        double d = xt_eikonal(i, j+1, k, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j+1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j+1, k );
        img(i, j+1, k) = 10*infval;
      }
    }
    if (k>0){
      if (img(i, j, k-1)==infval) {
        double d = xt_eikonal(i, j, k-1, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j<<","<<k-1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k-1 );
        img(i, j, k-1) = 10*infval;
      }
    }
    if (k<nz-1){
      if (img(i, j, k+1)==infval) {
        double d = xt_eikonal(i, j, k+1, h, infval, img);
        // std::cout<<"INIT ---->  ADD in narrow band : "<<i<<","<<j<<","<<k+1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k+1 );
        img(i, j, k+1) = 10*infval;
      }
    }
  }

  // std::cout<<"print heap : "<<std::endl;
  // myheap->print();

  std::size_t heap_size = 1000;
  while (heap_size > 0){
    // std::cout<<"heap_size : "<<heap_size<<std::endl;
    addist record=myheap->deletemin();
    std::size_t i = record.i;
    std::size_t j = record.j;
    std::size_t k = record.k;
    img(i,j,k) = record.dist;
    // std::cout<<"Computed distance = "<<record.dist<<" ijk = "<<i<<","<<j<<","<<k<<std::endl;
    if (i>0){
      if (img(i-1, j, k)==infval) {
        double d = xt_eikonal(i-1, j, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i-1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i-1, j, k );
        img(i-1, j, k) = 10*infval; // pour ne pas le mettre 2 fois dans le tas...
      }
    }
    if (i<nx-1){
      if (img(i+1, j, k)==infval) {
        double d = xt_eikonal(i+1, j, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i+1<<","<<j<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i+1, j, k );
        img(i+1, j, k) = 10*infval;
      }
    }
    if (j>0){
      if (img(i, j-1, k)==infval) {
        double d = xt_eikonal(i, j-1, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j-1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j-1, k );
        img(i, j-1, k) = 10*infval;
      }
    }
    if (j<ny-1){
      if (img(i, j+1, k)==infval) {
        double d = xt_eikonal(i, j+1, k, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j+1<<","<<k<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j+1, k );
        img(i, j+1, k) = 10*infval;
      }
    }
    if (k>0){
      if (img(i, j, k-1)==infval) {
        double d = xt_eikonal(i, j, k-1, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j<<","<<k-1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k-1 );
        img(i, j, k-1) = 10*infval;
      }
    }
    if (k<nz-1){
      if (img(i, j, k+1)==infval) {
        double d = xt_eikonal(i, j, k+1, h, infval, img);
        // std::cout<<" ----> ADD in narrow band : "<<i<<","<<j<<","<<k+1<<" d = "<<d<<std::endl;
        myheap->insert( d, i, j, k+1 );
        img(i, j, k+1) = 10*infval;
      }
    }

    heap_size = myheap -> size();
    //std::cout<<"heap_size = "<<heap_size<<std::endl;
    //exit(EXIT_FAILURE);
  }

  return img;
}


template <class E>
void test(
  xt::xexpression<E>& xd
) {
  auto& d = xd.derived_cast();
}



PYBIND11_MODULE(fm, m)
{
  xt::import_numpy();
  m.doc() = "Module for the fast-marching method";

  m.def("compute_distance",
  compute_distance,
  "Solve the Eikonale equation by a Fast-Marching method to calculate the shortest distance to a goal (pixels=0)");

  m.def("test",
  test< xt::pyarray<double> >, // cas avec template + xt::xexpression
  "Test xt::xexpression in order to pass pyarray from python or xarray from C++...");

}
