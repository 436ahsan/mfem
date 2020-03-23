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

#ifndef MFEM_TRANSPORT_SOLVER
#define MFEM_TRANSPORT_SOLVER

#include "../common/pfem_extras.hpp"
#include "../common/mesh_extras.hpp"
#include "plasma.hpp"

#ifdef MFEM_USE_MPI

namespace mfem
{

struct CoefficientByAttr
{
   Array<int> attr;
   Coefficient * coef;
};

namespace plasma
{

namespace transport
{

/**
   Returns the mean Electron-Ion mean collision time in seconds (see
   equation 2.5e)
   Te is the electron temperature in eV
   ni is the density of ions (assuming ni=ne) in particles per meter^3
   zi is the charge number of the ion species
   lnLambda is the Coulomb Logarithm
*/
inline double tau_e(double Te, double zi, double ni, double lnLambda)
{
   // The factor of eV_ is included to convert Te from eV to Joules
   return 0.75 * pow(4.0 * M_PI * epsilon0_, 2) *
          sqrt(0.5 * me_kg_ * pow(Te * eV_, 3) / M_PI) /
          (lnLambda * pow(q_, 4) * zi * zi * ni);
}
/// Derivative of tau_e wrt Te
inline double dtau_e_dT(double Te, double zi, double ni, double lnLambda)
{
   // The factor of eV_ is included to convert Te from eV to Joules
   return 1.125 * eV_ * pow(4.0 * M_PI * epsilon0_, 2) *
          sqrt(0.5 * me_kg_ * Te * eV_ / M_PI) /
          (lnLambda * pow(q_, 4) * zi * zi * ni);
}

/**
   Returns the mean Ion-Ion mean collision time in seconds (see equation 2.5i)
   mi is the ion mass in a.m.u.
   zi is the charge number of the ion species
   ni is the density of ions in particles per meter^3
   Ti is the ion temperature in eV
   lnLambda is the Coulomb Logarithm
*/
inline double tau_i(double mi, double zi, double ni, double Ti,
                    double lnLambda)
{
   // The factor of eV_ is included to convert Ti from eV to Joules
   return 0.75 * pow(4.0 * M_PI * epsilon0_, 2) *
          sqrt(mi * amu_ * pow(Ti * eV_, 3) / M_PI) /
          (lnLambda * pow(q_ * zi, 4) * ni);
}

/** Multispecies Electron-Ion Collision Time in seconds
 Te is the electron temperature in eV
 ns is the number of ion species
 ni is the density of ions (assuming ni=ne) in particles per meter^3
 zi is the charge number of the ion species
 lnLambda is the Coulomb Logarithm
*/
//double tau_e(double Te, int ns, double * ni, int * zi, double lnLambda);

/** Multispecies Ion-Ion Collision Time in seconds
   ma is the ion mass in a.m.u
   Ta is the ion temperature in eV
   ion is the ion species index for the desired collision time
   ns is the number of ion species
   ni is the density of ions (assuming ni=ne) in particles per meter^3
   zi is the charge number of the ion species
   lnLambda is the Coulomb Logarithm
*/
//double tau_i(double ma, double Ta, int ion, int ns, double * ni, int * zi,
//             double lnLambda);

/**
  Thermal diffusion coefficient along B field for electrons
  Return value is in m^2/s.
   Te is the electron temperature in eV
   ns is the number of ion species
   ni is the density of ions (assuming ni=ne) in particles per meter^3
   zi is the charge number of the ion species
*/
/*
inline double chi_e_para(double Te, int ns, double * ni, int * zi)
{
 // The factor of q_ is included to convert Te from eV to Joules
 return 3.16 * (q_ * Te / me_kg_) * tau_e(Te, ns, ni, zi, 17.0);
}
*/
/**
  Thermal diffusion coefficient perpendicular to B field for electrons
  Return value is in m^2/s.
*/
/*
inline double chi_e_perp()
{
 // The factor of q_ is included to convert Te from eV to Joules
 return 1.0;
}
*/
/**
  Thermal diffusion coefficient perpendicular to both B field and
  thermal gradient for electrons.
  Return value is in m^2/s.
   Te is the electron temperature in eV
   ni is the density of ions (assuming ni=ne) in particles per meter^3
   z is the charge number of the ion species
*/
/*
inline double chi_e_cross()
{
 // The factor of q_ is included to convert Te from eV to Joules
 return 0.0;
}
*/
/**
  Thermal diffusion coefficient along B field for ions
  Return value is in m^2/s.
   ma is the ion mass in a.m.u.
   Ta is the ion temperature in eV
   ion is the ion species index for the desired coefficient
   ns is the number of ion species
   nb is the density of ions in particles per meter^3
   zb is the charge number of the ion species
*/
/*
inline double chi_i_para(double ma, double Ta,
                       int ion, int ns, double * nb, int * zb)
{
 // The factor of q_ is included to convert Ta from eV to Joules
 // The factor of u_ is included to convert ma from a.m.u to kg
 return 3.9 * (q_ * Ta / (ma * amu_ ) ) *
        tau_i(ma, Ta, ion, ns, nb, zb, 17.0);
}
*/
/**
  Thermal diffusion coefficient perpendicular to B field for ions
  Return value is in m^2/s.
*/
/*
inline double chi_i_perp()
{
 // The factor of q_ is included to convert Ti from eV to Joules
 // The factor of u_ is included to convert mi from a.m.u to kg
 return 1.0;
}
*/
/**
  Thermal diffusion coefficient perpendicular to both B field and
  thermal gradient for ions
  Return value is in m^2/s.
*/
/*
inline double chi_i_cross()
{
 // The factor of q_ is included to convert Ti from eV to Joules
 // The factor of u_ is included to convert mi from a.m.u to kg
 return 0.0;
}
*/
/**
  Viscosity coefficient along B field for electrons
  Return value is in (a.m.u)*m^2/s.
   ne is the density of electrons in particles per meter^3
   Te is the electron temperature in eV
   ns is the number of ion species
   ni is the density of ions (assuming ni=ne) in particles per meter^3
   zi is the charge number of the ion species
*/
/*
inline double eta_e_para(double ne, double Te, int ns, double * ni, int * zi)
{
 // The factor of q_ is included to convert Te from eV to Joules
 // The factor of u_ is included to convert from kg to a.m.u
 return 0.73 * ne * (q_ * Te / amu_) * tau_e(Te, ns, ni, zi, 17.0);
}
*/
/**
  Viscosity coefficient along B field for ions
  Return value is in (a.m.u)*m^2/s.
   ma is the ion mass in a.m.u.
   Ta is the ion temperature in eV
   ion is the ion species index for the desired coefficient
   ns is the number of ion species
   nb is the density of ions in particles per meter^3
   zb is the charge number of the ion species
*/
/*
inline double eta_i_para(double ma, double Ta,
                       int ion, int ns, double * nb, int * zb)
{
 // The factor of q_ is included to convert Ti from eV to Joules
 // The factor of u_ is included to convert from kg to a.m.u
 return 0.96 * nb[ion] * (q_ * Ta / amu_) *
        tau_i(ma, Ta, ion, ns, nb, zb, 17.0);
}
*/

class ParGridFunctionArray : public Array<ParGridFunction*>
{
private:
   bool owns_data;

public:
   ParGridFunctionArray() : owns_data(true) {}
   ParGridFunctionArray(int size)
      : Array<ParGridFunction*>(size), owns_data(true) {}
   ParGridFunctionArray(int size, ParFiniteElementSpace *pf)
      : Array<ParGridFunction*>(size), owns_data(true)
   {
      for (int i=0; i<size; i++)
      {
         data[i] = new ParGridFunction(pf);
      }
   }

   ~ParGridFunctionArray()
   {
      if (owns_data)
      {
         for (int i=0; i<size; i++)
         {
            delete data[i];
         }
      }
   }

   void SetOwner(bool owner) { owns_data = owner; }
   bool GetOwner() const { return owns_data; }

   void ProjectCoefficient(Array<Coefficient*> &coeff)
   {
      for (int i=0; i<size; i++)
      {
         if (coeff[i] != NULL)
         {
            data[i]->ProjectCoefficient(*coeff[i]);
         }
         else
         {
            *data[i] = 0.0;
         }
      }
   }

   void Update()
   {
      for (int i=0; i<size; i++)
      {
         data[i]->Update();
      }
   }

   void ExchangeFaceNbrData()
   {
      for (int i=0; i<size; i++)
      {
         data[i]->ExchangeFaceNbrData();
      }
   }
};

enum FieldType {INVALID = -1,
                NEUTRAL_DENSITY      = 0,
                ION_DENSITY          = 1,
                ION_PARA_VELOCITY    = 2,
                ION_TEMPERATURE      = 3,
                ELECTRON_TEMPERATURE = 4
               };

std::string FieldSymbol(FieldType t);

class StateVariableFunc
{
public:

   virtual bool NonTrivialValue(FieldType deriv) const = 0;

   void SetDerivType(FieldType deriv) { derivType_ = deriv; }
   FieldType GetDerivType() const { return derivType_; }

protected:
   StateVariableFunc(FieldType deriv = INVALID) : derivType_(deriv) {}

   FieldType derivType_;
};


class StateVariableCoef : public StateVariableFunc, public Coefficient
{
public:

   virtual StateVariableCoef * Clone() const = 0;

   virtual double Eval(ElementTransformation &T,
                       const IntegrationPoint &ip)
   {
      switch (derivType_)
      {
         case INVALID:
            return Eval_Func(T, ip);
         case NEUTRAL_DENSITY:
            return Eval_dNn(T, ip);
         case ION_DENSITY:
            return Eval_dNi(T, ip);
         case ION_PARA_VELOCITY:
            return Eval_dVi(T, ip);
         case ION_TEMPERATURE:
            return Eval_dTi(T, ip);
         case ELECTRON_TEMPERATURE:
            return Eval_dTe(T, ip);
         default:
            return 0.0;
      }
   }

   virtual double Eval_Func(ElementTransformation &T,
                            const IntegrationPoint &ip) { return 0.0; }

   virtual double Eval_dNn(ElementTransformation &T,
                           const IntegrationPoint &ip) { return 0.0; }

   virtual double Eval_dNi(ElementTransformation &T,
                           const IntegrationPoint &ip) { return 0.0; }

   virtual double Eval_dVi(ElementTransformation &T,
                           const IntegrationPoint &ip) { return 0.0; }

   virtual double Eval_dTi(ElementTransformation &T,
                           const IntegrationPoint &ip) { return 0.0; }

