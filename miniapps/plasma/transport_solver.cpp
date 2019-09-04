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

#include "transport_solver.hpp"
//#include "braginskii_coefs.hpp"

#ifdef MFEM_USE_MPI

using namespace std;
namespace mfem
{
using namespace miniapps;

namespace plasma
{

namespace transport
{
/*
double tau_e(double Te, int ns, double * ni, int * zi, double lnLambda)
{
   double tau = 0.0;
   for (int i=0; i<ns; i++)
   {
      tau += meanElectronIonCollisionTime(Te, ni[i], zi[i], lnLambda);
   }
   return tau;
}

double tau_i(double ma, double Ta, int ion, int ns, double * ni, int * zi,
             double lnLambda)
{
   double tau = 0.0;
   for (int i=0; i<ns; i++)
   {
      tau += meanIonIonCollisionTime(ma, Ta, ni[i], zi[ion], zi[i], lnLambda);
   }
   return tau;
}
*/
/*
ChiParaCoefficient::ChiParaCoefficient(BlockVector & nBV, Array<int> & z)
   : nBV_(nBV),
     ion_(-1),
     z_(z),
     m_(NULL),
     n_(z.Size())
{}

ChiParaCoefficient::ChiParaCoefficient(BlockVector & nBV, int ion_species,
                                       Array<int> & z, Vector & m)
   : nBV_(nBV),
     ion_(ion_species),
     z_(z),
     m_(&m),
     n_(z.Size())
{}

void ChiParaCoefficient::SetT(ParGridFunction & T)
{
   TCoef_.SetGridFunction(&T);
   nGF_.MakeRef(T.ParFESpace(), nBV_.GetBlock(0));
}

double
ChiParaCoefficient::Eval(ElementTransformation &T, const IntegrationPoint &ip)
{
   double temp = TCoef_.Eval(T, ip);

   for (int i=0; i<z_.Size(); i++)
   {
      nGF_.SetData(nBV_.GetBlock(i));
      nCoef_.SetGridFunction(&nGF_);
      n_[i] = nCoef_.Eval(T, ip);
   }

   if (ion_ < 0)
   {
      return chi_e_para(temp, z_.Size(), n_, z_);
   }
   else
   {
      return chi_i_para((*m_)[ion_], temp, ion_, z_.Size(), n_, z_);
   }
}

ChiPerpCoefficient::ChiPerpCoefficient(BlockVector & nBV, Array<int> & z)
   : ion_(-1)
{}

ChiPerpCoefficient::ChiPerpCoefficient(BlockVector & nBV, int ion_species,
                                       Array<int> & z, Vector & m)
   : ion_(ion_species)
{}

double
ChiPerpCoefficient::Eval(ElementTransformation &T, const IntegrationPoint &ip)
{
   if (ion_ < 0)
   {
      return chi_e_perp();
   }
   else
   {
      return chi_i_perp();
   }
}

ChiCrossCoefficient::ChiCrossCoefficient(BlockVector & nBV, Array<int> & z)
   : ion_(-1)
{}

ChiCrossCoefficient::ChiCrossCoefficient(BlockVector & nBV, int ion_species,
                                         Array<int> & z, Vector & m)
   : ion_(ion_species)
{}

double
ChiCrossCoefficient::Eval(ElementTransformation &T, const IntegrationPoint &ip)
{
   if (ion_ < 0)
   {
      return chi_e_cross();
   }
   else
   {
      return chi_i_cross();
   }
}

ChiCoefficient::ChiCoefficient(int dim, BlockVector & nBV, Array<int> & charges)
   : MatrixCoefficient(dim),
     chiParaCoef_(nBV, charges),
     chiPerpCoef_(nBV, charges),
     chiCrossCoef_(nBV, charges),
     bHat_(dim)
{}

ChiCoefficient::ChiCoefficient(int dim, BlockVector & nBV, int ion_species,
                               Array<int> & charges, Vector & masses)
   : MatrixCoefficient(dim),
     chiParaCoef_(nBV, ion_species, charges, masses),
     chiPerpCoef_(nBV, ion_species, charges, masses),
     chiCrossCoef_(nBV, ion_species, charges, masses),
     bHat_(dim)
{}

void ChiCoefficient::SetT(ParGridFunction & T)
{
   chiParaCoef_.SetT(T);
}

void ChiCoefficient::SetB(ParGridFunction & B)
{
   BCoef_.SetGridFunction(&B);
}

void ChiCoefficient::Eval(DenseMatrix &K, ElementTransformation &T,
                          const IntegrationPoint &ip)
{
   double chi_para  = chiParaCoef_.Eval(T, ip);
   double chi_perp  = chiPerpCoef_.Eval(T, ip);
   double chi_cross = (width > 2) ? chiCrossCoef_.Eval(T, ip) : 0.0;

   BCoef_.Eval(bHat_, T, ip);
   bHat_ /= bHat_.Norml2();

   K.SetSize(bHat_.Size());

   if (width == 2)
   {
      K(0,0) = bHat_[0] * bHat_[0] * (chi_para - chi_perp) + chi_perp;
      K(0,1) = bHat_[0] * bHat_[1] * (chi_para - chi_perp);
      K(1,0) = K(0,1);
      K(1,1) = bHat_[1] * bHat_[1] * (chi_para - chi_perp) + chi_perp;
   }
   else
   {
      K(0,0) = bHat_[0] * bHat_[0] * (chi_para - chi_perp) + chi_perp;
      K(0,1) = bHat_[0] * bHat_[1] * (chi_para - chi_perp);
      K(0,2) = bHat_[0] * bHat_[2] * (chi_para - chi_perp);
      K(1,0) = K(0,1);
      K(1,1) = bHat_[1] * bHat_[1] * (chi_para - chi_perp) + chi_perp;
      K(1,2) = bHat_[1] * bHat_[2] * (chi_para - chi_perp);
      K(2,0) = K(0,2);
      K(2,1) = K(1,2);
      K(2,2) = bHat_[2] * bHat_[2] * (chi_para - chi_perp) + chi_perp;

      K(1,2) -= bHat_[0] * chi_cross;
      K(2,0) -= bHat_[1] * chi_cross;
      K(0,1) -= bHat_[2] * chi_cross;
      K(2,1) += bHat_[0] * chi_cross;
      K(0,2) += bHat_[1] * chi_cross;
      K(1,0) += bHat_[2] * chi_cross;

   }
}

EtaParaCoefficient::EtaParaCoefficient(BlockVector & nBV, Array<int> & z)
   : nBV_(nBV),
     ion_(-1),
     z_(z),
     m_(NULL),
     n_(z.Size())
{}

EtaParaCoefficient::EtaParaCoefficient(BlockVector & nBV, int ion_species,
                                       Array<int> & z, Vector & m)
   : nBV_(nBV),
     ion_(ion_species),
     z_(z),
     m_(&m),
     n_(z.Size())
{}

void EtaParaCoefficient::SetT(ParGridFunction & T)
{
   TCoef_.SetGridFunction(&T);
   nGF_.MakeRef(T.ParFESpace(), nBV_.GetBlock(0));
}

double
EtaParaCoefficient::Eval(ElementTransformation &T, const IntegrationPoint &ip)
{
   double temp = TCoef_.Eval(T, ip);

   for (int i=0; i<z_.Size(); i++)
   {
      nGF_.SetData(nBV_.GetBlock(i));
      nCoef_.SetGridFunction(&nGF_);
      n_[i] = nCoef_.Eval(T, ip);
   }

   if (ion_ < 0)
   {
      return eta_e_para(temp, z_.Size(), z_.Size(), n_, z_);
   }
   else
   {
      return eta_i_para((*m_)[ion_], temp, ion_, z_.Size(), n_, z_);
   }
}
*/
DGAdvectionDiffusionTDO::DGAdvectionDiffusionTDO(const DGParams & dg,
                                                 ParFiniteElementSpace &fes,
                                                 ParGridFunctionArray &pgf,
                                                 Coefficient &CCoef,
                                                 bool imex)
   : TimeDependentOperator(fes.GetVSize()),
     dg_(dg),
     imex_(imex),
     logging_(0),
     log_prefix_(""),
     dt_(-1.0),
     fes_(&fes),
     pgf_(&pgf),
     CCoef_(&CCoef),
     VCoef_(NULL),
     dCoef_(NULL),
     DCoef_(NULL),
     SCoef_(NULL),
     negVCoef_(NULL),
     dtNegVCoef_(NULL),
     dtdCoef_(NULL),
     dtDCoef_(NULL),
     dbcAttr_(0),
     dbcCoef_(NULL),
     nbcAttr_(0),
     nbcCoef_(NULL),
     m_(fes_),
     a_(NULL),
     b_(NULL),
     s_(NULL),
     k_(NULL),
     q_exp_(NULL),
     q_imp_(NULL),
     M_(NULL),
     // B_(NULL),
     // S_(NULL),
     rhs_(fes_),
     RHS_(fes_->GetTrueVSize()),
     X_(fes_->GetTrueVSize())
{
   m_.AddDomainIntegrator(new MassIntegrator(*CCoef_));

   M_prec_.SetType(HypreSmoother::Jacobi);
   M_solver_.SetPreconditioner(M_prec_);

   M_solver_.iterative_mode = false;
   M_solver_.SetRelTol(1e-9);
   M_solver_.SetAbsTol(0.0);
   M_solver_.SetMaxIter(100);
   M_solver_.SetPrintLevel(0);
}

DGAdvectionDiffusionTDO::~DGAdvectionDiffusionTDO()
{
   delete M_;
   // delete B_;
   // delete S_;

   delete a_;
   delete b_;
   delete s_;
   delete k_;
   delete q_exp_;
   delete q_imp_;

   delete negVCoef_;
   delete dtNegVCoef_;
   delete dtdCoef_;
   delete dtDCoef_;
}

void DGAdvectionDiffusionTDO::initM()
{
   m_.Assemble();
   m_.Finalize();

   delete M_;
   M_ = m_.ParallelAssemble();
   M_solver_.SetOperator(*M_);
}

void DGAdvectionDiffusionTDO::initA()
{
   if (a_ == NULL)
   {
      a_ = new ParBilinearForm(fes_);

      a_->AddDomainIntegrator(new MassIntegrator(*CCoef_));
      if (dCoef_)
      {
         a_->AddDomainIntegrator(new DiffusionIntegrator(*dtdCoef_));
         a_->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(*dtdCoef_,
                                                                 dg_.sigma,
                                                                 dg_.kappa));
         // a_->AddBdrFaceIntegrator(new DGDiffusionIntegrator(*dtdCoef_,
         //                                                 dg_.sigma,
         //                                                 dg_.kappa));
      }
      else if (DCoef_)
      {
         a_->AddDomainIntegrator(new DiffusionIntegrator(*dtDCoef_));
         a_->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(*dtDCoef_,
                                                                 dg_.sigma,
                                                                 dg_.kappa));
         // a_->AddBdrFaceIntegrator(new DGDiffusionIntegrator(*dtDCoef_,
         //                                                 dg_.sigma,
         //                                                 dg_.kappa));
      }
      if (negVCoef_ && !imex_)
      {
         a_->AddDomainIntegrator(new ConvectionIntegrator(*dtNegVCoef_, -1.0));
         a_->AddInteriorFaceIntegrator(
            new TransposeIntegrator(new DGTraceIntegrator(*dtNegVCoef_,
                                                          1.0, -0.5)));
         a_->AddBdrFaceIntegrator(
            new TransposeIntegrator(new DGTraceIntegrator(*dtNegVCoef_,
                                                          1.0, -0.5)));
      }
   }
}

void DGAdvectionDiffusionTDO::initB()
{
   if (b_ == NULL && (dCoef_ || DCoef_ || VCoef_))
   {
      b_ = new ParBilinearForm(fes_);

      if (dCoef_)
      {
         b_->AddDomainIntegrator(new DiffusionIntegrator(*dCoef_));
         b_->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(*dCoef_,
                                                                 dg_.sigma,
                                                                 dg_.kappa));
         // b_->AddBdrFaceIntegrator(new DGDiffusionIntegrator(*dCoef_,
         //                                                 dg_.sigma,
         //                                                 dg_.kappa));
      }
      else if (DCoef_)
      {
         b_->AddDomainIntegrator(new DiffusionIntegrator(*DCoef_));
         b_->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(*DCoef_,
                                                                 dg_.sigma,
                                                                 dg_.kappa));
         // b_->AddBdrFaceIntegrator(new DGDiffusionIntegrator(*DCoef_,
         //                                                 dg_.sigma,
         //                                                 dg_.kappa));
      }
      if (negVCoef_)
      {
         b_->AddDomainIntegrator(new ConvectionIntegrator(*negVCoef_, -1.0));
         b_->AddInteriorFaceIntegrator(
            new TransposeIntegrator(new DGTraceIntegrator(*negVCoef_,
                                                          1.0, -0.5)));
         b_->AddBdrFaceIntegrator(
            new TransposeIntegrator(new DGTraceIntegrator(*negVCoef_,
                                                          1.0, -0.5)));
      }
      /*
      b_->Assemble();
      b_->Finalize();

      delete B_;
      B_ = b_->ParallelAssemble();
      */
   }
}

void DGAdvectionDiffusionTDO::initS()
{
   if (s_ == NULL && (dCoef_ || DCoef_))
   {
      s_ = new ParBilinearForm(fes_);

      if (dCoef_)
      {
         s_->AddDomainIntegrator(new DiffusionIntegrator(*dCoef_));
         s_->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(*dCoef_,
                                                                 dg_.sigma,
                                                                 dg_.kappa));
         // s_->AddBdrFaceIntegrator(new DGDiffusionIntegrator(*dCoef_,
         //                                                 dg_.sigma,
         //                                                 dg_.kappa));
      }
      else if (DCoef_)
      {
         s_->AddDomainIntegrator(new DiffusionIntegrator(*DCoef_));
         s_->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(*DCoef_,
                                                                 dg_.sigma,
                                                                 dg_.kappa));
         // s_->AddBdrFaceIntegrator(new DGDiffusionIntegrator(*DCoef_,
         //                                                 dg_.sigma,
         //                                                 dg_.kappa));
      }
      /*
      s_->Assemble();
      s_->Finalize();

      delete S_;
      S_ = s_->ParallelAssemble();
      */
   }
}

