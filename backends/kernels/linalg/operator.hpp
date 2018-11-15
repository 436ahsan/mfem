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

#ifndef MFEM_BACKENDS_KERNELS_OPERATOR_HPP
#define MFEM_BACKENDS_KERNELS_OPERATOR_HPP

#include "../../../config/config.hpp"
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_KERNELS)

namespace mfem
{

namespace kernels
{

// **************************************************************************
class Operator : public mfem::Operator
{
public:
   /// Creare an operator with the same dimensions as @a orig.
   Operator(const Operator &orig) : mfem::Operator(orig) { }

   Operator(Layout &layout) : mfem::Operator(layout) { }

   Operator(Layout &in_layout, Layout &out_layout)
      : mfem::Operator(in_layout, out_layout) { }

   Layout &InLayout_() const
   { return *static_cast<Layout*>(in_layout.Get()); }

   Layout &OutLayout_() const
   { return *static_cast<Layout*>(out_layout.Get()); }

   virtual void Mult_(const kernels::Vector&, kernels::Vector&) const = 0;

   virtual void MultTranspose_(const kernels::Vector&, kernels::Vector&) const
   { MFEM_ABORT("method is not supported"); }

   // override
   virtual void Mult(const mfem::Vector &x, mfem::Vector &y) const
   {
      nvtx_push();
      Mult_(x.Get_PVector()->As<const kernels::Vector>(),
            y.Get_PVector()->As<kernels::Vector>());
      nvtx_pop();
   }

   // override
   virtual void MultTranspose(const mfem::Vector &x, mfem::Vector &y) const
   {
      nvtx_push();
      MultTranspose_(x.Get_PVector()->As<const kernels::Vector>(),
                     y.Get_PVector()->As<kernels::Vector>());
      nvtx_pop();
   }
};

} // namespace mfem::kernels

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_KERNELS)

#endif // MFEM_BACKENDS_KERNELS_OPERATOR_HPP