   virtual double Eval_dTe(ElementTransformation &T,
                           const IntegrationPoint &ip) { return 0.0; }

protected:
   StateVariableCoef(FieldType deriv = INVALID) : StateVariableFunc(deriv) {}
};

class StateVariableVecCoef : public StateVariableFunc,
   public VectorCoefficient
{
public:
   virtual void Eval(Vector &V,
                     ElementTransformation &T,
                     const IntegrationPoint &ip)
   {
      V.SetSize(vdim);

      switch (derivType_)
      {
         case INVALID:
            return Eval_Func(V, T, ip);
         case NEUTRAL_DENSITY:
            return Eval_dNn(V, T, ip);
         case ION_DENSITY:
            return Eval_dNi(V, T, ip);
         case ION_PARA_VELOCITY:
            return Eval_dVi(V, T, ip);
         case ION_TEMPERATURE:
            return Eval_dTi(V, T, ip);
         case ELECTRON_TEMPERATURE:
            return Eval_dTe(V, T, ip);
         default:
            V = 0.0;
            return;
      }
   }

   virtual void Eval_Func(Vector &V,
                          ElementTransformation &T,
                          const IntegrationPoint &ip) { V = 0.0; }

   virtual void Eval_dNn(Vector &V,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { V = 0.0; }

   virtual void Eval_dNi(Vector &V,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { V = 0.0; }

   virtual void Eval_dVi(Vector &V,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { V = 0.0; }

   virtual void Eval_dTi(Vector &V,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { V = 0.0; }

   virtual void Eval_dTe(Vector &V,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { V = 0.0; }

protected:
   StateVariableVecCoef(int dim, FieldType deriv = INVALID)
      : StateVariableFunc(deriv), VectorCoefficient(dim) {}
};

class StateVariableMatCoef : public StateVariableFunc,
   public MatrixCoefficient
{
public:
   virtual void Eval(DenseMatrix &M,
                     ElementTransformation &T,
                     const IntegrationPoint &ip)
   {
      M.SetSize(height, width);

      switch (derivType_)
      {
         case INVALID:
            return Eval_Func(M, T, ip);
         case NEUTRAL_DENSITY:
            return Eval_dNn(M, T, ip);
         case ION_DENSITY:
            return Eval_dNi(M, T, ip);
         case ION_PARA_VELOCITY:
            return Eval_dVi(M, T, ip);
         case ION_TEMPERATURE:
            return Eval_dTi(M, T, ip);
         case ELECTRON_TEMPERATURE:
            return Eval_dTe(M, T, ip);
         default:
            M = 0.0;
            return;
      }
   }

   virtual void Eval_Func(DenseMatrix &M,
                          ElementTransformation &T,
                          const IntegrationPoint &ip) { M = 0.0; }

   virtual void Eval_dNn(DenseMatrix &M,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { M = 0.0; }

   virtual void Eval_dNi(DenseMatrix &M,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { M = 0.0; }

   virtual void Eval_dVi(DenseMatrix &M,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { M = 0.0; }

   virtual void Eval_dTi(DenseMatrix &M,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { M = 0.0; }

   virtual void Eval_dTe(DenseMatrix &M,
                         ElementTransformation &T,
                         const IntegrationPoint &ip) { M = 0.0; }

protected:
   StateVariableMatCoef(int dim, FieldType deriv = INVALID)
      : StateVariableFunc(deriv), MatrixCoefficient(dim) {}

   StateVariableMatCoef(int h, int w, FieldType deriv = INVALID)
      : StateVariableFunc(deriv), MatrixCoefficient(h, w) {}
};

class StateVariableGridFunctionCoef : public StateVariableCoef
{
private:
   GridFunctionCoefficient gfc_;
   FieldType fieldType_;

public:
   StateVariableGridFunctionCoef(FieldType field)
      : fieldType_(field)
   {}

   StateVariableGridFunctionCoef(GridFunction *gf, FieldType field)
      : gfc_(gf), fieldType_(field)
   {}

   StateVariableGridFunctionCoef(const StateVariableGridFunctionCoef &other)
   {
      derivType_ = other.derivType_;
      fieldType_ = other.fieldType_;
      gfc_       = other.gfc_;
   }

   virtual StateVariableGridFunctionCoef * Clone() const
   {
      return new StateVariableGridFunctionCoef(*this);
   }

   void SetGridFunction(GridFunction *gf) { gfc_.SetGridFunction(gf); }
   GridFunction * GetGridFunction() const { return gfc_.GetGridFunction(); }

   void SetFieldType(FieldType field) { fieldType_ = field; }
   FieldType GetFieldType() const { return fieldType_; }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == fieldType_);
   }

   virtual double Eval(ElementTransformation &T,
                       const IntegrationPoint &ip)
   {
      if (derivType_ == INVALID)
      {
         return gfc_.Eval(T, ip);
      }
      else if (derivType_ == fieldType_)
      {
         return 1.0;
      }
      else
      {
         return 0.0;
      }
   }

};

class StateVariableSumCoef : public StateVariableCoef
{
private:
   StateVariableCoef *a;
   StateVariableCoef *b;

   double alpha;
   double beta;

public:
   // Result is _alpha * A + _beta * B
   StateVariableSumCoef(StateVariableCoef &A, StateVariableCoef &B,
                        double _alpha = 1.0, double _beta = 1.0)
      : a(A.Clone()), b(B.Clone()), alpha(_alpha), beta(_beta) {}

   ~StateVariableSumCoef()
   {
      if (a != NULL) { delete a; }
      if (b != NULL) { delete b; }
   }

   virtual StateVariableSumCoef * Clone() const
   {
      return new StateVariableSumCoef(*a, *b, alpha, beta);
   }

   void SetACoef(StateVariableCoef &A) { a = &A; }
   StateVariableCoef * GetACoef() const { return a; }

   void SetBCoef(StateVariableCoef &B) { b = &B; }
   StateVariableCoef * GetBCoef() const { return b; }

   void SetAlpha(double _alpha) { alpha = _alpha; }
   double GetAlpha() const { return alpha; }

   void SetBeta(double _beta) { beta = _beta; }
   double GetBeta() const { return beta; }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return a->NonTrivialValue(deriv) || b->NonTrivialValue(deriv);
   }

   /// Evaluate the coefficient
   virtual double Eval_Func(ElementTransformation &T,
                            const IntegrationPoint &ip)
   { return alpha * a->Eval_Func(T, ip) + beta * b->Eval_Func(T, ip); }

   virtual double Eval_dNn(ElementTransformation &T,
                           const IntegrationPoint &ip)
   { return alpha * a->Eval_dNn(T, ip) + beta * b->Eval_dNn(T, ip); }

   virtual double Eval_dNi(ElementTransformation &T,
                           const IntegrationPoint &ip)
   { return alpha * a->Eval_dNi(T, ip) + beta * b->Eval_dNi(T, ip); }

   virtual double Eval_dVi(ElementTransformation &T,
                           const IntegrationPoint &ip)
   { return alpha * a->Eval_dVi(T, ip) + beta * b->Eval_dVi(T, ip); }

   virtual double Eval_dTi(ElementTransformation &T,
                           const IntegrationPoint &ip)
   { return alpha * a->Eval_dTi(T, ip) + beta * b->Eval_dTi(T, ip); }

   virtual double Eval_dTe(ElementTransformation &T,
                           const IntegrationPoint &ip)
   { return alpha * a->Eval_dTe(T, ip) + beta * b->Eval_dTe(T, ip); }
};

/** Given the electron temperature in eV this coefficient returns an
    approximation to the expected ionization rate in m^3/s.
*/
class ApproxIonizationRate : public StateVariableCoef
{
private:
   Coefficient *TeCoef_;
   // GridFunction *Te_;

   // StateVariable derivType_;

public:
   // ApproxIonizationRate(GridFunction &Te) : Te_(&Te) {}
   ApproxIonizationRate(Coefficient &TeCoef)
      : TeCoef_(&TeCoef) {}

   ApproxIonizationRate(const ApproxIonizationRate &other)
   {
      derivType_ = other.derivType_;
      TeCoef_    = other.TeCoef_;
   }

   virtual ApproxIonizationRate * Clone() const
   {
      return new ApproxIonizationRate(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double Te2 = pow(TeCoef_->Eval(T, ip), 2);

      return 3.0e-16 * Te2 / (3.0 + 0.01 * Te2);
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double Te = TeCoef_->Eval(T, ip);

      return 2.0 * 3.0 * 3.0e-16 * Te / pow(3.0 + 0.01 * Te * Te, 2);
   }

};

/** Given the electron temperature in eV this coefficient returns an
    approximation to the expected recombination rate in m^3/s.
*/
class ApproxRecombinationRate : public StateVariableCoef
{
private:
   Coefficient *TeCoef_;

public:
   ApproxRecombinationRate(Coefficient &TeCoef)
      : TeCoef_(&TeCoef) {}

   ApproxRecombinationRate(const ApproxRecombinationRate &other)
   {
      derivType_ = other.derivType_;
      TeCoef_    = other.TeCoef_;
   }

   virtual ApproxRecombinationRate * Clone() const
   {
      return new ApproxRecombinationRate(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double Te2 = pow(TeCoef_->Eval(T, ip), 2);

      return 3.0e-19 * Te2 / (3.0 + 0.01 * Te2);
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double Te = TeCoef_->Eval(T, ip);

      return 2.0 * 3.0 * 3.0e-19 * Te / pow(3.0 + 0.01 * Te * Te, 2);
   }

};

class NeutralDiffusionCoef : public StateVariableCoef
{
private:
   ProductCoefficient * ne_;
   Coefficient        * vn_;
   StateVariableCoef  * iz_;

public:
   NeutralDiffusionCoef(ProductCoefficient &neCoef, Coefficient &vnCoef,
                        StateVariableCoef &izCoef)
      : ne_(&neCoef), vn_(&vnCoef), iz_(&izCoef) {}

   NeutralDiffusionCoef(const NeutralDiffusionCoef &other)
   {
      derivType_ = other.derivType_;
      ne_ = other.ne_;
      vn_ = other.vn_;
      iz_ = other.iz_;
   }

   virtual NeutralDiffusionCoef * Clone() const
   {
      return new NeutralDiffusionCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ION_DENSITY ||
              deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double vn = vn_->Eval(T, ip);
      double iz = iz_->Eval(T, ip);

      return vn * vn / (3.0 * ne * iz);
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double vn = vn_->Eval(T, ip);
      double iz = iz_->Eval(T, ip);

      double dNe_dNi = ne_->GetAConst();

      return -vn * vn * dNe_dNi / (3.0 * ne * ne * iz);
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double vn = vn_->Eval(T, ip);
      double iz = iz_->Eval_Func(T, ip);

      double diz_dTe = iz_->Eval_dTe(T, ip);

      return -vn * vn * diz_dTe / (3.0 * ne * iz * iz);
   }

};

class IonDiffusionCoef : public StateVariableMatCoef
{
private:
   Coefficient       * Dperp_;
   VectorCoefficient * B3_;

   mutable Vector B_;

public:
   IonDiffusionCoef(Coefficient &DperpCoef, VectorCoefficient &B3Coef)
      : StateVariableMatCoef(2), Dperp_(&DperpCoef), B3_(&B3Coef), B_(3) {}

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID);
   }

   void Eval_Func(DenseMatrix & M,
                  ElementTransformation &T,
                  const IntegrationPoint &ip)
   {
      M.SetSize(2);

      double Dperp = Dperp_->Eval(T, ip);

      B3_->Eval(B_, T, ip);

      double Bmag2 = B_ * B_;

      M(0,0) = (B_[1] * B_[1] + B_[2] * B_[2]) * Dperp / Bmag2;
      M(0,1) = -B_[0] * B_[1] * Dperp / Bmag2;
      M(1,0) = M(0,1);
      M(1,1) = (B_[0] * B_[0] + B_[2] * B_[2]) * Dperp / Bmag2;
   }

};

class IonAdvectionCoef : public StateVariableVecCoef
{
private:
   // double dt_;

   StateVariableGridFunctionCoef &vi_;
   // GridFunctionCoefficient vi0_;
   // GridFunctionCoefficient dvi0_;

   VectorCoefficient * B3_;

   mutable Vector B_;

public:
   IonAdvectionCoef(StateVariableGridFunctionCoef &vi,
                    VectorCoefficient &B3Coef)
      : StateVariableVecCoef(2),
        vi_(vi),
        B3_(&B3Coef), B_(3) {}

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ION_PARA_VELOCITY);
   }

   void Eval_Func(Vector & V,
                  ElementTransformation &T,
                  const IntegrationPoint &ip)
   {
      V.SetSize(2);

      double vi = vi_.Eval(T, ip);

      B3_->Eval(B_, T, ip);

      double Bmag = sqrt(B_ * B_);

      V[0] = vi * B_[0] / Bmag;
      V[1] = vi * B_[1] / Bmag;
   }

   void Eval_dVi(Vector &V, ElementTransformation &T,
                 const IntegrationPoint &ip)
   {
      V.SetSize(2);

      B3_->Eval(B_, T, ip);

      double Bmag = sqrt(B_ * B_);

      V[0] = B_[0] / Bmag;
      V[1] = B_[1] / Bmag;
   }
};

class IonSourceCoef : public StateVariableCoef
{
private:
   ProductCoefficient * ne_;
   Coefficient        * nn_;
   StateVariableCoef  * iz_;

   double nn0_;

public:
   IonSourceCoef(ProductCoefficient &neCoef, Coefficient &nnCoef,
                 StateVariableCoef &izCoef)
      : ne_(&neCoef), nn_(&nnCoef), iz_(&izCoef), nn0_(1e10) {}

   IonSourceCoef(const IonSourceCoef &other)
   {
      derivType_ = other.derivType_;
      ne_ = other.ne_;
      nn_ = other.nn_;
      iz_ = other.iz_;
   }

   virtual IonSourceCoef * Clone() const
   {
      return new IonSourceCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == NEUTRAL_DENSITY ||
              deriv == ION_DENSITY || deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double nn = nn_->Eval(T, ip);
      double iz = iz_->Eval(T, ip);

      return ne * (nn - nn0_) * iz;
   }

   double Eval_dNn(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double iz = iz_->Eval(T, ip);

      return ne * iz;
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double nn = nn_->Eval(T, ip);
      double iz = iz_->Eval(T, ip);

      double dNe_dNi = ne_->GetAConst();

      return dNe_dNi * (nn - nn0_) * iz;
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double nn = nn_->Eval(T, ip);

      double diz_dTe = iz_->Eval_dTe(T, ip);

      return ne * (nn - nn0_) * diz_dTe;
   }
};

class IonSinkCoef : public StateVariableCoef
{
private:
   ProductCoefficient * ne_;
   Coefficient        * ni_;
   StateVariableCoef  * rc_;

public:
   IonSinkCoef(ProductCoefficient &neCoef, Coefficient &niCoef,
               StateVariableCoef &rcCoef)
      : ne_(&neCoef), ni_(&niCoef), rc_(&rcCoef) {}

   IonSinkCoef(const IonSinkCoef &other)
   {
      derivType_ = other.derivType_;
      ne_ = other.ne_;
      ni_ = other.ni_;
      rc_ = other.rc_;
   }

   virtual IonSinkCoef * Clone() const
   {
      return new IonSinkCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID ||
              deriv == ION_DENSITY || deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double ni = ni_->Eval(T, ip);
      double rc = rc_->Eval(T, ip);

      return ne * ni * rc;
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ni = ni_->Eval(T, ip);
      double rc = rc_->Eval(T, ip);

      double dNe_dNi = ne_->GetAConst();

      return 2.0 * dNe_dNi * ni * rc;
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ne = ne_->Eval(T, ip);
      double ni = ni_->Eval(T, ip);

      double drc_dTe = rc_->Eval_dTe(T, ip);

      return ne * ni * drc_dTe;
   }
};

class IonMomentumParaCoef : public StateVariableCoef
{
private:
   double m_i_;
   StateVariableCoef &niCoef_;
   StateVariableCoef &viCoef_;

public:
   IonMomentumParaCoef(double m_i,
                       StateVariableCoef &niCoef,
                       StateVariableCoef &viCoef)
      : m_i_(m_i), niCoef_(niCoef), viCoef_(viCoef) {}

   IonMomentumParaCoef(const IonMomentumParaCoef &other)
      : niCoef_(other.niCoef_),
        viCoef_(other.viCoef_)
   {
      derivType_ = other.derivType_;
      m_i_ = other.m_i_;
   }

   virtual IonMomentumParaCoef * Clone() const
   {
      return new IonMomentumParaCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID ||
              deriv == ION_DENSITY || deriv == ION_PARA_VELOCITY);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ni = niCoef_.Eval_Func(T, ip);
      double vi = viCoef_.Eval_Func(T, ip);

      return m_i_ * ni * vi;
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double vi = viCoef_.Eval_Func(T, ip);

      return m_i_ * vi;
   }

   double Eval_dVi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ni = niCoef_.Eval_Func(T, ip);

      return m_i_ * ni;
   }
};

class IonMomentumParaDiffusionCoef : public StateVariableCoef
{
private:
   double z_i_;
   double m_i_;
   const double lnLambda_;
   const double a_;

   StateVariableCoef * TiCoef_;

public:
   IonMomentumParaDiffusionCoef(int ion_charge, double ion_mass,
                                StateVariableCoef &TiCoef)
      : z_i_((double)ion_charge), m_i_(ion_mass),
        lnLambda_(17.0),
        a_(0.96 * 0.75 * pow(4.0 * M_PI * epsilon0_, 2) *
           sqrt(m_i_ * amu_ * pow(eV_, 5) / M_PI) /
           (lnLambda_ * amu_ * pow(q_ * z_i_, 4))),
        TiCoef_(&TiCoef)
   {}

   IonMomentumParaDiffusionCoef(const IonMomentumParaDiffusionCoef &other)
      : z_i_(other.z_i_), m_i_(other.m_i_),
        lnLambda_(17.0),
        a_(0.96 * 0.75 * pow(4.0 * M_PI * epsilon0_, 2) *
           sqrt(m_i_ * amu_ * pow(eV_, 5) / M_PI) /
           (lnLambda_ * amu_ * pow(q_ * z_i_, 4))),
        TiCoef_(other.TiCoef_)
   {
      derivType_ = other.derivType_;
   }

   virtual IonMomentumParaDiffusionCoef * Clone() const
   {
      return new IonMomentumParaDiffusionCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ION_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double Ti = TiCoef_->Eval_Func(T, ip);
      double EtaPara = a_ * sqrt(pow(Ti, 5));
      return EtaPara;
   }

   double Eval_dTi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double Ti = TiCoef_->Eval_Func(T, ip);
      double dEtaPara = 2.5 * a_ * sqrt(pow(Ti, 3));
      return dEtaPara;
   }

};

class IonMomentumPerpDiffusionCoef : public StateVariableCoef
{
private:
   double Dperp_;
   double m_i_;

   StateVariableCoef * niCoef_;

public:
   IonMomentumPerpDiffusionCoef(double Dperp, double ion_mass,
                                StateVariableCoef &niCoef)
      : Dperp_(Dperp), m_i_(ion_mass),
        niCoef_(&niCoef)
   {}

   IonMomentumPerpDiffusionCoef(const IonMomentumPerpDiffusionCoef &other)
      : Dperp_(other.Dperp_), m_i_(other.m_i_),
        niCoef_(other.niCoef_)
   {
      derivType_ = other.derivType_;
   }

   virtual IonMomentumPerpDiffusionCoef * Clone() const
   {
      return new IonMomentumPerpDiffusionCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ION_DENSITY);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ni = niCoef_->Eval_Func(T, ip);
      double EtaPerp = Dperp_ * m_i_ * ni;
      return EtaPerp;
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double dEtaPerp = Dperp_ * m_i_;
      return dEtaPerp;
   }

};
/*
class IonMomentumDiffusionCoef : public StateVariableMatCoef
{
private:
   double zi_;
   double mi_;
   const double lnLambda_;
   const double a_;

   Coefficient       * Dperp_;
   Coefficient       * niCoef_;
   Coefficient       * TiCoef_;
   VectorCoefficient * B3_;

   mutable Vector B_;

public:
   IonMomentumDiffusionCoef(int ion_charge, double ion_mass,
                            Coefficient &DperpCoef,
                            Coefficient &niCoef, Coefficient &TiCoef,
                            VectorCoefficient &B3Coef)
      : StateVariableMatCoef(2), zi_((double)ion_charge), mi_(ion_mass),
        lnLambda_(17.0),
        a_(0.96 * 0.75 * pow(4.0 * M_PI * epsilon0_, 2) *
           sqrt(mi_ * amu_ * pow(eV_, 5) / M_PI) /
           (lnLambda_ * pow(q_ * zi_, 4))),
        Dperp_(&DperpCoef), niCoef_(&niCoef), TiCoef_(&TiCoef),
        B3_(&B3Coef), B_(3) {}

   bool NonTrivialValue(DerivType deriv) const
   {
      return (deriv == INVALID);
   }

   void Eval_Func(DenseMatrix & M,
                  ElementTransformation &T,
                  const IntegrationPoint &ip)
   {
      M.SetSize(2);

      double Dperp = Dperp_->Eval(T, ip);

      double ni = niCoef_->Eval(T, ip);
      double Ti = TiCoef_->Eval(T, ip);

      B3_->Eval(B_, T, ip);

      double Bmag2 = B_ * B_;

      double EtaPerp = mi_ * ni * Dperp;

      M(0,0) = (B_[1] * B_[1] + B_[2] * B_[2]) * EtaPerp / Bmag2;
      M(0,1) = -B_[0] * B_[1] * EtaPerp / Bmag2;
      M(1,0) = M(0,1);
      M(1,1) = (B_[0] * B_[0] + B_[2] * B_[2]) * EtaPerp / Bmag2;

      double EtaPara = a_ * sqrt(pow(Ti, 5));

      M(0,0) += B_[0] * B_[0] * EtaPara / Bmag2;
      M(0,1) += B_[0] * B_[1] * EtaPara / Bmag2;
      M(1,0) += B_[0] * B_[1] * EtaPara / Bmag2;
      M(1,1) += B_[1] * B_[1] * EtaPara / Bmag2;
   }

};
*/
class IonMomentumAdvectionCoef : public StateVariableVecCoef
{
private:
   double mi_;
   // double dt_;

