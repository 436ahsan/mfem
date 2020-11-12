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

double mu(double mass_e, double mass_i)
{
   return sqrt(mass_i / (2.0 * M_PI * mass_e));
}

double ff(double x)
{
   double a0 = 3.18553;
   double a1 = 3.70285;
   double a2 = 3.81991;
   double b1 = 1.13352;
   double b2 = 1.24171;
   double a3 = (2.0 * b2) / M_PI;
   double num = a0 + (a1 + (a2 + a3 * x) * x) * x;
   double den = 1.0 + (b1 + b2 * x) * x;

   return (num / den);
}

double gg(double w)
{
   double c0 = 0.966463;
   double c1 = 0.141639;

   return (c0+c1*tanh(w));
}

double phi0avg(double w, double xi)
{
   return (ff(gg(w)*xi));
}

double he(double x)
{
   double h1 = 0.607405;
   double h2 = 0.325497;
   double g1 = 0.624392;
   double g2 = 0.500595;
   double g3 = (M_PI * h2) / 4.0;
   double num = 1.0 + (h1 + h2 * x) * x;
   double den = 1.0 + (g1 + (g2 + g3 * x) * x) * x;

   return (num/den);
}

double phips(double bx, double wci, double mass_e, double mass_i)
{
   double mu_val = mu(mass_e, mass_i);
   double d3 = 0.995721;
   double arg = sqrt((mu_val * mu_val * bx * bx + 1.0)/(mu_val * mu_val + 1.0));
   double num = -log(arg);
   double den = 1.0 + d3 * wci * wci;

   return (num/den);
}

double niw(double wci, double bx, double phi, double mass_e,
           double mass_i)
{
   double d0 = 0.794443;
   double d1 = 0.803531;
   double d2 = 0.182378;
   double d4 = 0.0000901468;
   double nu1 = 1.455592;
   double abx = fabs(bx);
   double phid = phi - phips(abx,wci, mass_e, mass_i);
   double pre = d0 /(d2 + sqrt(phid));
   double wcip = wci * pow(phid, 0.25);
   double num = abx * abx + d4 + d1 * d1 * pow(wcip, (2.0*nu1));
   double den = 1.0 + d4 + d1 * d1 * pow(wcip, (2.0 * nu1));

   return (pre*sqrt(num/den));
}

double ye(double bx, double xi)
{
   double h0 = 1.161585;
   double abx = fabs(bx);

   return (h0*abx*he(xi));
}

double niww(double w, double wci, double bx, double xi,
            double mass_e, double mass_i)
{
   double k0 = 3.7616;
   double k1 = 0.22202;
   double phis = k0 + k1*(xi-k0);
   double phipr = phis + (phi0avg(w,xi)-phis)*tanh(w);

   return (niw(wci,bx,phipr,mass_e,mass_i));
}

complex<double> yd(double w, double wci, double bx, double xi,
                   double mass_e, double mass_i)
{
   double s0 = 1.12415;
   double delta = sqrt(phi0avg(w,xi)/niww(w,wci,bx,xi,mass_e,mass_i));
   complex<double> val(0.0, 1.0);

   return (-s0*val*(w/delta));
}

complex<double> yi(double w, double wci, double bx, double xi,
                   double mass_e, double mass_i)
{
   complex<double> val(0.0, 1.0);
   double p0 = 1.05554;
   complex<double> p1(0.797659, 0.0);
   double p2 = 1.47405;
   double p3 = 0.809615;
   double eps = 0.0001;
   complex<double> gfactornum(w * w - bx * bx * wci * wci, eps);
   complex<double> gfactorden(w * w - wci * wci, eps);
   complex<double> gfactor = gfactornum/gfactorden;
   double niwwa = niww(w,wci,bx,xi,mass_e,mass_i);
   double phi0avga = phi0avg(w,xi);
   complex<double> gamcup(fabs(bx)/(niwwa*sqrt(phi0avga)), 0.0);
   complex<double> wcup(p3*w/sqrt(niwwa), 0.0);
   complex<double> yicupden1 = wcup * wcup/gfactor - p1;
   complex<double> yicupden2 = p2*gamcup*wcup*val;
   complex<double> yicupden = yicupden1 + yicupden2;
   complex<double> yicup = val*p0*wcup/yicupden;

   return ((niwwa * yicup) / sqrt(phi0avga));
}

