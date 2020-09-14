// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#include "cold_plasma_dielectric_coefs.hpp"

using namespace std;
namespace mfem
{
using namespace common;

namespace plasma
{

complex<double> R_cold_plasma(double omega,
                              double Bmag,
                              const Vector & number,
                              const Vector & charge,
                              const Vector & mass,
                              const Vector & temp)
{
   complex<double> val(1.0, 0.0);
   complex<double> mass_correction(1.0, 0.0);
   for (int i=1; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      double Te = temp[i] * q_;
      double coul_log = CoulombLog(n, Te);
      double nuei = nu_ei(q, coul_log, m, Te, n);
      complex<double> collision_correction(0, nuei/omega);
      mass_correction += collision_correction;
   }

   for (int i=0; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      complex<double> m_eff = m*mass_correction;
      complex<double> w_c = omega_c(Bmag, q, m_eff);
      complex<double> w_p = omega_p(n, q, m_eff);
      val -= w_p * w_p / (omega * (omega + w_c));
   }
   return val;
}

complex<double> L_cold_plasma(double omega,
                              double Bmag,
                              const Vector & number,
                              const Vector & charge,
                              const Vector & mass,
                              const Vector & temp)
{
   complex<double> val(1.0, 0.0);
   complex<double> mass_correction(1.0, 0.0);
   for (int i=1; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      double Te = temp[i] * q_;
      double coul_log = CoulombLog(n, Te);
      double nuei = nu_ei(q, coul_log, m, Te, n);
      complex<double> collision_correction(0, nuei/omega);
      mass_correction += collision_correction;
   }

   for (int i=0; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      complex<double> m_eff = m*mass_correction;
      complex<double> w_c = omega_c(Bmag, q, m_eff);
      complex<double> w_p = omega_p(n, q, m_eff);
      val -= w_p * w_p / (omega * (omega - w_c));
   }
   return val;
}

complex<double> S_cold_plasma(double omega,
                              double Bmag,
                              const Vector & number,
                              const Vector & charge,
                              const Vector & mass,
                              const Vector & temp)
{
   complex<double> val(1.0, 0.0);
   complex<double> mass_correction(1.0, 0.0);
   for (int i=1; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      double Te = temp[i] * q_;
      double coul_log = CoulombLog(n, Te);
      double nuei = nu_ei(q, coul_log, m, Te, n);
      complex<double> collision_correction(0, nuei/omega);
      mass_correction += collision_correction;
   }

   for (int i=0; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      complex<double> m_eff = m*mass_correction;
      complex<double> w_c = omega_c(Bmag, q, m_eff);
      complex<double> w_p = omega_p(n, q, m_eff);
      val -= w_p * w_p / (omega * omega - w_c * w_c);
   }
   return val;
}

complex<double> D_cold_plasma(double omega,
                              double Bmag,
                              const Vector & number,
                              const Vector & charge,
                              const Vector & mass,
                              const Vector & temp)
{
   complex<double> val(0.0, 0.0);
   complex<double> mass_correction(1.0, 0.0);
   for (int i=1; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      double Te = temp[i] * q_;
      double coul_log = CoulombLog(n, Te);
      double nuei = nu_ei(q, coul_log, m, Te, n);
      complex<double> collision_correction(0, nuei/omega);
      mass_correction += collision_correction;
   }

   for (int i=0; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      complex<double> m_eff = m*mass_correction;
      complex<double> w_c = omega_c(Bmag, q, m_eff);
      complex<double> w_p = omega_p(n, q, m_eff);
      val += w_p * w_p * w_c / (omega *(omega * omega - w_c * w_c));
   }
   return val;
}

complex<double> P_cold_plasma(double omega,
                              const Vector & number,
                              const Vector & charge,
                              const Vector & mass,
                              const Vector & temp)
{
   complex<double> val(1.0, 0.0);
   complex<double> mass_correction(1.0, 0.0);
   for (int i=1; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      double Te = temp[i] * q_;
      double coul_log = CoulombLog(n, Te);
      double nuei = nu_ei(q, coul_log, m, Te, n);
      complex<double> collision_correction(0, nuei/omega);
      mass_correction += collision_correction;
   }

   for (int i=0; i<number.Size(); i++)
   {
      double n = number[i];
      double q = charge[i];
      double m = mass[i];
      complex<double> m_eff = m*mass_correction;
      complex<double> w_p = omega_p(n, q, m_eff);
      val -= w_p * w_p / (omega * omega);
   }
   return val;
}

void real_epsilon_sigma(double omega, const Vector &B,
                        const Vector &density_vals,
                        const Vector &temperature_vals,
                        double *real_epsilon, double *real_sigma)
{
   double Bnorm = B.Norml2();

   // double phi = 0.0;
   // double MData[9] = {cos(phi), 0.0, -sin(phi),
   //                    0.0,      1.0,       0.0,
   //                    sin(phi), 0.0,  cos(phi)
   //                   };
   // DenseMatrix M(MData, 3, 3);

   // Vector Blocal(3);
   // M.Mult(B, Blocal);

   double Z1 = 1.0, Z2 = 18.0;
   double qe = -q_, qi1 = Z1 * q_, qi2 = Z2 * q_;
   double mi1 = 2.01410178 * amu_, mi2 = 39.948 * amu_;
   double ne = density_vals[0], ni1 = density_vals[1], ni2 = density_vals[2];
   /*double Te = temperature_vals[0];
   double Ti1 = temperature_vals[1];
   double Ti2 = temperature_vals[2];
   double vTe = sqrt(2.0 * Te / me_);
   double debye_length = sqrt((epsilon0_ * Te) / (ne * pow(qe, 2)));
   double b90_1 = (qe * qi1)/(4.0 * M_PI * epsilon0_ * me_ * pow(vTe, 2)),
          b90_2 = (qe * qi2)/(4.0 * M_PI * epsilon0_ * me_ * pow(vTe, 2));
   double nu_ei1 = (pow(qe * qi1, 2) * ni1 * log(debye_length / b90_1)) /
          (4.0 * M_PI * pow(epsilon0_ * me_, 2) * pow(Te, 3.0/2.0));
   double nu_ei2 = (pow(qe * qi2, 2) * ni2 * log(debye_length / b90_2)) /
          (4.0 * M_PI * pow(epsilon0_ * me_, 2) * pow(Te, 3.0/2.0));

   // Effective Mass
   complex<double> me_eff(me_, -me_*(nu_ei1/omega + nu_ei2/omega));
   complex<double> mi1_eff(mi1, -mi1*(nu_ei1/omega + nu_ei2/omega));
   complex<double> mi2_eff(mi2, -mi2*(nu_ei1/omega + nu_ei2/omega));
    */

   // Squared plasma frequencies for each species
   double wpe  = (ne  * pow( qe, 2))/(me_kg_ * epsilon0_);
   double wpi1 = (ni1 * pow(qi1, 2))/(mi1 * epsilon0_);
   double wpi2 = (ni2 * pow(qi2, 2))/(mi2 * epsilon0_);

   // Cyclotron frequencies for each species
   double wce  = qe  * Bnorm / me_kg_;
   double wci1 = qi1 * Bnorm / mi1;
   double wci2 = qi2 * Bnorm / mi2;

   double S = (1.0 -
               wpe  / (pow(omega, 2) - pow( wce, 2)) -
               wpi1 / (pow(omega, 2) - pow(wci1, 2)) -
               wpi2 / (pow(omega, 2) - pow(wci2, 2)));
   double P = (1.0 -
               wpe  / pow(omega, 2) -
               wpi1 / pow(omega, 2) -
               wpi2 / pow(omega, 2));
   double D = (wce  * wpe  / (omega * (pow(omega, 2) - pow( wce, 2))) +
               wci1 * wpi1 / (omega * (pow(omega, 2) - pow(wci1, 2))) +
               wci2 * wpi2 / (omega * (pow(omega, 2) - pow(wci2, 2))));

   // Complex Dielectric tensor elements
   double th = atan2(B(2), B(0));
   double ph = atan2(B(0) * cos(th) + B(2) * sin(th), -B(1));

   double e_xx = (P - S) * pow(sin(ph), 2) * pow(cos(th), 2) + S;
   double e_yy = (P - S) * pow(cos(ph), 2) + S;
   double e_zz = (P - S) * pow(sin(ph), 2) * pow(sin(th), 2) + S;

   complex<double> e_xy(-(P - S) * cos(ph) * cos(th) * sin(ph),
                        - D * sin(th) * sin(ph));
   complex<double> e_xz((P - S) * pow(sin(ph), 2) * sin(th) * cos(th),
                        - D * cos(ph));
   complex<double> e_yz(-(P - S) * sin(th) * cos(ph) * sin(ph),
                        - D * cos(th) * sin(ph));

   complex<double> e_yx = std::conj(e_xy);
   complex<double> e_zx = std::conj(e_xz);
   complex<double> e_zy = std::conj(e_yz);

   if (real_epsilon != NULL)
   {
      real_epsilon[0] = epsilon0_ * e_xx;
      real_epsilon[1] = epsilon0_ * e_yx.real();
      real_epsilon[2] = epsilon0_ * e_zx.real();
      real_epsilon[3] = epsilon0_ * e_xy.real();
      real_epsilon[4] = epsilon0_ * e_yy;
      real_epsilon[5] = epsilon0_ * e_zy.real();
      real_epsilon[6] = epsilon0_ * e_xz.real();
      real_epsilon[7] = epsilon0_ * e_yz.real();
      real_epsilon[8] = epsilon0_ * e_zz;
   }
   if (real_sigma != NULL)
   {
      real_sigma[0] = 0.0;
      real_sigma[1] = e_yx.imag() * omega * epsilon0_;
      real_sigma[2] = e_zx.imag() * omega * epsilon0_;
      real_sigma[3] = e_xy.imag() * omega * epsilon0_;
      real_sigma[4] = 0.0;
      real_sigma[5] = e_zy.imag() * omega * epsilon0_;
      real_sigma[6] = e_xz.imag() * omega * epsilon0_;
      real_sigma[7] = e_yz.imag() * omega * epsilon0_;
      real_sigma[8] = 0.0;
   }

}

double mu(double mass_e, double mass_i)
{
   return pow((mass_i * pow(1.66053906, -27.0)) /
              (2.0 * M_PI * mass_e * pow(1.66053906, -27.0)), 0.5);
}

complex<double> ff(complex<double> x)
{
   complex<double> a0(3.18553, 0.0);
   double a1 = 3.70285;
   double a2 = 3.81991;
   double b1 = 1.13352;
   double b2 = 1.24171;
   double a3 = (2.0 * b2) / M_PI;
   complex<double> num = a0 + a1*x + a2*pow(x, 2) + a3*pow(x, 3);
   complex<double> val(1.0, 0);
   complex<double> den = val + b1*x + b2*pow(x, 2);

   return (num / den);
}

double gg(double w)
{
   double c0 = 0.966463;
   double c1 = 0.141639;

   return (c0+c1*tanh(w));
}

complex<double> phi0avg(double w, complex<double> xi)
{
   return (ff(gg(w)*xi));
}

complex<double> he(complex<double> x)
{
   double h1 = 0.607405;
   double h2 = 0.325497;
   double g1 = 0.624392;
   double g2 = 0.500595;
   double g3 = (M_PI * h2) / 4.0;
   complex<double> val(1.0, 0.0);
   complex<double> num = val + h1*x + h2*pow(x, 2);
   complex<double> den = val + g1*x + g2*pow(x, 2) + g3*pow(x, 3);

   return (num/den);
}

double phips(double bx, double wci, double mass_e, double mass_i)
{
   double mu_val = mu(mass_e, mass_i);
   double d3 = 0.995721;
   double arg = sqrt((pow(mu_val, 2.0) * pow(bx, 2.0) + 1.0)/(pow(mu_val,
                                                                  2.0) + 1.0));
   double num = -log(arg);
   double den = 1.0 + d3 * pow(wci, 2.0);

   return (num/den);
}

complex<double> niw(double wci, double bx, complex<double> phi, double mass_e,
                    double mass_i)
{
   double d0 = 0.794443;
   double d1 = 0.803531;
   double d2 = 0.182378;
   double d4 = 0.0000901468;
   double nu1 = 1.455592;
   double abx = abs(bx);
   complex<double> phips1(phips(abx,wci, mass_e, mass_i), 0.0);
   complex<double> phid = phi - phips1;
   complex<double> pre = d0 /(d2 + sqrt(phid));
   complex<double> wcip = wci * pow(phid, 0.25);
   complex<double> num = pow(abx, 2.0) + d4 + pow(d1, 2.0) * pow(wcip, (2.0*nu1));
   complex<double> den = 1.0 + d4 + pow(d1, 2.0) *pow(wcip, (2.0 * nu1));

   return (pre*sqrt(num/den));
}

complex<double> ye(double bx, complex<double> xi)
{
   double h0 = 1.161585;
   double abx = abs(bx);

   return (h0*abx*he(xi));
}

complex<double> niww(double w, double wci, double bx, complex<double> xi,
                     double mass_e, double mass_i)
{
   complex<double> k0(3.7616, 0.0);
   double k1 = 0.22202;
   complex<double> phis = k0 + k1*(xi-k0);
   complex<double> phipr = phis + (phi0avg(w,xi)-phis)*tanh(w);

   return (niw(wci,bx,phipr,mass_e,mass_i));
}

complex<double> yd(double w, double wci, double bx, complex<double> xi,
                   double mass_e, double mass_i)
{
   complex<double>  s0(0.0, 1.12415);
   complex<double> delta = sqrt(phi0avg(w,xi)/niww(w,wci,bx,xi,mass_e,mass_i));

   return (-s0*(w/delta));
}

complex<double> yi(double w, double wci, double bx, complex<double> xi,
                   double mass_e, double mass_i)
{
   complex<double> val(0.0, 1.0);
   double p0 = 1.05554;
   complex<double> p1(0.797659, 0.0);
   double p2 = 1.47405;
   double p3 = 0.809615;
   double eps = 0.0001;
   double gfactornum = pow(w, 2.0) - pow(bx, 2.0) * pow(wci, 2.0) + eps;
   double gfactorden = pow(w, 2.0) - pow(wci, 2.0) + eps;
   complex<double> gfactor(gfactornum/gfactorden, 0.0);
   complex<double> niwwa = niww(w,wci,bx,xi,mass_e,mass_i);
   complex<double> phi0avga = phi0avg(w,xi);
   complex<double> abx(abs(bx), 0.0);
   complex<double> gamcup = abx/(niwwa*sqrt(phi0avga));
   complex<double> wcup = p3*w/sqrt(niwwa);
   complex<double> yicupden1 = pow(wcup, 2.0)/gfactor - p1;
   complex<double> yicupden2 = p2*gamcup*wcup*val;
   complex<double> yicupden = yicupden1 + yicupden2;
   complex<double> yicup = val*p0*wcup/yicupden;

   return ((niwwa * yicup) / sqrt(phi0avga));
}

complex<double> ytot(double w, double wci, double bx, complex<double> xi,
                     double mass_e, double mass_i)
{
   complex<double> ytot = ye(bx,xi) + yd(w,wci,bx,xi,mass_e,mass_i) + yi(w,wci,bx,
                                                                         xi,mass_e,mass_i);
   return (ytot);
}

double debye(double Te, double n0_cm)
{
   return (7.43e2*pow((Te/n0_cm),0.5));
}


SheathImpedance::SheathImpedance(const ParGridFunction & B,
                                 const BlockVector & density,
                                 const BlockVector & temp,
                                 const ParComplexGridFunction & potential,
                                 const ParFiniteElementSpace & L2FESpace,
                                 const ParFiniteElementSpace & H1FESpace,
                                 double omega,
                                 const Vector & charges,
                                 const Vector & masses,
                                 bool realPart)
   : B_(B),
     density_(density),
     temp_(temp),
     potential_(potential),
     L2FESpace_(L2FESpace),
     H1FESpace_(H1FESpace),
     omega_(omega),
     charges_(charges),
     masses_(masses),
     realPart_(realPart)
{
   density_vals_.SetSize(charges_.Size());
   temp_vals_.SetSize(charges_.Size());
}

double SheathImpedance::Eval(ElementTransformation &T,
                             const IntegrationPoint &ip)
{
   // Collect density, temperature, magnetic field, and potential field values
   Vector B(3);
   B_.GetVectorValue(T, ip, B);
   double Bmag = B.Norml2();
    
   double phir = potential_.real().GetValue(T, ip);
   double phii = potential_.imag().GetValue(T, ip);

   for (int i=0; i<density_vals_.Size(); i++)
   {
      density_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&L2FESpace_),
                          const_cast<Vector&>(density_.GetBlock(i)));
      density_vals_[i] = density_gf_.GetValue(T, ip);
   }

