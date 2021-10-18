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

#ifndef MFEM_DIAG_TENSOR
#define MFEM_DIAG_TENSOR

#include "../tensor.hpp"

namespace mfem
{

/// Represent a Rank+2*DRank diagonal Tensor, where DRank is the diagonal rank.
template <int DRank, // The rank of diagonal values
          int Rank, // The rank of non-diagonal values
          typename Container,
          typename Layout>
class DiagonalTensor: public Tensor<Container,Layout>
{
public:
   MFEM_HOST_DEVICE
   DiagonalTensor(const Tensor<Container,Layout> &t)
   : Tensor<Container,Layout>(t)
   { }

   // TODO define a DRank accessor? probably not possible
   // private inheritance then?
};

template <int DRank, typename Tensor> MFEM_HOST_DEVICE inline
auto makeDiagonalTensor(const Tensor &t)
{
   return DiagonalTensor<DRank,
                         get_tensor_rank<Tensor>-DRank,
                         typename Tensor::container,
                         typename Tensor::layout
                        >(t);
}

/// DiagonalTensor Traits

// is_diagonal_tensor
template <typename Tensor>
struct is_diagonal_tensor_v
{
   static constexpr bool value = false;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct is_diagonal_tensor_v<DiagonalTensor<DRank, Rank, Container, Layout>>
{
   static constexpr bool value = true;
};

template <typename Tensor>
constexpr bool is_diagonal_tensor = is_diagonal_tensor_v<Tensor>::value;

// get_diagonal_tensor_rank
template <typename Tensor>
struct get_diagonal_tensor_rank_v
{
   static constexpr int value = Error;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct get_diagonal_tensor_rank_v<DiagonalTensor<DRank, Rank, Container, Layout>>
{
   static constexpr int value = 2*DRank + Rank;
};

template <typename Tensor>
constexpr int get_diagonal_tensor_rank = get_diagonal_tensor_rank_v<Tensor>::value;

// get_diagonal_tensor_diagonal_rank
template <typename Tensor>
struct get_diagonal_tensor_diagonal_rank_v
{
   static constexpr int value = Error;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct get_diagonal_tensor_diagonal_rank_v<DiagonalTensor<DRank, Rank, Container, Layout>>
{
   static constexpr int value = DRank;
};

template <typename Tensor>
constexpr int get_diagonal_tensor_diagonal_rank = get_diagonal_tensor_diagonal_rank_v<Tensor>::value;

// get_diagonal_tensor_values_rank
template <typename Tensor>
struct get_diagonal_tensor_values_rank_v
{
   static constexpr int value = Error;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct get_diagonal_tensor_values_rank_v<DiagonalTensor<DRank, Rank, Container, Layout>>
{
   static constexpr int value = Rank;
};

template <typename Tensor>
constexpr int get_diagonal_tensor_values_rank = get_diagonal_tensor_values_rank_v<Tensor>::value;

// TODO Rewrite only for 2D?
/// Represent a SRank+2*DRank symmetric Tensor, where SRank dims are symmetric.
template <int DRank, // The rank of diagonal values
          int SRank, // The rank of symmetric values
          typename Container,
          typename Layout>
class DiagonalSymmetricTensor: public Tensor<Container,Layout>
{
public:
   MFEM_HOST_DEVICE
   DiagonalSymmetricTensor(const Tensor<Container,Layout> &t)
   : Tensor<Container,Layout>(t)
   { }

   // TODO define a DRank accessor? probably not possible
   // private inheritance then?
};

template <int DRank, typename Tensor> MFEM_HOST_DEVICE inline
auto makeDiagonalSymmetricTensor(const Tensor &t)
{
   return DiagonalSymmetricTensor<DRank,
                                  get_tensor_rank<Tensor>-DRank,
                                  typename Tensor::container,
                                  typename Tensor::layout
                                 >(t);
}

/// DiagonalSymmetricTensor Traits

// is_diagonal_symmetric_tensor
template <typename Tensor>
struct is_diagonal_symmetric_tensor_v
{
   static constexpr bool value = false;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct is_diagonal_symmetric_tensor_v<DiagonalSymmetricTensor<DRank, Rank, Container, Layout>>
{
   static constexpr bool value = true;
};

template <typename Tensor>
constexpr bool is_diagonal_symmetric_tensor = is_diagonal_symmetric_tensor_v<Tensor>::value;

// get_diagonal_symmetric_tensor_rank
template <typename Tensor>
struct get_diagonal_symmetric_tensor_rank_v
{
   static constexpr int value = Error;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct get_diagonal_symmetric_tensor_rank_v<DiagonalSymmetricTensor<DRank, Rank, Container, Layout>>
{
   static constexpr int value = 2*DRank + 2*Rank;
};

template <typename Tensor>
constexpr int get_diagonal_symmetric_tensor_rank = get_diagonal_symmetric_tensor_rank_v<Tensor>::value;

// get_diagonal_symmetric_tensor_diagonal_rank
template <typename Tensor>
struct get_diagonal_symmetric_tensor_diagonal_rank_v
{
   static constexpr int value = Error;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct get_diagonal_symmetric_tensor_diagonal_rank_v<DiagonalSymmetricTensor<DRank, Rank, Container, Layout>>
{
   static constexpr int value = DRank;
};

template <typename Tensor>
constexpr int get_diagonal_symmetric_tensor_diagonal_rank = get_diagonal_symmetric_tensor_diagonal_rank_v<Tensor>::value;

// get_diagonal_symmetric_tensor_values_rank
template <typename Tensor>
struct get_diagonal_symmetric_tensor_values_rank_v
{
   static constexpr int value = Error;
};

template <int DRank, int Rank, typename Container, typename Layout>
struct get_diagonal_symmetric_tensor_values_rank_v<DiagonalSymmetricTensor<DRank, Rank, Container, Layout>>
{
   static constexpr int value = Rank;
};

template <typename Tensor>
constexpr int get_diagonal_symmetric_tensor_values_rank = get_diagonal_symmetric_tensor_values_rank_v<Tensor>::value;

// /// Represent a SRank+2*DRank symmetric Tensor, where SRank dims are symmetric.
// template <int DRank, // The rank of diagonal values
//           int SRank, // The rank of symmetric values
//           typename T = double,
//           typename Container = MemoryContainer<T>,
//           typename Layout = DynamicLayout<1> >
// class DiagonalSymmetricTensor: public Tensor<DRank+1,T,Container,Layout>
// {
// public:
//    // Storing the symmetric values linearly for the moment
//    DiagonalSymmetricTensor(const Tensor<DRank+1,T,Container,Layout> &t)
//    : Tensor<DRank+1,T,Container,Layout>(t)
//    { }

//    // TODO define a DRank accessor? probably not possible
//    // private inheritance then?
// };

// template <int DRank, typename Tensor>
// auto makeDiagonalSymmetricTensor(const Tensor &t)
// {
//    return DiagonalSymmetricTensor<DRank,
//                                   Tensor::rank-DRank,
//                                   typename Tensor::type,
//                                   typename Tensor::container,
//                                   typename Tensor::layout
//                                  >(t);
// }

} // namespace mfem

#endif // MFEM_DIAG_TENSOR
