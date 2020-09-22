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

#include "adv_diff_v1_solver.hpp"

#ifdef MFEM_USE_MPI

using namespace std;

namespace mfem
{

using namespace common;

namespace thermal
{

DiffusionTDO::DiffusionTDO(
   ParFiniteElementSpace &H1_FES,
   Coefficient & dTdtBdr,
   Array<int> & bdr_attr,
   Coefficient & c, bool td_c,
   Coefficient & k, bool td_k,
   Coefficient & Q, bool td_Q)
   : TimeDependentOperator(H1_FES.GetVSize(), 0.0),
     init_(false), //initA_(false), initAInv_(false),
     multCount_(0), solveCount_(0),
     H1_FESpace_(&H1_FES),
     mC_(NULL), sK_(NULL), a_(NULL), dTdt_gf_(NULL), Qs_(NULL),
     MCInv_(NULL), MCDiag_(NULL),
     AInv_(NULL), APrecond_(NULL),
     rhs_(NULL),
     bdr_attr_(&bdr_attr), ess_bdr_tdofs_(0), dTdtBdrCoef_(&dTdtBdr),
     tdQ_(td_Q), tdC_(td_c), tdK_(td_k),
     QCoef_(&Q), CCoef_(&c), kCoef_(&k), KCoef_(NULL),
     // CInvCoef_(NULL), kInvCoef_(NULL), KInvCoef_(NULL)
     dtkCoef_(NULL), dtKCoef_(NULL)
{
   this->init();
}

DiffusionTDO::DiffusionTDO(
   ParFiniteElementSpace &H1_FES,
   Coefficient & dTdtBdr,
   Array<int> & bdr_attr,
   Coefficient & c, bool td_c,
   MatrixCoefficient & K, bool td_k,
   Coefficient & Q, bool td_Q)
   : TimeDependentOperator(H1_FES.GetVSize(), 0.0),
     init_(false),
     multCount_(0), solveCount_(0),
     H1_FESpace_(&H1_FES),
     mC_(NULL), sK_(NULL), a_(NULL), dTdt_gf_(NULL), Qs_(NULL),
     MCInv_(NULL), MCDiag_(NULL),
     AInv_(NULL), APrecond_(NULL),
     rhs_(NULL),
     bdr_attr_(&bdr_attr), ess_bdr_tdofs_(0), dTdtBdrCoef_(&dTdtBdr),
     tdQ_(td_Q), tdC_(td_c), tdK_(td_k),
     QCoef_(&Q), CCoef_(&c), kCoef_(NULL), KCoef_(&K),
     // CInvCoef_(NULL), kInvCoef_(NULL), KInvCoef_(NULL)
     dtkCoef_(NULL), dtKCoef_(NULL)
{
   this->init();
}

DiffusionTDO::~DiffusionTDO()
{
   delete a_;
   delete mC_;
   delete sK_;
   delete dTdt_gf_;
   delete Qs_;
   delete MCInv_;
   delete MCDiag_;
   delete AInv_;
   delete APrecond_;
}

void
DiffusionTDO::init()
{
   if ( init_ ) { return; }

   if ( mC_ == NULL )
   {
      mC_ = new ParBilinearForm(H1_FESpace_);
      mC_->AddDomainIntegrator(new MassIntegrator(*CCoef_));
      mC_->Assemble();
   }

   if ( sK_ == NULL )
   {
      sK_ = new ParBilinearForm(H1_FESpace_);
      if ( kCoef_ != NULL )
      {
         sK_->AddDomainIntegrator(new DiffusionIntegrator(*kCoef_));
      }
      else if ( KCoef_ != NULL )
      {
         sK_->AddDomainIntegrator(new DiffusionIntegrator(*KCoef_));
      }
      sK_->Assemble();
   }
   if ( dTdt_gf_ == NULL )
   {
      dTdt_gf_ = new ParGridFunction(H1_FESpace_);
   }
   if ( Qs_ == NULL && QCoef_ != NULL )
   {
      Qs_ = new ParLinearForm(H1_FESpace_);
      Qs_->AddDomainIntegrator(new DomainLFIntegrator(*QCoef_));
      Qs_->Assemble();
      rhs_ = new Vector(Qs_->Size());
   }
   /*
   CInvCoef_ = new InverseCoefficient(*CCoef_);
   if ( kCoef_ != NULL ) kInvCoef_ = new InverseCoefficient(*kCoef_);
   if ( KCoef_ != NULL ) KInvCoef_ = new MatrixInverseCoefficient(*KCoef_);
   */
   H1_FESpace_->GetEssentialTrueDofs(*bdr_attr_, ess_bdr_tdofs_);

   init_ = true;
}

void
DiffusionTDO::SetTime(const double time)
{
   this->TimeDependentOperator::SetTime(time);

   dTdtBdrCoef_->SetTime(t);

   if ( tdQ_ )
   {
      QCoef_->SetTime(t);
      Qs_->Assemble();
   }

   if ( tdC_ )
   {
      CCoef_->SetTime(t);
      mC_->Assemble();
   }

   if ( tdK_ )
   {
      if ( kCoef_ != NULL ) { kCoef_->SetTime(t); }
      if ( KCoef_ != NULL ) { KCoef_->SetTime(t); }
      sK_->Assemble();
   }

   if ( ( tdC_ || tdK_ ) && a_ != NULL )
   {
      a_->Assemble();
   }

   newTime_ = true;
}
/*
void
DiffusionTDOOperator::SetHeatSource(Coefficient & Q, bool time_dep)
{
if ( ownsQ_ )
  {
    delete QCoef_;
  }

tdQ_   = time_dep;
QCoef_ = &Q;
}

void
DiffusionTDOOperator::SetConductivityCoefficient(Coefficient & k,
                  bool time_dep)
{
if ( ownsK_ )
  {
    delete kCoef_;
    delete KCoef_;
  }

tdK_   = time_dep;
kCoef_ = &k;
KCoef_ = NULL;
}

void
DiffusionTDOOperator::SetConductivityCoefficient(MatrixCoefficient & K,
                  bool time_dep)
{
if ( ownsK_ )
  {
    delete kCoef_;
    delete KCoef_;
  }

tdK_   = time_dep;
kCoef_ = NULL;
KCoef_ = &K;
}

void
DiffusionTDOOperator::SetSpecificHeatCoefficient(Coefficient & c,
                  bool time_dep)
{
if ( ownsC_ )
  {
    delete CCoef_;
  }

tdC_   = time_dep;
CCoef_ = &c;
}
*/
void
DiffusionTDO::initMult() const
{
   if ( tdC_ || MCInv_ == NULL || MCDiag_ == NULL )
   {
      if ( MCInv_ == NULL )
      {
         MCInv_ = new HyprePCG(MC_);
         MCInv_->SetTol(1e-12);
         MCInv_->SetMaxIter(200);
         MCInv_->SetPrintLevel(0);
      }
      else
      {
         MCInv_->SetOperator(MC_);
      }
      if ( MCDiag_ == NULL )
      {
         MCDiag_ = new HypreDiagScale(MC_);
         MCInv_->SetPreconditioner(*MCDiag_);
      }
      else
      {
         MCDiag_->SetOperator(MC_);
      }
   }
}

void
DiffusionTDO::Mult(const Vector &T, Vector &dT_dt) const
{
   dT_dt = 0.0;

   sK_->Mult(T, *rhs_);

   *rhs_ -= *Qs_;
   rhs_->Neg();

   dTdt_gf_->ProjectBdrCoefficient(*dTdtBdrCoef_, *bdr_attr_);

   mC_->FormLinearSystem(ess_bdr_tdofs_, *dTdt_gf_, *rhs_, MC_, dTdt_, RHS_);

   this->initMult();

   MCInv_->Mult(RHS_, dTdt_);

   mC_->RecoverFEMSolution(dTdt_, *rhs_, dT_dt);

   multCount_++;
}

void
DiffusionTDO::initA(double dt)
{
   if ( kCoef_ != NULL )
   {
      dtkCoef_ = new ScaledCoefficient(dt, *kCoef_);
   }
   else
   {
      dtKCoef_ = new ScaledMatrixCoefficient(dt, *KCoef_);
   }
   if ( a_ == NULL)
   {
      a_ = new ParBilinearForm(H1_FESpace_);
      a_->AddDomainIntegrator(new MassIntegrator(*CCoef_));
      if ( kCoef_ != NULL)
      {
         a_->AddDomainIntegrator(new DiffusionIntegrator(*dtkCoef_));
      }
      else
      {
         a_->AddDomainIntegrator(new DiffusionIntegrator(*dtKCoef_));
      }

      a_->Assemble();
   }
}

void
DiffusionTDO::initImplicitSolve()
{
   if ( tdC_ || tdK_ || AInv_ == NULL || APrecond_ == NULL )
   {
      if ( AInv_ == NULL )
      {
         AInv_ = new HyprePCG(A_);
         AInv_->SetTol(1e-12);
         AInv_->SetMaxIter(200);
         AInv_->SetPrintLevel(0);
      }
      else
      {
         AInv_->SetOperator(A_);
      }
      if ( APrecond_ == NULL )
      {
         APrecond_ = new HypreBoomerAMG(A_);
         APrecond_->SetPrintLevel(0);
         AInv_->SetPreconditioner(*APrecond_);
      }
      else
      {
         APrecond_->SetOperator(A_);
      }
   }
}

void
DiffusionTDO::ImplicitSolve(const double dt,
                            const Vector &T, Vector &dT_dt)
{
   dT_dt = 0.0;
   // cout << "sK size: " << sK_->Width() << ", T size: " << T.Size() << ", rhs_ size: " << rhs_->Size() << endl;
   /*
   ostringstream ossT; ossT << "T_" << solveCount_ << ".vec";
   ofstream ofsT(ossT.str().c_str());
   T.Print(ofsT);
   ofsT.close();
   */
   sK_->Mult(T, *rhs_);
   /*
   ofstream ofsrhs("rhs.vec");
   rhs_->Print(ofsrhs);

   ofstream ofsQ("Q.vec");
   Qs_->Print(ofsQ);
   */
   *rhs_ -= *Qs_;
   *rhs_ *= -1.0;

   dTdt_gf_->ProjectBdrCoefficient(*dTdtBdrCoef_, *bdr_attr_);

   this->initA(dt);

   a_->FormLinearSystem(ess_bdr_tdofs_, *dTdt_gf_, *rhs_, A_, dTdt_, RHS_);
   /*
   A_.Print("A.mat");
   ofstream ofsB("b.vec");
   RHS_.Print(ofsB);
   */
   this->initImplicitSolve();

   AInv_->Mult(RHS_, dTdt_);

   a_->RecoverFEMSolution(dTdt_, *rhs_, dT_dt);

   solveCount_++;
}

AdvectionTDO::AdvectionTDO(ParFiniteElementSpace &H1_FES,
                           VectorCoefficient &velCoef)
   : TimeDependentOperator(H1_FES.GetVSize(), 0.0),
     H1_FESpace_(H1_FES),
     velCoef_(velCoef),
     ess_bdr_tdofs_(0),
     m1_(&H1_FES),
     adv1_(&H1_FES),
     M1Inv_(NULL),
     M1Diag_(NULL),
     SOL_(H1_FES.GetTrueVSize()),
     RHS_(H1_FES.GetTrueVSize()),
     rhs_(H1_FES.GetVSize())
{
   m1_.AddDomainIntegrator(new MassIntegrator);
   m1_.Assemble();

   adv1_.AddDomainIntegrator(new MixedScalarWeakDivergenceIntegrator(velCoef_));
   adv1_.Assemble();
}

AdvectionTDO::~AdvectionTDO()
{
   delete M1Inv_;
   delete M1Diag_;
}

void
AdvectionTDO::initMult() const
{
   if ( M1Inv_ == NULL || M1Diag_ == NULL )
   {
      if ( M1Inv_ == NULL )
      {
         M1Inv_ = new HyprePCG(M1_);
         M1Inv_->SetTol(1e-12);
         M1Inv_->SetMaxIter(200);
         M1Inv_->SetPrintLevel(0);
      }
      else
      {
         M1Inv_->SetOperator(M1_);
      }
      if ( M1Diag_ == NULL )
      {
         M1Diag_ = new HypreDiagScale(M1_);
         M1Inv_->SetPreconditioner(*M1Diag_);
      }
      else
      {
         M1Diag_->SetOperator(M1_);
      }
   }
}

void AdvectionTDO::Mult(const Vector &y, Vector &dydt) const
{
   dydt_gf_.MakeRef(&H1_FESpace_, dydt);
   adv1_.Mult(y, rhs_);
   rhs_ *= -1.0;

   dydt_gf_ = 0.0;
   m1_.FormLinearSystem(ess_bdr_tdofs_, dydt_gf_, rhs_, M1_, SOL_, RHS_);

   this->initMult();

   M1Inv_->Mult(RHS_, SOL_);

   m1_.RecoverFEMSolution(SOL_, rhs_, dydt_gf_);

}

} // namespace thermal

void
MatrixInverseCoefficient::Eval(DenseMatrix &K, ElementTransformation &T,
                               const IntegrationPoint &ip)
{
   M_->Eval(K, T, ip); K.Invert();
}

void
ScaledMatrixCoefficient::Eval(DenseMatrix &K, ElementTransformation &T,
                              const IntegrationPoint &ip)
{
   M_->Eval(K, T, ip); K *= a_;
}


} // namespace mfem

#endif // MFEM_USE_MPI
