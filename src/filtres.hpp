
#ifndef FILTRES_HPP
#define FILTRES_HPP

float multi_well_potential(float x, int N, float Xb);
float multi_well_grad(float x, int N, float Xb) ;
float diode(float x, float th);
float bistableFilter(float xi, float si, float ni, float dt, float tau, float Xb);
float bistablePotential(float x, float th);
float rubber(float x, float th);
float multiWellFilter(float xi, float si, float ni, float dt,float tau, int N, float Xb);

#endif // FILTRES_HPP
      