void DGAdvectionDiffusionTDO::initK()
{
   if (k_ == NULL && VCoef_)
   {
      k_ = new ParBilinearForm(fes_);

      if (negVCoef_)
      {
         k_->AddDomainIntegrator(new ConvectionIntegrator(*negVCoef_, -1.0));
         k_->AddInteriorFaceIntegrator(
            new TransposeIntegrator(new DGTraceIntegrator(*negVCoef_,
                                                          1.0, -0.5)));
         k_->AddBdrFaceIntegrator(
            new TransposeIntegrator(new DGTraceIntegrator(*negVCoef_,
                                                          1.0, -0.5)));
      }
      k_->Assemble();
      k_->Finalize();
   }
}

void DGAdvectionDiffusionTDO::initQ()
{
   if (imex_)
   {
      if (q_exp_ == NULL &&
          (SCoef_ || (dbcCoef_ && (dCoef_ || DCoef_ || VCoef_))))
      {
         q_exp_ = new ParLinearForm(fes_);

         if (SCoef_)
         {
            q_exp_->AddDomainIntegrator(new DomainLFIntegrator(*SCoef_));
         }
         if (dbcCoef_ && VCoef_ && !(dCoef_ || DCoef_))
         {
            q_exp_->AddBdrFaceIntegrator(
               new BoundaryFlowIntegrator(*dbcCoef_, *negVCoef_,
                                          -1.0, -0.5),
               dbcAttr_);
         }
         q_exp_->Assemble();
      }
      if (q_imp_ == NULL &&
          (SCoef_ || (dbcCoef_ && (dCoef_ || DCoef_ || VCoef_))))
      {
         q_imp_ = new ParLinearForm(fes_);

         if (dbcCoef_ && dCoef_)
         {
            q_imp_->AddBdrFaceIntegrator(
               new DGDirichletLFIntegrator(*dbcCoef_, *dCoef_,
                                           dg_.sigma, dg_.kappa),
               dbcAttr_);
         }
         else if (dbcCoef_ && DCoef_)
         {
            q_imp_->AddBdrFaceIntegrator(
               new DGDirichletLFIntegrator(*dbcCoef_, *DCoef_,
                                           dg_.sigma, dg_.kappa),
               dbcAttr_);
         }
         q_imp_->Assemble();
      }
   }
   else
   {
      if (q_imp_ == NULL &&
          (SCoef_ || (dbcCoef_ && (dCoef_ || DCoef_ || VCoef_))))
      {
         q_imp_ = new ParLinearForm(fes_);

         if (SCoef_)
         {
            q_imp_->AddDomainIntegrator(new DomainLFIntegrator(*SCoef_));
         }
         if (dbcCoef_ && dCoef_)
         {
            q_imp_->AddBdrFaceIntegrator(
               new DGDirichletLFIntegrator(*dbcCoef_, *dCoef_,
                                           dg_.sigma, dg_.kappa),
               dbcAttr_);
         }
         else if (dbcCoef_ && DCoef_)
         {
            q_imp_->AddBdrFaceIntegrator(
               new DGDirichletLFIntegrator(*dbcCoef_, *DCoef_,
                                           dg_.sigma, dg_.kappa),
               dbcAttr_);
         }
         else if (dbcCoef_ && VCoef_)
         {
            q_imp_->AddBdrFaceIntegrator(
               new BoundaryFlowIntegrator(*dbcCoef_, *negVCoef_,
                                          -1.0, -0.5),
               dbcAttr_);
         }
         q_imp_->Assemble();
      }
   }
}

void DGAdvectionDiffusionTDO::SetTime(const double _t)
{
   this->TimeDependentOperator::SetTime(_t);

   if (fes_->GetMyRank() == 0 && logging_)
   {
      cout << log_prefix_ << "SetTime with t = " << _t << endl;
   }

   this->initM();

   this->initA();

   if (imex_)
   {
      this->initS();
      this->initK();
   }
   else
   {
      this->initB();
   }

   this->initQ();
}

void DGAdvectionDiffusionTDO::SetLogging(int logging, const string & prefix)
{
   logging_ = logging;
   log_prefix_ = prefix;
}

void DGAdvectionDiffusionTDO::SetAdvectionCoefficient(VectorCoefficient &VCoef)
{
   VCoef_ = &VCoef;
   if (negVCoef_ == NULL)
   {
      negVCoef_ = new ScalarVectorProductCoefficient(-1.0, VCoef);
   }
   else
   {
      negVCoef_->SetBCoef(VCoef);
   }
   if (dtNegVCoef_ == NULL)
   {
      dtNegVCoef_ = new ScalarVectorProductCoefficient(dt_, *negVCoef_);
   }
   if (imex_)
   {
      delete k_; k_ = NULL;
   }
   else
   {
      delete a_; a_ = NULL;
      delete b_; b_ = NULL;
   }
}

void DGAdvectionDiffusionTDO::SetDiffusionCoefficient(Coefficient &dCoef)
{
   dCoef_ = &dCoef;
   if (dtdCoef_ == NULL)
   {
      dtdCoef_ = new ProductCoefficient(dt_, dCoef);
   }
   else
   {
      dtdCoef_->SetBCoef(dCoef);
   }
   if (imex_)
   {
      delete a_; a_ = NULL;
      delete s_; s_ = NULL;
   }
   else
   {
      delete a_; a_ = NULL;
      delete b_; b_ = NULL;
   }
}

void DGAdvectionDiffusionTDO::SetDiffusionCoefficient(MatrixCoefficient &DCoef)
{
   DCoef_ = &DCoef;
   if (dtDCoef_ == NULL)
   {
      dtDCoef_ = new ScalarMatrixProductCoefficient(dt_, DCoef);
   }
   else
   {
      dtDCoef_->SetBCoef(DCoef);
   }
   if (imex_)
   {
      delete a_; a_ = NULL;
      delete s_; s_ = NULL;
   }
   else
   {
      delete a_; a_ = NULL;
      delete b_; b_ = NULL;
   }
}

void DGAdvectionDiffusionTDO::SetSourceCoefficient(Coefficient &SCoef)
{
   SCoef_ = &SCoef;
   delete q_exp_; q_exp_ = NULL;
   delete q_imp_; q_imp_ = NULL;
}

void DGAdvectionDiffusionTDO::SetDirichletBC(Array<int> &dbc_attr,
                                             Coefficient &dbc)
{
   dbcAttr_ = dbc_attr;
   dbcCoef_ = &dbc;
   delete q_exp_; q_exp_ = NULL;
   delete q_imp_; q_imp_ = NULL;
}

void DGAdvectionDiffusionTDO::SetNeumannBC(Array<int> &nbc_attr,
                                           Coefficient &nbc)
{
   nbcAttr_ = nbc_attr;
   nbcCoef_ = &nbc;
   delete q_exp_; q_exp_ = NULL;
   delete q_imp_; q_imp_ = NULL;
}

void DGAdvectionDiffusionTDO::ExplicitMult(const Vector &x, Vector &fx) const
{
   MFEM_VERIFY(imex_, "Unexpected call to ExplicitMult for non-IMEX method!");

   pgf_->ExchangeFaceNbrData();

   if (q_exp_)
   {
      rhs_ = *q_exp_;
   }
   else
   {
      rhs_ = 0.0;
   }
   if (k_) { k_->AddMult(x, rhs_, -1.0); }

   rhs_.ParallelAssemble(RHS_);
   // double nrmR = InnerProduct(M_->GetComm(), RHS_, RHS_);
   // cout << "Norm^2 RHS: " << nrmR << endl;
   M_solver_.Mult(RHS_, X_);

   // double nrmX = InnerProduct(M_->GetComm(), X_, X_);
   // cout << "Norm^2 X: " << nrmX << endl;

   ParGridFunction fx_gf(fes_, (double*)NULL);
   fx_gf.MakeRef(fes_, &fx[0]);
   fx_gf = X_;
}

void DGAdvectionDiffusionTDO::ImplicitSolve(const double dt,
                                            const Vector &u, Vector &dudt)
{
   pgf_->ExchangeFaceNbrData();

   if (fes_->GetMyRank() == 0 && logging_)
   {
      cout << log_prefix_ << "ImplicitSolve with dt = " << dt << endl;
   }

   if (fabs(dt - dt_) > 1e-4 * dt_)
   {
      // cout << "Setting time step" << endl;
      if (dtdCoef_   ) { dtdCoef_->SetAConst(dt); }
      if (dtDCoef_   ) { dtDCoef_->SetAConst(dt); }
      if (dtNegVCoef_) { dtNegVCoef_->SetAConst(dt); }

      dt_ = dt;
   }
   // cout << "applying q_imp" << endl;
   if (q_imp_)
   {
      rhs_ = *q_imp_;
   }
   else
   {
      rhs_ = 0.0;
   }
   rhs_.ParallelAssemble(RHS_);

   fes_->Dof_TrueDof_Matrix()->Mult(u, X_);

   if (imex_)
   {
      if (s_)
      {
         s_->Assemble();
         s_->Finalize();

         HypreParMatrix * S = s_->ParallelAssemble();

         S->Mult(-1.0, X_, 1.0, RHS_);
         delete S;
      }
   }
   else
   {
      //cout << "applying b" << endl;
      if (b_)
      {
         // cout << "DGA::ImplicitSolve assembling b" << endl;
         b_->Assemble();
         b_->Finalize();

         HypreParMatrix * B = b_->ParallelAssemble();

         B->Mult(-1.0, X_, 1.0, RHS_);
         delete B;
      }
   }
   // cout << "DGA::ImplicitSolve assembling a" << endl;
   a_->Assemble();
   a_->Finalize();
   HypreParMatrix *A = a_->ParallelAssemble();

   HypreBoomerAMG *A_prec = new HypreBoomerAMG(*A);
   A_prec->SetPrintLevel(0);
   if (imex_)
   {
      // cout << "solving with CG" << endl;
      CGSolver *A_solver = new CGSolver(A->GetComm());
      A_solver->SetOperator(*A);
      A_solver->SetPreconditioner(*A_prec);

      A_solver->iterative_mode = false;
      A_solver->SetRelTol(1e-9);
      A_solver->SetAbsTol(0.0);
      A_solver->SetMaxIter(100);
      A_solver->SetPrintLevel(0);
      A_solver->Mult(RHS_, X_);
      delete A_solver;
   }
   else
   {
      // cout << "solving with GMRES" << endl;
      GMRESSolver *A_solver = new GMRESSolver(A->GetComm());
      A_solver->SetOperator(*A);
      A_solver->SetPreconditioner(*A_prec);

      A_solver->iterative_mode = false;
      A_solver->SetRelTol(1e-9);
      A_solver->SetAbsTol(0.0);
      A_solver->SetMaxIter(100);
      A_solver->SetPrintLevel(0);
      A_solver->Mult(RHS_, X_);
      delete A_solver;
   }
   // cout << "done with solve" << endl;

   ParGridFunction dudt_gf(fes_, (double*)NULL);
   dudt_gf.MakeRef(fes_, &dudt[0]);
   dudt_gf = X_;

   //delete A_solver;
   delete A_prec;
   delete A;
}

void DGAdvectionDiffusionTDO::Update()
{
   height = width = fes_->GetVSize();
   // cout << "DGA::Update m" << endl;
   m_.Update(); m_.Assemble(); m_.Finalize();
   // cout << "DGA::Update a" << endl;
   a_->Update(); // a_->Assemble(); a_->Finalize();
   // cout << "DGA::Update b" << endl;
   if (b_) { b_->Update(); /*b_->Assemble(); b_->Finalize();*/ }
   // cout << "DGA::Update s" << endl;
   if (s_) { s_->Update(); /*s_->Assemble(); s_->Finalize();*/ }
   // cout << "DGA::Update k" << endl;
   if (k_) { k_->Update(); k_->Assemble(); k_->Finalize(); }
   // cout << "DGA::Update q_exp" << endl;
   if (q_exp_) { q_exp_->Update(); q_exp_->Assemble(); }
   // cout << "DGA::Update q_imp" << endl;
   if (q_imp_) { q_imp_->Update(); q_imp_->Assemble(); }
   // cout << "DGA::Update rhs" << endl;
   rhs_.Update();
   RHS_.SetSize(fes_->GetTrueVSize());
   X_.SetSize(fes_->GetTrueVSize());
}

TransportPrec::TransportPrec(const Array<int> &offsets)
   : BlockDiagonalPreconditioner(offsets), diag_prec_(5)
{
   diag_prec_ = NULL;
}

TransportPrec::~TransportPrec()
{
   for (int i=0; i<diag_prec_.Size(); i++)
   {
      delete diag_prec_[i];
   }
}

void TransportPrec::SetOperator(const Operator &op)
{
   height = width = op.Height();

   const BlockOperator *blk_op = dynamic_cast<const BlockOperator*>(&op);

   if (blk_op)
   {
      this->Offsets() = blk_op->RowOffsets();

      for (int i=0; i<diag_prec_.Size(); i++)
      {
         if (!blk_op->IsZeroBlock(i,i))
         {
            delete diag_prec_[i];

            Operator & diag_op = blk_op->GetBlock(i,i);
            HypreParMatrix & M = dynamic_cast<HypreParMatrix&>(diag_op);

            if (i == 0)
            {
               cout << "Building new HypreBoomerAMG preconditioner" << endl;
               diag_prec_[i] = new HypreBoomerAMG(M);
            }
            else
            {
               diag_prec_[i] = new HypreDiagScale(M);
            }
            SetDiagonalBlock(i, diag_prec_[i]);
         }
      }
   }
}

