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

#include "../../../config/config.hpp"
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_KERNELS)

#include "../kernels.hpp"

namespace mfem
{

namespace kernels
{

// *****************************************************************************
kBilinearForm::kBilinearForm(kFiniteElementSpace *kfes) :
   Operator(kfes->KernelsVLayout()),
   localX(kfes->KernelsEVLayout()),
   localY(kfes->KernelsEVLayout())
{
   nvtx_push();
   Init(kfes->KernelsEngine(), kfes, kfes);
   nvtx_pop();
}

// *****************************************************************************
kBilinearForm::kBilinearForm(kFiniteElementSpace *kTrialFes_,
                             kFiniteElementSpace *kTestFes_) :
   Operator(kTrialFes_->KernelsVLayout(), kTestFes_->KernelsVLayout()),
   localX(kTrialFes_->KernelsEVLayout()),
   localY(kTestFes_->KernelsEVLayout())
{
   nvtx_push();
   Init(kTrialFes_->KernelsEngine(), kTrialFes_, kTestFes_);
   nvtx_pop();
}

// *****************************************************************************
void kBilinearForm::Init(const Engine &e,
                         kFiniteElementSpace *kTrialFes_,
                         kFiniteElementSpace *kTestFes_)
{
   nvtx_push();
   ng.Reset(&e);
   kTrialFes = kTrialFes_;
   mTrialFes  = kTrialFes_->GetFESpace();

   kTestFes = kTestFes_;
   mTestFes  = kTestFes_->GetFESpace();

   mesh = mTrialFes->GetMesh();
   nvtx_pop();
}

// *****************************************************************************
kFiniteElementSpace& kBilinearForm::GetTrialKernelsFESpace() const
{
   nvtx_push();
   assert(kTrialFes);
   nvtx_pop();
   return *kTrialFes;
}

// *****************************************************************************
kFiniteElementSpace& kBilinearForm::GetTestKernelsFESpace() const
{
   nvtx_push();
   assert(kTestFes);
   nvtx_pop();
   return *kTestFes;
}

// *****************************************************************************
mfem::FiniteElementSpace& kBilinearForm::GetTrialFESpace() const
{
   nvtx_push();
   assert(mTrialFes);
   nvtx_pop();
   return *mTrialFes;
}

// *****************************************************************************
mfem::FiniteElementSpace& kBilinearForm::GetTestFESpace() const
{
   nvtx_push();
   assert(mTestFes);
   nvtx_pop();
   return *mTestFes;
}

int64_t kBilinearForm::GetTrialNDofs() const
{
   nvtx_push(); nvtx_pop();
   return mTrialFes->GetNDofs();
}

int64_t kBilinearForm::GetTestNDofs() const
{
   return mTestFes->GetNDofs();
}

int64_t kBilinearForm::GetTrialVDim() const
{
   return mTrialFes->GetVDim();
}

int64_t kBilinearForm::GetTestVDim() const
{
   return mTestFes->GetVDim();
}

const FiniteElement& kBilinearForm::GetTrialFE(const int i) const
{
   return *(mTrialFes->GetFE(i));
}

const FiniteElement& kBilinearForm::GetTestFE(const int i) const
{
   return *(mTestFes->GetFE(i));
}

// Adds new Domain Integrator.
void kBilinearForm::AddDomainIntegrator(KernelsIntegrator *integrator)
{
   nvtx_push();
   AddIntegrator(integrator, DomainIntegrator);
   nvtx_pop();
}

// Adds new Boundary Integrator.
void kBilinearForm::AddBoundaryIntegrator(KernelsIntegrator *integrator)
{
   nvtx_push();
   AddIntegrator(integrator, BoundaryIntegrator);
   nvtx_pop();
}

// Adds new interior Face Integrator.
void kBilinearForm::AddInteriorFaceIntegrator(KernelsIntegrator
                                              *integrator)
{
   nvtx_push();
   AddIntegrator(integrator, InteriorFaceIntegrator);
   nvtx_pop();
}

// Adds new boundary Face Integrator.
void kBilinearForm::AddBoundaryFaceIntegrator(KernelsIntegrator
                                              *integrator)
{
   nvtx_push();
   AddIntegrator(integrator, BoundaryFaceIntegrator);
   nvtx_pop();
}

// Adds Integrator based on KernelsIntegratorType
void kBilinearForm::AddIntegrator(KernelsIntegrator *integrator,
                                  const KernelsIntegratorType itype)
{
   nvtx_push();
   if (integrator == NULL)
   {
      std::stringstream error_ss;
      error_ss << "kBilinearForm::";
      switch (itype)
      {
         case DomainIntegrator      : error_ss << "AddDomainIntegrator";       break;
         case BoundaryIntegrator    : error_ss << "AddBoundaryIntegrator";     break;
         case InteriorFaceIntegrator: error_ss << "AddInteriorFaceIntegrator"; break;
         case BoundaryFaceIntegrator: error_ss << "AddBoundaryFaceIntegrator"; break;
      }
      error_ss << " (...):\n"
               << "  Integrator is NULL";
      const std::string error = error_ss.str();
      mfem_error(error.c_str());
   }
   integrator->SetupIntegrator(*this, itype);
   integrators.push_back(integrator);
   nvtx_pop();
}

const mfem::Operator* kBilinearForm::GetTrialProlongation() const
{
   return kTrialFes->GetProlongationOperator();
}

const mfem::Operator* kBilinearForm::GetTestProlongation() const
{
   return kTestFes->GetProlongationOperator();
}

const mfem::Operator* kBilinearForm::GetTrialRestriction() const
{
   return kTrialFes->GetRestrictionOperator();
}

const mfem::Operator* kBilinearForm::GetTestRestriction() const
{
   return kTestFes->GetRestrictionOperator();
}

// *****************************************************************************
void kBilinearForm::Assemble()
{
   nvtx_push();
   const int integratorCount = (int) integrators.size();
   for (int i = 0; i < integratorCount; ++i)
   {
      integrators[i]->Assemble();
   }
   nvtx_pop();
}

// *****************************************************************************
void kBilinearForm::FormLinearSystem(const mfem::Array<int>
                                     &constraintList,
                                     mfem::Vector &x, mfem::Vector &b,
                                     mfem::Operator *&Aout,
                                     mfem::Vector &X, mfem::Vector &B,
                                     int copy_interior)
{
   nvtx_push();
   assert(false);
   /*
   FormOperator(constraintList, Aout);
   InitRHS(constraintList, x, b, Aout, X, B, copy_interior);
   */
   nvtx_pop();
}

// *****************************************************************************
void kBilinearForm::FormOperator(const mfem::Array<int> &constraintList,
                                 mfem::Operator *&Aout)
{
   nvtx_push();
   const mfem::Operator *trialP = GetTrialProlongation();
   const mfem::Operator *testP  = GetTestProlongation();
   mfem::Operator *rap = this;
   if (trialP)
   {
      dbg("\033[7mnew RAPOperator");
      rap = new RAPOperator(*testP, *this, *trialP);
   }
   dbg("\033[7mnew kConstrainedOperator");
   dbg("\033[7mrap->Height()=%d",rap->Height());
   dbg("\033[7mrap->Width()=%d",rap->Width());
   Aout = new kConstrainedOperator(rap, constraintList, rap != this);
   dbg("\033[7mdone");
   //assert(false);
   nvtx_pop();
}

// *****************************************************************************
void kBilinearForm::InitRHS(const mfem::Array<int> &constraintList,
                            mfem::Vector &x, mfem::Vector &b,
                            mfem::Operator *A,
                            mfem::Vector &X, mfem::Vector &B,
                            int copy_interior)
{
   nvtx_push(); //assert(false);// ex1pd comes here, Laghos dont

   const mfem::Operator *P = GetTrialProlongation();
   const mfem::Operator *R = GetTrialRestriction();
   if (P)
   {
      // Variational restriction with P
      B.Resize(P->InLayout());
      P->MultTranspose(b, B);
      X.Resize(R->OutLayout());
      R->Mult(x, X);
   }
   else
   {
      // rap, X and B point to the same data as this, x and b
      assert(false);
      X.MakeRef(x);
      B.MakeRef(b);
   }

   if (!copy_interior && constraintList.Size() > 0)
   {
      //assert(false);
      const Array &constrList = constraintList.Get_PArray()->As<Array>();
      Vector subvec(constrList.KernelsLayout());
      vector_get_subvector(constraintList.Size(),
                           (double*)subvec.KernelsMem().ptr(),
                           (double*)X.Get_PVector()->As<Vector>().KernelsMem().ptr(),
                           (int*)constrList.KernelsMem().ptr());
      X.Fill(0.0);
      vector_set_subvector(constraintList.Size(),
                           (double*)X.Get_PVector()->As<Vector>().KernelsMem().ptr(),
                           (double*)subvec.KernelsMem().ptr(),
                           (int*)constrList.KernelsMem().ptr());
   }

   kConstrainedOperator *cA = dynamic_cast<kConstrainedOperator*>(A);
   if (cA)
   {
      cA->EliminateRHS(X.Get_PVector()->As<Vector>(),
                       B.Get_PVector()->As<Vector>());
   }
   else
   {
      mfem_error("kBilinearForm::InitRHS expects an kConstrainedOperator");
   }
   nvtx_pop();
}


// Matrix vector multiplication ************************************************
void kBilinearForm::Mult_(const kernels::Vector &x,
                          kernels::Vector &y) const
{
   dbg("\033[7mkBilinearForm::Mult_");
   kTrialFes->GlobalToLocal(x, localX);
   localY.Fill<double>(0.0);
   const int integratorCount = (int) integrators.size();
   for (int i = 0; i < integratorCount; ++i)
   {
      integrators[i]->MultAdd(localX, localY);
   }
   kTestFes->LocalToGlobal(localY, y);
   nvtx_pop();
}

// Matrix transpose vector multiplication **************************************
void kBilinearForm::MultTranspose_(const kernels::Vector &x,
                                   kernels::Vector &y) const
{
   nvtx_push();
   kTestFes->GlobalToLocal(x, localX);
   localY.Fill<double>(0.0);
   const int integratorCount = (int) integrators.size();
   for (int i = 0; i < integratorCount; ++i)
   {
      integrators[i]->MultTransposeAdd(localX, localY);
   }
   kTrialFes->LocalToGlobal(localY, y);
   nvtx_pop();
}

// *****************************************************************************
void kBilinearForm::KernelsRecoverFEMSolution(const mfem::Vector &X,
                                              const mfem::Vector &b,
                                              mfem::Vector &x)
{
   nvtx_push();
   const mfem::Operator *P = this->GetTrialProlongation();
   if (P)
   {
      // Apply conforming prolongation
      x.Resize(P->OutLayout());
      P->Mult(X, x);
   }
   // Otherwise X and x point to the same data
   nvtx_pop();
}

// Frees memory bilinear form **************************************************
kBilinearForm::~kBilinearForm()
{
   // Make sure all integrators free their data
   IntegratorVector::iterator it = integrators.begin();
   while (it != integrators.end())
   {
      delete *it;
      ++it;
   }
}

} // namespace mfem::kernels

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_KERNELS)
