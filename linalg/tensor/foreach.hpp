// Copyright (c) 2010-2020, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#ifndef MFEM_TENSOR_FOREACH
#define MFEM_TENSOR_FOREACH

#include "../../general/forall.hpp"
#include "tensor_traits.hpp"

namespace mfem
{

/// Apply a lambda function based on the dimensions of a Tensor.
template <int N,
            typename Tensor,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               !is_threaded_tensor_dim<Tensor, N>,
            bool> = true
         >
MFEM_HOST_DEVICE inline
void Foreach(const Tensor &t, Lambda &&func, Idx... idx)
{
   for (int i = 0; i < t.template Size<N>(); i++)
   {
      func(i,idx...);
   }
}

template <int N,
            typename Tensor,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               is_threaded_tensor_dim<Tensor, N> && N==0,
            bool> = true
         >
MFEM_HOST_DEVICE inline
void Foreach(const Tensor &t, Lambda &&func, Idx... idx)
{
   MFEM_FOREACH_THREAD(i,x,t.template Size<N>())
   {
      func(i,idx...);
   }
}

template <int N,
            typename Tensor,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               is_threaded_tensor_dim<Tensor, N> && N==1,
            bool> = true
         >
MFEM_HOST_DEVICE inline
void Foreach(const Tensor &t, Lambda &&func, Idx... idx)
{
   MFEM_FOREACH_THREAD(i,y,t.template Size<N>())
   {
      func(i,idx...);
   }
}

template <int N,
            typename Tensor,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               is_threaded_tensor_dim<Tensor, N> && N==2,
            bool> = true
         >
MFEM_HOST_DEVICE inline
void Foreach(const Tensor &t, Lambda &&func, Idx... idx)
{
   MFEM_FOREACH_THREAD(i,z,t.template Size<N>())
   {
      func(i,idx...);
   }
}

template <int N,
            typename TensorLHS,
            typename TensorRHS,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               (is_serial_tensor_dim<TensorLHS, N> &&
                is_serial_tensor_dim<TensorRHS, N>),
            bool> = true
         >
MFEM_HOST_DEVICE inline
void ForeachBinOp(const TensorLHS &lhs, const TensorRHS &rhs,
                  Lambda &&func, Idx... idx)
{
   // TODO assert lhs.template Size<N>() == rhs.template Size<N>()
   for (int i = 0; i < lhs.template Size<N>(); i++)
   {
      func(i,idx...);
   }
}

template <int N,
            typename TensorLHS,
            typename TensorRHS,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
            ((is_threaded_tensor_dim<TensorLHS, N> &&
              has_pointer_container<TensorRHS>) ||
             (has_pointer_container<TensorLHS> &&
              is_threaded_tensor_dim<TensorRHS, N>) ||
             (is_threaded_tensor_dim<TensorLHS, N> &&
              is_threaded_tensor_dim<TensorRHS, N>)) &&
            (N==0),
            bool> = true
         >
MFEM_HOST_DEVICE inline
void ForeachBinOp(const TensorLHS &lhs, const TensorRHS &rhs,
                  Lambda &&func, Idx... idx)
{
   // TODO assert lhs.template Size<N>() == rhs.template Size<N>()
   MFEM_FOREACH_THREAD(i,x,lhs.template Size<N>())
   {
      func(i,idx...);
   }
}

template <int N,
            typename TensorLHS,
            typename TensorRHS,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               ((is_threaded_tensor_dim<TensorLHS, N> &&
                 has_pointer_container<TensorRHS>) ||
                (has_pointer_container<TensorLHS> &&
                 is_threaded_tensor_dim<TensorRHS, N>) ||
                (is_threaded_tensor_dim<TensorLHS, N> &&
                 is_threaded_tensor_dim<TensorRHS, N>)) &&
               (N==1),
            bool> = true
         >
MFEM_HOST_DEVICE inline
void ForeachBinOp(const TensorLHS &lhs, const TensorRHS &rhs,
                  Lambda &&func, Idx... idx)
{
   // TODO assert lhs.template Size<N>() == rhs.template Size<N>()
   MFEM_FOREACH_THREAD(i,y,lhs.template Size<N>())
   {
      func(i,idx...);
   }
}

template <int N,
            typename TensorLHS,
            typename TensorRHS,
            typename Lambda,
            typename... Idx,
            std::enable_if_t<
               ((is_threaded_tensor_dim<TensorLHS, N> &&
                 has_pointer_container<TensorRHS>) ||
                (has_pointer_container<TensorLHS> &&
                 is_threaded_tensor_dim<TensorRHS, N>) ||
                (is_threaded_tensor_dim<TensorLHS, N> &&
                 is_threaded_tensor_dim<TensorRHS, N>)) &&
               (N==2),
            bool> = true
         >
MFEM_HOST_DEVICE inline
void ForeachBinOp(const TensorLHS &lhs, const TensorRHS &rhs,
                  Lambda &&func, Idx... idx)
{
   // TODO assert lhs.template Size<N>() == rhs.template Size<N>()
   MFEM_FOREACH_THREAD(i,z,lhs.template Size<N>())
   {
      func(i,idx...);
   }
}

/// A structure that applies Foreach to all given dimensions.
template <int Dim, int... Dims>
struct Forall
{
   /// Apply a lambda function based on the dimensions of a Tensor.
   template <typename Tensor, typename Lambda, typename... Idx>
   MFEM_HOST_DEVICE inline
   static void Apply(const Tensor &t, Lambda &&func, Idx... idx)
   {
      Foreach<Dim>(
         t,
         [&](auto... i){
            Forall<Dims...>::Apply(t, func, i...);
         },
         idx...
      );
   }