complex<double> ytot(double w, double wci, double bx, double xi,
                     double mass_e, double mass_i)
{
   complex<double> ytot = ye(bx,xi) +
                          yd(w,wci,bx,xi,mass_e,mass_i) + yi(w,wci,bx,xi,mass_e,mass_i);
   return (ytot);
}

double debye(double Te, double n0)
{
   //return (7.43e2*pow((Te/n0_cm),0.5));
   return sqrt((epsilon0_*Te*q_)/(n0*q_*q_));
}

SheathBase::SheathBase(const BlockVector & density,
                       const BlockVector & temp,
                       const ParFiniteElementSpace & L2FESpace,
                       const ParFiniteElementSpace & H1FESpace,
                       double omega,
                       const Vector & charges,
                       const Vector & masses,
                       bool realPart)
   : density_(density),
     temp_(temp),
     potential_(NULL),
     L2FESpace_(L2FESpace),
     H1FESpace_(H1FESpace),
     omega_(omega),
     realPart_(realPart),
     charges_(charges),
     masses_(masses)
{
}

SheathBase::SheathBase(const SheathBase &sb, bool realPart)
   : density_(sb.density_),
     temp_(sb.temp_),
     potential_(sb.potential_),
     L2FESpace_(sb.L2FESpace_),
     H1FESpace_(sb.H1FESpace_),
     omega_(sb.omega_),
     realPart_(realPart),
     charges_(sb.charges_),
     masses_(sb.masses_)
{}

double SheathBase::EvalIonDensity(ElementTransformation &T,
                                  const IntegrationPoint &ip)
{
   density_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&L2FESpace_),
                       const_cast<Vector&>(density_.GetBlock(1)));
   return density_gf_.GetValue(T, ip);
}

double SheathBase::EvalElectronTemp(ElementTransformation &T,
                                    const IntegrationPoint &ip)
{
   temperature_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&H1FESpace_),
                           const_cast<Vector&>(temp_.GetBlock(0)));
   return temperature_gf_.GetValue(T, ip);
}

complex<double> SheathBase::EvalSheathPotential(ElementTransformation &T,
                                                const IntegrationPoint &ip)
{
   double phir = (potential_) ? potential_->real().GetValue(T, ip) : 0.0 ;
   double phii = (potential_) ? potential_->imag().GetValue(T, ip) : 0.0 ;
   return complex<double>(phir, phii);
}

RectifiedSheathPotential::RectifiedSheathPotential(
   const BlockVector & density,
   const BlockVector & temp,
   const ParFiniteElementSpace & L2FESpace,
   const ParFiniteElementSpace & H1FESpace,
   double omega,
   const Vector & charges,
   const Vector & masses,
   bool realPart)
   : SheathBase(density, temp, L2FESpace, H1FESpace,
                omega, charges, masses, realPart)
{}

double RectifiedSheathPotential::Eval(ElementTransformation &T,
                                      const IntegrationPoint &ip)
{
   double density_val = EvalIonDensity(T, ip);
   double temp_val = EvalElectronTemp(T, ip);

   double Te = temp_val * q_; // Electron temperature, Units: J

   double wpi = omega_p(density_val, charges_[1], masses_[1]);

   double vnorm = Te / (charges_[1] * q_);
   double w_norm = omega_ / wpi;

   complex<double> phi = EvalSheathPotential(T, ip);
   double phi_mag = sqrt(pow(phi.real(), 2) + pow(phi.imag(), 2));
   double volt_norm = (phi_mag/2)/temp_val ;

   double phiRec = phi0avg(w_norm, volt_norm);

   if (realPart_)
   {
      return phiRec; // * temp_val;
   }
   else
   {
      return phiRec; // * temp_val;
   }
}

SheathImpedance::SheathImpedance(const ParGridFunction & B,
                                 const BlockVector & density,
                                 const BlockVector & temp,
                                 const ParFiniteElementSpace & L2FESpace,
                                 const ParFiniteElementSpace & H1FESpace,
                                 double omega,
                                 const Vector & charges,
                                 const Vector & masses,
                                 bool realPart)
   : SheathBase(density, temp, L2FESpace, H1FESpace,
                omega, charges, masses, realPart),
     B_(B)
{}