DGTransportTDO::DGTransportTDO(const MPI_Session & mpi, const DGParams & dg,
                               ParFiniteElementSpace &fes,
                               ParFiniteElementSpace &ffes,
                               Array<int> & offsets,
                               ParGridFunctionArray &pgf,
                               ParGridFunctionArray &dpgf,
                               int ion_charge,
                               double ion_mass,
                               double neutral_mass,
                               double neutral_temp,
                               double Di_perp,
                               VectorCoefficient &B3Coef,
                               VectorCoefficient &bHatCoef,
                               MatrixCoefficient &perpCoef,
                               Coefficient &MomCCoef,
                               Coefficient &TiCCoef,
                               Coefficient &TeCCoef,
                               bool imex, unsigned int op_flag, int logging)
   : TimeDependentOperator(ffes.GetVSize()),
     mpi_(mpi),
     // MyRank_(fes.GetMyRank()),
     logging_(logging),
     fes_(&fes),
     ffes_(&ffes),
     pgf_(&pgf),
     dpgf_(&dpgf),
     offsets_(offsets),
     newton_op_prec_(offsets),
     newton_op_solver_(fes.GetComm()),
     newton_solver_(fes.GetComm()),
     op_(mpi, dg, pgf, dpgf, offsets_,
         ion_charge, ion_mass, neutral_mass, neutral_temp, Di_perp,
         B3Coef, bHatCoef, perpCoef, op_flag, logging)//,
     // oneCoef_(1.0),
     // n_n_oper_(dg, fes, pgf, oneCoef_, imex),
     // n_i_oper_(dg, fes, pgf, oneCoef_, imex),
     // v_i_oper_(dg, fes, pgf, MomCCoef, imex),
     // T_i_oper_(dg, fes, pgf,  TiCCoef, imex),
     // T_e_oper_(dg, fes, pgf,  TeCCoef, imex)
{
   if ( mpi_.Root() && logging_ > 1)
   {
      cout << "Constructing DGTransportTDO" << endl;
   }
   const double rel_tol = 1e-8;
   /*
   {
      int loc_size = fes_->GetVSize();
      SparseMatrix spZ(loc_size, loc_size, 0);
      HypreParMatrix Z(fes_->GetComm(), fes_->GlobalVSize(),
             fes_->GetDofOffsets(), &spZ);

      HypreSmoother * prec0 = new HypreSmoother(Z, HypreSmoother::Jacobi);
      HypreSmoother * prec1 = new HypreSmoother(Z, HypreSmoother::Jacobi);
      HypreSmoother * prec2 = new HypreSmoother(Z, HypreSmoother::Jacobi);
      HypreSmoother * prec3 = new HypreSmoother(Z, HypreSmoother::Jacobi);
      HypreSmoother * prec4 = new HypreSmoother(Z, HypreSmoother::Jacobi);

      prec0->SetType(HypreSmoother::l1Jacobi);
      prec0->SetPositiveDiagonal(true);

      prec1->SetType(HypreSmoother::l1Jacobi);
      prec1->SetPositiveDiagonal(true);

      prec2->SetType(HypreSmoother::l1Jacobi);
      prec2->SetPositiveDiagonal(true);

      prec3->SetType(HypreSmoother::l1Jacobi);
      prec3->SetPositiveDiagonal(true);

      prec4->SetType(HypreSmoother::l1Jacobi);
      prec4->SetPositiveDiagonal(true);

      newton_op_prec_.SetDiagonalBlock(0, prec0);
      newton_op_prec_.SetDiagonalBlock(1, prec1);
      newton_op_prec_.SetDiagonalBlock(2, prec2);
      newton_op_prec_.SetDiagonalBlock(3, prec3);
      newton_op_prec_.SetDiagonalBlock(4, prec4);
   }
   */
   newton_op_solver_.SetRelTol(rel_tol * 1.0e-2);
   newton_op_solver_.SetAbsTol(0.0);
   newton_op_solver_.SetMaxIter(300);
   newton_op_solver_.SetPrintLevel(1);
   newton_op_solver_.SetPreconditioner(newton_op_prec_);

   newton_solver_.iterative_mode = false;
   newton_solver_.SetSolver(newton_op_solver_);
   newton_solver_.SetOperator(op_);
   newton_solver_.SetPrintLevel(1); // print Newton iterations
   newton_solver_.SetRelTol(rel_tol);
   newton_solver_.SetAbsTol(0.0);
   newton_solver_.SetMaxIter(10);
   if (mpi_.Root() && logging_ > 1)
   {
      cout << "Done constructing DGTransportTDO" << endl;
   }
}

DGTransportTDO::~DGTransportTDO() {}

void DGTransportTDO::SetTime(const double _t)
{
   if (mpi_.Root() && logging_ > 1)
   {
      cout << "Entering DGTransportTDO::SetTime" << endl;
   }
   this->TimeDependentOperator::SetTime(_t);

   // n_n_oper_.SetTime(_t);
   // n_i_oper_.SetTime(_t);
   // v_i_oper_.SetTime(_t);
   // T_i_oper_.SetTime(_t);
   // T_e_oper_.SetTime(_t);

   if ( mpi_.Root() && logging_ > 1)
   {
      cout << "Leaving DGTransportTDO::SetTime" << endl;
   }
}

void DGTransportTDO::SetLogging(int logging)
{
   op_.SetLogging(logging);
   // n_n_oper_.SetLogging(logging, "Neutral Density:       ");
   // n_i_oper_.SetLogging(logging, "Ion Density:           ");
   // v_i_oper_.SetLogging(logging, "Ion Parallel Velocity: ");
   // T_i_oper_.SetLogging(logging, "Ion Temp:              ");
   // T_e_oper_.SetLogging(logging, "Electron Temp:         ");
}
/*
void DGTransportTDO::SetNnDiffusionCoefficient(Coefficient &dCoef)
{
 n_n_oper_.SetDiffusionCoefficient(dCoef);
}

void DGTransportTDO::SetNnDiffusionCoefficient(MatrixCoefficient &DCoef)
{
 n_n_oper_.SetDiffusionCoefficient(DCoef);
}

void DGTransportTDO::SetNnSourceCoefficient(Coefficient &SCoef)
{
 n_n_oper_.SetSourceCoefficient(SCoef);
}

void DGTransportTDO::SetNnDirichletBC(Array<int> &dbc_attr, Coefficient &dbc)
{
 n_n_oper_.SetDirichletBC(dbc_attr, dbc);
}

void DGTransportTDO::SetNnNeumannBC(Array<int> &nbc_attr, Coefficient &nbc)
{
 n_n_oper_.SetNeumannBC(nbc_attr, nbc);
}

void DGTransportTDO::SetNiAdvectionCoefficient(VectorCoefficient &VCoef)
{
 n_i_oper_.SetAdvectionCoefficient(VCoef);
}

void DGTransportTDO::SetNiDiffusionCoefficient(Coefficient &dCoef)
{
 n_i_oper_.SetDiffusionCoefficient(dCoef);
}

void DGTransportTDO::SetNiDiffusionCoefficient(MatrixCoefficient &DCoef)
{
 n_i_oper_.SetDiffusionCoefficient(DCoef);
}

void DGTransportTDO::SetNiSourceCoefficient(Coefficient &SCoef)
{
 n_i_oper_.SetSourceCoefficient(SCoef);
}

void DGTransportTDO::SetNiDirichletBC(Array<int> &dbc_attr, Coefficient &dbc)
{
 n_i_oper_.SetDirichletBC(dbc_attr, dbc);
}

void DGTransportTDO::SetNiNeumannBC(Array<int> &nbc_attr, Coefficient &nbc)
{
 n_i_oper_.SetNeumannBC(nbc_attr, nbc);
}
*/
/*
void DGTransportTDO::SetViAdvectionCoefficient(VectorCoefficient &VCoef)
{
 v_i_oper_.SetAdvectionCoefficient(VCoef);
}

void DGTransportTDO::SetViDiffusionCoefficient(Coefficient &dCoef)
{
 v_i_oper_.SetDiffusionCoefficient(dCoef);
}

void DGTransportTDO::SetViDiffusionCoefficient(MatrixCoefficient &DCoef)
{
 v_i_oper_.SetDiffusionCoefficient(DCoef);
}

void DGTransportTDO::SetViSourceCoefficient(Coefficient &SCoef)
{
 v_i_oper_.SetSourceCoefficient(SCoef);
}

void DGTransportTDO::SetViDirichletBC(Array<int> &dbc_attr, Coefficient &dbc)
{
 v_i_oper_.SetDirichletBC(dbc_attr, dbc);
}

void DGTransportTDO::SetViNeumannBC(Array<int> &nbc_attr, Coefficient &nbc)
{
 v_i_oper_.SetNeumannBC(nbc_attr, nbc);
}

void DGTransportTDO::SetTiAdvectionCoefficient(VectorCoefficient &VCoef)
{
 T_i_oper_.SetAdvectionCoefficient(VCoef);
}

void DGTransportTDO::SetTiDiffusionCoefficient(Coefficient &dCoef)
{
 T_i_oper_.SetDiffusionCoefficient(dCoef);
}

void DGTransportTDO::SetTiDiffusionCoefficient(MatrixCoefficient &DCoef)
{
 T_i_oper_.SetDiffusionCoefficient(DCoef);
}

void DGTransportTDO::SetTiSourceCoefficient(Coefficient &SCoef)
{
 T_i_oper_.SetSourceCoefficient(SCoef);
}

void DGTransportTDO::SetTiDirichletBC(Array<int> &dbc_attr, Coefficient &dbc)
{
 T_i_oper_.SetDirichletBC(dbc_attr, dbc);
}

void DGTransportTDO::SetTiNeumannBC(Array<int> &nbc_attr, Coefficient &nbc)
{
 T_i_oper_.SetNeumannBC(nbc_attr, nbc);
}

void DGTransportTDO::SetTeAdvectionCoefficient(VectorCoefficient &VCoef)
{
 T_e_oper_.SetAdvectionCoefficient(VCoef);
}

void DGTransportTDO::SetTeDiffusionCoefficient(Coefficient &dCoef)
{
 T_e_oper_.SetDiffusionCoefficient(dCoef);
}

void DGTransportTDO::SetTeDiffusionCoefficient(MatrixCoefficient &DCoef)
{
 T_e_oper_.SetDiffusionCoefficient(DCoef);
}

void DGTransportTDO::SetTeSourceCoefficient(Coefficient &SCoef)
{
 T_e_oper_.SetSourceCoefficient(SCoef);
}

void DGTransportTDO::SetTeDirichletBC(Array<int> &dbc_attr, Coefficient &dbc)
{
 T_e_oper_.SetDirichletBC(dbc_attr, dbc);
}

void DGTransportTDO::SetTeNeumannBC(Array<int> &nbc_attr, Coefficient &nbc)
{
 T_e_oper_.SetNeumannBC(nbc_attr, nbc);
}
*/
/*
void DGTransportTDO::ExplicitMult(const Vector &x, Vector &y) const
{
 y = 0.0;

 // pgf_->ExchangeFaceNbrData();

 int size = fes_->GetVSize();

 x_.SetDataAndSize(const_cast<double*>(&x[0*size]), size);
 y_.SetDataAndSize(&y[0*size], size);
 n_n_oper_.ExplicitMult(x_, y_);

 x_.SetDataAndSize(const_cast<double*>(&x[1*size]), size);
 y_.SetDataAndSize(&y[1*size], size);
 n_i_oper_.ExplicitMult(x_, y_);

 x_.SetDataAndSize(const_cast<double*>(&x[2*size]), size);
 y_.SetDataAndSize(&y[2*size], size);
 v_i_oper_.ExplicitMult(x_, y_);

 x_.SetDataAndSize(const_cast<double*>(&x[3*size]), size);
 y_.SetDataAndSize(&y[3*size], size);
 T_i_oper_.ExplicitMult(x_, y_);

 x_.SetDataAndSize(const_cast<double*>(&x[4*size]), size);
 y_.SetDataAndSize(&y[4*size], size);
 T_e_oper_.ExplicitMult(x_, y_);
}
*/
void DGTransportTDO::ImplicitSolve(const double dt, const Vector &u,
                                   Vector &dudt)
{
   if (mpi_.Root() && logging_ > 1)
   {
      cout << "Entering DGTransportTDO::ImplicitSolve" << endl;
   }

   // cout << "calling DGTransportTDO::ImplicitSolve with arg u = " << u.GetData() <<
   //   ", arg dudt = " << dudt.GetData() << ", pgf = " << (*pgf_)[0]->GetData() <<
   //   " -> " << u.GetData() << ", dpgf = " << (*dpgf_)[0]->GetData() << " -> " <<
   //   dudt.GetData() << endl;

   dudt = 0.0;

   // Coefficient inside the NLOperator classes use data from pgf_ to evaluate
   // fields.  We need to make sure this data accesses the provided vector u.
   double *prev_u = (*pgf_)[0]->GetData();

   for (int i=0; i<offsets_.Size() - 1; i++)
   {
      // cout << "offsets_[" << i << "] = " << offsets_[i] << endl;
      // (*pgf_)[i]->SetData(u.GetData() + offsets_[i]);
      (*pgf_)[i]->MakeRef(fes_, u.GetData() + offsets_[i]);
   }
   pgf_->ExchangeFaceNbrData();

   double *prev_du = (*dpgf_)[0]->GetData();

   for (int i=0; i<offsets_.Size() - 1; i++)
   {
      // (*dpgf_)[i]->SetData(dudt.GetData() + offsets_[i]);
      (*dpgf_)[i]->MakeRef(fes_, dudt.GetData() + offsets_[i]);
   }
   dpgf_->ExchangeFaceNbrData();

   if (mpi_.Root() == 0 && logging_ > 0)
   {
      cout << "Setting time step: " << dt << endl;
   }
   op_.SetTimeStep(dt);
   // op_.UpdateGradient();
   /*
   int size = fes_->GetVSize();

   {
      Vector x(5*size); x = 0.0;
      Vector h(5*size);
      Vector h1(h.GetData(), size);

      h = 0.0;
      h1.Randomize();

      double nrmu = u.Norml2();
      double nrmh = h1.Norml2();
      h1 *= 0.01 * nrmu / nrmh;

      double cg = newton_solver_.CheckGradient(x, h);

      cout << "Gradient check returns (d n_n / d n_n): " << cg << endl;
   }
   */
   /*
   {
     Vector x(5*size); x = u;
     Vector h(5*size);
     h = 0.0;

     Vector x2(x.GetData(), 2*size);
     Vector h1(h.GetData(), size);
     Vector h2(h.GetData(), 2*size);

     double nrmx = x2.Norml2();
     h1.Randomize();
     double nrmh = h1.Norml2();
     h1 *= 0.01 * nrmx / nrmh;
     cout << "Norms " << nrmx << "\t" << h1.Norml2() << endl;
     double cg = newton_solver_.CheckGradient(x2, h2);

     cout << "Gradient check returns (n_n): " << cg << endl;
   }
   {
     Vector x(5*size); x = u;
     Vector h(5*size);
     h = 0.0;

     Vector x2(x.GetData(), 2*size);
     Vector h1(&(h.GetData()[size]), size);
     Vector h2(h.GetData(), 2*size);

     double nrmx = x2.Norml2();
     h1.Randomize();
     double nrmh = h1.Norml2();
     h1 *= 0.01 * nrmx / nrmh;

     double cg = newton_solver_.CheckGradient(x2, h2);

     cout << "Gradient check returns (n_i): " << cg << endl;
   }
   {
     Vector x(5*size); x = u;
     Vector h(5*size);
     h = 0.0;

     Vector x2(x.GetData(), 2*size);
     Vector h2(h.GetData(), 2*size);

     double nrmx = x2.Norml2();
     h2.Randomize();
     double nrmh = h2.Norml2();
     h2 *= 0.01 * nrmx / nrmh;

     double cg = newton_solver_.CheckGradient(x2, h2);

     cout << "Gradient check returns: " << cg << endl;
   }
   */
   // return;
   Vector zero;
   // Vector dndt(dudt.GetData(), 2 * size);
   // newton_solver_.Mult(zero, dndt);
   /*
   Operator &grad = op_.GetGradient(zero);
   BlockOperator &blk_grad = dynamic_cast<BlockOperator&>(grad);
   for (int i=0; i<5; i++)
     if (!blk_grad.IsZeroBlock(i,i))
     {
       newton_op_prec_blocks_[i]->SetOperator(blk_grad.GetBlock(i,i));
       newton_op_prec_.SetDiagonalBlock(i, newton_op_prec_blocks_[i]);
     }
   */
   newton_solver_.Mult(zero, dudt);
   /*
   u_.SetDataAndSize(const_cast<double*>(&u[0*size]), size);
   dudt_.SetDataAndSize(&dudt[0*size], size);
   n_n_oper_.ImplicitSolve(dt, u_, dudt_);

   u_.SetDataAndSize(const_cast<double*>(&u[1*size]), size);
   dudt_.SetDataAndSize(&dudt[1*size], size);
   n_i_oper_.ImplicitSolve(dt, u_, dudt_);
   */
   /*
   u_.SetDataAndSize(const_cast<double*>(&u[2*size]), size);
   dudt_.SetDataAndSize(&dudt[2*size], size);
   dudt_ = 0.0;
   // v_i_oper_.ImplicitSolve(dt, u_, dudt_);

   u_.SetDataAndSize(const_cast<double*>(&u[3*size]), size);
   dudt_.SetDataAndSize(&dudt[3*size], size);
   dudt_ = 0.0;
   // T_i_oper_.ImplicitSolve(dt, u_, dudt_);

   u_.SetDataAndSize(const_cast<double*>(&u[4*size]), size);
   dudt_.SetDataAndSize(&dudt[4*size], size);
   dudt_ = 0.0;
   */
   // Restore the data arrays to those used before this method was called.
   for (int i=0; i<offsets_.Size() - 1; i++)
   {
      // (*pgf_)[i]->SetData(prev_u + offsets_[i]);
      (*pgf_)[i]->MakeRef(fes_, prev_u + offsets_[i]);
   }
   pgf_->ExchangeFaceNbrData();

   for (int i=0; i<offsets_.Size() - 1; i++)
   {
      // (*dpgf_)[i]->SetData(prev_du + offsets_[i]);
      (*dpgf_)[i]->MakeRef(fes_, prev_du + offsets_[i]);
   }
   if (prev_du != NULL)
   {
      dpgf_->ExchangeFaceNbrData();
   }

   // T_e_oper_.ImplicitSolve(dt, u_, dudt_);
   if (mpi_.Root() && logging_ > 1)
   {
      cout << "Leaving DGTransportTDO::ImplicitSolve" << endl;
   }
}

