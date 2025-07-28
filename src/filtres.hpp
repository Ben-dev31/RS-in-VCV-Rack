
#ifndef FILTRES_HPP
#define FILTRES_HPP

double multi_well_potential(double x, int N, double Xb);
double multi_well_grad(double x, int N, double Xb) ;
float diode(float x, float th);
float bistableFilter(float xi, float si, float ni, float dt, float tau, float Xb);
float bistablePotential(float x, float th);
float rubber(float x, float th);
float multiWellFilter(float xi, float si, float ni, float dt,float tau, int N, double Xb);

#endif // FILTRES_HPP
      