   for (int i=0; i<temp_vals_.Size(); i++)
   {
      temperature_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&H1FESpace_),
                              const_cast<Vector&>(temp_.GetBlock(i)));
      temp_vals_[i] = temperature_gf_.GetValue(T, ip);
   }

   double Te = temp_vals_[0] * q_; // Electron temperature, Units: J
   double wci = (charges_[1] * q_ * Bmag) / (masses_[1] * amu_);
   double wpi = fabs(charges_[1] * q_) * 1.0 * sqrt(density_vals_[1] /
                                                    (epsilon0_ * masses_[1] * amu_));
   double vnorm = Te / (charges_[1] * q_);

   double w_norm = omega_ / wpi;
   double wci_norm = wci / wpi;
   complex<double> volt_norm(phir/vnorm, phii/vnorm);
    
   double debye_length = debye(temp_vals_[0],
                               density_vals_[1]*1e-6); // Input temp needs to be in eV, Units: cm

   Vector nor(T.GetSpaceDim());
   CalcOrtho(T.Jacobian(), nor);
   double normag = nor.Norml2();
   double bn = (B * nor)/(normag*Bmag);

   complex<double> zsheath_norm = 1.0 / ytot(w_norm, wci_norm, bn, volt_norm,
                                             masses_[0], masses_[1]);
    
   if (realPart_)
   {
      return (zsheath_norm.real()*9.0*1e11*1e-4*
              (4.0*M_PI*debye_length))/wpi; // Units: Ohm m^2
   }
   else
   {
      return (zsheath_norm.imag()*9.0*1e11*1e-4*
              (4.0*M_PI*debye_length))/wpi; // Units: Ohm m^2
   }
}