void DGTransportTDO::Update()
{
   height = width = ffes_->GetVSize();

   op_.Update();

   newton_solver_.SetOperator(op_);
   // n_n_oper_.Update();
   // n_i_oper_.Update();
   // v_i_oper_.Update();
   // T_i_oper_.Update();
   // T_e_oper_.Update();
}

DGTransportTDO::NLOperator::NLOperator(const MPI_Session & mpi,
                                       const DGParams & dg, int index,
                                       ParGridFunctionArray & pgf,
                                       ParGridFunctionArray & dpgf)
   : Operator(pgf[0]->ParFESpace()->GetVSize(),
              5*(pgf[0]->ParFESpace()->GetVSize())),
     mpi_(mpi), dg_(dg), index_(index), dt_(0.0),
     fes_(pgf[0]->ParFESpace()),
     pmesh_(fes_->GetParMesh()),
     pgf_(&pgf), dpgf_(&dpgf),
     dbfi_m_(5),
     blf_(5)
{
   blf_ = NULL;
}

DGTransportTDO::NLOperator::~NLOperator()
{
   for (int i=0; i<5; i++)
   {
      delete blf_[i];
   }
}

void DGTransportTDO::NLOperator::SetLogging(int logging, const string & prefix)
{
   logging_ = logging;
   log_prefix_ = prefix;
}

void DGTransportTDO::NLOperator::Mult(const Vector &k, Vector &y) const
{
   if (mpi_.Root() && logging_)
   {
      cout << log_prefix_ << "DGTransportTDO::NLOperator::Mult" << endl;
   }

   y = 0.0;

   // cout << "| u[" << index_ << "]| : " << (*pgf_)[index_]->Norml2() << endl;
   // cout << "|du[" << index_ << "]| : " << (*dpgf_)[index_]->Norml2() << endl;

   ElementTransformation *eltrans = NULL;

   for (int i=0; i < fes_->GetNE(); i++)
   {
      fes_->GetElementVDofs(i, vdofs_);

      const FiniteElement &fe = *fes_->GetFE(i);
      eltrans = fes_->GetElementTransformation(i);

      int ndof = vdofs_.Size();

      elvec_.SetSize(ndof);
      locdvec_.SetSize(ndof);

      elvec_ = 0.0;

      for (int j=0; j<5; j++)
      {
         if (dbfi_m_[j].Size() > 0)
         {
            (*dpgf_)[j]->GetSubVector(vdofs_, locdvec_);

            dbfi_m_[j][0]->AssembleElementMatrix(fe, *eltrans, elmat_);
            for (int k = 1; k < dbfi_m_[j].Size(); k++)
            {
               dbfi_m_[j][k]->AssembleElementMatrix(fe, *eltrans, elmat_k_);
               elmat_ += elmat_k_;
            }

            elmat_.AddMult(locdvec_, elvec_);
         }
         y.AddElementVector(vdofs_, elvec_);
      }
   }

   // cout << "|y| after dbfi_m: " << y.Norml2() << endl;
   if (mpi_.Root() && logging_)
   {
      cout << log_prefix_
           << "DGTransportTDO::NLOperator::Mult element loop done" << endl;
   }
   if (dbfi_.Size())
   {
      ElementTransformation *eltrans = NULL;

      for (int i=0; i < fes_->GetNE(); i++)
      {
         fes_->GetElementVDofs(i, vdofs_);

         const FiniteElement &fe = *fes_->GetFE(i);
         eltrans = fes_->GetElementTransformation(i);

         int ndof = vdofs_.Size();

         elvec_.SetSize(ndof);
         locvec_.SetSize(ndof);
         locdvec_.SetSize(ndof);

         (*pgf_)[index_]->GetSubVector(vdofs_, locvec_);
         (*dpgf_)[index_]->GetSubVector(vdofs_, locdvec_);

         locvec_.Add(dt_, locdvec_);

         dbfi_[0]->AssembleElementMatrix(fe, *eltrans, elmat_);
         for (int k = 1; k < dbfi_.Size(); k++)
         {
            dbfi_[k]->AssembleElementMatrix(fe, *eltrans, elmat_k_);
            elmat_ += elmat_k_;
         }

         elmat_.Mult(locvec_, elvec_);

         y.AddElementVector(vdofs_, elvec_);
      }
   }
   // cout << "|y| after dbfi: " << y.Norml2() << endl;
   if (mpi_.Root() && logging_)
   {
      cout << log_prefix_
           << "DGTransportTDO::NLOperator::Mult element loop done" << endl;
   }
   if (fbfi_.Size())
   {
      FaceElementTransformations *ftrans = NULL;

      for (int i = 0; i < pmesh_->GetNumFaces(); i++)
      {
         ftrans = pmesh_->GetInteriorFaceTransformations(i);
         if (ftrans != NULL)
         {
            fes_->GetElementVDofs(ftrans->Elem1No, vdofs_);
            fes_->GetElementVDofs(ftrans->Elem2No, vdofs2_);
            vdofs_.Append(vdofs2_);

            const FiniteElement &fe1 = *fes_->GetFE(ftrans->Elem1No);
            const FiniteElement &fe2 = *fes_->GetFE(ftrans->Elem2No);

            fbfi_[0]->AssembleFaceMatrix(fe1, fe2, *ftrans, elmat_);
            for (int k = 1; k < fbfi_.Size(); k++)
            {
               fbfi_[k]->AssembleFaceMatrix(fe1, fe2, *ftrans, elmat_k_);
               elmat_ += elmat_k_;
            }

            int ndof = vdofs_.Size();

            elvec_.SetSize(ndof);
            locvec_.SetSize(ndof);
            locdvec_.SetSize(ndof);

            (*pgf_)[index_]->GetSubVector(vdofs_, locvec_);
            (*dpgf_)[index_]->GetSubVector(vdofs_, locdvec_);

            locvec_.Add(dt_, locdvec_);

            elmat_.Mult(locvec_, elvec_);

            y.AddElementVector(vdofs_, elvec_);
         }
      }
      // cout << "Size of y vector: " << y.Size() << ", height = " << height << ", fnd = " << (*pgf_)[index_]->FaceNbrData().Size() << endl;

      Vector elvec(NULL, 0);
      Vector locvec1(NULL, 0);
      Vector locvec2(NULL, 0);
      Vector locdvec1(NULL, 0);
      Vector locdvec2(NULL, 0);

      // DenseMatrix elmat(NULL, 0, 0);

      int nsfaces = pmesh_->GetNSharedFaces();
      for (int i = 0; i < nsfaces; i++)
      {
         ftrans = pmesh_->GetSharedFaceTransformations(i);
         fes_->GetElementVDofs(ftrans->Elem1No, vdofs_);
         fes_->GetFaceNbrElementVDofs(ftrans->Elem2No, vdofs2_);
         // cout << "vdofs2_ = {" << vdofs2_[0] << "..." << vdofs2_[vdofs2_.Size()-1] << endl;
         /*
         vdofs_.Copy(vdofs_all_);
         for (int j = 0; j < vdofs2_.Size(); j++)
         {
               if (vdofs2_[j] >= 0)
               {
                  vdofs2_[j] += height;
               }
               else
               {
                  vdofs2_[j] -= height;
               }
            }

            vdofs_all_.Append(vdofs2_);
         */
         for (int k = 0; k < fbfi_.Size(); k++)
         {
            fbfi_[k]->AssembleFaceMatrix(*fes_->GetFE(ftrans->Elem1No),
                                         *fes_->GetFaceNbrFE(ftrans->Elem2No),
                                         *ftrans, elmat_);
            // cout << "vdof sizes " << vdofs_.Size() << " " << vdofs2_.Size() << " " << vdofs_all_.Size() << ", elmat " << elmat_.Height() << "x" << elmat_.Width() << endl;
            /*
            if (keep_nbr_block)
            {
              mat->AddSubMatrix(vdofs_all, vdofs_all, elemmat, skip_zeros);
            }
            else
            {
              mat->AddSubMatrix(vdofs1, vdofs_all, elemmat, skip_zeros);
            }
            */
            int ndof  = vdofs_.Size();
            int ndof2 = vdofs2_.Size();

            elvec_.SetSize(ndof+ndof2);
            locvec_.SetSize(ndof+ndof2);
            locdvec_.SetSize(ndof+ndof2);

            elvec.SetDataAndSize(&elvec_[0], ndof);

            locvec1.SetDataAndSize(&locvec_[0], ndof);
            locvec2.SetDataAndSize(&locvec_[ndof], ndof2);

            locdvec1.SetDataAndSize(&locdvec_[0], ndof);
            locdvec2.SetDataAndSize(&locdvec_[ndof], ndof2);

            // elmat.UseExternalData(elmat_.Data(), ndof, ndof + ndof2);

            (*pgf_)[index_]->GetSubVector(vdofs_, locvec1);
            (*dpgf_)[index_]->GetSubVector(vdofs_, locdvec1);

            (*pgf_)[index_]->FaceNbrData().GetSubVector(vdofs2_, locvec2);
            (*dpgf_)[index_]->FaceNbrData().GetSubVector(vdofs2_, locdvec2);

            locvec_.Add(dt_, locdvec_);

            elmat_.Mult(locvec_, elvec_);

            y.AddElementVector(vdofs_, elvec);
         }
      }

   }
   // cout << "|y| after fbfi: " << y.Norml2() << endl;
   if (mpi_.Root() && logging_)
   {
      cout << log_prefix_
           << "DGTransportTDO::NLOperator::Mult face loop done" << endl;
   }
   if (bfbfi_.Size())
   {
      FaceElementTransformations *ftrans = NULL;

      // Which boundary attributes need to be processed?
      Array<int> bdr_attr_marker(pmesh_->bdr_attributes.Size() ?
                                 pmesh_->bdr_attributes.Max() : 0);
      bdr_attr_marker = 0;
      for (int k = 0; k < bfbfi_.Size(); k++)
      {
         if (bfbfi_marker_[k] == NULL)
         {
            bdr_attr_marker = 1;
            break;
         }
         Array<int> &bdr_marker = *bfbfi_marker_[k];
         MFEM_ASSERT(bdr_marker.Size() == bdr_attr_marker.Size(),
                     "invalid boundary marker for boundary face integrator #"
                     << k << ", counting from zero");
         for (int i = 0; i < bdr_attr_marker.Size(); i++)
         {
            bdr_attr_marker[i] |= bdr_marker[i];
         }
      }

      for (int i = 0; i < fes_ -> GetNBE(); i++)
      {
         const int bdr_attr = pmesh_->GetBdrAttribute(i);
         if (bdr_attr_marker[bdr_attr-1] == 0) { continue; }

         ftrans = pmesh_->GetBdrFaceTransformations(i);
         if (ftrans != NULL)
         {
            fes_->GetElementVDofs(ftrans->Elem1No, vdofs_);

            int ndof = vdofs_.Size();

            const FiniteElement &fe1 = *fes_->GetFE(ftrans->Elem1No);
            const FiniteElement &fe2 = fe1;

            elmat_.SetSize(ndof);
            elmat_ = 0.0;

            for (int k = 0; k < fbfi_.Size(); k++)
            {
               if (bfbfi_marker_[k] &&
                   (*bfbfi_marker_[k])[bdr_attr-1] == 0) { continue; }

               bfbfi_[k]->AssembleFaceMatrix(fe1, fe2, *ftrans, elmat_k_);
               elmat_ += elmat_k_;
            }

            elvec_.SetSize(ndof);
            locvec_.SetSize(ndof);
            locdvec_.SetSize(ndof);

            (*pgf_)[index_]->GetSubVector(vdofs_, locvec_);
            (*dpgf_)[index_]->GetSubVector(vdofs_, locdvec_);

            locvec_.Add(dt_, locdvec_);

            elmat_.Mult(locvec_, elvec_);

            y.AddElementVector(vdofs_, elvec_);
         }
      }
   }
   // cout << "|y| after bfbfi: " << y.Norml2() << endl;
   if (dlfi_.Size())
   {
      ElementTransformation *eltrans = NULL;

      for (int i=0; i < fes_->GetNE(); i++)
      {
         fes_->GetElementVDofs(i, vdofs_);
         eltrans = fes_->GetElementTransformation(i);

         int ndof = vdofs_.Size();
         elvec_.SetSize(ndof);

         for (int k=0; k < dlfi_.Size(); k++)
         {
            dlfi_[k]->AssembleRHSElementVect(*fes_->GetFE(i), *eltrans, elvec_);
            y.AddElementVector (vdofs_, elvec_);
         }
      }
   }

   if (mpi_.Root() && logging_)
   {
      cout << log_prefix_
           << "DGTransportTDO::NLOperator::Mult done" << endl;
   }
   /*
   {
      ParBilinearForm m((*pgf_)[0]->ParFESpace());
      m.AddDomainIntegrator(new MassIntegrator);
      m.Assemble();
      m.Finalize();
      HypreParMatrix *M = m.ParallelAssemble();

      ParGridFunction ygf((*pgf_)[0]->ParFESpace());

      CG(*M, y, ygf);

      socketstream sock;
      char vishost[] = "localhost";
      int  visport   = 19916;

      VisualizeField(sock, vishost, visport, ygf, "y n_n", 0,0,200,200);

      delete M;
   }
   */
}