double SheathImpedance::Eval(ElementTransformation &T,
                             const IntegrationPoint &ip)
{
   // Collect density, temperature, magnetic field, and potential field values
   Vector B(3);
   B_.GetVectorValue(T, ip, B);
   double Bmag = B.Norml2();                         // Units: T

   complex<double> phi = EvalSheathPotential(T, ip); // Units: V

   double density_val = EvalIonDensity(T, ip);       // Units: # / m^3
   double temp_val = EvalElectronTemp(T, ip);        // Units: eV

   double wci = omega_c(Bmag, charges_[1], masses_[1]);        // Units: s^{-1}
   double wpi = omega_p(density_val, charges_[1], masses_[1]); // Units: s^{-1}

   double w_norm = omega_ / wpi; // Unitless
   double wci_norm = wci / wpi;  // Unitless
   double phi_mag = sqrt(pow(phi.real(), 2) + pow(phi.imag(), 2));
   double volt_norm = (phi_mag/2)/temp_val ; // Unitless

   //double debye_length = debye(temp_val, density_val*1e-6); // Input temp needs to be in eV, Units: cm
   double debye_length = debye(temp_val, density_val); // Units: m
   Vector nor(T.GetSpaceDim());
   CalcOrtho(T.Jacobian(), nor);
   double normag = nor.Norml2();
   double bn = (B * nor)/(normag*Bmag); // Unitless

   complex<double> zsheath_norm = 1.0 / ytot(w_norm, wci_norm, bn, volt_norm,
                                             masses_[0], masses_[1]);
   /*
   cout << "Check 1:" << phi0avg(0.4, 6.) - 6.43176481712605 << endl;
   cout << "Check 2:" << niw(.2, .3, 13,masses_[0], masses_[1])- 0.07646452845544677 << endl;
   cout << "Check 3:" << niww(0.2, 0.3, 0.4, 13,masses_[0], masses_[1]) - 0.14077643642166277 << endl;
   cout << "Check 4:" << yd(0.2, 0.3, 0.4, 13,masses_[0], masses_[1]).imag()+0.025738204728120898 << endl;
   cout << "Check 5: " << ye(0.4, 3.6) - 0.1588274616204441 << endl;
   cout << "Check 6:" << yi(0.2, 0.3, 0.4, 13,masses_[0], masses_[1]).real() - 0.006543897148693344 << yi(0.2, 0.3, 0.4, 13,masses_[0], masses_[1]).imag()+0.013727440802110503 << endl;
   cout << "Check 7:" << ytot(0.2, 0.3, 0.4, 13,masses_[0], masses_[1]).real()-0.05185050837032144 << ytot(0.2, 0.3, 0.4, 13,masses_[0], masses_[1]).imag()+0.0394656455302314 << endl;
    */

   //complex<double> zsheath_norm(57.4699936705, 21.39395629068357);
   double f = 0.01;
   if (realPart_)
   {
      // return (zsheath_norm.real()*9.0e11*1e-4*
      //        (4.0*M_PI*debye_length))/wpi; // Units: Ohm m^2

      //return (zsheath_norm.real()*9.0e11*1e-4*debye_length)/wpi; // Units: Ohm m^2
      return f * (zsheath_norm.real()*debye_length)/(epsilon0_*wpi);

   }
   else
   {
      // return (zsheath_norm.imag()*9.0e11*1e-4*
      //        (4.0*M_PI*debye_length))/wpi; // Units: Ohm m^2

      //return (zsheath_norm.imag()*9.0e11*1e-4*debye_length)/wpi; // Units: Ohm m^2
      return f * (zsheath_norm.imag()*debye_length)/(epsilon0_*wpi);
   }
}

StixCoefBase::StixCoefBase(const ParGridFunction & B,
                           const BlockVector & density,
                           const BlockVector & temp,
                           const ParFiniteElementSpace & L2FESpace,
                           const ParFiniteElementSpace & H1FESpace,
                           double omega,
                           const Vector & charges,
                           const Vector & masses,
                           bool realPart)
   : B_(B),
     density_(density),
     temp_(temp),
     L2FESpace_(L2FESpace),
     H1FESpace_(H1FESpace),
     omega_(omega),
     realPart_(realPart),
     BVec_(3),
     charges_(charges),
     masses_(masses)
{
   density_vals_.SetSize(charges_.Size());
   temp_vals_.SetSize(charges_.Size());
}

