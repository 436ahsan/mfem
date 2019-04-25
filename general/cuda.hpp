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

#ifndef MFEM_CUDA_HPP
#define MFEM_CUDA_HPP

#include "../config/config.hpp"
#include "error.hpp"

#ifdef MFEM_USE_CUDA
#include <cuda_runtime.h>
#include <cuda.h>
#endif

// CUDA block size used by MFEM.
#define MFEM_CUDA_BLOCKS 256

#ifdef MFEM_USE_CUDA
#define MFEM_ATTR_DEVICE __device__
#define MFEM_ATTR_HOST_DEVICE __host__ __device__
// Define a CUDA error check macro, MFEM_CUDA_CHECK(x), where x returns/is of
// type 'cudaError_t'. This macro evaluates 'x' and raises an error if the
// result is not cudaSuccess.
#define MFEM_CUDA_CHECK(x) \
   do \
   { \
      cudaError_t err = (x); \
      if (err != cudaSuccess && err != cudaErrorCudartUnloading) \
      { \
         mfem_cuda_error(err, #x, _MFEM_FUNC_NAME, __FILE__, __LINE__); \
      } \
   } \
   while (0)
#else // MFEM_USE_CUDA
#define MFEM_ATTR_DEVICE
#define MFEM_ATTR_HOST_DEVICE
#endif // MFEM_USE_CUDA

namespace mfem
{

#ifdef MFEM_USE_CUDA
// Function used by the macro MFEM_CUDA_CHECK.
void mfem_cuda_error(cudaError_t err, const char *expr, const char *func,
                     const char *file, int line);
#endif

// Function to setup the CUDA device and get the number of GPU
void CudaDeviceSetup(const int dev, int &ngpu);

} // namespace mfem

#endif // MFEM_CUDA_HPP