   StateVariableGridFunctionCoef &ni_;
   StateVariableGridFunctionCoef &vi_;

   // GridFunctionCoefficient ni0_;
   // GridFunctionCoefficient vi0_;

   // GridFunctionCoefficient dni0_;
   // GridFunctionCoefficient dvi0_;

   GradientGridFunctionCoefficient grad_ni_;
   // GradientGridFunctionCoefficient grad_dni0_;

   Coefficient       * Dperp_;
   VectorCoefficient * B3_;

   mutable Vector gni_;
   // mutable Vector gdni0_;
   // mutable Vector gni1_;

   mutable Vector B_;

public:
   /*
   IonMomentumAdvectionCoef(ParGridFunctionArray &yGF,
                             ParGridFunctionArray &kGF,
                             double ion_mass,
                             Coefficient &DperpCoef,
                             VectorCoefficient &B3Coef)
       : StateVariableVecCoef(2),
         mi_(ion_mass),
         dt_(0.0),
         ni0_(yGF[ION_DENSITY]), vi0_(yGF[ION_PARA_VELOCITY]),
         dni0_(kGF[ION_DENSITY]), dvi0_(kGF[ION_PARA_VELOCITY]),
         grad_ni0_(yGF[ION_DENSITY]), grad_dni0_(kGF[ION_DENSITY]),
         Dperp_(&DperpCoef),
         B3_(&B3Coef), B_(3)
    {}
   */
   IonMomentumAdvectionCoef(StateVariableGridFunctionCoef &ni,
                            StateVariableGridFunctionCoef &vi,
                            double ion_mass,
                            Coefficient &DperpCoef,
                            VectorCoefficient &B3Coef)
      : StateVariableVecCoef(2),
        mi_(ion_mass),
        // dt_(0.0),
        ni_(ni),
        vi_(vi),
        // ni0_(yGF[ION_DENSITY]), vi0_(yGF[ION_PARA_VELOCITY]),
        // dni0_(kGF[ION_DENSITY]), dvi0_(kGF[ION_PARA_VELOCITY]),
        grad_ni_(ni.GetGridFunction()),
        //grad_ni0_(yGF[ION_DENSITY]), grad_dni0_(kGF[ION_DENSITY]),
        Dperp_(&DperpCoef),
        B3_(&B3Coef), B_(3)
   {}

   // void SetTimeStep(double dt) { dt_ = dt; }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID);
   }

   void Eval_Func(Vector & V,
                  ElementTransformation &T,
                  const IntegrationPoint &ip)
   {
      V.SetSize(2);

      double ni = ni_.Eval(T, ip);
      double vi = vi_.Eval(T, ip);

      // double dni0 = dni0_.Eval(T, ip);
      // double dvi0 = dvi0_.Eval(T, ip);

      // double ni1 = ni0 + dt_ * dni0;
      // double vi1 = vi0 + dt_ * dvi0;

      grad_ni_.Eval(gni_, T, ip);
      // grad_dni0_.Eval(gdni0_, T, ip);

      // gni1_.SetSize(gni0_.Size());
      // add(gni0_, dt_, gdni0_, gni1_);

      double Dperp = Dperp_->Eval(T, ip);

      B3_->Eval(B_, T, ip);

      double Bmag2 = B_ * B_;
      double Bmag = sqrt(Bmag2);

      V[0] = mi_ * (ni * vi * B_[0] / Bmag +
                    Dperp * ((B_[1] * B_[1] + B_[2] * B_[2]) * gni_[0] -
                             B_[0] * B_[1] * gni_[1]) / Bmag2);
      V[1] = mi_ * (ni * vi * B_[1] / Bmag +
                    Dperp * ((B_[0] * B_[0] + B_[2] * B_[2]) * gni_[1] -
                             B_[0] * B_[1] * gni_[0]) / Bmag2);
   }
};

class StaticPressureCoef : public StateVariableCoef
{
private:
   FieldType fieldType_;
   int z_i_;
   StateVariableGridFunctionCoef &niCoef_;
   StateVariableGridFunctionCoef &TCoef_;

public:
   StaticPressureCoef(StateVariableGridFunctionCoef &niCoef,
                      StateVariableGridFunctionCoef &TCoef)
      : fieldType_(ION_TEMPERATURE),
        z_i_(1), niCoef_(niCoef), TCoef_(TCoef) {}

   StaticPressureCoef(int z_i,
                      StateVariableGridFunctionCoef &niCoef,
                      StateVariableGridFunctionCoef &TCoef)
      : fieldType_(ELECTRON_TEMPERATURE),
        z_i_(z_i), niCoef_(niCoef), TCoef_(TCoef) {}

   StaticPressureCoef(const StaticPressureCoef &other)
      : niCoef_(other.niCoef_),
        TCoef_(other.TCoef_)
   {
      derivType_ = other.derivType_;
      fieldType_ = other.fieldType_;
      z_i_       = other.z_i_;
   }

   virtual StaticPressureCoef * Clone() const
   {
      return new StaticPressureCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID ||
              deriv == ION_DENSITY || deriv == fieldType_);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ni = niCoef_.Eval(T, ip);
      double Ts = TCoef_.Eval(T, ip);

      return 1.5 * z_i_ * ni * Ts;
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double Ts = TCoef_.Eval(T, ip);

      return 1.5 * z_i_ * Ts;
   }