DielectricTensor::DielectricTensor(const ParGridFunction & B,
                                   const BlockVector & density,
                                   const BlockVector & temp,
                                   const ParFiniteElementSpace & L2FESpace,
                                   const ParFiniteElementSpace & H1FESpace,
                                   double omega,
                                   const Vector & charges,
                                   const Vector & masses,
                                   bool realPart)
   : MatrixCoefficient(3),
     B_(B),
     density_(density),
     temp_(temp),
     L2FESpace_(L2FESpace),
     H1FESpace_(H1FESpace),
     omega_(omega),
     realPart_(realPart),
     charges_(charges),
     masses_(masses)
{
   density_vals_.SetSize(charges_.Size());
   temp_vals_.SetSize(charges_.Size());
}

void DielectricTensor::Eval(DenseMatrix &epsilon, ElementTransformation &T,
                            const IntegrationPoint &ip)
{
   // Initialize dielectric tensor to appropriate size
   epsilon.SetSize(3);

   // Collect density, temperature, and magnetic field values
   Vector B(3);
   B_.GetVectorValue(T.ElementNo, ip, B);
   double Bmag = B.Norml2();
   double th = atan2(B(2), B(0));
   double ph = atan2(B(0) * cos(th) + B(2) * sin(th), -B(1));

   for (int i=0; i<density_vals_.Size(); i++)
   {
      density_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&L2FESpace_),
                          const_cast<Vector&>(density_.GetBlock(i)));
      density_vals_[i] = density_gf_.GetValue(T.ElementNo, ip);
   }

   for (int i=0; i<temp_vals_.Size(); i++)
   {
      temperature_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&H1FESpace_),
                              const_cast<Vector&>(temp_.GetBlock(i)));
      temp_vals_[i] = temperature_gf_.GetValue(T.ElementNo, ip);
   }

   if (realPart_)
   {
      complex<double> S = S_cold_plasma(omega_, Bmag, density_vals_,
                                        charges_, masses_, temp_vals_);
      complex<double> P = P_cold_plasma(omega_, density_vals_,
                                        charges_, masses_, temp_vals_);
      complex<double> D = D_cold_plasma(omega_, Bmag, density_vals_,
                                        charges_, masses_, temp_vals_);

      epsilon(0,0) =  (real(P) - real(S)) *
                      pow(sin(ph), 2) * pow(cos(th), 2) + real(S);
      epsilon(1,1) =  (real(P) - real(S)) * pow(cos(ph), 2) + real(S);
      epsilon(2,2) =  (real(P) - real(S)) *
                      pow(sin(ph), 2) * pow(sin(th), 2) + real(S);
      epsilon(0,1) = (real(P) - real(S)) * cos(ph) * cos(th) * sin(ph) +
                     imag(D) * sin(th) * sin(ph);
      epsilon(0,2) =  (real(P) - real(S)) *
                      pow(sin(ph), 2) * sin(th) * cos(th) + imag(D) * cos(ph);
      epsilon(1,2) = (real(P) - real(S)) * sin(th) * cos(ph) * sin(ph) -
                     imag(D) * cos(th) * sin(ph);
      epsilon(1,0) = (real(P) - real(S)) * cos(ph) * cos(th) * sin(ph) -
                     imag(D) * sin(th) * sin(ph);
      epsilon(2,1) = (real(P) - real(S)) * sin(th) * cos(ph) * sin(ph) +
                     imag(D) * cos(th) * sin(ph);
      epsilon(2,0) = (real(P) - real(S)) *
                     pow(sin(ph),2) * sin(th) * cos(th) - imag(D) * cos(ph);
   }
   else
   {
      complex<double> S = S_cold_plasma(omega_, Bmag, density_vals_,
                                        charges_, masses_, temp_);
      complex<double> P = P_cold_plasma(omega_, density_vals_,
                                        charges_, masses_, temp_);
      complex<double> D = D_cold_plasma(omega_, Bmag, density_vals_,
                                        charges_, masses_, temp_);

      epsilon(0,0) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * pow(cos(th), 2) + imag(S);
      epsilon(1,1) = (imag(P) - imag(S)) * pow(cos(ph), 2) + imag(S);
      epsilon(2,2) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * pow(sin(th), 2) + imag(S);
      epsilon(0,1) = (imag(P) - imag(S)) * cos(ph) * cos(th) * sin(ph) -
                     real(D) * sin(th) * sin(ph);
      epsilon(0,2) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * sin(th) * cos(th) - real(D) * cos(ph);
      epsilon(1,2) = (imag(P) - imag(S)) * sin(th) * cos(ph) * sin(ph) +
                     real(D) * cos(th) * sin(ph);
      epsilon(1,0) = (imag(P) - imag(S)) * cos(ph) * cos(th) * sin(ph) +
                     real(D) * sin(th) * sin(ph);
      epsilon(2,1) = (imag(P) - imag(S)) * sin(th) * cos(ph) * sin(ph) -
                     real(D) * cos(th) * sin(ph);
      epsilon(2,0) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * sin(th) * cos(th) + real(D) * cos(ph);
   }
   epsilon *= epsilon0_;
   /*
   Vector lambda(3);
   epsilon.Eigenvalues(lambda);
   if (realPart_)
      cout << "Dielectric tensor eigenvalues: "
           << lambda[0] << " " << lambda[1] << " " << lambda[2]
      << " for B " << B[0] << " " << B[1] << " " << B[2]
      << " and rho " << density_vals_[0]
      << " " << density_vals_[1] << " " << density_vals_[2]
      << endl;
   else
      cout << "Conductivity tensor eigenvalues: "
           << lambda[0] << " " << lambda[1] << " " << lambda[2] << endl;
   */
}

