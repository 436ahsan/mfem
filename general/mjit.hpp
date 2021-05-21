// Copyright (c) 2010-2021, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.
#ifndef MFEM_JIT_HPP
#define MFEM_JIT_HPP

#include <cstring>
#include <cassert>
#include <climits>
#include <functional>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MFEM_DEBUG_COLOR 118
#include "debug.hpp"

namespace mfem
{

namespace jit
{

// One character used as the kernel prefix
#define MFEM_JIT_SYMBOL_PREFIX 'k'

// command line option to launch a compilation
#define MFEM_JIT_SHELL_COMMAND "-c"

// base name of the cache library
#define MFEM_JIT_CACHE "libmjit"

// Hash numbers used to combine arguments and its <const char*> specialization
constexpr size_t M_PHI = 0x9e3779b9ull;
constexpr size_t M_FNV_PRIME = 0x100000001b3ull;
constexpr size_t M_FNV_BASIS = 0xcbf29ce484222325ull;

// Generic templated hash function
template <typename T> struct hash
{
   inline size_t operator()(const T& h) const noexcept
   {
      return std::hash<T> {}(h);
   }
};

// Specialized <const char*> hash function
template<> struct hash<const char*>
{
   inline size_t operator()(const char *s) const noexcept
   {
      size_t hash = M_FNV_BASIS;
      for (size_t n = strlen(s); n; n--)
      { hash = (hash * M_FNV_PRIME) ^ static_cast<size_t>(s[n]); }
      return hash;
   }
};

// Hash combine function
template <typename T> inline
size_t hash_combine(const size_t &s, const T &v) noexcept
{ return s ^ (mfem::jit::hash<T> {}(v) + M_PHI + (s<<6) + (s>>2));}

// Terminal hash arguments function
template<typename T> inline
size_t hash_args(const size_t &seed, const T &that) noexcept
{ return hash_combine(seed, that); }

// Templated hash arguments function
template<typename T, typename... Args> inline
size_t hash_args(const size_t &seed, const T &arg, Args... args)
noexcept { return hash_args(hash_combine(seed, arg), args...); }

// Union to hold either a double or a uint64_t
typedef union {double d; uint64_t u;} union_du_t;

// 32 bits hash to string function, shifted to offset which should be sized to
// MFEM_JIT_SYMBOL_PREFIX
inline void uint32str(uint64_t h, char *str, const size_t offset = 1)
{
   h = ((h & 0xFFFFull) << 32) | ((h & 0xFFFF0000ull) >> 16);
   h = ((h & 0x0000FF000000FF00ull) >> 8) | (h & 0x000000FF000000FFull) << 16;
   h = ((h & 0x00F000F000F000F0ull) >> 4) | (h & 0x000F000F000F000Full) << 8;
   constexpr uint64_t odds = 0x0101010101010101ull;
   const uint64_t mask = ((h + 0x0606060606060606ull) >> 4) & odds;
   h |= 0x3030303030303030ull;
   h += 0x27ull * mask;
   memcpy(str + offset, &h, sizeof(h));
}

// 64 bits hash to string function
inline void uint64str(uint64_t hash, char *str, const char *ext = "")
{
   str[0] = MFEM_JIT_SYMBOL_PREFIX;
   uint32str(hash >> 32, str);
   uint32str(hash & 0xFFFFFFFFull, str + 8);
   memcpy(str + 1 + 16, ext, strlen(ext));
   str[1 + 16 + strlen(ext)] = 0;
}

/// Returns true if MPI world rank is zero.
bool Root();

/// Returns the shared library version of the current run.
/// Initialized at '0' and can be incremented by setting 'inc' to true.
int GetVersion(bool inc = false);


/// Base file class
template <class T> class File
{
protected:
   int fd;
   size_t size;
   const char *pathname;
public:
   File(const char *file, size_t sz = 0)
   {
      size = sz;
      pathname = file;
      static_cast<T*>(this)->init();
   }
   static int Open() { return T::Open(); }
   static int Close() { return T::Close(); }
   static int Unlink() { return T::Unlink(); }
};

/// Standard file class
class StdFile: public File<StdFile>
{
public:
   int init(const char *pathname, size_t size)
   {
      dbg();
      int fd = Open(pathname);
      return fd;
   }