   double Eval_dTi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      if (fieldType_ == ION_TEMPERATURE)
      {
         double ni = niCoef_.Eval(T, ip);

         return 1.5 * z_i_ * ni;
      }
      else
      {
         return 0.0;
      }
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      if (fieldType_ == ELECTRON_TEMPERATURE)
      {
         double ni = niCoef_.Eval(T, ip);

         return 1.5 * z_i_ * ni;
      }
      else
      {
         return 0.0;
      }
   }
};

class IonThermalParaDiffusionCoef : public StateVariableCoef
{
private:
   double z_i_;
   double m_i_;
   double m_i_kg_;

   Coefficient * niCoef_;
   Coefficient * TiCoef_;

public:
   IonThermalParaDiffusionCoef(double ion_charge,
                               double ion_mass,
                               Coefficient &niCoef,
                               Coefficient &TiCoef)
      : StateVariableCoef(),
        z_i_(ion_charge), m_i_(ion_mass), m_i_kg_(ion_mass * amu_),
        niCoef_(&niCoef), TiCoef_(&TiCoef)
   {}

   IonThermalParaDiffusionCoef(const IonThermalParaDiffusionCoef &other)
   {
      derivType_ = other.derivType_;
      z_i_       = other.z_i_;
      m_i_       = other.m_i_;
      m_i_kg_    = other.m_i_kg_;
      niCoef_    = other.niCoef_;
      TiCoef_    = other.TiCoef_;

   }

   virtual IonThermalParaDiffusionCoef * Clone() const
   {
      return new IonThermalParaDiffusionCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ni = niCoef_->Eval(T, ip);
      double Ti = TiCoef_->Eval(T, ip);
      MFEM_VERIFY(ni > 0.0,
                  "Ion density (" << ni << ") "
                  "less than or equal to zero in "
                  "IonThermalParaDiffusionCoef.");
      MFEM_VERIFY(Ti > 0.0,
                  "Ion temperature (" << Ti << ") "
                  "less than or equal to zero in "
                  "IonThermalParaDiffusionCoef.");

      double tau = tau_i(m_i_, z_i_, ni, Ti, 17.0);
      // std::cout << "Chi_e parallel: " << 3.16 * ne * Te * eV_ * tau / me_kg_
      // << ", n_e: " << ne << ", T_e: " << Te << std::endl;
      return 3.9 * ni * Ti * eV_ * tau / m_i_kg_;
   }

};

class ElectronThermalParaDiffusionCoef : public StateVariableCoef
{
private:
   double z_i_;

   Coefficient * neCoef_;
   Coefficient * TeCoef_;

public:
   ElectronThermalParaDiffusionCoef(double ion_charge,
                                    Coefficient &neCoef,
                                    Coefficient &TeCoef,
                                    FieldType deriv = INVALID)
      : StateVariableCoef(deriv),
        z_i_(ion_charge), neCoef_(&neCoef), TeCoef_(&TeCoef)
   {}

   ElectronThermalParaDiffusionCoef(
      const ElectronThermalParaDiffusionCoef &other)
   {
      derivType_ = other.derivType_;
      z_i_ = other.z_i_;
      neCoef_ = other.neCoef_;
      TeCoef_ = other.TeCoef_;
   }

   virtual ElectronThermalParaDiffusionCoef * Clone() const
   {
      return new ElectronThermalParaDiffusionCoef(*this);
   }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID || deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ne = neCoef_->Eval(T, ip);
      double Te = TeCoef_->Eval(T, ip);
      MFEM_VERIFY(ne > 0.0,
                  "Electron density (" << ne << ") "
                  "less than or equal to zero in "
                  "ElectronThermalParaDiffusionCoef.");
      MFEM_VERIFY(Te > 0.0,
                  "Electron temperature (" << Te << ") "
                  "less than or equal to zero in "
                  "ElectronThermalParaDiffusionCoef.");

      double tau = tau_e(Te, z_i_, ne, 17.0);
      // std::cout << "Chi_e parallel: " << 3.16 * ne * Te * eV_ * tau / me_kg_
      // << ", n_e: " << ne << ", T_e: " << Te << std::endl;
      return 3.16 * ne * Te * eV_ * tau / me_kg_;
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      double ne = neCoef_->Eval(T, ip);
      double Te = TeCoef_->Eval(T, ip);
      MFEM_VERIFY(ne > 0.0,
                  "Electron density (" << ne << ") "
                  "less than or equal to zero in "
                  "ElectronThermalParaDiffusionCoef.");
      MFEM_VERIFY(Te > 0.0,
                  "Electron temperature (" << Te << ") "
                  "less than or equal to zero in "
                  "ElectronThermalParaDiffusionCoef.");

      double tau = tau_e(Te, z_i_, ne, 17.0);
      double dtau = dtau_e_dT(Te, z_i_, ne, 17.0);
      // std::cout << "Chi_e parallel: " << 3.16 * ne * Te * eV_ * tau / me_kg_
      // << ", n_e: " << ne << ", T_e: " << Te << std::endl;
      return 3.16 * ne * eV_ * (tau + Te * dtau)/ me_kg_;
   }

};

class VectorXYCoefficient : public VectorCoefficient
{
private:
   VectorCoefficient * V_;
   Vector v3_;

public:
   VectorXYCoefficient(VectorCoefficient &V)
      : VectorCoefficient(2), V_(&V), v3_(3) {}

   void Eval(Vector &v,
             ElementTransformation &T,
             const IntegrationPoint &ip)
   { v.SetSize(2); V_->Eval(v3_, T, ip); v[0] = v3_[0]; v[1] = v3_[1]; }
};

class VectorZCoefficient : public Coefficient
{
private:
   VectorCoefficient * V_;
   Vector v3_;

public:
   VectorZCoefficient(VectorCoefficient &V)
      : V_(&V), v3_(3) {}

   double Eval(ElementTransformation &T,
               const IntegrationPoint &ip)
   { V_->Eval(v3_, T, ip); return v3_[2]; }
};

class Aniso2DDiffusionCoef : public StateVariableMatCoef
{
private:
   Coefficient       * Para_;
   Coefficient       * Perp_;
   VectorCoefficient * B3_;

   mutable Vector B_;

public:
   Aniso2DDiffusionCoef(Coefficient &ParaCoef, Coefficient &PerpCoef,
                        VectorCoefficient &B3Coef)
      : StateVariableMatCoef(2),
        Para_(&ParaCoef), Perp_(&PerpCoef),
        B3_(&B3Coef), B_(3) {}

   Aniso2DDiffusionCoef(bool para, Coefficient &Coef,
                        VectorCoefficient &B3Coef)
      : StateVariableMatCoef(2),
        Para_(para ? &Coef : NULL), Perp_(para ? NULL : &Coef),
        B3_(&B3Coef), B_(3) {}

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID);
   }

   void Eval_Func(DenseMatrix & M,
                  ElementTransformation &T,
                  const IntegrationPoint &ip)
   {
      M.SetSize(2);

      double para = Para_ ? Para_->Eval(T, ip) : 0.0;
      double perp = Perp_ ? Perp_->Eval(T, ip) : 0.0;

      B3_->Eval(B_, T, ip);

      double Bmag2 = B_ * B_;

      M(0,0) = (B_[0] * B_[0] * para +
                (B_[1] * B_[1] + B_[2] * B_[2]) * perp ) / Bmag2;
      M(0,1) = B_[0] * B_[1] * (para - perp) / Bmag2;
      M(1,0) = M(0,1);
      M(1,1) = (B_[1] * B_[1] * para +
                (B_[0] * B_[0] + B_[2] * B_[2]) * perp ) / Bmag2;
   }
};

class GradPressureCoefficient : public StateVariableCoef
{
private:
   double zi_; // Stored as a double to avoid type casting in Eval methods
   double dt_;

   GridFunctionCoefficient ni0_;
   GridFunctionCoefficient Ti0_;
   GridFunctionCoefficient Te0_;

   GridFunctionCoefficient dni0_;
   GridFunctionCoefficient dTi0_;
   GridFunctionCoefficient dTe0_;

   GradientGridFunctionCoefficient grad_ni0_;
   GradientGridFunctionCoefficient grad_Ti0_;
   GradientGridFunctionCoefficient grad_Te0_;

   GradientGridFunctionCoefficient grad_dni0_;
   GradientGridFunctionCoefficient grad_dTi0_;
   GradientGridFunctionCoefficient grad_dTe0_;

   VectorCoefficient * B3_;

   mutable Vector gni0_;
   mutable Vector gTi0_;
   mutable Vector gTe0_;

   mutable Vector gdni0_;
   mutable Vector gdTi0_;
   mutable Vector gdTe0_;

   mutable Vector gni1_;
   mutable Vector gTi1_;
   mutable Vector gTe1_;

   mutable Vector B_;

public:
   GradPressureCoefficient(ParGridFunctionArray &yGF,
                           ParGridFunctionArray &kGF,
                           int zi, VectorCoefficient & B3Coef)
      : zi_((double)zi), dt_(0.0),
        ni0_(yGF[ION_DENSITY]),
        Ti0_(yGF[ION_TEMPERATURE]),
        Te0_(yGF[ELECTRON_TEMPERATURE]),
        dni0_(kGF[ION_DENSITY]),
        dTi0_(kGF[ION_TEMPERATURE]),
        dTe0_(kGF[ELECTRON_TEMPERATURE]),
        grad_ni0_(yGF[ION_DENSITY]),
        grad_Ti0_(yGF[ION_TEMPERATURE]),
        grad_Te0_(yGF[ELECTRON_TEMPERATURE]),
        grad_dni0_(kGF[ION_DENSITY]),
        grad_dTi0_(kGF[ION_TEMPERATURE]),
        grad_dTe0_(kGF[ELECTRON_TEMPERATURE]),
        B3_(&B3Coef), B_(3) {}

   GradPressureCoefficient(const GradPressureCoefficient & other)
      : ni0_(other.ni0_),
        Ti0_(other.Ti0_),
        Te0_(other.Te0_),
        dni0_(other.dni0_),
        dTi0_(other.dTi0_),
        dTe0_(other.dTe0_),
        grad_ni0_(ni0_.GetGridFunction()),
        grad_Ti0_(Ti0_.GetGridFunction()),
        grad_Te0_(Te0_.GetGridFunction()),
        grad_dni0_(dni0_.GetGridFunction()),
        grad_dTi0_(dTi0_.GetGridFunction()),
        grad_dTe0_(dTe0_.GetGridFunction()),
        B_(3)
   {
      derivType_ = other.derivType_;
      zi_ = other.zi_;
      dt_ = other.dt_;
      B3_ = other.B3_;
   }

   virtual GradPressureCoefficient * Clone() const
   {
      return new GradPressureCoefficient(*this);
   }

   void SetTimeStep(double dt) { dt_ = dt; }

   virtual bool NonTrivialValue(FieldType deriv) const
   {
      return (deriv == INVALID ||
              deriv == ION_DENSITY || deriv == ION_TEMPERATURE ||
              deriv == ELECTRON_TEMPERATURE);
   }

   double Eval_Func(ElementTransformation &T,
                    const IntegrationPoint &ip)
   {
      double ni0 = ni0_.Eval(T, ip);
      double Ti0 = Ti0_.Eval(T, ip);
      double Te0 = Te0_.Eval(T, ip);

      double dni0 = dni0_.Eval(T, ip);
      double dTi0 = dTi0_.Eval(T, ip);
      double dTe0 = dTe0_.Eval(T, ip);

      double ni1 = ni0 + dt_ * dni0;
      double Ti1 = Ti0 + dt_ * dTi0;
      double Te1 = Te0 + dt_ * dTe0;

      grad_ni0_.Eval(gni0_, T, ip);
      grad_Ti0_.Eval(gTi0_, T, ip);
      grad_Te0_.Eval(gTe0_, T, ip);

      grad_dni0_.Eval(gdni0_, T, ip);
      grad_dTi0_.Eval(gdTi0_, T, ip);
      grad_dTe0_.Eval(gdTe0_, T, ip);

      gni1_.SetSize(gni0_.Size());
      gTi1_.SetSize(gTi0_.Size());
      gTe1_.SetSize(gTe0_.Size());

      add(gni0_, dt_, gdni0_, gni1_);
      add(gTi0_, dt_, gdTi0_, gTi1_);
      add(gTe0_, dt_, gdTe0_, gTe1_);

      B3_->Eval(B_, T, ip);

      double Bmag = sqrt(B_ * B_);

      return eV_ * ((zi_ * Te1 + Ti1) * (B_[0] * gni1_[0] + B_[1] * gni1_[1]) +
                    (zi_ * (B_[0] * gTe1_[0] + B_[1] * gTe1_[1]) +
                     (B_[0] * gTi1_[0] + B_[1] * gTi1_[1])) * ni1) / Bmag;
   }