void DGTransportTDO::NLOperator::Update()
{
   height = fes_->GetVSize();
   width  = 5 * fes_->GetVSize();

   for (int i=0; i<5; i++)
   {
      if (blf_[i] != NULL)
      {
         blf_[i]->Update();
      }
   }
}

Operator *DGTransportTDO::NLOperator::GetGradientBlock(int i)
{
   if ( blf_[i] != NULL)
   {
      blf_[i]->Update();
      blf_[i]->Assemble();
      blf_[i]->Finalize();
      Operator * D = blf_[i]->ParallelAssemble();
      return D;
   }
   else
   {
      return NULL;
   }
   /*
    switch (i)
    {
       default:
          return NULL;
    }
   */
}

DGTransportTDO::CombinedOp::CombinedOp(const MPI_Session & mpi,
                                       const DGParams & dg,
                                       ParGridFunctionArray & pgf,
                                       ParGridFunctionArray & dpgf,
                                       Array<int> & offsets,
                                       int ion_charge, double ion_mass,
                                       double neutral_mass, double neutral_temp,
                                       double DiPerp,
                                       VectorCoefficient &B3Coef,
                                       VectorCoefficient &bHatCoef,
                                       MatrixCoefficient &PerpCoef,
                                       unsigned int op_flag, int logging)
   : mpi_(mpi),
     neq_(5),
     //MyRank_(pgf[0]->ParFESpace()->GetMyRank()),
     logging_(logging),
     fes_(pgf[0]->ParFESpace()),
     pgf_(&pgf), dpgf_(&dpgf),
     /*
     n_n_op_(dg, pgf, dpgf, ion_charge, neutral_mass, neutral_temp),
     n_i_op_(dg, pgf, dpgf, ion_charge, DiPerp, PerpCoef),
     v_i_op_(dg, pgf, dpgf, 2),
     t_i_op_(dg, pgf, dpgf, 3),
     t_e_op_(dg, pgf, dpgf, 4),
     */
     op_(neq_),
     //offsets_(neq_+1),
     offsets_(offsets),
     grad_(NULL)
{
   op_ = NULL;

   if ((op_flag >> 0) & 1)
   {
      op_[0] = new NeutralDensityOp(mpi, dg, pgf, dpgf, ion_charge,
                                    neutral_mass, neutral_temp);
      op_[0]->SetLogging(logging, "n_n: ");
   }
   else
   {
      op_[0] = new DummyOp(mpi, dg, pgf, dpgf, 0);
      op_[0]->SetLogging(logging, "n_n (dummy): ");
   }

   if ((op_flag >> 1) & 1)
   {
      op_[1] = new IonDensityOp(mpi, dg, pgf, dpgf, ion_charge, DiPerp,
                                bHatCoef, PerpCoef);
      op_[1]->SetLogging(logging, "n_i: ");
   }
   else
   {
      op_[1] = new DummyOp(mpi, dg, pgf, dpgf, 1);
      op_[1]->SetLogging(logging, "n_i (dummy): ");
   }

   if ((op_flag >> 2) & 1)
   {
      op_[2] = new IonMomentumOp(mpi, dg, pgf, dpgf, ion_charge, ion_mass,
                                 DiPerp, B3Coef, bHatCoef, PerpCoef);
      op_[2]->SetLogging(logging, "v_i: ");
   }
   else
   {
      op_[2] = new DummyOp(mpi, dg, pgf, dpgf, 2);
      op_[2]->SetLogging(logging, "v_i (dummy): ");
   }

   op_[3] = new DummyOp(mpi, dg, pgf, dpgf, 3);
   op_[3]->SetLogging(logging, "T_i (dummy): ");

   op_[4] = new DummyOp(mpi, dg, pgf, dpgf, 4);
   op_[4]->SetLogging(logging, "T_e (dummy): ");
   /*
   op_[0] = &n_n_op_;
   op_[1] = &n_i_op_;
   op_[2] = &v_i_op_;
   op_[3] = &t_i_op_;
   op_[4] = &t_e_op_;
   */
   this->updateOffsets();
}

DGTransportTDO::CombinedOp::~CombinedOp()
{
   delete grad_;

   for (int i=0; i<op_.Size(); i++) { delete op_[i]; }
   op_.SetSize(0);
}

void DGTransportTDO::CombinedOp::updateOffsets()
{
   offsets_[0] = 0;

   for (int i=0; i<neq_; i++)
   {
      offsets_[i+1] = op_[i]->Height();
   }

   offsets_.PartialSum();

   height = width = offsets_[neq_];
}

void DGTransportTDO::CombinedOp::SetTimeStep(double dt)
{
   if ( mpi_.Root() && logging_ > 0)
   {
      cout << "Setting time step: " << dt << " in CombinedOp" << endl;
   }
   for (int i=0; i<neq_; i++)
   {
      op_[i]->SetTimeStep(dt);
   }
}

void DGTransportTDO::CombinedOp::SetLogging(int logging)
{
   logging_ = logging;

   op_[0]->SetLogging(logging, "n_n: ");
   op_[1]->SetLogging(logging, "n_i: ");
   op_[2]->SetLogging(logging, "v_i: ");
   op_[3]->SetLogging(logging, "T_i: ");
   op_[4]->SetLogging(logging, "T_e: ");
}

void DGTransportTDO::CombinedOp::Update()
{
   for (int i=0; i<neq_; i++)
   {
      op_[i]->Update();
   }

   this->updateOffsets();
}

void DGTransportTDO::CombinedOp::UpdateGradient(const Vector &x) const
{
   if ( mpi_.Root() && logging_ > 1)
   {
      cout << "DGTransportTDO::CombinedOp::UpdateGradient" << endl;
   }
   // cout << "calling CombinedOp::UpdateGradient with arg x = " << x.GetData() <<
   //       ", pgf = " << (*pgf_)[0]->GetData() << ", dpgf = " << (*dpgf_)[0]->GetData() <<
   //     " -> " << x.GetData() << endl;

   delete grad_;

   double *prev_x = (*dpgf_)[0]->GetData();

   for (int i=0; i<dpgf_->Size(); i++)
   {
      (*dpgf_)[i]->MakeRef(fes_, x.GetData() + offsets_[i]);
   }
   dpgf_->ExchangeFaceNbrData();

   grad_ = new BlockOperator(offsets_);
   grad_->owns_blocks = true;

   for (int i=0; i<neq_; i++)
   {
      for (int j=0; j<neq_; j++)
      {
         Operator * gradIJ = op_[i]->GetGradientBlock(j);
         if (gradIJ)
         {
            // cout << "setting gradient block " << i << " " << j << endl;
            grad_->SetBlock(i, j, gradIJ);
         }
      }
   }

   for (int i=0; i<offsets_.Size() - 1; i++)
   {
      // (*dpgf_)[i]->SetData(prev_x + offsets_[i]);
      (*dpgf_)[i]->MakeRef(fes_, prev_x + offsets_[i]);
   }
   if (prev_x != NULL)
   {
      dpgf_->ExchangeFaceNbrData();
   }

   if ( mpi_.Root() && logging_ > 1)
   {
      cout << "DGTransportTDO::CombinedOp::UpdateGradient done" << endl;
   }
}

void DGTransportTDO::CombinedOp::Mult(const Vector &k, Vector &y) const
{
   if ( mpi_.Root() && logging_ > 1)
   {
      cout << "DGTransportTDO::CombinedOp::Mult" << endl;
   }
   // cout << "calling CombinedOp::Mult with arg k = " << k.GetData() << ", pgf = "
   //    << (*pgf_)[0]->GetData() << ", dpgf = " << (*dpgf_)[0]->GetData() << " -> " <<
   //   k.GetData() << endl;

   double *prev_k = (*dpgf_)[0]->GetData();

   for (int i=0; i<dpgf_->Size(); i++)
   {
      (*dpgf_)[i]->MakeRef(fes_, k.GetData() + offsets_[i]);
   }
   dpgf_->ExchangeFaceNbrData();

   for (int i=0; i<neq_; i++)
   {
      int size = offsets_[i+1] - offsets_[i];

      // Vector k_i(const_cast<double*>(&k[offsets_[i]]), size);
      Vector y_i(&y[offsets_[i]], size);

      op_[i]->Mult(k, y_i);

      // cout << "Norm of y_" << i << " " << y_i.Norml2() << ", offsets = " <<
      //   offsets_[i] << endl;
   }

   for (int i=0; i<offsets_.Size() - 1; i++)
   {
      // (*dpgf_)[i]->SetData(prev_k + offsets_[i]);
      (*dpgf_)[i]->MakeRef(fes_, prev_k + offsets_[i]);
   }
   if (prev_k != NULL)
   {
      dpgf_->ExchangeFaceNbrData();
   }

   if ( mpi_.Root() && logging_ > 1)
   {
      cout << "DGTransportTDO::CombinedOp::Mult done" << endl;
   }
}

DGTransportTDO::NeutralDensityOp::NeutralDensityOp(const MPI_Session & mpi,
                                                   const DGParams & dg,
                                                   ParGridFunctionArray & pgf,
                                                   ParGridFunctionArray & dpgf,
                                                   int ion_charge,
                                                   double neutral_mass,
                                                   double neutral_temp)
   : NLOperator(mpi, dg, 0, pgf, dpgf),
     z_i_(ion_charge), m_n_(neutral_mass), T_n_(neutral_temp),
     nn0Coef_(pgf[0]), ni0Coef_(pgf[1]), Te0Coef_(pgf[4]),
     dnnCoef_(dpgf[0]), dniCoef_(dpgf[1]), dTeCoef_(dpgf[4]),
     nn1Coef_(nn0Coef_, dnnCoef_), ni1Coef_(ni0Coef_, dniCoef_),
     Te1Coef_(Te0Coef_, dTeCoef_),
     ne0Coef_(z_i_, ni0Coef_),
     ne1Coef_(z_i_, ni1Coef_),
     vnCoef_(sqrt(8.0 * T_n_ * eV_ / (M_PI * m_n_ * amu_))),
     izCoef_(Te1Coef_),
     DCoef_(ne1Coef_, vnCoef_, izCoef_),
     dtDCoef_(0.0, DCoef_),
     SizCoef_(ne1Coef_, nn1Coef_, izCoef_),
     nnizCoef_(nn1Coef_, izCoef_),
     neizCoef_(ne1Coef_, izCoef_),
     dtdSndnnCoef_(0.0, neizCoef_),
     dtdSndniCoef_(0.0, nnizCoef_)//,
     // diff_(DCoef_),
     // dg_diff_(DCoef_, dg_.sigma, dg_.kappa)
{
   dbfi_m_[0].Append(new MassIntegrator);
   dbfi_.Append(new DiffusionIntegrator(DCoef_));
   fbfi_.Append(new DGDiffusionIntegrator(DCoef_,
                                          dg_.sigma,
                                          dg_.kappa));

   dlfi_.Append(new DomainLFIntegrator(SizCoef_));

   blf_[0] = new ParBilinearForm((*pgf_)[0]->ParFESpace());
   blf_[0]->AddDomainIntegrator(new MassIntegrator);
   blf_[0]->AddDomainIntegrator(new DiffusionIntegrator(dtDCoef_));
   blf_[0]->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(dtDCoef_,
                                                                dg_.sigma,
                                                                dg_.kappa));
   // blf_[0]->AddDomainIntegrator(new MassIntegrator(dtdSndnnCoef_));
   // blf_[1] = new ParBilinearForm((*pgf_)[1]->ParFESpace());
   // blf_[1]->AddDomainIntegrator(new MassIntegrator(dtdSndniCoef_));
}