   static int Open(const char *pathname)
   {
      dbg("pathname: %s", pathname);
      const int fd = ::open(pathname, O_RDONLY, S_IRUSR|S_IRWXG);
      if (fd == -1)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[open] file failed: %s\033[m\n",
                     strerror(errno)));
      return fd;
   }

   static int Close(const int fd)
   {
      if (::close(fd) == -1)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[close] failed: %s\033[m\n",
                     strerror(errno)));
      return 0;
   }

   static int Unlink(const char *pathname)
   {
      dbg("pathname: %s",pathname);
      if (::unlink(pathname) == -1)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[unlink] Error removing %s: %s\033[m\n",
                     pathname, strerror(errno)));
      return EXIT_SUCCESS;
   }
};

/// Shared-memory file class
class ShmFile: public File<ShmFile>
{
public:
   int init(const char *pathname, size_t size)
   {
      dbg();
      int fd = Open(pathname);
      if (::ftruncate(fd, size) < 0)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[shmInit] ftruncate failed: %s\033[m\n",
                     strerror(errno)));
      return fd;
   }

   static int Open(const char *pathname)
   {
      dbg("Shared memory name: %s", pathname);
      const int fd = shm_open(pathname, O_CREAT | O_RDWR, 0666);
      if (fd == -1)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[shmInit] Shared memory failed: %s\033[m\n",
                     strerror(errno)));
      return fd;
   }

   static int Close(const int fd)
   {
      if (::close(fd) == -1)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[shmClose] Close failed: %s\033[m\n",
                     strerror(errno)));
      return 0;
   }

   static int Unlink(const char *pathname)
   {
      if (::shm_unlink(pathname) == -1)
         exit(EXIT_FAILURE|
              printf("\033[31;1m[shmUnlink] Error removing %s: %s\033[m\n",
                     pathname, strerror(errno)));
      return EXIT_SUCCESS;
   }
};


/// Root MPI process file creation, outputing the source of the kernel.
template<typename... Args>
inline bool CreateInput(const char *input, const size_t hash,
                        const char *src, Args... args)
{
   if (!Root()) { return true; }

   dbg("input: '(/dev/shm/)%s'",input);

   int size;
   char *bfr = (char*) malloc(size*sizeof(char));
   if ((size = sprintf(bfr, src, hash, hash, hash, args...)) < 0)
   {
      free(bfr);
      dbg("!sprintf");
      return false;
   }
   dbg("size:%d",size);

   // Remove shared memory segment if it already exists.
   ::shm_unlink(input);

   // Attempt to create shared memory segment
   // On Linux this segment corresponds to the file: '/dev/shm/...'
   const mode_t mode = S_IRUSR | S_IWUSR;
   const int oflag = O_CREAT | O_RDWR | O_EXCL;
   const int fd = ::shm_open(input, oflag, mode);

   if (fd < 0)
   {
      dbg("!fd");
      exit(EXIT_FAILURE|
           printf("\033[31;1m[shmOpen] Shared memory failed: %s\033[m\n",
                  strerror(errno)));
      return false;
   }

   // resize shm to the right size
   if (::ftruncate(fd, size) < 0)
   {
      free(bfr);
      ::shm_unlink(input); // ipcs -m
      dbg("!ftruncate");
      return false;
   }
   // Map shared memory segment into process address space
   void *pmap = mmap(nullptr,           // Most of the time set to nullptr
                     size,              // Size of memory mapping
                     PROT_READ | PROT_WRITE,   // Allows reading and writing operations
                     MAP_SHARED,        // This flag makes this segment visible by other processes.
                     fd,                // File descriptor
                     0x00);             // Offset from beggining of file
   if (pmap == MAP_FAILED) { dbg("!pmap"); return false; }
   // Copy the source into the mapped shm file
   ::memcpy(pmap, bfr, size+1);
   if (::close(fd) < 0) { dbg("!close"); return false; }
   dbg("done");
   return true;
}