   double Eval_dNi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      grad_Ti0_.Eval(gTi0_, T, ip);
      grad_Te0_.Eval(gTe0_, T, ip);

      grad_dTi0_.Eval(gdTi0_, T, ip);
      grad_dTe0_.Eval(gdTe0_, T, ip);

      gTi1_.SetSize(gTi0_.Size());
      gTe1_.SetSize(gTe0_.Size());

      add(gTi0_, dt_, gdTi0_, gTi1_);
      add(gTe0_, dt_, gdTe0_, gTe1_);

      B3_->Eval(B_, T, ip);

      double Bmag = sqrt(B_ * B_);

      return eV_ * (dt_ * zi_ * (B_[0] * gTe1_[0] + B_[1] * gTe1_[1]) +
                    (B_[0] * gTi1_[0] + B_[1] * gTi1_[1])) / Bmag;
   }

   double Eval_dTi(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      grad_ni0_.Eval(gni0_, T, ip);

      grad_dni0_.Eval(gdni0_, T, ip);

      gni1_.SetSize(gni0_.Size());

      add(gni0_, dt_, gdni0_, gni1_);

      B3_->Eval(B_, T, ip);

      double Bmag = sqrt(B_ * B_);

      return eV_ * dt_ * (B_[0] * gni1_[0] + B_[1] * gni1_[1]) / Bmag;
   }

   double Eval_dTe(ElementTransformation &T,
                   const IntegrationPoint &ip)
   {
      grad_ni0_.Eval(gni0_, T, ip);

      grad_dni0_.Eval(gdni0_, T, ip);

      gni1_.SetSize(gni0_.Size());

      add(gni0_, dt_, gdni0_, gni1_);

      B3_->Eval(B_, T, ip);

      double Bmag = sqrt(B_ * B_);

      return eV_ * dt_ * zi_ * (B_[0] * gni1_[0] + B_[1] * gni1_[1]) / Bmag;
   }
};

struct DGParams
{
   double sigma;
   double kappa;
};

struct PlasmaParams
{
   double m_n;
   double T_n;
   double m_i;
   int    z_i;
};

class DGAdvectionDiffusionTDO : public TimeDependentOperator
{
private:
   const DGParams & dg_;

   bool imex_;
   int logging_;
   std::string log_prefix_;
   double dt_;

   ParFiniteElementSpace *fes_;
   ParGridFunctionArray  *pgf_;

   Coefficient       *CCoef_;    // Scalar coefficient in front of du/dt
   VectorCoefficient *VCoef_;    // Velocity coefficient
   Coefficient       *dCoef_;    // Scalar diffusion coefficient
   MatrixCoefficient *DCoef_;    // Tensor diffusion coefficient
   Coefficient       *SCoef_;    // Source coefficient

   ScalarVectorProductCoefficient *negVCoef_;   // -1  * VCoef
   ScalarVectorProductCoefficient *dtNegVCoef_; // -dt * VCoef
   ProductCoefficient             *dtdCoef_;    //  dt * dCoef
   ScalarMatrixProductCoefficient *dtDCoef_;    //  dt * DCoef

   Array<int>   dbcAttr_;
   Coefficient *dbcCoef_; // Dirichlet BC coefficient

   Array<int>   nbcAttr_;
   Coefficient *nbcCoef_; // Neumann BC coefficient

   ParBilinearForm  m_;
   ParBilinearForm *a_;
   ParBilinearForm *b_;
   ParBilinearForm *s_;
   ParBilinearForm *k_;
   ParLinearForm   *q_exp_;
   ParLinearForm   *q_imp_;

   HypreParMatrix * M_;
   HypreSmoother M_prec_;
   CGSolver M_solver_;

   // HypreParMatrix * B_;
   // HypreParMatrix * S_;

   mutable ParLinearForm rhs_;
   mutable Vector RHS_;
   mutable Vector X_;

   void initM();
   void initA();
   void initB();
   void initS();
   void initK();
   void initQ();

public:
   DGAdvectionDiffusionTDO(const DGParams & dg,
                           ParFiniteElementSpace &fes,
                           ParGridFunctionArray &pgf,
                           Coefficient &CCoef, bool imex = true);

   ~DGAdvectionDiffusionTDO();

   void SetTime(const double _t);

   void SetLogging(int logging, const std::string & prefix = "");

   void SetAdvectionCoefficient(VectorCoefficient &VCoef);
   void SetDiffusionCoefficient(Coefficient &dCoef);
   void SetDiffusionCoefficient(MatrixCoefficient &DCoef);
   void SetSourceCoefficient(Coefficient &SCoef);

   void SetDirichletBC(Array<int> &dbc_attr, Coefficient &dbc);
   void SetNeumannBC(Array<int> &nbc_attr, Coefficient &nbc);

   virtual void ExplicitMult(const Vector &x, Vector &y) const;
   virtual void ImplicitSolve(const double dt, const Vector &u, Vector &dudt);

   void Update();
};

class TransportPrec : public BlockDiagonalPreconditioner
{
private:
   Array<Operator*> diag_prec_;

public:
   TransportPrec(const Array<int> &offsets);
   ~TransportPrec();

   virtual void SetOperator(const Operator &op);
};

class DGTransportTDO : public TimeDependentOperator
{
private:
   const MPI_Session & mpi_;
   int logging_;

   ParFiniteElementSpace &fes_;
   ParFiniteElementSpace &vfes_;
   ParFiniteElementSpace &ffes_;
   ParGridFunctionArray  &yGF_;
   ParGridFunctionArray  &kGF_;

   Array<int> &offsets_;

   // HypreSmoother newton_op_prec_;
   // Array<HypreSmoother*> newton_op_prec_blocks_;
   TransportPrec newton_op_prec_;
   GMRESSolver   newton_op_solver_;
   NewtonSolver  newton_solver_;

   // Data collection used to write data files
   DataCollection * dc_;

   // Sockets used to communicate with GLVis
   std::map<std::string, socketstream*> socks_;

   class NLOperator : public Operator
   {
   private:
      StateVariableGridFunctionCoef dummyCoef_;
      Array<StateVariableCoef*> coefs_;
      Array<ProductCoefficient*>             dtSCoefs_;
      Array<ProductCoefficient*>             negdtSCoefs_;
      Array<ScalarVectorProductCoefficient*> dtVCoefs_;
      Array<ScalarMatrixProductCoefficient*> dtMCoefs_;
      std::vector<socketstream*> sout_;
      ParGridFunction coefGF_;

   protected:
      const MPI_Session &mpi_;
      const DGParams &dg_;

      int logging_;
      std::string log_prefix_;

      int index_;
      std::string field_name_;
      double dt_;
      ParFiniteElementSpace &fes_;
      ParMesh               &pmesh_;
      ParGridFunctionArray  &yGF_;
      ParGridFunctionArray  &kGF_;

      Array<StateVariableGridFunctionCoef*> yCoefPtrs_;
      Array<StateVariableGridFunctionCoef*> kCoefPtrs_;

      // mutable Vector shape_;
      // mutable DenseMatrix dshape_;
      // mutable DenseMatrix dshapedxt_;
      mutable Array<int> vdofs_;
      mutable Array<int> vdofs2_;
      mutable DenseMatrix elmat_;
      mutable DenseMatrix elmat_k_;
      mutable Vector elvec_;
      mutable Vector locvec_;
      mutable Vector locdvec_;
      // mutable Vector vec_;

      // Domain integrators for time derivatives of field variables
      Array<Array<BilinearFormIntegrator*> > dbfi_m_;  // Domain Integrators
      // Array<Array<StateVariableCoef*> >      dbfi_mc_; // Domain Integrators

      // Domain integrators for field variables at next time step
      Array<BilinearFormIntegrator*> dbfi_;  // Domain Integrators
      Array<BilinearFormIntegrator*> fbfi_;  // Interior Face Integrators
      Array<BilinearFormIntegrator*> bfbfi_; // Boundary Face Integrators
      Array<Array<int>*>             bfbfi_marker_; ///< Entries are not owned.

      // Domain integrators for source terms
      Array<LinearFormIntegrator*> dlfi_;  // Domain Integrators
      Array<LinearFormIntegrator*> flfi_;
      Array<Array<int>*>           flfi_marker_; ///< Entries are owned.

      Array<ParBilinearForm*> blf_; // Bilinear Form Objects for Gradients

      int term_flag_;
      int vis_flag_;

      // Data collection used to write data files
      DataCollection * dc_;

      // Sockets used to communicate with GLVis
      std::map<std::string, socketstream*> socks_;

      NLOperator(const MPI_Session & mpi, const DGParams & dg,
                 int index,
                 const std::string &field_name,
                 ParGridFunctionArray & yGF,
                 ParGridFunctionArray & kGF,
                 int term_flag, int vis_flag, int logging = 0,
                 const std::string & log_prefix = "");


      /** Sets the time derivative on the left hand side of the equation to be:
             d MCoef / dt
      */
      void SetTimeDerivativeTerm(StateVariableCoef &MCoef);

      /** Sets the diffusion term on the right hand side of the equation
          to be:
             Div(DCoef Grad y[index])
          where index is the index of the equation.
       */
      void SetDiffusionTerm(StateVariableCoef &DCoef, bool bc = false);
      void SetDiffusionTerm(StateVariableMatCoef &DCoef, bool bc = false);

      /** Sets the advection term on the right hand side of the
      equation to be:
             Div(VCoef y[index])
          where index is the index of the equation.
       */
      void SetAdvectionTerm(StateVariableVecCoef &VCoef, bool bc = false);

      void SetSourceTerm(StateVariableCoef &SCoef);

   public:

      virtual ~NLOperator();

      void SetLogging(int logging, const std::string & prefix = "");

      virtual void SetTimeStep(double dt);

      virtual void Mult(const Vector &k, Vector &y) const;

      virtual void Update();
      virtual Operator *GetGradientBlock(int i);

      inline bool CheckTermFlag(int flag) { return (term_flag_>> flag) & 1; }

      inline bool CheckVisFlag(int flag) { return (vis_flag_>> flag) & 1; }

      virtual int GetDefaultVisFlag() { return 1; }

      virtual void RegisterDataFields(DataCollection & dc);

      virtual void PrepareDataFields();

      virtual void InitializeGLVis();

      virtual void DisplayToGLVis();
   };

   class TransportOp : public NLOperator
   {
   protected:
      const PlasmaParams &plasma_;

      double m_n_;
      double T_n_;
      double v_n_;
      double m_i_;
      int    z_i_;

      StateVariableGridFunctionCoef &nn0Coef_;
      StateVariableGridFunctionCoef &ni0Coef_;
      StateVariableGridFunctionCoef &vi0Coef_;
      StateVariableGridFunctionCoef &Ti0Coef_;
      StateVariableGridFunctionCoef &Te0Coef_;

      ProductCoefficient      ne0Coef_;

      TransportOp(const MPI_Session & mpi, const DGParams & dg,
                  const PlasmaParams & plasma, int index,
                  const std::string &field_name,
                  ParGridFunctionArray & yGF,
                  ParGridFunctionArray & kGF,
                  int term_flag, int vis_flag,
                  int logging = 0,
                  const std::string & log_prefix = "")
         : NLOperator(mpi, dg, index, field_name,
                      yGF, kGF, term_flag, vis_flag, logging, log_prefix),
           plasma_(plasma),
           m_n_(plasma.m_n),
           T_n_(plasma.T_n),
           v_n_(sqrt(8.0 * T_n_ * eV_ / (M_PI * m_n_ * amu_))),
           m_i_(plasma.m_i),
           z_i_(plasma.z_i),
           nn0Coef_(*yCoefPtrs_[NEUTRAL_DENSITY]),
           ni0Coef_(*yCoefPtrs_[ION_DENSITY]),
           vi0Coef_(*yCoefPtrs_[ION_PARA_VELOCITY]),
           Ti0Coef_(*yCoefPtrs_[ION_TEMPERATURE]),
           Te0Coef_(*yCoefPtrs_[ELECTRON_TEMPERATURE]),
           ne0Coef_(z_i_, ni0Coef_)
      {}

   };