void DGTransportTDO::NeutralDensityOp::SetTimeStep(double dt)
{
   if (mpi_.Root() && logging_)
   {
      cout << "Setting time step: " << dt << " in NeutralDensityOp" << endl;
   }
   NLOperator::SetTimeStep(dt);

   nn1Coef_.SetBeta(dt);
   ni1Coef_.SetBeta(dt);
   Te1Coef_.SetBeta(dt);

   dtDCoef_.SetAConst(dt);
   dtdSndnnCoef_.SetAConst(dt);
   dtdSndniCoef_.SetAConst(dt * z_i_);
}

void DGTransportTDO::NeutralDensityOp::Update()
{
   NLOperator::Update();
}
/*
void DGTransportTDO::NeutralDensityOp::Mult(const Vector &k, Vector &y) const
{
 cout << "DGTransportTDO::NeutralDensityOp::Mult" << endl;
 ParFiniteElementSpace *fespace = (*pgf_)[0]->ParFESpace();
 ParMesh *pmesh = fespace->GetParMesh();

 y = 0.0;

 {
    Array<int> vdofs;

    for (int e=0; e<fespace->GetNE(); e++)
    {
       const FiniteElement *fe = fespace->GetFE(e);
       ElementTransformation *eltrans = fespace->GetElementTransformation(e);

       int ndof = fe->GetDof();
       int dim  = fe->GetDim();
       int bord = fe->GetOrder();

       shape_.SetSize(ndof);
       dshape_.SetSize(ndof, dim);
       dshapedxt_.SetSize(ndof, dim);
       elvec_.SetSize(ndof);
       locvec_.SetSize(ndof);
       locdvec_.SetSize(ndof);
       vec_.SetSize(dim);

       elvec_ = 0.0;

       fespace->GetElementVDofs (e, vdofs);
       (*pgf_)[0]->GetSubVector(vdofs, locvec_);
       (*dpgf_)[0]->GetSubVector(vdofs, locdvec_);

       locvec_.Add(dt_, locdvec_);

       Geometry::Type geom = fe->GetGeomType();

       // int iord = 2 * bord + eltrans->OrderW();
       int iord = 7;

       const IntegrationRule *ir = &IntRules.Get(geom, iord);
       // std::cout << "IR Order: " << ir->GetOrder() << std::endl;

       for (int i=0; i<ir->GetNPoints(); i++)
       {
          const IntegrationPoint &ip = ir->IntPoint(i);
          eltrans->SetIntPoint(&ip);

          double detJ = eltrans->Weight();

          fe->CalcShape(ip, shape_);
          fe->CalcDShape(ip, dshape_);

          mfem::Mult(dshape_, eltrans->AdjugateJacobian(), dshapedxt_);

          // double nn = nn1Coef_.Eval(*eltrans, ip);
          // double ni = ni1Coef_.Eval(*eltrans, ip);
          // double Te = Te1Coef_.Eval(*eltrans, ip);
          double nn = nn0Coef_.Eval(*eltrans, ip);
          double ni = ni0Coef_.Eval(*eltrans, ip);
          double Te = Te0Coef_.Eval(*eltrans, ip);

          double ne = ni * z_i_;

          const double vn2 = 8.0 * T_n_ * eV_ / (M_PI * m_n_ * amu_);
          double sv_iz = 3.0e-16 * Te * Te / (3.0 + 0.01 * Te * Te);

          double Dn = vn2 / (3.0 * ne * sv_iz);
          double Sn = ne * nn * sv_iz;

          // add(elvec_, detJ * ip.weight * (locdvec_ * shape_), shape_, elvec_);

          dshapedxt_.MultTranspose(locvec_, vec_);
          dshapedxt_.AddMult_a(Dn * ip.weight / detJ, vec_, elvec_);

          // add(elvec_, detJ * ip.weight * Sn, shape_, elvec_);
       }

       y.AddElementVector(vdofs, elvec_);
    }
 }
 cout << "DGTransportTDO::NeutralDensityOp::Mult element loop done" << endl;
 {
    FaceElementTransformations *ftrans = NULL;
    Array<int> vdofs;
    Array<int> vdofs2;

    for (int f = 0; f < pmesh->GetNumFaces(); f++)
    {
       ftrans = pmesh->GetInteriorFaceTransformations(f);
       if (ftrans != NULL)
       {
          fespace->GetElementVDofs(ftrans->Elem1No, vdofs);
          fespace->GetElementVDofs(ftrans->Elem2No, vdofs2);
          vdofs.Append(vdofs2);

          const FiniteElement *fe1 = fespace->GetFE(ftrans->Elem1No);
          const FiniteElement *fe2 = fespace->GetFE(ftrans->Elem2No);

          dg_diff_.AssembleFaceMatrix(*fe1, *fe2, *ftrans, elmat_);

          (*pgf_)[0]->GetSubVector(vdofs, locvec_);
          (*dpgf_)[0]->GetSubVector(vdofs, locdvec_);

          locvec_.Add(dt_, locdvec_);

          elvec_.SetSize(elmat_.Height());
          elmat_.Mult(locvec_, elvec_);
          // elvec_ *= -1.0;
          // y.AddElementVector(vdofs, elvec_);
       }
    }
 }
 cout << "DGTransportTDO::NeutralDensityOp::Mult done" << endl;
 {
    ParBilinearForm m((*pgf_)[0]->ParFESpace());
    m.AddDomainIntegrator(new MassIntegrator);
    m.Assemble();
    m.Finalize();
    HypreParMatrix *M = m.ParallelAssemble();

    ParGridFunction ygf((*pgf_)[0]->ParFESpace());

    CG(*M, y, ygf);

    socketstream sock;
    char vishost[] = "localhost";
    int  visport   = 19916;

    VisualizeField(sock, vishost, visport, ygf, "y n_n", 0,0,200,200);

    delete M;
 }
}
*/
/*
Operator *DGTransportTDO::NeutralDensityOp::GetGradientBlock(int i)
{
 switch (i)
 {
    default:
       return NULL;
 }
}
*/
DGTransportTDO::IonDensityOp::IonDensityOp(const MPI_Session & mpi,
                                           const DGParams & dg,
                                           ParGridFunctionArray & pgf,
                                           ParGridFunctionArray & dpgf,
                                           int ion_charge,
                                           double DPerp,
                                           VectorCoefficient & bHatCoef,
                                           MatrixCoefficient & PerpCoef)
   : NLOperator(mpi, dg, 1, pgf, dpgf), z_i_(ion_charge), DPerpConst_(DPerp),
     nn0Coef_(pgf[0]), ni0Coef_(pgf[1]), vi0Coef_(pgf[2]), Te0Coef_(pgf[4]),
     dnnCoef_(dpgf[0]), dniCoef_(dpgf[1]), dviCoef_(dpgf[2]), dTeCoef_(dpgf[4]),
     nn1Coef_(nn0Coef_, dnnCoef_), ni1Coef_(ni0Coef_, dniCoef_),
     vi1Coef_(vi0Coef_, dviCoef_), Te1Coef_(Te0Coef_, dTeCoef_),
     ne0Coef_(z_i_, ni0Coef_),
     ne1Coef_(z_i_, ni1Coef_),
     izCoef_(Te1Coef_),
     DPerpCoef_(DPerp),
     PerpCoef_(&PerpCoef), DCoef_(DPerpCoef_, *PerpCoef_),
     dtDCoef_(0.0, DCoef_),
     bHatCoef_(&bHatCoef),
     ViCoef_(vi1Coef_, *bHatCoef_), dtViCoef_(0.0, ViCoef_),
     SizCoef_(ne1Coef_, nn1Coef_, izCoef_),
     negSizCoef_(-1.0, SizCoef_),
     nnizCoef_(nn1Coef_, izCoef_),
     niizCoef_(ni1Coef_, izCoef_)//,
     // dtdSndnnCoef_(0.0, niizCoef_),
     // dtdSndniCoef_(0.0, nnizCoef_)
{
   // dn_i / dt
   dbfi_m_[1].Append(new MassIntegrator);

   // -Div(D Grad n_i)
   dbfi_.Append(new DiffusionIntegrator(DCoef_));
   fbfi_.Append(new DGDiffusionIntegrator(DCoef_,
                                          dg_.sigma,
                                          dg_.kappa));

   // Div(v_i n_i)
   dbfi_.Append(new MixedScalarWeakDivergenceIntegrator(ViCoef_));
   fbfi_.Append(new DGTraceIntegrator(ViCoef_, 1.0, -0.5));
   // bfbfi_.Append(new DGTraceIntegrator(ViCoef_, 1.0, -0.5));

   // -S_{iz}
   dlfi_.Append(new DomainLFIntegrator(negSizCoef_));

   // blf_[0] = new ParBilinearForm((*pgf_)[0]->ParFESpace());
   // blf_[0]->AddDomainIntegrator(new MassIntegrator(dtdSndnnCoef_));
   blf_[1] = new ParBilinearForm((*pgf_)[1]->ParFESpace());
   blf_[1]->AddDomainIntegrator(new MassIntegrator);
   blf_[1]->AddDomainIntegrator(new DiffusionIntegrator(dtDCoef_));
   blf_[1]->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(dtDCoef_,
                                                                dg_.sigma,
                                                                dg_.kappa));
   blf_[1]->AddDomainIntegrator(
      new MixedScalarWeakDivergenceIntegrator(dtViCoef_));
   blf_[1]->AddInteriorFaceIntegrator(new DGTraceIntegrator(dtViCoef_,
                                                            1.0, -0.5));
   // blf_[1]->AddDomainIntegrator(new MassIntegrator(dtdSndniCoef_));
}


void DGTransportTDO::IonDensityOp::SetTimeStep(double dt)
{
   if (mpi_.Root() && logging_)
   {
      cout << "Setting time step: " << dt << " in IonDensityOp" << endl;
   }
   NLOperator::SetTimeStep(dt);

   nn1Coef_.SetBeta(dt);
   ni1Coef_.SetBeta(dt);
   vi1Coef_.SetBeta(dt);
   Te1Coef_.SetBeta(dt);

   dtDCoef_.SetAConst(dt);
   dtViCoef_.SetAConst(dt);
   // dtdSndnnCoef_.SetAConst(-dt);
   // dtdSndniCoef_.SetAConst(-dt * z_i_);
}

void DGTransportTDO::IonDensityOp::Update()
{
   NLOperator::Update();
}
/*
void DGTransportTDO::IonDensityOp::Mult(const Vector &k, Vector &y) const
{
   cout << "DGTransportTDO::IonDensityOp::Mult" << endl;
   ParFiniteElementSpace *fespace = (*pgf_)[0]->ParFESpace();
   ParMesh *pmesh = fespace->GetParMesh();

   y = 0.0;
   return;
   {
      Array<int> vdofs;

      for (int e=0; e<fespace->GetNE(); e++)
      {
         fespace->GetElementVDofs (e, vdofs);

         const FiniteElement *fe = fespace->GetFE(e);
         ElementTransformation *eltrans = fespace->GetElementTransformation(e);
         diff_.AssembleElementMatrix(*fe, *eltrans, elmat_);

         (*pgf_)[0]->GetSubVector(vdofs, locvec_);
         (*dpgf_)[0]->GetSubVector(vdofs, locdvec_);

         locvec_.Add(dt_, locdvec_);

         elvec_.SetSize(elmat_.Height());
         elmat_.Mult(locvec_, elvec_);
         // elvec_ *= -1.0;

         int ndof = fe->GetDof();
         // int dim  = fe->GetDim();
         int bord = fe->GetOrder();

         shape_.SetSize(ndof);
         // dshape_.SetSize(ndof, dim);
         // dshapedxt_.SetSize(ndof, dim);
         // elvec_.SetSize(ndof);
         // locvec_.SetSize(ndof);
         // locdvec_.SetSize(ndof);
         // vec_.SetSize(dim);

         // elvec_ = 0.0;

         // fespace->GetElementVDofs (e, vdofs);
         // (*pgf_)[0]->GetSubVector(vdofs, locvec_);
         // (*dpgf_)[0]->GetSubVector(vdofs, locdvec_);

         // locvec_.Add(dt_, locdvec_);

         Geometry::Type geom = fe->GetGeomType();

         int iord = 2 * bord;

         const IntegrationRule *ir = &IntRules.Get(geom, iord);

         for (int i=0; i<ir->GetNPoints(); i++)
         {
            const IntegrationPoint &ip = ir->IntPoint(i);
            eltrans->SetIntPoint(&ip);

            double detJ = eltrans->Weight();

            fe->CalcShape(ip, shape_);

            double nn = nn1Coef_.Eval(*eltrans, ip);
            double ni = ni1Coef_.Eval(*eltrans, ip);
            double Te = Te1Coef_.Eval(*eltrans, ip);

            double ne = ni * z_i_;

            // const double vn2 = 8.0 * T_n_ * eV_ / (M_PI * m_n_ * amu_);
            double sv_iz = 3.0e-16 * Te * Te / (3.0 + 0.01 * Te * Te);

            double Sn = ne * nn * sv_iz;

            // dshapedxt_.MultTranspose(locvec_, vec_);
            // dshapedxt_.AddMult_a(Dn, vec_, elvec_);

            add(elvec_,
                detJ * ip.weight * ((shape_ * locdvec_) - Sn), shape_,
                elvec_);
         }

         y.AddElementVector(vdofs, elvec_);
      }
   }
   cout << "DGTransportTDO::IonDensityOp::Mult element loop done" << endl;
   {
      FaceElementTransformations *ftrans = NULL;
      Array<int> vdofs;
      Array<int> vdofs2;

      for (int f = 0; f < pmesh->GetNumFaces(); f++)
      {
         ftrans = pmesh->GetInteriorFaceTransformations(f);
         if (ftrans != NULL)
         {
            fespace->GetElementVDofs(ftrans->Elem1No, vdofs);
            fespace->GetElementVDofs(ftrans->Elem2No, vdofs2);
            vdofs.Append(vdofs2);

            const FiniteElement *fe1 = fespace->GetFE(ftrans->Elem1No);
            const FiniteElement *fe2 = fespace->GetFE(ftrans->Elem2No);

            dg_diff_.AssembleFaceMatrix(*fe1, *fe2, *ftrans, elmat_);

            (*pgf_)[0]->GetSubVector(vdofs, locvec_);
            (*dpgf_)[0]->GetSubVector(vdofs, locdvec_);

            locvec_.Add(dt_, locdvec_);

            elvec_.SetSize(elmat_.Height());
            elmat_.Mult(locvec_, elvec_);
            y.AddElementVector(vdofs, elvec_);
         }
      }
   }
   cout << "DGTransportTDO::IonDensityOp::Mult done" << endl;
   {
      ParBilinearForm m((*pgf_)[0]->ParFESpace());
      m.AddDomainIntegrator(new MassIntegrator);
      m.Assemble();
      m.Finalize();
      HypreParMatrix *M = m.ParallelAssemble();

      ParGridFunction ygf((*pgf_)[0]->ParFESpace());

      CG(*M, y, ygf);

      socketstream sock;
      char vishost[] = "localhost";
      int  visport   = 19916;

      VisualizeField(sock, vishost, visport, ygf, "y n_i", 0,0,200,200);

      delete M;
   }
}
*/
/*
Operator *DGTransportTDO::IonDensityOp::GetGradientBlock(int i)
{
 switch (i)
 {
    default:
       return NULL;
 }
}
*/