/// Root MPI process file creation, outputing the source of the kernel.
/// ipcrm -a
/// ipcs -m
inline bool CreateOutput(const char *output)
{
   if (!Root()) { return true; }
   dbg("output: '(/dev/shm/)%s'",output);
   constexpr int SHM_MAX_SIZE = 2*1024*1024;

   // Remove shared memory segment if it already exists.
   dbg("shm_unlink");
   ::shm_unlink(output);

   // Attempt to create shared  memory segment
   // On Linux this segment corresponds to the file: '/dev/shm/...'
   const mode_t mode = S_IRUSR | S_IWUSR;
   const int oflag = O_CREAT | O_RDWR | O_TRUNC;
   dbg("shm_open");
   const int fd = ::shm_open(output, oflag, mode);
   dbg("shm_open'ed");

   if (fd < 0) { dbg("!fd"); return false; }

   // resize shm to the right size
   if (::ftruncate(fd, SHM_MAX_SIZE) < 0)
   {
      ::shm_unlink(output);
      dbg("!ftruncate");
      return false;
   }

   if (::close(fd) < 0) { dbg("!close"); return false; }
   dbg("done");
   return true;
}

/// Compile the source file with PIC flags, updating the cache library.
bool Compile(const char *input,  // kernel source file name
             const char *output, // kernel object file name
             const char *cxx,    // compiler
             const char *flags,  // compilation flags
             const char *mfem_source_dir,
             const char *mfem_install_dir,
             const bool check);  // check for existing archive

template<typename... Args>
inline bool Compile(const size_t hash, // kernel hash id
                    const bool check, // check for existing archive
                    const char *src,  // kernel source
                    const char *cxx,  // compiler used when compiling MFEM
                    const char *flags,// MFEM_CXXFLAGS
                    const char *msrc, // MFEM_SOURCE_DIR
                    const char *mins, // MFEM_INSTALL_DIR
                    Args... args)
{
   // MFEM_JIT_SYMBOL_PREFIX + hex64 string + extension + '\0': 1 + 16 + 3 + 1
   char input[21], output[21];
   uint64str(hash, output, ".co");
   uint64str(hash, input, ".cc");
   dbg("Create, input:(shm)%s => output:%s", input, output);
   if (!CreateInput(input, hash, src, args...) !=0 )
   {
      dbg("Error in Create");
      return false;
   }
   if (!CreateOutput(output) !=0 )
   {
      dbg("Error in Create");
      return false;
   }
   char shm_input[9+21], shm_output[9+21];
   ::memcpy(shm_input, "/dev/shm/", 9);
   ::memcpy(shm_output, "/dev/shm/", 9);
   uint64str(hash, shm_input + 9, ".cc");
   uint64str(hash, shm_output + 9, ".co");
   return Compile(shm_input, shm_output, cxx, flags, msrc, mins, check);
}