   /** The NeutralDensityOp is an mfem::Operator designed to work with
       a NewtonSolver as one row in a block system of non-linear
       transport equations.  Specifically, this operator models the
       mass conservation equation for a neutral species.

          d n_n / dt = Div(D_n Grad(n_n)) + S_n

       Where the diffusion coefficient D_n is a function of n_e and T_e
       (the electron density and temperature respectively) and the
       source term S_n is a function of n_e, T_e, and n_n.  Note that n_e is
       not a state variable but is related to n_i by the simple relation
          n_e = z_i n_i
       where z_i is the charge of the ions and n_i is the ion density.

       To advance this equation in time we need to find k_nn = d n_n / dt
       which satisfies:
          k_nn - Div(D_n(n_e, T_e) Grad(n_n + dt k_nn))
               - S_n(n_e, T_e, n_n + dt k_nn) = 0
       Where n_e and T_e are also evaluated at the next time step.  This is
       done with a Newton solver which needs the Jacobian of this block of
       equations.

       The diagonal block is given by:
          1 - dt Div(D_n Grad) - dt d S_n / d n_n

       The other non-trivial blocks are:
          - dt Div(d D_n / d n_i Grad(n_n)) - dt d S_n / d n_i
          - dt Div(d D_n / d T_e Grad(n_n)) - dt d S_n / d T_e

       The blocks of the Jacobian will be assembled finite element matrices.
       For the diagonal block we need a mass integrator with coefficient
       (1 - dt d S_n / d n_n), and a set of integrators to model the DG
       diffusion operator with coefficient (dt D_n).

       The off-diagonal blocks will consist of a mass integrator with
       coefficient (-dt d S_n / d n_i) or (-dt d S_n / d
       T_e). Currently, (-dt d S_n / d T_e) is not implemented.
    */
   class NeutralDensityOp : public TransportOp
   {
   private:
      enum TermFlag {DIFFUSION_TERM = 0, SOURCE_TERM = 1};
      enum VisField {DIFFUSION_COEF = 0, SOURCE_COEF = 1};
      /*
       int    z_i_;
       double m_n_;
       double T_n_;
      */
      /*
       StateVariableGridFunctionCoef &nn0Coef_;
       StateVariableGridFunctionCoef &ni0Coef_;
       StateVariableGridFunctionCoef &Te0Coef_;
      */
      // SumCoefficient &nn1Coef_;
      // SumCoefficient &ni1Coef_;
      // SumCoefficient &Te1Coef_;

      // ProductCoefficient      ne0Coef_;
      // ProductCoefficient      ne1Coef_;
      /*
       mutable GridFunctionCoefficient nn0Coef_;
       mutable GridFunctionCoefficient ni0Coef_;
       mutable GridFunctionCoefficient Te0Coef_;

       GridFunctionCoefficient dnnCoef_;
       GridFunctionCoefficient dniCoef_;
       GridFunctionCoefficient dTeCoef_;

       mutable SumCoefficient  nn1Coef_;
       mutable SumCoefficient  ni1Coef_;
       mutable SumCoefficient  Te1Coef_;
      */
      // ProductCoefficient      ne0Coef_;
      // ProductCoefficient      ne1Coef_;
      ConstantCoefficient      vnCoef_;
      ApproxIonizationRate     izCoef_;
      ApproxRecombinationRate  rcCoef_;

      NeutralDiffusionCoef      DCoef_;
      // ProductCoefficient      dtDCoef_;

      IonSourceCoef           SizCoef_;
      IonSinkCoef             SrcCoef_;

      //ProductCoefficient   negSrcCoef_;

      StateVariableSumCoef SCoef_;

      // IonSourceCoef       dSizdnnCoef_;
      // IonSourceCoef       dSizdniCoef_;

      // ProductCoefficient dtdSizdnnCoef_;
      // ProductCoefficient dtdSizdniCoef_;

      ParGridFunction * DGF_;
      ParGridFunction * SGF_;

      // ProductCoefficient     nnizCoef_; // nn * iz
      // ProductCoefficient     neizCoef_; // ne * iz
      // ProductCoefficient dtdSndnnCoef_; // - dt * dSn/dnn
      // ProductCoefficient dtdSndniCoef_; // - dt * dSn/dni

      // mutable DiffusionIntegrator   diff_;
      // mutable DGDiffusionIntegrator dg_diff_;

   public:
      NeutralDensityOp(const MPI_Session & mpi, const DGParams & dg,
                       const PlasmaParams & plasma,
                       ParGridFunctionArray & yGF,
                       ParGridFunctionArray & kGF,
                       int term_flag = 3,
                       int vis_flag = 0, int logging = 0,
                       const std::string & log_prefix = "");

      virtual ~NeutralDensityOp();

      virtual void SetTimeStep(double dt);

      void Update();

      int GetDefaultVisFlag() { return 7; }

      virtual void RegisterDataFields(DataCollection & dc);

      virtual void PrepareDataFields();

      // void Mult(const Vector &k, Vector &y) const;

      // Operator *GetGradientBlock(int i);
   };

   /** The IonDensityOp is an mfem::Operator designed to worth with a
       NewtonSolver as one row in a block system of non-linear
       transport equations.  Specifically, this operator models the
       mass conservation equation for a single ion species.

       d n_i / dt = Div(D_i Grad n_i)) - Div(v_i n_i b_hat) + S_i

       Where the diffusion coefficient D_i is a function of the
       magnetic field direction, v_i is the velocity of the ions
       parallel to B, and the source term S_i is a function of the
       electron and neutral densities as well as the electron
       temperature.

       To advance this equation in time we need to find k_ni = d n_i / dt
       which satisfies:
          k_ni - Div(D_i Grad(n_i + dt k_ni)) + Div(v_i (n_i + dt k_ni) b_hat)
               - S_i(n_e + z_i dt k_ni, T_e, n_n) = 0
       Where n_n and T_e are also evaluated at the next time step.  This is
       done with a Newton solver which needs the Jacobian of this block of
       equations.

       The diagonal block is given by:
          1 - dt Div(D_i Grad) + dt Div(v_i b_hat) - dt d S_i / d n_i

       The other non-trivial blocks are:
          - dt d S_i / d n_n
          + dt Div(n_i b_hat)
          - dt d S_i / d T_e

       The blocks of the Jacobian will be assembled finite element
       matrices.  For the diagonal block we need a mass integrator
       with coefficient (1 - dt d S_i / d n_i), a set of integrators
       to model the DG diffusion operator with coefficient (dt D_i),
       and a weak divergence integrator with coefficient (dt v_i).

       The off-diagonal blocks will consist of a mass integrator with
       coefficient (-dt d S_i / d n_n) or (-dt d S_i / d T_e).
       Currently, (dt Div(n_i b_hat)) and (-dt d S_i / d T_e) are not
       implemented.
    */
   class IonDensityOp : public TransportOp
   {
   private:
      enum TermFlag {DIFFUSION_TERM = 0,
                     ADVECTION_TERM = 1, SOURCE_TERM = 2
                    };
      enum VisField {DIFFUSION_PERP_COEF = 0,
                     ADVECTION_COEF = 1, SOURCE_COEF = 2
                    };

      // int    z_i_;
      double DPerpConst_;

      // StateVariableGridFunctionCoef &ni0Coef_;

      // SumCoefficient &nn1Coef_;
      // SumCoefficient &ni1Coef_;
      // SumCoefficient &Te1Coef_;

      // ProductCoefficient ne1Coef_;
      /*
        GridFunctionCoefficient nn0Coef_;
        GridFunctionCoefficient ni0Coef_;
        GridFunctionCoefficient vi0Coef_;
        GridFunctionCoefficient Te0Coef_;

        GridFunctionCoefficient dnnCoef_;
        GridFunctionCoefficient dniCoef_;
        GridFunctionCoefficient dviCoef_;
        GridFunctionCoefficient dTeCoef_;

        mutable SumCoefficient  nn1Coef_;
        mutable SumCoefficient  ni1Coef_;
        mutable SumCoefficient  vi1Coef_;
        mutable SumCoefficient  Te1Coef_;
       */
      //ProductCoefficient      ne0Coef_;
      // ProductCoefficient      ne1Coef_;

      ApproxIonizationRate     izCoef_;
      ApproxRecombinationRate  rcCoef_;

      ConstantCoefficient DPerpCoef_;
      // MatrixCoefficient * PerpCoef_;
      // ScalarMatrixProductCoefficient DCoef_;
      IonDiffusionCoef DCoef_;
      // ScalarMatrixProductCoefficient dtDCoef_;

      // VectorCoefficient * B3Coef_;
      IonAdvectionCoef ViCoef_;
      // ScalarVectorProductCoefficient ViCoef_;
      // ScalarVectorProductCoefficient dtViCoef_;

      IonSourceCoef           SizCoef_;
      IonSinkCoef             SrcCoef_;

      StateVariableSumCoef SCoef_;
      // ProductCoefficient   negSizCoef_;

      // IonSourceCoef           dSizdnnCoef_;
      // IonSourceCoef           dSizdniCoef_;

      // ProductCoefficient   negdtdSizdnnCoef_;
      // ProductCoefficient   negdtdSizdniCoef_;

      // ProductCoefficient nnizCoef_;
      // ProductCoefficient niizCoef_;

      ParGridFunction * DPerpGF_;
      ParGridFunction * AGF_;
      ParGridFunction * SGF_;

   public:
      IonDensityOp(const MPI_Session & mpi, const DGParams & dg,
                   const PlasmaParams & plasma,
                   ParFiniteElementSpace & vfes,
                   ParGridFunctionArray & yGF,
                   ParGridFunctionArray & kGF,
                   double DPerp,
                   VectorCoefficient & B3Coef,
                   int term_flag = 7, int vis_flag = 0, int logging = 0,
                   const std::string & log_prefix = "");

      virtual ~IonDensityOp();

      virtual void SetTimeStep(double dt);

      void Update();

      int GetDefaultVisFlag() { return 11; }

      virtual void RegisterDataFields(DataCollection & dc);

      virtual void PrepareDataFields();
   };

   /** The IonMomentumOp is an mfem::Operator designed to work with a
       NewtonSolver as one row in a block system of non-linear
       transport equations.  Specifically, this operator models the
       momentum conservation equation for a single ion species.

       m_i v_i d n_i / dt + m_i n_i d v_i / dt
          = Div(Eta Grad v_i) - Div(m_i n_i v_i v_i) - b.Grad(p_i + p_e)

       Where the diffusion coefficient Eta is a function of the
       magnetic field, ion density, and ion temperature.

       To advance this equation in time we need to find k_vi = d v_i / dt
       which satisfies:
          m_i n_i k_vi - Div(Eta Grad(v_i + dt k_vi))
             + Div(m_i n_i v_i (v_i + dt k_vi)) + b.Grad(p_i + p_e) = 0
       Where n_i, p_i, and p_e are also evaluated at the next time
       step.  This is done with a Newton solver which needs the
       Jacobian of this block of equations.

       The diagonal block is given by:
          m_i n_i - dt Div(Eta Grad) + dt Div(m_i n_i v_i)
       MLS: Why is the advection term not doubled?

       The other non-trivial blocks are:
          m_i v_i - dt Div(d Eta / d n_i Grad(v_i)) + dt Div(m_i v_i v_i)
          - dt Div(d Eta / d T_i Grad(v_i)) + dt b.Grad(d p_i / d T_i)
          + dt b.Grad(d p_e / d T_e)

       Currently, the static pressure terms and the derivatives of Eta
       do not contribute to the Jacobian.
    */
   class IonMomentumOp : public TransportOp
   {
   private:
      enum TermFlag {DIFFUSION_TERM = 0,
                     ADVECTION_TERM = 1, SOURCE_TERM = 2
                    };
      enum VisField {DIFFUSION_PARA_COEF = 0, DIFFUSION_PERP_COEF = 1,
                     ADVECTION_COEF = 2, SOURCE_COEF = 3
                    };

      // int    z_i_;
      // double m_i_;
      double DPerpConst_;
      ConstantCoefficient DPerpCoef_;

      // StateVariableGridFunctionCoef &ni0Coef_;
      // StateVariableGridFunctionCoef &vi0Coef_;
      // StateVariableGridFunctionCoef &Ti0Coef_;

      // SumCoefficient &nn1Coef_;
      // SumCoefficient &ni1Coef_;
      // SumCoefficient &Te1Coef_;
      /*
        GridFunctionCoefficient nn0Coef_;
        GridFunctionCoefficient ni0Coef_;
        GridFunctionCoefficient vi0Coef_;
        GridFunctionCoefficient Ti0Coef_;
        GridFunctionCoefficient Te0Coef_;

        GridFunctionCoefficient dnnCoef_;
        GridFunctionCoefficient dniCoef_;
        GridFunctionCoefficient dviCoef_;
        GridFunctionCoefficient dTiCoef_;
        GridFunctionCoefficient dTeCoef_;

        mutable SumCoefficient  nn1Coef_;
        mutable SumCoefficient  ni1Coef_;
        mutable SumCoefficient  vi1Coef_;
        mutable SumCoefficient  Ti1Coef_;
        mutable SumCoefficient  Te1Coef_;
       */
      // ProductCoefficient      ne0Coef_;
      // ProductCoefficient      ne1Coef_;

      // ProductCoefficient    mini1Coef_;
      // ProductCoefficient    mivi1Coef_;