DGTransportTDO::IonMomentumOp::IonMomentumOp(const MPI_Session & mpi,
                                             const DGParams & dg,
                                             ParGridFunctionArray & pgf,
                                             ParGridFunctionArray & dpgf,
                                             int ion_charge,
                                             double ion_mass,
                                             double DPerp,
                                             VectorCoefficient & B3Coef,
                                             VectorCoefficient & bHatCoef,
                                             MatrixCoefficient & PerpCoef)
   : NLOperator(mpi, dg, 2, pgf, dpgf), z_i_(ion_charge), m_i_(ion_mass),
     DPerpConst_(DPerp),
     DPerpCoef_(DPerp),
     nn0Coef_(pgf[0]), ni0Coef_(pgf[1]), vi0Coef_(pgf[2]),
     Ti0Coef_(pgf[3]), Te0Coef_(pgf[4]),
     dnnCoef_(dpgf[0]), dniCoef_(dpgf[1]), dviCoef_(dpgf[2]),
     dTiCoef_(dpgf[3]), dTeCoef_(dpgf[4]),
     nn1Coef_(nn0Coef_, dnnCoef_), ni1Coef_(ni0Coef_, dniCoef_),
     vi1Coef_(vi0Coef_, dviCoef_),
     Ti1Coef_(Ti0Coef_, dTiCoef_), Te1Coef_(Te0Coef_, dTeCoef_),
     ne0Coef_(z_i_, ni0Coef_),
     ne1Coef_(z_i_, ni1Coef_),
     mini1Coef_(m_i_, ni1Coef_),
     mivi1Coef_(m_i_, vi1Coef_),
     EtaCoef_(ion_charge, ion_mass, DPerpCoef_, ni1Coef_, Ti1Coef_, B3Coef),
     dtEtaCoef_(0.0, EtaCoef_),
     gradPCoef_(pgf, dpgf, ion_charge, bHatCoef),
     izCoef_(Te1Coef_),
     // PerpCoef_(&PerpCoef), DCoef_(DPerpCoef_, *PerpCoef_),
     // dtDCoef_(0.0, DCoef_),
     B3Coef_(&B3Coef),
     bHatCoef_(&bHatCoef),
     ViCoef_(vi1Coef_, *bHatCoef_), dtViCoef_(0.0, ViCoef_),
     SizCoef_(ne1Coef_, nn1Coef_, izCoef_),
     negSizCoef_(-1.0, SizCoef_),
     nnizCoef_(nn1Coef_, izCoef_),
     niizCoef_(ni1Coef_, izCoef_)
{
   // m_i v_i dn_i/dt
   dbfi_m_[1].Append(new MassIntegrator(mivi1Coef_));

   // m_i n_i dv_i/dt
   dbfi_m_[2].Append(new MassIntegrator(mini1Coef_));

   // -Div(eta Grad v_i)
   dbfi_.Append(new DiffusionIntegrator(EtaCoef_));
   fbfi_.Append(new DGDiffusionIntegrator(EtaCoef_,
                                          dg_.sigma,
                                          dg_.kappa));

   // b . Grad(p_i + p_e)
   dlfi_.Append(new DomainLFIntegrator(gradPCoef_));
   /*
   dbfi_.Append(new DiffusionIntegrator(DCoef_));
   fbfi_.Append(new DGDiffusionIntegrator(DCoef_,
                                          dg_.sigma,
                                          dg_.kappa));

   dbfi_.Append(new MixedScalarWeakDivergenceIntegrator(ViCoef_));
   fbfi_.Append(new DGTraceIntegrator(ViCoef_, 1.0, -0.5));

   dlfi_.Append(new DomainLFIntegrator(negSizCoef_));

   blf_[2] = new ParBilinearForm((*pgf_)[2]->ParFESpace());
   blf_[2]->AddDomainIntegrator(new MassIntegrator);
   blf_[2]->AddDomainIntegrator(new DiffusionIntegrator(dtDCoef_));
   blf_[2]->AddInteriorFaceIntegrator(new DGDiffusionIntegrator(dtDCoef_,
                                                                dg_.sigma,
                                                                dg_.kappa));
   blf_[2]->AddDomainIntegrator(
      new MixedScalarWeakDivergenceIntegrator(dtViCoef_));
   blf_[2]->AddInteriorFaceIntegrator(new DGTraceIntegrator(dtViCoef_,
                                                            1.0, -0.5));
   */
}


void DGTransportTDO::IonMomentumOp::SetTimeStep(double dt)
{
   if (mpi_.Root() && logging_)
   {
      cout << "Setting time step: " << dt << " in IonMomentumOp" << endl;
   }
   NLOperator::SetTimeStep(dt);

   nn1Coef_.SetBeta(dt);
   ni1Coef_.SetBeta(dt);
   vi1Coef_.SetBeta(dt);
   Ti1Coef_.SetBeta(dt);
   Te1Coef_.SetBeta(dt);

   dtEtaCoef_.SetAConst(dt);
   dtViCoef_.SetAConst(dt);
}

void DGTransportTDO::IonMomentumOp::Update()
{
   NLOperator::Update();
}

DGTransportTDO::DummyOp::DummyOp(const MPI_Session & mpi, const DGParams & dg,
                                 ParGridFunctionArray & pgf,
                                 ParGridFunctionArray & dpgf,
                                 int index)
   : NLOperator(mpi, dg, index, pgf, dpgf)
{
   dbfi_m_[index].Append(new MassIntegrator);

   blf_[index_] = new ParBilinearForm((*pgf_)[index_]->ParFESpace());
   blf_[index_]->AddDomainIntegrator(new MassIntegrator);
   blf_[index_]->Assemble();
   blf_[index_]->Finalize();
}

void DGTransportTDO::DummyOp::Update()
{
   NLOperator::Update();
}
/*
void DGTransportTDO::DummyOp::Mult(const Vector &k, Vector &y) const
{
 std::cout << "DGTransportTDO::DummyOp::Mult" << std::endl;
 y = 0.0;
 blf_[index_]->Mult(*(*dpgf_)[index_], y);
}
*/
TransportSolver::TransportSolver(ODESolver * implicitSolver,
                                 ODESolver * explicitSolver,
                                 ParFiniteElementSpace & sfes,
                                 ParFiniteElementSpace & vfes,
                                 ParFiniteElementSpace & ffes,
                                 BlockVector & nBV,
                                 ParGridFunction & B,
                                 Array<int> & charges,
                                 Vector & masses)
   : impSolver_(implicitSolver),
     expSolver_(explicitSolver),
     sfes_(sfes),
     vfes_(vfes),
     ffes_(ffes),
     nBV_(nBV),
     B_(B),
     charges_(charges),
     masses_(masses),
     msDiff_(NULL)
{
   this->initDiffusion();
}

TransportSolver::~TransportSolver()
{
   delete msDiff_;
}

void TransportSolver::initDiffusion()
{
   msDiff_ = new MultiSpeciesDiffusion(sfes_, vfes_, nBV_, charges_, masses_);
}

void TransportSolver::Update()
{
   msDiff_->Update();
}

void TransportSolver::Step(Vector &x, double &t, double &dt)
{
   msDiff_->Assemble();
   impSolver_->Step(x, t, dt);
}

MultiSpeciesDiffusion::MultiSpeciesDiffusion(ParFiniteElementSpace & sfes,
                                             ParFiniteElementSpace & vfes,
                                             BlockVector & nBV,
                                             Array<int> & charges,
                                             Vector & masses)
   : sfes_(sfes),
     vfes_(vfes),
     nBV_(nBV),
     charges_(charges),
     masses_(masses)
{}

MultiSpeciesDiffusion::~MultiSpeciesDiffusion()
{}

void MultiSpeciesDiffusion::initCoefficients()
{}

void MultiSpeciesDiffusion::initBilinearForms()
{}

void MultiSpeciesDiffusion::Assemble()
{}

void MultiSpeciesDiffusion::Update()
{}

void MultiSpeciesDiffusion::ImplicitSolve(const double dt,
                                          const Vector &x, Vector &y)
{}

DiffusionTDO::DiffusionTDO(ParFiniteElementSpace &fes,
                           ParFiniteElementSpace &dfes,
                           ParFiniteElementSpace &vfes,
                           MatrixCoefficient & nuCoef,
                           double dg_sigma,
                           double dg_kappa)
   : TimeDependentOperator(vfes.GetTrueVSize()),
     dim_(vfes.GetFE(0)->GetDim()),
     dt_(0.0),
     dg_sigma_(dg_sigma),
     dg_kappa_(dg_kappa),
     fes_(fes),
     dfes_(dfes),
     vfes_(vfes),
     m_(&fes_),
     d_(&fes_),
     rhs_(&fes_),
     x_(&vfes_),
     M_(NULL),
     D_(NULL),
     RHS_(fes_.GetTrueVSize()),
     X_(fes_.GetTrueVSize()),
     solver_(NULL),
     amg_(NULL),
     nuCoef_(nuCoef),
     dtNuCoef_(0.0, nuCoef_)
{
   m_.AddDomainIntegrator(new MassIntegrator);
   m_.AddDomainIntegrator(new DiffusionIntegrator(dtNuCoef_));
   m_.AddInteriorFaceIntegrator(new DGDiffusionIntegrator(dtNuCoef_,
                                                          dg_sigma_,
                                                          dg_kappa_));
   m_.AddBdrFaceIntegrator(new DGDiffusionIntegrator(dtNuCoef_,
                                                     dg_sigma_, dg_kappa_));
   d_.AddDomainIntegrator(new DiffusionIntegrator(nuCoef_));
   d_.AddInteriorFaceIntegrator(new DGDiffusionIntegrator(nuCoef_,
                                                          dg_sigma_,
                                                          dg_kappa_));
   d_.AddBdrFaceIntegrator(new DGDiffusionIntegrator(nuCoef_,
                                                     dg_sigma_, dg_kappa_));
   d_.Assemble();
   d_.Finalize();
   D_ = d_.ParallelAssemble();
}

void DiffusionTDO::ImplicitSolve(const double dt, const Vector &x, Vector &y)
{
   y = 0.0;

   this->initSolver(dt);

   for (int d=0; d<dim_; d++)
   {
      ParGridFunction xd(&fes_, &(x.GetData()[(d+1) * fes_.GetVSize()]));
      ParGridFunction yd(&fes_, &(y.GetData()[(d+1) * fes_.GetVSize()]));

      D_->Mult(xd, rhs_);
      rhs_ *= -1.0;
      rhs_.ParallelAssemble(RHS_);

      X_ = 0.0;
      solver_->Mult(RHS_, X_);

      yd = X_;
   }
}

void DiffusionTDO::initSolver(double dt)
{
   bool newM = false;
   if (fabs(dt - dt_) > 1e-4 * dt)
   {
      dt_ = dt;
      dtNuCoef_.SetAConst(dt);
      m_.Assemble(0);
      m_.Finalize(0);
      if (M_ != NULL)
      {
         delete M_;
      }
      M_ = m_.ParallelAssemble();
      newM = true;
   }

   if (amg_ == NULL || newM)
   {
      if (amg_ != NULL) { delete amg_; }
      amg_ = new HypreBoomerAMG(*M_);
   }
   if (solver_ == NULL || newM)
   {
      if (solver_ != NULL) { delete solver_; }
      if (dg_sigma_ == -1.0)
      {
         HyprePCG * pcg = new HyprePCG(*M_);
         pcg->SetTol(1e-12);
         pcg->SetMaxIter(200);
         pcg->SetPrintLevel(0);
         pcg->SetPreconditioner(*amg_);
         solver_ = pcg;
      }
      else
      {
         HypreGMRES * gmres = new HypreGMRES(*M_);
         gmres->SetTol(1e-12);
         gmres->SetMaxIter(200);
         gmres->SetKDim(10);
         gmres->SetPrintLevel(0);
         gmres->SetPreconditioner(*amg_);
         solver_ = gmres;
      }

   }
}