SPDDielectricTensor::SPDDielectricTensor(
   const ParGridFunction & B,
   const BlockVector & density,
   const BlockVector & temp,
   const ParFiniteElementSpace & L2FESpace,
   const ParFiniteElementSpace & H1FESpace,
   double omega,
   const Vector & charges,
   const Vector & masses)
   : MatrixCoefficient(3),
     B_(B),
     density_(density),
     temp_(temp),
     L2FESpace_(L2FESpace),
     H1FESpace_(H1FESpace),
     omega_(omega),
     charges_(charges),
     masses_(masses)
{
   density_vals_.SetSize(charges_.Size());
   temp_vals_.SetSize(charges_.Size());
}

void SPDDielectricTensor::Eval(DenseMatrix &epsilon, ElementTransformation &T,
                               const IntegrationPoint &ip)
{
   // Initialize dielectric tensor to appropriate size
   epsilon.SetSize(3);

   // Collect density, temperature, and magnetic field values
   Vector B(3);
   B_.GetVectorValue(T.ElementNo, ip, B);
   double Bmag = B.Norml2();
   double th = atan2(B(2), B(0));
   double ph = atan2(B(0) * cos(th) + B(2) * sin(th), -B(1));

   for (int i=0; i<density_vals_.Size(); i++)
   {
      density_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&L2FESpace_),
                          const_cast<Vector&>(density_.GetBlock(i)));
      density_vals_[i] = density_gf_.GetValue(T.ElementNo, ip);
   }

   for (int i=0; i<temp_vals_.Size(); i++)
   {
      temperature_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&H1FESpace_),
                              const_cast<Vector&>(temp_.GetBlock(i)));
      temp_vals_[i] = temperature_gf_.GetValue(T.ElementNo, ip);
   }

   complex<double> S = S_cold_plasma(omega_, Bmag, density_vals_,
                                     charges_, masses_, temp_vals_);
   complex<double> P = P_cold_plasma(omega_, density_vals_,
                                     charges_, masses_, temp_vals_);
   complex<double> D = D_cold_plasma(omega_, Bmag, density_vals_,
                                     charges_, masses_, temp_vals_);

   epsilon(0,0) = abs((P - S) * pow(sin(ph), 2) * pow(cos(th), 2) + S);
   epsilon(1,1) = abs((P - S) * pow(cos(ph), 2) + S);
   epsilon(2,2) = abs((P - S) * pow(sin(ph), 2) * pow(sin(th), 2) + S);
   epsilon(0,1) = abs((P - S) * cos(ph) * cos(th) * sin(ph) -
                      D * sin(th) * sin(ph));
   epsilon(0,2) = abs((P - S) * pow(sin(ph), 2) * sin(th) * cos(th) -
                      D * cos(ph));
   epsilon(1,2) = abs((P - S) * sin(th) * cos(ph) * sin(ph) +
                      D * cos(th) * sin(ph));
   epsilon(1,0) = abs((P - S) * cos(ph) * cos(th) * sin(ph) +
                      D * sin(th) * sin(ph));
   epsilon(2,1) = abs((P - S) * sin(th) * cos(ph) * sin(ph) -
                      D * cos(th) * sin(ph));
   epsilon(2,0) = abs((P - S) * pow(sin(ph), 2) * sin(th) * cos(th) +
                      D * cos(ph));

   /*
    double aP = fabs(P);
    double aSD = 0.5 * (fabs(S + D) + fabs(S - D));

   epsilon(0,0) =  (aP - aSD) * pow(sin(ph), 2) * pow(cos(th), 2) + aSD;
   epsilon(1,1) =  (aP - aSD) * pow(cos(ph), 2) + aSD;
   epsilon(2,2) =  (aP - aSD) * pow(sin(ph), 2) * pow(sin(th), 2) + aSD;
   epsilon(0,1) = -(aP - aSD) * cos(ph) * cos(th) * sin(ph);
   epsilon(0,2) =  (aP - aSD) * pow(sin(ph), 2) * sin(th) * cos(th);
   epsilon(1,2) = -(aP - aSD) * sin(th) * cos(ph) * sin(ph);
   epsilon(1,0) = epsilon(0,1);
   epsilon(2,1) = epsilon(1,2);
   epsilon(0,2) = epsilon(2,0);
    */

   epsilon *= epsilon0_;
}