   template <typename TensorLHS,
             typename TensorRHS,
             typename Lambda,
             typename... Idx>
   MFEM_HOST_DEVICE inline
   static void ApplyBinOp(const TensorLHS &lhs, const TensorRHS &rhs,
                          Lambda &&func, Idx... idx)
   {
      ForeachBinOp<Dim>(
         lhs,
         rhs,
         [&](auto... i){
            Forall<Dims...>::ApplyBinOp(lhs, rhs, func, i...);
         },
         idx...
      );
   }
};

template <int Dim>
struct Forall<Dim>
{
   /// Apply a lambda function based on the dimensions of a Tensor.
   template <typename Tensor, typename Lambda, typename... Idx>
   MFEM_HOST_DEVICE inline
   static void Apply(const Tensor &t, Lambda &&func, Idx... idx)
   {
      Foreach<Dim>(t, func, idx...);
   }

   template <typename TensorLHS,
             typename TensorRHS,
             typename Lambda,
             typename... Idx>
   MFEM_HOST_DEVICE inline
   static void ApplyBinOp(const TensorLHS &lhs, const TensorRHS &rhs,
                          Lambda &&func, Idx... idx)
   {
      ForeachBinOp<Dim>(lhs, rhs, func, idx...);
   }
};

/// A structure applying Foreach to each dimension of the Tensor.
template <typename Tensor, int Dim = get_tensor_rank<Tensor>-1>
struct ForallDims
{
   /// Apply a lambda function based on the dimensions of a Tensor.
   template <typename Lambda, typename... Idx>
   MFEM_HOST_DEVICE inline
   static void Apply(const Tensor &t, Lambda &&func, Idx... idx)
   {
      Foreach<Dim>(
         t,
         [&](auto... i){
            ForallDims<Tensor,Dim-1>::Apply(t, func, i...);
         },
         idx...
      );
   }

   template <typename TensorRHS, typename Lambda, typename... Idx>
   MFEM_HOST_DEVICE inline
   static void ApplyBinOp(const Tensor &lhs, const TensorRHS &rhs,
                          Lambda &&func, Idx... idx)
   {
      ForeachBinOp<Dim>(
         lhs,
         rhs,
         [&](auto... i){
            ForallDims<Tensor,Dim-1>::ApplyBinOp(lhs, rhs, func, i...);
         },
         idx...
      );
   }
};

template <typename Tensor>
struct ForallDims<Tensor, 0>
{
   /// Apply a lambda function based on the dimensions of a Tensor.
   template <typename Lambda, typename... Idx>
   MFEM_HOST_DEVICE inline
   static void Apply(const Tensor &t, Lambda &&func, Idx... idx)
   {
      Foreach<0>(t, func, idx...);
   }

   template <typename TensorRHS, typename Lambda, typename... Idx>
   MFEM_HOST_DEVICE inline
   static void ApplyBinOp(const Tensor &lhs, const TensorRHS &rhs,
                          Lambda &&func, Idx... idx)
   {
      ForeachBinOp<0>(lhs, rhs, func, idx...);
   }
};

} // namespace mfem

#endif // MFEM_TENSOR_FOREACH