      VectorCoefficient * B3Coef_;

      IonMomentumParaCoef            momCoef_;
      IonMomentumParaDiffusionCoef   EtaParaCoef_;
      IonMomentumPerpDiffusionCoef   EtaPerpCoef_;
      Aniso2DDiffusionCoef           EtaCoef_;
      // ScalarMatrixProductCoefficient dtEtaCoef_;

      IonMomentumAdvectionCoef miniViCoef_;
      // ScalarVectorProductCoefficient dtminiViCoef_;

      GradPressureCoefficient gradPCoef_;

      ApproxIonizationRate     izCoef_;

      IonSourceCoef           SizCoef_;
      ProductCoefficient   negSizCoef_;

      ProductCoefficient nnizCoef_;
      ProductCoefficient niizCoef_;

      ParGridFunction * EtaParaGF_;
      ParGridFunction * EtaPerpGF_;
      ParGridFunction * MomParaGF_;
      ParGridFunction * SGF_;

   public:
      IonMomentumOp(const MPI_Session & mpi, const DGParams & dg,
                    const PlasmaParams & plasma,
                    ParFiniteElementSpace & vfes,
                    ParGridFunctionArray & yGF, ParGridFunctionArray & kGF,
                    // int ion_charge, double ion_mass,
                    double DPerp,
                    VectorCoefficient & B3Coef,
                    int term_flag = 7, int vis_flag = 0, int logging = 0,
                    const std::string & log_prefix = "");

      virtual ~IonMomentumOp();

      virtual void SetTimeStep(double dt);

      void Update();

      int GetDefaultVisFlag() { return 19; }

      virtual void RegisterDataFields(DataCollection & dc);

      virtual void PrepareDataFields();
   };

   /** The IonStaticPressureOp is an mfem::Operator designed to work
       with a NewtonSolver as one row in a block system of non-linear
       transport equations.  Specifically, this operator models the
       static pressure equation (related to conservation of energy)
       for a single ion species.

       1.5 T_i d n_i / dt + 1.5 n_i d T_i / dt
          = Div(Chi_i Grad(T_i))

       Where the diffusion coefficient Chi_i is a function of the
       magnetic field direction, ion density and temperature.

       MLS: Clearly this equation is incomplete.  We stopped at this
       point to focus on implementing a non-linear Robin boundary
       condition.

       To advance this equation in time we need to find
       k_Ti = d T_i / dt which satisfies:
          (3/2)(T_i d n_i / dt + n_i k_Ti) - Div(Chi_i Grad T_i) = 0
       Where n_i is also evaluated at the next time step.  This is
       done with a Newton solver which needs the Jacobian of this
       block of equations.

       The diagonal block is given by:
          1.5 n_i - dt Div(Chi_i Grad)

       The other non-trivial blocks are:
          1.5 T_i - dt Div(d Chi_i / d n_i Grad(T_i))

       MLS: Many more terms will arise once the full equation is implemented.
   */
   class IonStaticPressureOp : public TransportOp
   {
   private:
      enum TermFlag {DIFFUSION_TERM = 0, SOURCE_TERM = 1};
      enum VisField {DIFFUSION_COEF = 0, SOURCE_COEF = 1};

      // int    z_i_;
      // double m_i_;
      double ChiPerpConst_;

      // StateVariableGridFunctionCoef &ni0Coef_;
      // StateVariableGridFunctionCoef &Ti0Coef_;
      /*
        GridFunctionCoefficient nn0Coef_;
        GridFunctionCoefficient ni0Coef_;
        GridFunctionCoefficient vi0Coef_;
        GridFunctionCoefficient Ti0Coef_;
        GridFunctionCoefficient Te0Coef_;

        GridFunctionCoefficient dnnCoef_;
        GridFunctionCoefficient dniCoef_;
        GridFunctionCoefficient dviCoef_;
        GridFunctionCoefficient dTiCoef_;
        GridFunctionCoefficient dTeCoef_;

        mutable SumCoefficient  nn1Coef_;
        mutable SumCoefficient  ni1Coef_;
        mutable SumCoefficient  vi1Coef_;
        mutable SumCoefficient  Ti1Coef_;
        mutable SumCoefficient  Te1Coef_;
       */
      // ProductCoefficient      ne0Coef_;
      // ProductCoefficient      ne1Coef_;

      // ProductCoefficient      thTiCoef_; // 3/2 * Ti
      // ProductCoefficient      thniCoef_; // 3/2 * ni

      ApproxIonizationRate     izCoef_;

      VectorCoefficient *      B3Coef_;

      StaticPressureCoef               presCoef_;
      IonThermalParaDiffusionCoef      ChiParaCoef_;
      ProductCoefficient               ChiPerpCoef_;
      Aniso2DDiffusionCoef             ChiCoef_;
      // ScalarMatrixProductCoefficient   dtChiCoef_;

      const std::vector<CoefficientByAttr> & dbc_;

      ParGridFunction * ChiParaGF_;
      ParGridFunction * ChiPerpGF_;

   public:
      IonStaticPressureOp(const MPI_Session & mpi, const DGParams & dg,
                          const PlasmaParams & plasma,
                          ParGridFunctionArray & yGF,
                          ParGridFunctionArray & kGF,
                          // int ion_charge, double ion_mass,
                          double ChiPerp,
                          VectorCoefficient & B3Coef,
                          std::vector<CoefficientByAttr> & dbc,
                          int term_flag = 0, int vis_flag = 0, int logging = 0,
                          const std::string & log_prefix = "");

      virtual ~IonStaticPressureOp();

      virtual void SetTimeStep(double dt);

      void Update();

      virtual void RegisterDataFields(DataCollection & dc);

      virtual void PrepareDataFields();
   };

   /** The ElectronStaticPressureOp is an mfem::Operator designed to
       work with a NewtonSolver as one row in a block system of
       non-linear transport equations.  Specifically, this operator
       models the static pressure equation (related to conservation of
       energy) for the flow of electrons.

       1.5 T_e d n_e / dt + 1.5 n_e d T_e / dt
          = Div(Chi_e Grad(T_e))

       Where the diffusion coefficient Chi_e is a function of the
       magnetic field direction, electron density and temperature.

       MLS: Clearly this equation is incomplete.  We stopped at this
       point to focus on implementing a non-linear Robin boundary
       condition.

       To advance this equation in time we need to find
       k_Te = d T_e / dt which satisfies:
          (3/2)(T_e d n_e / dt + n_e k_Te) - Div(Chi_e Grad T_e) = 0
       Where n_e is also evaluated at the next time step.  This is
       done with a Newton solver which needs the Jacobian of this
       block of equations.

       The diagonal block is given by:
          1.5 n_e - dt Div(Chi_e Grad)

       The other non-trivial blocks are:
          1.5 T_e - dt Div(d Chi_e / d n_e Grad(T_e))

       MLS: Many more terms will arise once the full equation is implemented.
   */
   class ElectronStaticPressureOp : public TransportOp
   {
   private:
      enum TermFlag {DIFFUSION_TERM = 0, SOURCE_TERM = 1};
      enum VisField {DIFFUSION_COEF = 0, SOURCE_COEF = 1};

      // int    z_i_;
      // double m_i_;
      double ChiPerpConst_;

      // StateVariableGridFunctionCoef &ni0Coef_;
      // StateVariableGridFunctionCoef &Te0Coef_;
      /*
       GridFunctionCoefficient nn0Coef_;
       GridFunctionCoefficient ni0Coef_;
       GridFunctionCoefficient vi0Coef_;
       GridFunctionCoefficient Ti0Coef_;
       GridFunctionCoefficient Te0Coef_;

       GridFunctionCoefficient dnnCoef_;
       GridFunctionCoefficient dniCoef_;
       GridFunctionCoefficient dviCoef_;
       GridFunctionCoefficient dTiCoef_;
       GridFunctionCoefficient dTeCoef_;
      */
      // GradientGridFunctionCoefficient grad_Te0Coef_;
      // GradientGridFunctionCoefficient grad_dTeCoef_;
      /*
       mutable SumCoefficient  nn1Coef_;
       mutable SumCoefficient  ni1Coef_;
       mutable SumCoefficient  vi1Coef_;
       mutable SumCoefficient  Ti1Coef_;
       mutable SumCoefficient  Te1Coef_;
      */
      // mutable VectorSumCoefficient  grad_Te1Coef_;

      // ProductCoefficient      ne0Coef_;
      // ProductCoefficient      ne1Coef_;

      // ProductCoefficient      thTeCoef_; // 3/2 * Te
      // ProductCoefficient      thneCoef_; // 3/2 * ne

      ApproxIonizationRate     izCoef_;

      VectorCoefficient *      B3Coef_;

      StaticPressureCoef               presCoef_;
      ElectronThermalParaDiffusionCoef ChiParaCoef_;
      // ElectronThermalParaDiffusionCoef dChidTParaCoef_;
      ProductCoefficient               ChiPerpCoef_;
      Aniso2DDiffusionCoef             ChiCoef_;
      // ScalarMatrixProductCoefficient   dtChiCoef_;

      // Aniso2DDiffusionCoef             dChidTCoef_;
      // MatVecCoefficient                dChiGradTCoef_;
      // ScalarVectorProductCoefficient   dtdChiGradTCoef_;

      const std::vector<CoefficientByAttr> & dbc_;

      ParGridFunction * ChiParaGF_;
      ParGridFunction * ChiPerpGF_;

   public:
      ElectronStaticPressureOp(const MPI_Session & mpi, const DGParams & dg,
                               const PlasmaParams & plasma,
                               ParGridFunctionArray & yGF,
                               ParGridFunctionArray & kGF,
                               // int ion_charge, double ion_mass,
                               double ChiPerp,
                               VectorCoefficient & B3Coef,
                               std::vector<CoefficientByAttr> & dbc,
                               int term_flag = 0, int vis_flag = 0,
                               int logging = 0,
                               const std::string & log_prefix = "");

      virtual ~ElectronStaticPressureOp();

      virtual void SetTimeStep(double dt);

      void RegisterDataFields(DataCollection & dc);

      void PrepareDataFields();

      void Update();
   };

   class DummyOp : public TransportOp
   {
   public:
      DummyOp(const MPI_Session & mpi, const DGParams & dg,
              const PlasmaParams & plasma,
              ParGridFunctionArray & yGF,
              ParGridFunctionArray & kGF,
              int index, const std::string &field_name,
              int term_flag = 0, int vis_flag = 0,
              int logging = 0, const std::string & log_prefix = "");

      virtual void SetTimeStep(double dt)
      {
         if (mpi_.Root() && logging_ > 0)
         {
            std::cout << "Setting time step: " << dt << " in DummyOp\n";
         }
         NLOperator::SetTimeStep(dt);
      }

      void Update();
   };

   class CombinedOp : public Operator
   {
   private:
      const MPI_Session & mpi_;
      int neq_;
      int logging_;

      ParFiniteElementSpace &fes_;
      ParGridFunctionArray  &yGF_;
      ParGridFunctionArray  &kGF_;
      /*
       NeutralDensityOp n_n_op_;
       IonDensityOp     n_i_op_;
       DummyOp          v_i_op_;
       DummyOp          t_i_op_;
       DummyOp          t_e_op_;
      */
      Array<NLOperator*> op_;

      Array<int> & offsets_;
      mutable BlockOperator *grad_;

      void updateOffsets();

   public:
      CombinedOp(const MPI_Session & mpi, const DGParams & dg,
                 const PlasmaParams & plasma,
                 ParFiniteElementSpace & vfes,
                 ParGridFunctionArray & yGF, ParGridFunctionArray & kGF,
                 Array<int> & offsets,
                 // int ion_charge, double ion_mass,
                 // double neutral_mass, double neutral_temp,
                 double DiPerp, double XiPerp, double XePerp,
                 VectorCoefficient & B3Coef,
                 std::vector<CoefficientByAttr> & Ti_dbc,
                 std::vector<CoefficientByAttr> & Te_dbc,
                 const Array<int> & term_flags,
                 const Array<int> & vis_flags,
                 // VectorCoefficient & bHatCoef,
                 // MatrixCoefficient & PerpCoef,
                 unsigned int op_flag = 31, int logging = 0);

      ~CombinedOp();

      void SetTimeStep(double dt);
      void SetLogging(int logging);

      void Update();

      void Mult(const Vector &k, Vector &y) const;

      void UpdateGradient(const Vector &x) const;

      Operator &GetGradient(const Vector &x) const
      { UpdateGradient(x); return *grad_; }

      void RegisterDataFields(DataCollection & dc);

      void PrepareDataFields();

      void InitializeGLVis();

      void DisplayToGLVis();
   };

   CombinedOp op_;

   VectorXYCoefficient BxyCoef_;
   VectorZCoefficient  BzCoef_;