StixCoefBase::StixCoefBase(StixCoefBase & s)
   : B_(s.GetBField()),
     density_(s.GetDensityFields()),
     temp_(s.GetTemperatureFields()),
     L2FESpace_(s.GetDensityFESpace()),
     H1FESpace_(s.GetTemperatureFESpace()),
     omega_(s.GetOmega()),
     realPart_(s.GetRealPartFlag()),
     BVec_(3),
     charges_(s.GetCharges()),
     masses_(s.GetMasses())
{
   density_vals_.SetSize(charges_.Size());
   temp_vals_.SetSize(charges_.Size());
}

double StixCoefBase::getBMagnitude(ElementTransformation &T,
                                   const IntegrationPoint &ip,
                                   double *theta, double *phi)
{
   B_.GetVectorValue(T.ElementNo, ip, BVec_);

   if (theta != NULL)
   {
      *theta = atan2(BVec_(2), BVec_(0));

      if (phi != NULL)
      {
         *phi = atan2(BVec_(0) * cos(*theta) + BVec_(2) * sin(*theta),
                      -BVec_(1));
      }
   }

   return BVec_.Norml2();
}

void StixCoefBase::fillDensityVals(ElementTransformation &T,
                                   const IntegrationPoint &ip)
{
   for (int i=0; i<density_vals_.Size(); i++)
   {
      density_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&L2FESpace_),
                          const_cast<Vector&>(density_.GetBlock(i)));
      density_vals_[i] = density_gf_.GetValue(T.ElementNo, ip);
   }
}

void StixCoefBase::fillTemperatureVals(ElementTransformation &T,
                                       const IntegrationPoint &ip)
{
   for (int i=0; i<temp_vals_.Size(); i++)
   {
      temperature_gf_.MakeRef(const_cast<ParFiniteElementSpace*>(&H1FESpace_),
                              const_cast<Vector&>(temp_.GetBlock(i)));
      temp_vals_[i] = temperature_gf_.GetValue(T.ElementNo, ip);
   }
}

StixSCoef::StixSCoef(const ParGridFunction & B,
                     const BlockVector & density,
                     const BlockVector & temp,
                     const ParFiniteElementSpace & L2FESpace,
                     const ParFiniteElementSpace & H1FESpace,
                     double omega,
                     const Vector & charges,
                     const Vector & masses,
                     bool realPart)
   : StixCoefBase(B, density, temp, L2FESpace, H1FESpace, omega,
                  charges, masses, realPart)
{}

double StixSCoef::Eval(ElementTransformation &T,
                       const IntegrationPoint &ip)
{
   // Collect density, temperature, and magnetic field values
   double Bmag = this->getBMagnitude(T, ip);

   this->fillDensityVals(T, ip);
   this->fillTemperatureVals(T, ip);

   // Evaluate Stix Coefficient
   complex<double> S = S_cold_plasma(omega_, Bmag, density_vals_,
                                     charges_, masses_, temp_vals_);

   // Return the selected component
   if (realPart_)
   {
      return S.real();
   }
   else
   {
      return S.imag();
   }
}

StixDCoef::StixDCoef(const ParGridFunction & B,
                     const BlockVector & density,
                     const BlockVector & temp,
                     const ParFiniteElementSpace & L2FESpace,
                     const ParFiniteElementSpace & H1FESpace,
                     double omega,
                     const Vector & charges,
                     const Vector & masses,
                     bool realPart)
   : StixCoefBase(B, density, temp, L2FESpace, H1FESpace, omega,
                  charges, masses, realPart)
{}

double StixDCoef::Eval(ElementTransformation &T,
                       const IntegrationPoint &ip)
{
   // Collect density, temperature, and magnetic field values
   double Bmag = this->getBMagnitude(T, ip);

   this->fillDensityVals(T, ip);
   this->fillTemperatureVals(T, ip);

   // Evaluate Stix Coefficient
   complex<double> D = D_cold_plasma(omega_, Bmag, density_vals_,
                                     charges_, masses_, temp_vals_);

   // Return the selected component
   if (realPart_)
   {
      return D.real();
   }
   else
   {
      return D.imag();
   }
}