/// Lookup in the cache for the kernel with the given hash
template<typename... Args>
inline void *Lookup(const size_t hash, Args... args)
{
   char symbol[18]; // MFEM_JIT_SYMBOL_PREFIX + hex64 string + '\0' = 18
   uint64str(hash, symbol);
   constexpr int mode = RTLD_NOW | RTLD_LOCAL;
   constexpr const char *so_name = MFEM_JIT_CACHE ".so";

   constexpr int PM = PATH_MAX;
   char so_version[PM];
   const int version = GetVersion();
   if (snprintf(so_version,PM,"%s.so.%d",MFEM_JIT_CACHE,version)<0)
   { return nullptr; }

   void *handle = nullptr;
   const bool first_compilation = (version == 0);
   // We first try to open the shared cache library
   handle = dlopen(first_compilation ? so_name : so_version, mode);
   // If no handle was found, fold back looking for the archive
   if (!handle)
   {
      constexpr bool check_for_archive = true;
      if (!Compile(hash, check_for_archive, args...)) { return nullptr; }
      handle = dlopen(first_compilation ? so_name : so_version, mode);
   }
   if (!handle) { return nullptr; }
   // Now look for the kernel symbol
   if (!dlsym(handle, symbol))
   {
      // If not found, avoid using the archive and update the shared objects
      dlclose(handle);
      constexpr bool check_for_archive = false;
      if (!Compile(hash, check_for_archive, args...)) { return nullptr; }
      handle = dlopen(so_version, mode);
   }
   if (!handle) { return nullptr; }
   if (!dlsym(handle, symbol)) { return nullptr; }
   if (!getenv("TMP")) { shm_unlink(so_version); }
   return handle;
}

/// Symbol search from a given handle
template<typename kernel_t>
inline kernel_t Symbol(const size_t hash, void *handle)
{
   char symbol[18];
   uint64str(hash, symbol);
   return (kernel_t) dlsym(handle, symbol);
}

/// Kernel class
template<typename kernel_t> class kernel
{
   const size_t seed, hash;
   const char *name;
   void *handle;
   kernel_t code;
   char symbol[18];
   const char *cxx, *src, *flags, *msrc, *mins;

public:
   template<typename... Args>
   kernel(const char *name,  // kernel name
          const char *cxx,   // compiler
          const char *src,   // kernel source filename
          const char *flags, // MFEM_CXXFLAGS
          const char *msrc,  // MFEM_SOURCE_DIR
          const char* mins,  // MFEM_INSTALL_DIR
          Args... args):
      seed(jit::hash<const char*>()(src)),
      hash(hash_args(seed, cxx, flags, msrc, mins, args...)),
      name((uint64str(hash, symbol),name)),
      handle(Lookup(hash, src, cxx, flags, msrc, mins, args...)),
      code(Symbol<kernel_t>(hash, handle)),
      cxx(cxx), src(src), flags(flags), msrc(msrc), mins(mins)
   { assert(handle); }

   /// Kernel launch without return type
   template<typename... Args>
   void operator_void(Args... args) { code(args...); }

   /// Kernel launch with return type
   template<typename T, typename... Args>
   T operator()(const T type, Args... args) { return code(type, args...); }

   ~kernel() { dlclose(handle); }
};

} // namespace jit

} // namespace mfem

#ifdef MJIT_FORALL

#include <iostream>

#define MFEM_VERIFY(x, msg) \
    if (!(x)) { \
    std::cerr << "Verification failed: (" << #x << ") is false:\n --> " \
              << msg << std::endl; }

#define MFEM_ASSERT(x, msg) \
    if (!(x)) { \
    std::cerr << "Verification failed: (" << #x << ") is false:\n --> " \
              << msg << std::endl; }

#include "../config/config.hpp"

#define MAX_D1D 1
#define MAX_Q1D 1

#ifdef MFEM_USE_CUDA

#define MFEM_CUDA_BLOCKS 256

#include <cuda_runtime.h>
#include <cuda.h>

#define MFEM_DEVICE __device__
#define MFEM_HOST_DEVICE __host__ __device__

#define MFEM_GPU_CHECK(x) \
   do \
   { \
      cudaError_t err = (x); \
      if (err != cudaSuccess) \
      { \
         printf(cudaGetErrorString(err)); \
      } \
   } \
   while (0)

#define MFEM_DEVICE_SYNC MFEM_GPU_CHECK(cudaDeviceSynchronize())
#define MFEM_STREAM_SYNC MFEM_GPU_CHECK(cudaStreamSynchronize(0))