// Implementation of class FE_Evolution
AdvectionTDO::AdvectionTDO(ParFiniteElementSpace &vfes,
                           Operator &A, SparseMatrix &Aflux, int num_equation,
                           double specific_heat_ratio)
   : TimeDependentOperator(A.Height()),
     dim_(vfes.GetFE(0)->GetDim()),
     num_equation_(num_equation),
     specific_heat_ratio_(specific_heat_ratio),
     vfes_(vfes),
     A_(A),
     Aflux_(Aflux),
     Me_inv_(vfes.GetFE(0)->GetDof(), vfes.GetFE(0)->GetDof(), vfes.GetNE()),
     state_(num_equation_),
     f_(num_equation_, dim_),
     flux_(vfes.GetNDofs(), dim_, num_equation_),
     z_(A.Height())
{
   // Standard local assembly and inversion for energy mass matrices.
   const int dof = vfes_.GetFE(0)->GetDof();
   DenseMatrix Me(dof);
   DenseMatrixInverse inv(&Me);
   MassIntegrator mi;
   for (int i = 0; i < vfes_.GetNE(); i++)
   {
      mi.AssembleElementMatrix(*vfes_.GetFE(i),
                               *vfes_.GetElementTransformation(i), Me);
      inv.Factor();
      inv.GetInverseMatrix(Me_inv_(i));
   }
}

void AdvectionTDO::Mult(const Vector &x, Vector &y) const
{
   // 0. Reset wavespeed computation before operator application.
   max_char_speed_ = 0.;

   // 1. Create the vector z with the face terms -<F.n(u), [w]>.
   A_.Mult(x, z_);

   // 2. Add the element terms.
   // i.  computing the flux approximately as a grid function by interpolating
   //     at the solution nodes.
   // ii. multiplying this grid function by a (constant) mixed bilinear form for
   //     each of the num_equation, computing (F(u), grad(w)) for each equation.

   DenseMatrix xmat(x.GetData(), vfes_.GetNDofs(), num_equation_);
   GetFlux(xmat, flux_);

   for (int k = 0; k < num_equation_; k++)
   {
      Vector fk(flux_(k).GetData(), dim_ * vfes_.GetNDofs());
      Vector zk(z_.GetData() + k * vfes_.GetNDofs(), vfes_.GetNDofs());
      Aflux_.AddMult(fk, zk);
   }

   // 3. Multiply element-wise by the inverse mass matrices.
   Vector zval;
   Array<int> vdofs;
   const int dof = vfes_.GetFE(0)->GetDof();
   DenseMatrix zmat, ymat(dof, num_equation_);

   for (int i = 0; i < vfes_.GetNE(); i++)
   {
      // Return the vdofs ordered byNODES
      vfes_.GetElementVDofs(i, vdofs);
      z_.GetSubVector(vdofs, zval);
      zmat.UseExternalData(zval.GetData(), dof, num_equation_);
      mfem::Mult(Me_inv_(i), zmat, ymat);
      y.SetSubVector(vdofs, ymat.GetData());
   }
}

// Physicality check (at end)
bool StateIsPhysical(const Vector &state, const int dim,
                     const double specific_heat_ratio);

// Pressure (EOS) computation
inline double ComputePressure(const Vector &state, int dim,
                              double specific_heat_ratio)
{
   const double den = state(0);
   const Vector den_vel(state.GetData() + 1, dim);
   const double den_energy = state(1 + dim);

   double den_vel2 = 0;
   for (int d = 0; d < dim; d++) { den_vel2 += den_vel(d) * den_vel(d); }
   den_vel2 /= den;

   return (specific_heat_ratio - 1.0) * (den_energy - 0.5 * den_vel2);
}

// Compute the vector flux F(u)
void ComputeFlux(const Vector &state, int dim, double specific_heat_ratio,
                 DenseMatrix &flux)
{
   const double den = state(0);
   const Vector den_vel(state.GetData() + 1, dim);
   const double den_energy = state(1 + dim);

   MFEM_ASSERT(StateIsPhysical(state, dim, specific_heat_ratio), "");

   const double pres = ComputePressure(state, dim, specific_heat_ratio);

   for (int d = 0; d < dim; d++)
   {
      flux(0, d) = den_vel(d);
      for (int i = 0; i < dim; i++)
      {
         flux(1+i, d) = den_vel(i) * den_vel(d) / den;
      }
      flux(1+d, d) += pres;
   }

   const double H = (den_energy + pres) / den;
   for (int d = 0; d < dim; d++)
   {
      flux(1+dim, d) = den_vel(d) * H;
   }
}

// Compute the scalar F(u).n
void ComputeFluxDotN(const Vector &state, const Vector &nor,
                     double specific_heat_ratio,
                     Vector &fluxN)
{
   // NOTE: nor in general is not a unit normal
   const int dim = nor.Size();
   const double den = state(0);
   const Vector den_vel(state.GetData() + 1, dim);
   const double den_energy = state(1 + dim);

   MFEM_ASSERT(StateIsPhysical(state, dim, specific_heat_ratio), "");

   const double pres = ComputePressure(state, dim, specific_heat_ratio);

   double den_velN = 0;
   for (int d = 0; d < dim; d++) { den_velN += den_vel(d) * nor(d); }

   fluxN(0) = den_velN;
   for (int d = 0; d < dim; d++)
   {
      fluxN(1+d) = den_velN * den_vel(d) / den + pres * nor(d);
   }

   const double H = (den_energy + pres) / den;
   fluxN(1 + dim) = den_velN * H;
}

// Compute the maximum characteristic speed.
inline double ComputeMaxCharSpeed(const Vector &state,
                                  int dim, double specific_heat_ratio)
{
   const double den = state(0);
   const Vector den_vel(state.GetData() + 1, dim);

   double den_vel2 = 0;
   for (int d = 0; d < dim; d++) { den_vel2 += den_vel(d) * den_vel(d); }
   den_vel2 /= den;

   const double pres = ComputePressure(state, dim, specific_heat_ratio);
   const double sound = sqrt(specific_heat_ratio * pres / den);
   const double vel = sqrt(den_vel2 / den);

   return vel + sound;
}

// Compute the flux at solution nodes.
void AdvectionTDO::GetFlux(const DenseMatrix &x, DenseTensor &flux) const
{
   const int dof = flux.SizeI();
   const int dim = flux.SizeJ();

   for (int i = 0; i < dof; i++)
   {
      for (int k = 0; k < num_equation_; k++) { state_(k) = x(i, k); }
      ComputeFlux(state_, dim, specific_heat_ratio_, f_);

      for (int d = 0; d < dim; d++)
      {
         for (int k = 0; k < num_equation_; k++)
         {
            flux(i, d, k) = f_(k, d);
         }
      }

      // Update max char speed
      const double mcs = ComputeMaxCharSpeed(state_, dim, specific_heat_ratio_);
      if (mcs > max_char_speed_) { max_char_speed_ = mcs; }
   }
}

// Implementation of class RiemannSolver
RiemannSolver::RiemannSolver(int num_equation, double specific_heat_ratio) :
   num_equation_(num_equation),
   specific_heat_ratio_(specific_heat_ratio),
   flux1_(num_equation),
   flux2_(num_equation) { }

double RiemannSolver::Eval(const Vector &state1, const Vector &state2,
                           const Vector &nor, Vector &flux)
{
   // NOTE: nor in general is not a unit normal
   const int dim = nor.Size();

   MFEM_ASSERT(StateIsPhysical(state1, dim, specific_heat_ratio_), "");
   MFEM_ASSERT(StateIsPhysical(state2, dim, specific_heat_ratio_), "");

   const double maxE1 = ComputeMaxCharSpeed(state1, dim, specific_heat_ratio_);
   const double maxE2 = ComputeMaxCharSpeed(state2, dim, specific_heat_ratio_);

   const double maxE = max(maxE1, maxE2);

   ComputeFluxDotN(state1, nor, specific_heat_ratio_, flux1_);
   ComputeFluxDotN(state2, nor, specific_heat_ratio_, flux2_);

   double normag = 0;
   for (int i = 0; i < dim; i++)
   {
      normag += nor(i) * nor(i);
   }
   normag = sqrt(normag);

   for (int i = 0; i < num_equation_; i++)
   {
      flux(i) = 0.5 * (flux1_(i) + flux2_(i))
                - 0.5 * maxE * (state2(i) - state1(i)) * normag;
   }

   return maxE;
}

// Implementation of class DomainIntegrator
DomainIntegrator::DomainIntegrator(const int dim, int num_equation)
   : flux_(num_equation, dim) { }

void DomainIntegrator::AssembleElementMatrix2(const FiniteElement &trial_fe,
                                              const FiniteElement &test_fe,
                                              ElementTransformation &Tr,
                                              DenseMatrix &elmat)
{
   // Assemble the form (vec(v), grad(w))

   // Trial space = vector L2 space (mesh dim)
   // Test space  = scalar L2 space

   const int dof_trial = trial_fe.GetDof();
   const int dof_test = test_fe.GetDof();
   const int dim = trial_fe.GetDim();

   shape_.SetSize(dof_trial);
   dshapedr_.SetSize(dof_test, dim);
   dshapedx_.SetSize(dof_test, dim);

   elmat.SetSize(dof_test, dof_trial * dim);
   elmat = 0.0;

   const int maxorder = max(trial_fe.GetOrder(), test_fe.GetOrder());
   const int intorder = 2 * maxorder;
   const IntegrationRule *ir = &IntRules.Get(trial_fe.GetGeomType(), intorder);

   for (int i = 0; i < ir->GetNPoints(); i++)
   {
      const IntegrationPoint &ip = ir->IntPoint(i);

      // Calculate the shape functions
      trial_fe.CalcShape(ip, shape_);
      shape_ *= ip.weight;

      // Compute the physical gradients of the test functions
      Tr.SetIntPoint(&ip);
      test_fe.CalcDShape(ip, dshapedr_);
      Mult(dshapedr_, Tr.AdjugateJacobian(), dshapedx_);

      for (int d = 0; d < dim; d++)
      {
         for (int j = 0; j < dof_test; j++)
         {
            for (int k = 0; k < dof_trial; k++)
            {
               elmat(j, k + d * dof_trial) += shape_(k) * dshapedx_(j, d);
            }
         }
      }
   }
}

// Implementation of class FaceIntegrator
FaceIntegrator::FaceIntegrator(RiemannSolver &rsolver, const int dim,
                               const int num_equation) :
   num_equation_(num_equation),
   max_char_speed_(0.0),
   rsolver_(rsolver),
   funval1_(num_equation_),
   funval2_(num_equation_),
   nor_(dim),
   fluxN_(num_equation_) { }

void FaceIntegrator::AssembleFaceVector(const FiniteElement &el1,
                                        const FiniteElement &el2,
                                        FaceElementTransformations &Tr,
                                        const Vector &elfun, Vector &elvect)
{
   // Compute the term <F.n(u),[w]> on the interior faces.
   const int dof1 = el1.GetDof();
   const int dof2 = el2.GetDof();

   shape1_.SetSize(dof1);
   shape2_.SetSize(dof2);

   elvect.SetSize((dof1 + dof2) * num_equation_);
   elvect = 0.0;

   DenseMatrix elfun1_mat(elfun.GetData(), dof1, num_equation_);
   DenseMatrix elfun2_mat(elfun.GetData() + dof1 * num_equation_, dof2,
                          num_equation_);

   DenseMatrix elvect1_mat(elvect.GetData(), dof1, num_equation_);
   DenseMatrix elvect2_mat(elvect.GetData() + dof1 * num_equation_, dof2,
                           num_equation_);

   // Integration order calculation from DGTraceIntegrator
   int intorder;
   if (Tr.Elem2No >= 0)
      intorder = (min(Tr.Elem1->OrderW(), Tr.Elem2->OrderW()) +
                  2*max(el1.GetOrder(), el2.GetOrder()));
   else
   {
      intorder = Tr.Elem1->OrderW() + 2*el1.GetOrder();
   }
   if (el1.Space() == FunctionSpace::Pk)
   {
      intorder++;
   }
   const IntegrationRule *ir = &IntRules.Get(Tr.FaceGeom, intorder);

   for (int i = 0; i < ir->GetNPoints(); i++)
   {
      const IntegrationPoint &ip = ir->IntPoint(i);

      Tr.Loc1.Transform(ip, eip1_);
      Tr.Loc2.Transform(ip, eip2_);

      // Calculate basis functions on both elements at the face
      el1.CalcShape(eip1_, shape1_);
      el2.CalcShape(eip2_, shape2_);

      // Interpolate elfun at the point
      elfun1_mat.MultTranspose(shape1_, funval1_);
      elfun2_mat.MultTranspose(shape2_, funval2_);

      Tr.Face->SetIntPoint(&ip);

      // Get the normal vector and the flux on the face
      CalcOrtho(Tr.Face->Jacobian(), nor_);
      const double mcs = rsolver_.Eval(funval1_, funval2_, nor_, fluxN_);

      // Update max char speed
      if (mcs > max_char_speed_) { max_char_speed_ = mcs; }

      fluxN_ *= ip.weight;
      for (int k = 0; k < num_equation_; k++)
      {
         for (int s = 0; s < dof1; s++)
         {
            elvect1_mat(s, k) -= fluxN_(k) * shape1_(s);
         }
         for (int s = 0; s < dof2; s++)
         {
            elvect2_mat(s, k) += fluxN_(k) * shape2_(s);
         }
      }
   }
}

// Check that the state is physical - enabled in debug mode
bool StateIsPhysical(const Vector &state, int dim,
                     double specific_heat_ratio)
{
   const double den = state(0);
   const Vector den_vel(state.GetData() + 1, dim);
   const double den_energy = state(1 + dim);

   if (den < 0)
   {
      cout << "Negative density: ";
      for (int i = 0; i < state.Size(); i++)
      {
         cout << state(i) << " ";
      }
      cout << endl;
      return false;
   }
   if (den_energy <= 0)
   {
      cout << "Negative energy: ";
      for (int i = 0; i < state.Size(); i++)
      {
         cout << state(i) << " ";
      }
      cout << endl;
      return false;
   }

   double den_vel2 = 0;
   for (int i = 0; i < dim; i++) { den_vel2 += den_vel(i) * den_vel(i); }
   den_vel2 /= den;

   const double pres = (specific_heat_ratio - 1.0) *
                       (den_energy - 0.5 * den_vel2);

   if (pres <= 0)
   {
      cout << "Negative pressure: " << pres << ", state: ";
      for (int i = 0; i < state.Size(); i++)
      {
         cout << state(i) << " ";
      }
      cout << endl;
      return false;
   }
   return true;
}

} // namespace transport

} // namespace plasma

} // namespace mfem

#endif // MFEM_USE_MPI