StixPCoef::StixPCoef(const ParGridFunction & B,
                     const BlockVector & density,
                     const BlockVector & temp,
                     const ParFiniteElementSpace & L2FESpace,
                     const ParFiniteElementSpace & H1FESpace,
                     double omega,
                     const Vector & charges,
                     const Vector & masses,
                     bool realPart)
   : StixCoefBase(B, density, temp, L2FESpace, H1FESpace, omega,
                  charges, masses, realPart)
{}

double StixPCoef::Eval(ElementTransformation &T,
                       const IntegrationPoint &ip)
{
   // Collect density and temperature field values
   this->fillDensityVals(T, ip);
   this->fillTemperatureVals(T, ip);

   // Evaluate Stix Coefficient
   complex<double> P = P_cold_plasma(omega_, density_vals_,
                                     charges_, masses_, temp_vals_);

   // Return the selected component
   if (realPart_)
   {
      return P.real();
   }
   else
   {
      return P.imag();
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
     StixCoefBase(B, density, temp, L2FESpace, H1FESpace, omega,
                  charges, masses, realPart)
{}

void DielectricTensor::Eval(DenseMatrix &epsilon, ElementTransformation &T,
                            const IntegrationPoint &ip)
{
   // Initialize dielectric tensor to appropriate size
   epsilon.SetSize(3);

   // Collect density, temperature, and magnetic field values
   double th = 0.0;
   double ph = 0.0;
   double Bmag = this->getBMagnitude(T, ip, &th, &ph);

   this->fillDensityVals(T, ip);
   this->fillTemperatureVals(T, ip);

   // Evaluate the Stix Coefficients
   complex<double> S = S_cold_plasma(omega_, Bmag, density_vals_,
                                     charges_, masses_, temp_vals_);
   complex<double> P = P_cold_plasma(omega_, density_vals_,
                                     charges_, masses_, temp_vals_);
   complex<double> D = D_cold_plasma(omega_, Bmag, density_vals_,
                                     charges_, masses_, temp_vals_);

   if (realPart_)
   {
      epsilon(0,0) =  (real(P) - real(S)) *
                      pow(sin(ph), 2) * pow(cos(th), 2) + real(S);
      epsilon(1,1) =  (real(P) - real(S)) * pow(cos(ph), 2) + real(S);
      epsilon(2,2) =  (real(P) - real(S)) *
                      pow(sin(ph), 2) * pow(sin(th), 2) + real(S);
      epsilon(0,1) = (real(P) - real(S)) * cos(ph) * cos(th) * sin(ph) -
                     imag(D) * sin(th) * sin(ph);
      epsilon(0,2) =  (real(P) - real(S)) *
                      pow(sin(ph), 2) * sin(th) * cos(th) + imag(D) * cos(ph);
      epsilon(1,2) = (real(P) - real(S)) * sin(th) * cos(ph) * sin(ph) -
                     imag(D) * cos(th) * sin(ph);
      epsilon(1,0) = (real(P) - real(S)) * cos(ph) * cos(th) * sin(ph) +
                     imag(D) * sin(th) * sin(ph);
      epsilon(2,1) = (real(P) - real(S)) * sin(th) * cos(ph) * sin(ph) +
                     imag(D) * cos(th) * sin(ph);
      epsilon(2,0) = (real(P) - real(S)) *
                     pow(sin(ph),2) * sin(th) * cos(th) - imag(D) * cos(ph);
   }
   else
   {
      epsilon(0,0) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * pow(cos(th), 2) + imag(S);
      epsilon(1,1) = (imag(P) - imag(S)) * pow(cos(ph), 2) + imag(S);
      epsilon(2,2) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * pow(sin(th), 2) + imag(S);
      epsilon(0,1) = (imag(P) - imag(S)) * cos(ph) * cos(th) * sin(ph) +
                     real(D) * sin(th) * sin(ph);
      epsilon(0,2) = (imag(P) - imag(S)) *
                     pow(sin(ph), 2) * sin(th) * cos(th) - real(D) * cos(ph);
      epsilon(1,2) = (imag(P) - imag(S)) * sin(th) * cos(ph) * sin(ph) +
                     real(D) * cos(th) * sin(ph);
      epsilon(1,0) = (imag(P) - imag(S)) * cos(ph) * cos(th) * sin(ph) -
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
   MFEM_VERIFY(params.Size() == np_[type],
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