   ParGridFunction * BxyGF_;
   ParGridFunction * BzGF_;
   // ConstantCoefficient oneCoef_;

   // DGAdvectionDiffusionTDO n_n_oper_; // Neutral Density
   // DGAdvectionDiffusionTDO n_i_oper_; // Ion Density
   // DGAdvectionDiffusionTDO v_i_oper_; // Ion Parallel Velocity
   // DGAdvectionDiffusionTDO T_i_oper_; // Ion Temperature
   // DGAdvectionDiffusionTDO T_e_oper_; // Electron Temperature

   mutable Vector x_;
   mutable Vector y_;
   Vector u_;
   Vector dudt_;

public:
   DGTransportTDO(const MPI_Session & mpi, const DGParams & dg,
                  const PlasmaParams & plasma,
                  ParFiniteElementSpace &fes,
                  ParFiniteElementSpace &vfes,
                  ParFiniteElementSpace &ffes,
                  Array<int> &offsets,
                  ParGridFunctionArray &yGF,
                  ParGridFunctionArray &kGF,
                  double Di_perp, double Xi_perp, double Xe_perp,
                  VectorCoefficient & B3Coef,
                  std::vector<CoefficientByAttr> & Ti_dbc,
                  std::vector<CoefficientByAttr> & Te_dbc,
                  const Array<int> & term_flags,
                  const Array<int> & vis_flags,
                  bool imex = true,
                  unsigned int op_flag = 31,
                  int logging = 0);

   ~DGTransportTDO();

   void SetTime(const double _t);
   void SetLogging(int logging);

   void RegisterDataFields(DataCollection & dc);

   void PrepareDataFields();

   void InitializeGLVis();

   void DisplayToGLVis();

   /*
    void SetNnDiffusionCoefficient(Coefficient &dCoef);
    void SetNnDiffusionCoefficient(MatrixCoefficient &DCoef);
    void SetNnSourceCoefficient(Coefficient &SCoef);

    void SetNnDirichletBC(Array<int> &dbc_attr, Coefficient &dbc);
    void SetNnNeumannBC(Array<int> &nbc_attr, Coefficient &nbc);

    void SetNiAdvectionCoefficient(VectorCoefficient &VCoef);
    void SetNiDiffusionCoefficient(Coefficient &dCoef);
    void SetNiDiffusionCoefficient(MatrixCoefficient &DCoef);
    void SetNiSourceCoefficient(Coefficient &SCoef);

    void SetNiDirichletBC(Array<int> &dbc_attr, Coefficient &dbc);
    void SetNiNeumannBC(Array<int> &nbc_attr, Coefficient &nbc);
   */
   /*
    void SetViAdvectionCoefficient(VectorCoefficient &VCoef);
    void SetViDiffusionCoefficient(Coefficient &dCoef);
    void SetViDiffusionCoefficient(MatrixCoefficient &DCoef);
    void SetViSourceCoefficient(Coefficient &SCoef);

    void SetViDirichletBC(Array<int> &dbc_attr, Coefficient &dbc);
    void SetViNeumannBC(Array<int> &nbc_attr, Coefficient &nbc);

    void SetTiAdvectionCoefficient(VectorCoefficient &VCoef);
    void SetTiDiffusionCoefficient(Coefficient &dCoef);
    void SetTiDiffusionCoefficient(MatrixCoefficient &DCoef);
    void SetTiSourceCoefficient(Coefficient &SCoef);

    void SetTiDirichletBC(Array<int> &dbc_attr, Coefficient &dbc);
    void SetTiNeumannBC(Array<int> &nbc_attr, Coefficient &nbc);

    void SetTeAdvectionCoefficient(VectorCoefficient &VCoef);
    void SetTeDiffusionCoefficient(Coefficient &dCoef);
    void SetTeDiffusionCoefficient(MatrixCoefficient &DCoef);
    void SetTeSourceCoefficient(Coefficient &SCoef);

    void SetTeDirichletBC(Array<int> &dbc_attr, Coefficient &dbc);
    void SetTeNeumannBC(Array<int> &nbc_attr, Coefficient &nbc);
   */
   // virtual void ExplicitMult(const Vector &x, Vector &y) const;
   virtual void ImplicitSolve(const double dt, const Vector &y, Vector &k);

   void Update();
};

class MultiSpeciesDiffusion;
class MultiSpeciesAdvection;

class TransportSolver : public ODESolver
{
private:
   ODESolver * impSolver_;
   ODESolver * expSolver_;

   ParFiniteElementSpace & sfes_; // Scalar fields
   ParFiniteElementSpace & vfes_; // Vector fields
   ParFiniteElementSpace & ffes_; // Full system

   BlockVector & nBV_;

   ParGridFunction & B_;

   Array<int> & charges_;
   Vector & masses_;

   MultiSpeciesDiffusion * msDiff_;

   void initDiffusion();

public:
   TransportSolver(ODESolver * implicitSolver, ODESolver * explicitSolver,
                   ParFiniteElementSpace & sfes,
                   ParFiniteElementSpace & vfes,
                   ParFiniteElementSpace & ffes,
                   BlockVector & nBV,
                   ParGridFunction & B,
                   Array<int> & charges,
                   Vector & masses);
   ~TransportSolver();

   void Update();

   void Step(Vector &x, double &t, double &dt);
};
/*
class ChiParaCoefficient : public Coefficient
{
private:
 BlockVector & nBV_;
 ParGridFunction nGF_;
 GridFunctionCoefficient nCoef_;
 GridFunctionCoefficient TCoef_;

 int ion_;
 Array<int> & z_;
 Vector     * m_;
 Vector       n_;

public:
 ChiParaCoefficient(BlockVector & nBV, Array<int> & charges);
 ChiParaCoefficient(BlockVector & nBV, int ion_species,
                    Array<int> & charges, Vector & masses);
 void SetT(ParGridFunction & T);

 double Eval(ElementTransformation &T, const IntegrationPoint &ip);
};

class ChiPerpCoefficient : public Coefficient
{
private:
 int ion_;

public:
 ChiPerpCoefficient(BlockVector & nBV, Array<int> & charges);
 ChiPerpCoefficient(BlockVector & nBV, int ion_species,
                    Array<int> & charges, Vector & masses);

 double Eval(ElementTransformation &T, const IntegrationPoint &ip);
};

class ChiCrossCoefficient : public Coefficient
{
private:
 int ion_;

public:
 ChiCrossCoefficient(BlockVector & nBV, Array<int> & charges);
 ChiCrossCoefficient(BlockVector & nBV, int ion_species,
                     Array<int> & charges, Vector & masses);

 double Eval(ElementTransformation &T, const IntegrationPoint &ip);
};

class ChiCoefficient : public MatrixCoefficient
{
private:
 ChiParaCoefficient  chiParaCoef_;
 ChiPerpCoefficient  chiPerpCoef_;
 ChiCrossCoefficient chiCrossCoef_;
 VectorGridFunctionCoefficient BCoef_;

 Vector bHat_;

public:
 ChiCoefficient(int dim, BlockVector & nBV, Array<int> & charges);
 ChiCoefficient(int dim, BlockVector & nBV, int ion_species,
                Array<int> & charges, Vector & masses);

 void SetT(ParGridFunction & T);
 void SetB(ParGridFunction & B);

 void Eval(DenseMatrix &K, ElementTransformation &T,
           const IntegrationPoint &ip);
};
*/
/*
class EtaParaCoefficient : public Coefficient
{
private:
 BlockVector & nBV_;
 ParGridFunction nGF_;
 GridFunctionCoefficient nCoef_;
 GridFunctionCoefficient TCoef_;

 int ion_;
 Array<int> & z_;
 Vector     * m_;
 Vector       n_;

public:
 EtaParaCoefficient(BlockVector & nBV, Array<int> & charges);
 EtaParaCoefficient(BlockVector & nBV, int ion_species,
                    Array<int> & charges, Vector & masses);

 void SetT(ParGridFunction & T);

 double Eval(ElementTransformation &T, const IntegrationPoint &ip);
};
*/
class MultiSpeciesDiffusion : public TimeDependentOperator
{
private:
   ParFiniteElementSpace &sfes_;
   ParFiniteElementSpace &vfes_;

   BlockVector & nBV_;

   Array<int> & charges_;
   Vector & masses_;

   void initCoefficients();
   void initBilinearForms();

public:
   MultiSpeciesDiffusion(ParFiniteElementSpace & sfes,
                         ParFiniteElementSpace & vfes,
                         BlockVector & nBV,
                         Array<int> & charges,
                         Vector & masses);

   ~MultiSpeciesDiffusion();

   void Assemble();

   void Update();

   void ImplicitSolve(const double dt, const Vector &x, Vector &y);
};


// Time-dependent operator for the right-hand side of the ODE representing the
// DG weak form for the diffusion term. (modified from ex14p)
class DiffusionTDO : public TimeDependentOperator
{
private:
   const int dim_;
   double dt_;
   double dg_sigma_;
   double dg_kappa_;

   ParFiniteElementSpace &fes_;
   ParFiniteElementSpace &dfes_;
   ParFiniteElementSpace &vfes_;

   ParBilinearForm m_;
   ParBilinearForm d_;

   ParLinearForm rhs_;
   ParGridFunction x_;

   HypreParMatrix * M_;
   HypreParMatrix * D_;

   Vector RHS_;
   Vector X_;

   HypreSolver * solver_;
   HypreSolver * amg_;

   MatrixCoefficient &nuCoef_;
   ScalarMatrixProductCoefficient dtNuCoef_;

   void initSolver(double dt);

public:
   DiffusionTDO(ParFiniteElementSpace &fes,
                ParFiniteElementSpace &dfes,
                ParFiniteElementSpace &_vfes,
                MatrixCoefficient & nuCoef,
                double dg_sigma,
                double dg_kappa);

   // virtual void Mult(const Vector &x, Vector &y) const;

   virtual void ImplicitSolve(const double dt, const Vector &x, Vector &y);

   virtual ~DiffusionTDO() { }
};

// Time-dependent operator for the right-hand side of the ODE representing the
// DG weak form for the advection term.
class AdvectionTDO : public TimeDependentOperator
{
private:
   const int dim_;
   const int num_equation_;
   const double specific_heat_ratio_;

   mutable double max_char_speed_;

   ParFiniteElementSpace &vfes_;
   Operator &A_;
   SparseMatrix &Aflux_;
   DenseTensor Me_inv_;

   mutable Vector state_;
   mutable DenseMatrix f_;
   mutable DenseTensor flux_;
   mutable Vector z_;

   void GetFlux(const DenseMatrix &state, DenseTensor &flux) const;

public:
   AdvectionTDO(ParFiniteElementSpace &_vfes,
                Operator &A, SparseMatrix &Aflux, int num_equation,
                double specific_heat_ratio);

   virtual void Mult(const Vector &x, Vector &y) const;

   virtual ~AdvectionTDO() { }
};

// Implements a simple Rusanov flux
class RiemannSolver
{
private:
   int num_equation_;
   double specific_heat_ratio_;
   Vector flux1_;
   Vector flux2_;

public:
   RiemannSolver(int num_equation, double specific_heat_ratio);
   double Eval(const Vector &state1, const Vector &state2,
               const Vector &nor, Vector &flux);
};


// Constant (in time) mixed bilinear form multiplying the flux grid function.
// The form is (vec(v), grad(w)) where the trial space = vector L2 space (mesh
// dim) and test space = scalar L2 space.
class DomainIntegrator : public BilinearFormIntegrator
{
private:
   Vector shape_;
   DenseMatrix flux_;
   DenseMatrix dshapedr_;
   DenseMatrix dshapedx_;

public:
   DomainIntegrator(const int dim, const int num_equation);

   virtual void AssembleElementMatrix2(const FiniteElement &trial_fe,
                                       const FiniteElement &test_fe,
                                       ElementTransformation &Tr,
                                       DenseMatrix &elmat);
};

// Interior face term: <F.n(u),[w]>
class FaceIntegrator : public NonlinearFormIntegrator
{
private:
   int num_equation_;
   double max_char_speed_;
   RiemannSolver rsolver_;
   Vector shape1_;
   Vector shape2_;
   Vector funval1_;
   Vector funval2_;
   Vector nor_;
   Vector fluxN_;
   IntegrationPoint eip1_;
   IntegrationPoint eip2_;

public:
   FaceIntegrator(RiemannSolver &rsolver_, const int dim,
                  const int num_equation);

   virtual void AssembleFaceVector(const FiniteElement &el1,
                                   const FiniteElement &el2,
                                   FaceElementTransformations &Tr,
                                   const Vector &elfun, Vector &elvect);
};

} // namespace transport

} // namespace plasma

} // namespace mfem

#endif // MFEM_USE_MPI

#endif // MFEM_TRANSPORT_SOLVER