#if defined(MFEM_USE_CUDA) && defined(__CUDA_ARCH__)
#define MFEM_SHARED __shared__
#define MFEM_SYNC_THREAD __syncthreads()
#define MFEM_THREAD_ID(k) threadIdx.k
#define MFEM_THREAD_SIZE(k) blockDim.k
#define MFEM_FOREACH_THREAD(i,k,N) for(int i=threadIdx.k; i<N; i+=blockDim.k)
#endif

#if !(defined(MFEM_USE_CUDA) || defined(MFEM_USE_HIP))
#define MFEM_DEVICE
#define MFEM_HOST_DEVICE
#define MFEM_DEVICE_SYNC
#define MFEM_STREAM_SYNC
#endif

#if !((defined(MFEM_USE_CUDA) && defined(__CUDA_ARCH__)) || \
      (defined(MFEM_USE_HIP)  && defined(__ROCM_ARCH__)))
#define MFEM_SHARED
#define MFEM_SYNC_THREAD
#define MFEM_THREAD_ID(k) 0
#define MFEM_THREAD_SIZE(k) 1
#define MFEM_FOREACH_THREAD(i,k,N) for(int i=0; i<N; i++)
#endif

template <typename BODY> __global__ static
void CuKernel1D(const int N, BODY body)
{
   const int k = blockDim.x*blockIdx.x + threadIdx.x;
   if (k >= N) { return; }
   body(k);
}

template <typename BODY> __global__ static
void CuKernel2D(const int N, BODY body, const int BZ)
{
   const int k = blockIdx.x*BZ + threadIdx.z;
   if (k >= N) { return; }
   body(k);
}

template <typename BODY> __global__ static
void CuKernel3D(const int N, BODY body)
{
   const int k = blockIdx.x;
   if (k >= N) { return; }
   body(k);
}

template <const int BLCK = MFEM_CUDA_BLOCKS, typename DBODY>
void CuWrap1D(const int N, DBODY &&d_body)
{
   if (N==0) { return; }
   const int GRID = (N+BLCK-1)/BLCK;
   CuKernel1D<<<GRID,BLCK>>>(N, d_body);
   MFEM_GPU_CHECK(cudaGetLastError());
}

template <typename DBODY>
void CuWrap2D(const int N, DBODY &&d_body,
              const int X, const int Y, const int BZ)
{
   if (N==0) { return; }
   MFEM_VERIFY(BZ>0, "");
   const int GRID = (N+BZ-1)/BZ;
   const dim3 BLCK(X,Y,BZ);
   CuKernel2D<<<GRID,BLCK>>>(N,d_body,BZ);
   MFEM_GPU_CHECK(cudaGetLastError());
}

template <typename DBODY>
void CuWrap3D(const int N, DBODY &&d_body,
              const int X, const int Y, const int Z)
{
   if (N==0) { return; }
   const int GRID = N;
   const dim3 BLCK(X,Y,Z);
   CuKernel3D<<<GRID,BLCK>>>(N,d_body);
   MFEM_GPU_CHECK(cudaGetLastError());
}

#else // MFEM_USE_CUDA

#define MFEM_DEVICE
#define MFEM_HOST_DEVICE
#define MFEM_DEVICE_SYNC
#define MFEM_STREAM_SYNC

#define MFEM_SHARED
#define MFEM_SYNC_THREAD
#define MFEM_THREAD_ID(k) 0
#define MFEM_THREAD_SIZE(k) 1
#define MFEM_FOREACH_THREAD(i,k,N) for(int i=0; i<N; i++)

template <typename DBODY>
void CuWrap2D(const int N, DBODY &&d_body,
              const int X, const int Y, const int BZ) { }

template <typename DBODY>
void CuWrap3D(const int N, DBODY &&d_body,
              const int X, const int Y, const int Z) { }

#endif // MFEM_USE_CUDA

// Include dtensor, but skip the backends headers we just short-circuited
#define MFEM_CUDA_HPP
#define MFEM_HIP_HPP
#include "../linalg/dtensor.hpp"

#endif // MJIT_FORALL

#endif // MFEM_JIT_HPP