PlasmaProfile::PlasmaProfile(Type type, const Vector & params)
   : type_(type), p_(params), x_(3)
{
   MFEM_ASSERT(params.Size() == np_[type],
               "Incorrect number of parameters, " << params.Size()
               << ", for profile of type: " << type << ".");
}

double PlasmaProfile::Eval(ElementTransformation &T,
                           const IntegrationPoint &ip)
{
   if (type_ != CONSTANT)
   {
      T.Transform(ip, x_);
   }

   switch (type_)
   {
      case CONSTANT:
         return p_[0];
         break;
      case GRADIENT:
      {
         Vector x0(&p_[1], 3);
         Vector grad(&p_[4],3);

         x_ -= x0;

         return p_[0] + (grad * x_);
      }
      break;
      case TANH:
      {
         Vector x0(&p_[3], 3);
         Vector grad(&p_[6], 3);

         x_ -= x0;
         double a = 0.5 * log(3.0) * (grad * x_) / p_[2];

         if (fabs(a) < 10.0)
         {
            return p_[0] + (p_[1] - p_[0]) * tanh(a);
         }
         else
         {
            return p_[1];
         }
      }
      break;
      case ELLIPTIC_COS:
      {
         double pmin = p_[0];
         double pmax = p_[1];
         double a = p_[2];
         double b = p_[3];
         Vector x0(&p_[4], 3);

         x_ -= x0;
         double r = pow(x_[0] / a, 2) + pow(x_[1] / b, 2);
         return pmin + (pmax - pmin) * (0.5 + 0.5 * cos(M_PI * sqrt(r)));
      }
      break;
      default:
         return 0.0;
   }
}

} // namespace plasma

} // namespace mfem
