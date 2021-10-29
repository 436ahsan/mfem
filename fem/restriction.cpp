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

#include "../config/config.hpp"
#include "nonlininteg.hpp"
#include "fespace.hpp"
#include "fem.hpp"

#include "restriction.hpp"
#include "gridfunc.hpp"
#include "fespace.hpp"
#include "../general/forall.hpp"
#include <climits>

#ifdef MFEM_USE_MPI

#include "pfespace.hpp"

#endif

#include <chrono>


namespace mfem
{

ElementRestriction::ElementRestriction(const FiniteElementSpace &f,
                                       ElementDofOrdering e_ordering)
   : fes(f),
     ne(fes.GetNE()),
     vdim(fes.GetVDim()),
     byvdim(fes.GetOrdering() == Ordering::byVDIM),
     ndofs(fes.GetNDofs()),
     dof(ne > 0 ? fes.GetFE(0)->GetDof() : 0),
     nedofs(ne*dof),
     offsets(ndofs+1),
     indices(ne*dof),
     gatherMap(ne*dof)
{
   // Assuming all finite elements are the same.
   height = vdim*ne*dof;
   width = fes.GetVSize();
   const bool dof_reorder = (e_ordering == ElementDofOrdering::LEXICOGRAPHIC);
   const int *dof_map = NULL;
   if (dof_reorder && ne > 0)
   {
      for (int e = 0; e < ne; ++e)
      {
         const FiniteElement *fe = fes.GetFE(e);
         const TensorBasisElement* el =
            dynamic_cast<const TensorBasisElement*>(fe);
         if (el) { continue; }
         mfem_error("Finite element not suitable for lexicographic ordering");
      }
      const FiniteElement *fe = fes.GetFE(0);
      const TensorBasisElement* el =
         dynamic_cast<const TensorBasisElement*>(fe);
      const Array<int> &fe_dof_map = el->GetDofMap();
      MFEM_VERIFY(fe_dof_map.Size() > 0, "invalid dof map");
      dof_map = fe_dof_map.GetData();
   }
   const Table& e2dTable = fes.GetElementToDofTable();
   const int* elementMap = e2dTable.GetJ();
   // We will be keeping a count of how many local nodes point to its global dof
   for (int i = 0; i <= ndofs; ++i)
   {
      offsets[i] = 0;
   }
   for (int e = 0; e < ne; ++e)
   {
      for (int d = 0; d < dof; ++d)
      {
         const int sgid = elementMap[dof*e + d];  // signed
         const int gid = (sgid >= 0) ? sgid : -1 - sgid;
         ++offsets[gid + 1];
      }
   }
   // Aggregate to find offsets for each global dof
   for (int i = 1; i <= ndofs; ++i)
   {
      offsets[i] += offsets[i - 1];
   }
   // For each global dof, fill in all local nodes that point to it
   for (int e = 0; e < ne; ++e)
   {
      for (int d = 0; d < dof; ++d)
      {
         const int sdid = dof_reorder ? dof_map[d] : 0;  // signed
         const int did = (!dof_reorder)?d:(sdid >= 0 ? sdid : -1-sdid);
         const int sgid = elementMap[dof*e + did];  // signed
         const int gid = (sgid >= 0) ? sgid : -1-sgid;
         const int lid = dof*e + d;
         const bool plus = (sgid >= 0 && sdid >= 0) || (sgid < 0 && sdid < 0);
         gatherMap[lid] = plus ? gid : -1-gid;
         indices[offsets[gid]++] = plus ? lid : -1-lid;
      }
   }
   // We shifted the offsets vector by 1 by using it as a counter.
   // Now we shift it back.
   for (int i = ndofs; i > 0; --i)
   {
      offsets[i] = offsets[i - 1];
   }
   offsets[0] = 0;
}

void ElementRestriction::Mult(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
   auto d_y = Reshape(y.Write(), nd, vd, ne);
   auto d_gatherMap = gatherMap.Read();
   MFEM_FORALL(i, dof*ne,
   {
      const int gid = d_gatherMap[i];
      const bool plus = gid >= 0;
      const int j = plus ? gid : -1-gid;
      for (int c = 0; c < vd; ++c)
      {
         const double dofValue = d_x(t?c:j, t?j:c);
         d_y(i % nd, c, i / nd) = plus ? dofValue : -dofValue;
      }
   });
}

void ElementRestriction::MultUnsigned(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
   auto d_y = Reshape(y.Write(), nd, vd, ne);
   auto d_gatherMap = gatherMap.Read();

   MFEM_FORALL(i, dof*ne,
   {
      const int gid = d_gatherMap[i];
      const int j = gid >= 0 ? gid : -1-gid;
      for (int c = 0; c < vd; ++c)
      {
         d_y(i % nd, c, i / nd) = d_x(t?c:j, t?j:c);
      }
   });
}

void ElementRestriction::MultTranspose(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_offsets = offsets.Read();
   auto d_indices = indices.Read();
   auto d_x = Reshape(x.Read(), nd, vd, ne);
   auto d_y = Reshape(y.Write(), t?vd:ndofs, t?ndofs:vd);
   MFEM_FORALL(i, ndofs,
   {
      const int offset = d_offsets[i];
      const int nextOffset = d_offsets[i + 1];
      for (int c = 0; c < vd; ++c)
      {
         double dofValue = 0;
         for (int j = offset; j < nextOffset; ++j)
         {
            const int idx_j = (d_indices[j] >= 0) ? d_indices[j] : -1 - d_indices[j];
            dofValue += ((d_indices[j] >= 0) ? d_x(idx_j % nd, c, idx_j / nd) :
                         -d_x(idx_j % nd, c, idx_j / nd));
         }
         d_y(t?c:i,t?i:c) = dofValue;
      }
   });
}

void ElementRestriction::MultTransposeUnsigned(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_offsets = offsets.Read();
   auto d_indices = indices.Read();
   auto d_x = Reshape(x.Read(), nd, vd, ne);
   auto d_y = Reshape(y.Write(), t?vd:ndofs, t?ndofs:vd);
   MFEM_FORALL(i, ndofs,
   {
      const int offset = d_offsets[i];
      const int nextOffset = d_offsets[i + 1];
      for (int c = 0; c < vd; ++c)
      {
         double dofValue = 0;
         for (int j = offset; j < nextOffset; ++j)
         {
            const int idx_j = (d_indices[j] >= 0) ? d_indices[j] : -1 - d_indices[j];
            dofValue += d_x(idx_j % nd, c, idx_j / nd);
         }
         d_y(t?c:i,t?i:c) = dofValue;
      }
   });
}

void ElementRestriction::MultLeftInverse(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_offsets = offsets.Read();
   auto d_indices = indices.Read();
   auto d_x = Reshape(x.Read(), nd, vd, ne);
   auto d_y = Reshape(y.Write(), t?vd:ndofs, t?ndofs:vd);
   MFEM_FORALL(i, ndofs,
   {
      const int nextOffset = d_offsets[i + 1];
      for (int c = 0; c < vd; ++c)
      {
         double dofValue = 0;
         const int j = nextOffset - 1;
         const int idx_j = (d_indices[j] >= 0) ? d_indices[j] : -1 - d_indices[j];
         dofValue = (d_indices[j] >= 0) ? d_x(idx_j % nd, c, idx_j / nd) :
         -d_x(idx_j % nd, c, idx_j / nd);
         d_y(t?c:i,t?i:c) = dofValue;
      }
   });
}

void ElementRestriction::BooleanMask(Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;

   Array<char> processed(vd * ndofs);
   processed = 0;

   auto d_offsets = offsets.HostRead();
   auto d_indices = indices.HostRead();
   auto d_x = Reshape(processed.HostReadWrite(), t?vd:ndofs, t?ndofs:vd);
   auto d_y = Reshape(y.HostWrite(), nd, vd, ne);
   for (int i = 0; i < ndofs; ++i)
   {
      const int offset = d_offsets[i];
      const int nextOffset = d_offsets[i+1];
      for (int c = 0; c < vd; ++c)
      {
         for (int j = offset; j < nextOffset; ++j)
         {
            const int idx_j = d_indices[j];
            if (d_x(t?c:i,t?i:c))
            {
               d_y(idx_j % nd, c, idx_j / nd) = 0.0;
            }
            else
            {
               d_y(idx_j % nd, c, idx_j / nd) = 1.0;
               d_x(t?c:i,t?i:c) = 1;
            }
         }
      }
   }
}

void ElementRestriction::FillSparseMatrix(const Vector &mat_ea,
                                          SparseMatrix &mat) const
{
   mat.GetMemoryI().New(mat.Height()+1, mat.GetMemoryI().GetMemoryType());
   const int nnz = FillI(mat);
   mat.GetMemoryJ().New(nnz, mat.GetMemoryJ().GetMemoryType());
   mat.GetMemoryData().New(nnz, mat.GetMemoryData().GetMemoryType());
   FillJAndData(mat_ea, mat);
}

static MFEM_HOST_DEVICE int GetMinElt(const int *my_elts, const int nbElts,
                                      const int *nbr_elts, const int nbrNbElts)
{
   // Find the minimal element index found in both my_elts[] and nbr_elts[]
   int min_el = INT_MAX;
   for (int i = 0; i < nbElts; i++)
   {
      const int e_i = my_elts[i];
      if (e_i >= min_el) { continue; }
      for (int j = 0; j < nbrNbElts; j++)
      {
         if (e_i==nbr_elts[j])
         {
            min_el = e_i; // we already know e_i < min_el
            break;
         }
      }
   }
   return min_el;
}

/** Returns the index where a non-zero entry should be added and increment the
    number of non-zeros for the row i_L. */
static MFEM_HOST_DEVICE int GetAndIncrementNnzIndex(const int i_L, int* I)
{
   int ind = AtomicAdd(I[i_L],1);
   return ind;
}

int ElementRestriction::FillI(SparseMatrix &mat) const
{
   static constexpr int Max = MaxNbNbr;
   const int all_dofs = ndofs;
   const int vd = vdim;
   const int elt_dofs = dof;
   auto I = mat.ReadWriteI();
   auto d_offsets = offsets.Read();
   auto d_indices = indices.Read();
   auto d_gatherMap = gatherMap.Read();
   MFEM_FORALL(i_L, vd*all_dofs+1,
   {
      I[i_L] = 0;
   });
   MFEM_FORALL(e, ne,
   {
      for (int i = 0; i < elt_dofs; i++)
      {
         int i_elts[Max];
         const int i_E = e*elt_dofs + i;
         const int i_L = d_gatherMap[i_E];
         const int i_offset = d_offsets[i_L];
         const int i_nextOffset = d_offsets[i_L+1];
         const int i_nbElts = i_nextOffset - i_offset;
         for (int e_i = 0; e_i < i_nbElts; ++e_i)
         {
            const int i_E = d_indices[i_offset+e_i];
            i_elts[e_i] = i_E/elt_dofs;
         }
         for (int j = 0; j < elt_dofs; j++)
         {
            const int j_E = e*elt_dofs + j;
            const int j_L = d_gatherMap[j_E];
            const int j_offset = d_offsets[j_L];
            const int j_nextOffset = d_offsets[j_L+1];
            const int j_nbElts = j_nextOffset - j_offset;
            if (i_nbElts == 1 || j_nbElts == 1) // no assembly required
            {
               GetAndIncrementNnzIndex(i_L, I);
            }
            else // assembly required
            {
               int j_elts[Max];
               for (int e_j = 0; e_j < j_nbElts; ++e_j)
               {
                  const int j_E = d_indices[j_offset+e_j];
                  const int elt = j_E/elt_dofs;
                  j_elts[e_j] = elt;
               }
               int min_e = GetMinElt(i_elts, i_nbElts, j_elts, j_nbElts);
               if (e == min_e) // add the nnz only once
               {
                  GetAndIncrementNnzIndex(i_L, I);
               }
            }
         }
      }
   });
   // We need to sum the entries of I, we do it on CPU as it is very sequential.
   auto h_I = mat.HostReadWriteI();
   const int nTdofs = vd*all_dofs;
   int sum = 0;
   for (int i = 0; i < nTdofs; i++)
   {
      const int nnz = h_I[i];
      h_I[i] = sum;
      sum+=nnz;
   }
   h_I[nTdofs] = sum;
   // We return the number of nnz
   return h_I[nTdofs];
}

void ElementRestriction::FillJAndData(const Vector &ea_data,
                                      SparseMatrix &mat) const
{
   static constexpr int Max = MaxNbNbr;
   const int all_dofs = ndofs;
   const int vd = vdim;
   const int elt_dofs = dof;
   auto I = mat.ReadWriteI();
   auto J = mat.WriteJ();
   auto Data = mat.WriteData();
   auto d_offsets = offsets.Read();
   auto d_indices = indices.Read();
   auto d_gatherMap = gatherMap.Read();
   auto mat_ea = Reshape(ea_data.Read(), elt_dofs, elt_dofs, ne);
   MFEM_FORALL(e, ne,
   {
      for (int i = 0; i < elt_dofs; i++)
      {
         int i_elts[Max];
         int i_B[Max];
         const int i_E = e*elt_dofs + i;
         const int i_L = d_gatherMap[i_E];
         const int i_offset = d_offsets[i_L];
         const int i_nextOffset = d_offsets[i_L+1];
         const int i_nbElts = i_nextOffset - i_offset;
         for (int e_i = 0; e_i < i_nbElts; ++e_i)
         {
            const int i_E = d_indices[i_offset+e_i];
            i_elts[e_i] = i_E/elt_dofs;
            i_B[e_i]    = i_E%elt_dofs;
         }
         for (int j = 0; j < elt_dofs; j++)
         {
            const int j_E = e*elt_dofs + j;
            const int j_L = d_gatherMap[j_E];
            const int j_offset = d_offsets[j_L];
            const int j_nextOffset = d_offsets[j_L+1];
            const int j_nbElts = j_nextOffset - j_offset;
            if (i_nbElts == 1 || j_nbElts == 1) // no assembly required
            {
               const int nnz = GetAndIncrementNnzIndex(i_L, I);
               J[nnz] = j_L;
               Data[nnz] = mat_ea(j,i,e);
            }
            else // assembly required
            {
               int j_elts[Max];
               int j_B[Max];
               for (int e_j = 0; e_j < j_nbElts; ++e_j)
               {
                  const int j_E = d_indices[j_offset+e_j];
                  const int elt = j_E/elt_dofs;
                  j_elts[e_j] = elt;
                  j_B[e_j]    = j_E%elt_dofs;
               }
               int min_e = GetMinElt(i_elts, i_nbElts, j_elts, j_nbElts);
               if (e == min_e) // add the nnz only once
               {
                  double val = 0.0;
                  for (int i = 0; i < i_nbElts; i++)
                  {
                     const int e_i = i_elts[i];
                     const int i_Bloc = i_B[i];
                     for (int j = 0; j < j_nbElts; j++)
                     {
                        const int e_j = j_elts[j];
                        const int j_Bloc = j_B[j];
                        if (e_i == e_j)
                        {
                           val += mat_ea(j_Bloc, i_Bloc, e_i);
                        }
                     }
                  }
                  const int nnz = GetAndIncrementNnzIndex(i_L, I);
                  J[nnz] = j_L;
                  Data[nnz] = val;
               }
            }
         }
      }
   });
   // We need to shift again the entries of I, we do it on CPU as it is very
   // sequential.
   auto h_I = mat.HostReadWriteI();
   const int size = vd*all_dofs;
   for (int i = 0; i < size; i++)
   {
      h_I[size-i] = h_I[size-(i+1)];
   }
   h_I[0] = 0;
}

L2ElementRestriction::L2ElementRestriction(const FiniteElementSpace &fes)
   : ne(fes.GetNE()),
     vdim(fes.GetVDim()),
     byvdim(fes.GetOrdering() == Ordering::byVDIM),
     ndof(ne > 0 ? fes.GetFE(0)->GetDof() : 0),
     ndofs(fes.GetNDofs())
{
   height = vdim*ne*ndof;
   width = vdim*ne*ndof;
}

void L2ElementRestriction::Mult(const Vector &x, Vector &y) const
{
   const int nd = ndof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
   auto d_y = Reshape(y.Write(), nd, vd, ne);
   MFEM_FORALL(i, ndofs,
   {
      const int idx = i;
      const int dof = idx % nd;
      const int e = idx / nd;
      for (int c = 0; c < vd; ++c)
      {
         d_y(dof, c, e) = d_x(t?c:idx, t?idx:c);
      }
   });
}

void L2ElementRestriction::MultTranspose(const Vector &x, Vector &y) const
{
   const int nd = ndof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_x = Reshape(x.Read(), nd, vd, ne);
   auto d_y = Reshape(y.Write(), t?vd:ndofs, t?ndofs:vd);
   MFEM_FORALL(i, ndofs,
   {
      const int idx = i;
      const int dof = idx % nd;
      const int e = idx / nd;
      for (int c = 0; c < vd; ++c)
      {
         d_y(t?c:idx,t?idx:c) = d_x(dof, c, e);
      }
   });
}

void L2ElementRestriction::FillI(SparseMatrix &mat) const
{
   const int elem_dofs = ndof;
   const int vd = vdim;
   auto I = mat.WriteI();
   const int isize = mat.Height() + 1;
   const int interior_dofs = ne*elem_dofs*vd;
   MFEM_FORALL(dof, isize,
   {
      I[dof] = dof<interior_dofs ? elem_dofs : 0;
   });
}

static MFEM_HOST_DEVICE int AddNnz(const int iE, int *I, const int dofs)
{
   int val = AtomicAdd(I[iE],dofs);
   return val;
}

void L2ElementRestriction::FillJAndData(const Vector &ea_data,
                                        SparseMatrix &mat) const
{
   const int elem_dofs = ndof;
   const int vd = vdim;
   auto I = mat.ReadWriteI();
   auto J = mat.WriteJ();
   auto Data = mat.WriteData();
   auto mat_ea = Reshape(ea_data.Read(), elem_dofs, elem_dofs, ne);
   MFEM_FORALL(iE, ne*elem_dofs*vd,
   {
      const int offset = AddNnz(iE,I,elem_dofs);
      const int e = iE/elem_dofs;
      const int i = iE%elem_dofs;
      for (int j = 0; j < elem_dofs; j++)
      {
         J[offset+j] = e*elem_dofs+j;
         Data[offset+j] = mat_ea(j,i,e);
      }
   });
}

int inline Get_dof_from_ijk(int i, int j, int k, int ndofs1d)
{
   return i + ndofs1d*j + ndofs1d*ndofs1d*k;
}

void inline Get_ijk_from_dof(int dof, int &i, int &j, int &k, int ndofs1d)
{
   i = dof % ndofs1d;
   j = dof/ndofs1d % ndofs1d;
   k = dof/ndofs1d/ndofs1d;
}

// Return the face degrees of freedom returned in Lexicographic order.
void GetNormalDFaceDofStencil(const int dim, 
                              const int face_id,
                              const int ndofs1d, 
                              Array<int> &facemapnor)
{
   int end = ndofs1d-1;
   switch (dim)
   {
      case 1:
         MFEM_ABORT("GetNormalDFaceDofStencil not implemented for 1D!");         
      case 2:
         switch (face_id)
         {
            // i is WEST to EAST
            // j is SOUTH to NORTH
            // dof = i + j*ndofs1d
            case 0: // SOUTH, j = 0
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*i+s] = Get_dof_from_ijk(i,s,0,ndofs1d);
                     //i + s*ndofs1d;
                  }
               }
               break;
            case 1: // EAST, i = ndofs1d-1
               for (int j = 0; j < ndofs1d; ++j)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*j+s] = Get_dof_from_ijk(end-s,j,0,ndofs1d);//end-s + j*ndofs1d;
                  }
               }
               break;
            case 2: // NORTH, j = ndofs1d-1
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*i+s] = Get_dof_from_ijk(i,end-s,0,ndofs1d);//(end-s)*ndofs1d + i;
                  }
               }
               break;
            case 3: // WEST, i = 0
               for (int j = 0; j < ndofs1d; ++j)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*j+s] = Get_dof_from_ijk(s,j,0,ndofs1d);//s + j*ndofs1d;
                  }
               }
               break;
            default:
               MFEM_ABORT("Invalid face_id");
         }
         break;
      case 3:
         switch (face_id)
         {
            // dof = i + j*ndofs1d + k*ndofs1d*ndofs1d 
            case 0: // BOTTOM
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,j,s,ndofs1d);//i + j*ndofs1d + s*ndofs1d*ndofs1d;
                     }
                  }
               }
               break;
            case 1: // SOUTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,s,j,ndofs1d);//i + s*ndofs1d + j*ndofs1d*ndofs1d;
                     }
                  }
               }
               break;
            case 2: // EAST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(end-s,i,j,ndofs1d);//end-s + i*ndofs1d + j*ndofs1d*ndofs1d;
                     }
                  }
               }
               break;
            case 3: // NORTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,end-s,j,ndofs1d);//(end-s)*ndofs1d + i + j*ndofs1d*ndofs1d;
                     }
                  }
               }
               break;
            case 4: // WEST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(s,i,j,ndofs1d);//s + i*ndofs1d + j*ndofs1d*ndofs1d;
                     }
                  }
               }
               break;
            case 5: // TOP
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,j,end-s,ndofs1d);//(end-s)*ndofs1d*ndofs1d + i + j*ndofs1d;
                     }
                  }
               }
               break;
            default: 
               MFEM_ABORT("Invalid face_id");
         }
         break;
#ifdef MFEM_DEBUG
         for(int k = 0 ; k < ndofs1d*ndofs1d*ndofs1d ; k++ )
         {
            MFEM_VERIFY( (facemapnor[k] >= ndofs1d*ndofs1d*ndofs1d) or (facemapnor[k] < 0 ) ,
            "Invalid facemapnor values.");
         }
#endif
   }
}

int GetGid(const int ipid, 
           const int k,
           const int ndofs1d,
           const int elemid,
           const int elem_dofs,
           Array<int> &facemap,
           const int* elementMap)
{
   const int face_dof = facemap[ipid*ndofs1d+k];
   const int gid = elementMap[elemid*elem_dofs + face_dof];

#ifdef MFEM_DEBUG
/*
   std::cout << "---------------------" << std::endl;
   std::cout << "ipid = " << ipid << std::endl;
   std::cout << "k = " << k << std::endl;
   std::cout << "ipid*ndofs1d+k = " << ipid*ndofs1d+k << std::endl;
   std::cout << "face_dof = " << face_dof << std::endl;
   std::cout << "elemid " << elemid << std::endl;
   std::cout << "gid " << gid << std::endl;
  */ 
#endif
   return gid;
}

int GetGid(const int ipid, 
           const int ndofs1d, //todo: get rid of ndofs1d by making a facemap for simple restriction
            const int elemid,
           const int elem_dofs,
           Array<int> &facemap,
           const int* elementMap)
{
   const int face_dof = facemap[ipid*ndofs1d+0];
   const int gid = elementMap[elemid*elem_dofs + face_dof];
#ifdef MFEM_DEBUG
   if(  elemid < 0 )
   {
      std::cout << "elemid = " << elemid << std::endl;
      exit(1);
   }
   /*
   std::cout << "---------------------" << std::endl;
   std::cout << "ipid = " << ipid << std::endl;
   std::cout << "ipid*ndofs1d+k = " << ipid*ndofs1d+0 << std::endl;
   std::cout << "face_dof = " << face_dof << std::endl;
   std::cout << "elemid " << elemid << std::endl;
   std::cout << "gid " << gid << std::endl;
   */
#endif
   return gid;
}

int GetLid(const int d,
           const int face_id,
           const int ndofs_face)
{
   const int lid = d + ndofs_face*face_id;
#ifdef MFEM_DEBUG
/*
   std::cout << " face_id = " << face_id << std::endl;
   std::cout << " ndofs_face = " << ndofs_face << std::endl;
   std::cout << " d = " << d << std::endl;
   std::cout << " lid = " << lid << std::endl;
   */
#endif
   return lid;
}

int GetLid(const int d,
           const int k, 
           const int face_id,
           const int ndofs1d,
           const int ndofs_face)
{
   const int lid = k + ndofs1d*( d + ndofs_face*(face_id));// ndofs_face*ndofs1d*face_id + ndofs1d*d + k;
#ifdef MFEM_DEBUG
/*
   std::cout << " face_id = " << face_id << std::endl;
   std::cout << " ndofs1d = " << ndofs1d << std::endl;
   std::cout << " ndofs_face = " << ndofs_face << std::endl;
   std::cout << " d = " << d << std::endl;
   std::cout << " k = " << k << std::endl;
   std::cout << " lid = " << lid << std::endl;
   */
#endif 
   return lid;
}

void GetFromLid(const int lid,
               int &d,
               int &k, 
               int &face_id,
               int ndofs1d,
               int ndofs_face)
{
   if( lid == 123456789 )
   {
      std::cout << " lid = " << lid << std::endl;
      exit(1);
   }
   k = lid % ndofs1d;
   d = (lid/ndofs1d) % ndofs_face;
   face_id = lid/ndofs1d/ndofs_face;
   /*
   std::cout << " lid = " << lid << std::endl;
   std::cout << " face_id = " << face_id << std::endl;
   std::cout << " ndofs1d = " << ndofs1d << std::endl;
   std::cout << " face_id = " << face_id << std::endl;
   std::cout << " d = " << d << std::endl;
   std::cout << " k = " << k << std::endl;
   */
}

void GetFromLid(const int lid,
               int &d,
               int &face_id,
               int ndofs_face)
{
   if( lid == 123456789 )
   {
      std::cout << " lid = " << lid << std::endl;
      exit(1);
   }
   d = lid % ndofs_face;
   face_id = lid/ndofs_face;
#ifdef MFEM_DEBUG
   /*
   std::cout << " lid = " << lid << std::endl;
   std::cout << " face_id = " << face_id << std::endl;
   std::cout << " d = " << d << std::endl;
   */
#endif
}

void GetGradFaceDofStencil(const int dim, 
                              const int face_id,
                              const int ndofs1d, 
                              Array<int> &facemapnor,
                              Array<int> &facemaptan1,
                              Array<int> &facemaptan2)
{
   int end = ndofs1d-1;
   switch (dim)
   {
      case 1:
         MFEM_ABORT("GetNormalDFaceDofStencil not implemented for 1D!");         
      case 2:
         switch (face_id)
         {
            // i is WEST to EAST
            // j is SOUTH to NORTH
            // dof = i + j*ndofs1d
            case 0: // SOUTH, j = 0
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*i+s] = Get_dof_from_ijk(i,s,0,ndofs1d);
                     facemaptan1[ndofs1d*i+s] = Get_dof_from_ijk(s,0,0,ndofs1d);
                  }
               }
               break;
            case 1: // EAST, i = ndofs1d-1
               for (int j = 0; j < ndofs1d; ++j)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*j+s] = Get_dof_from_ijk(end-s,j,0,ndofs1d);
                     facemaptan1[ndofs1d*j+s] = Get_dof_from_ijk(end,s,0,ndofs1d);
                  }
               }
               break;
            case 2: // NORTH, j = ndofs1d-1
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*i+s] = Get_dof_from_ijk(i,end-s,0,ndofs1d);
                     facemaptan1[ndofs1d*i+s] = Get_dof_from_ijk(s,end,0,ndofs1d);                     
                  }
               }
               break;
            case 3: // WEST, i = 0
               for (int j = 0; j < ndofs1d; ++j)
               {
                  for (int s = 0; s < ndofs1d; ++s)
                  {
                     facemapnor[ndofs1d*j+s] = Get_dof_from_ijk(s,j,0,ndofs1d);
                     facemaptan1[ndofs1d*j+s] = Get_dof_from_ijk(0,s,0,ndofs1d);
                  }
               }
               break;
            default:
               MFEM_ABORT("Invalid face_id");
         }
         break;
      case 3:
         switch (face_id)
         {
            // dof = i + j*ndofs1d + k*ndofs1d*ndofs1d 
            case 0: // BOTTOM
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,j,s,ndofs1d);
                        facemaptan1[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(s,j,0,ndofs1d);
                        facemaptan2[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,s,0,ndofs1d);
                     }
                  }
               }
               break;
            case 1: // SOUTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,s,j,ndofs1d);
                        facemaptan1[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(s,0,j,ndofs1d);
                        facemaptan2[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,0,s,ndofs1d);
                     }
                  }
               }
               break;
            case 2: // EAST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(end-s,i,j,ndofs1d);
                        facemaptan1[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(end,s,j,ndofs1d);
                        facemaptan2[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(end,i,s,ndofs1d);
                     }
                  }
               }
               break;
            case 3: // NORTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,end-s,j,ndofs1d);
                        facemaptan1[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(s,end,j,ndofs1d);
                        facemaptan2[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,end,s,ndofs1d);
                     }
                  }
               }
               break;
            case 4: // WEST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(s,i,j,ndofs1d);
                        facemaptan1[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(0,s,j,ndofs1d);
                        facemaptan2[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(0,i,s,ndofs1d);
                     }
                  }
               }
               break;
            case 5: // TOP
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     for (int s = 0; s < ndofs1d; ++s)
                     {
                        facemapnor[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,j,end-s,ndofs1d);
                        facemaptan1[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(s,j,end,ndofs1d);
                        facemaptan2[s+i*ndofs1d+j*ndofs1d*ndofs1d] = Get_dof_from_ijk(i,s,end,ndofs1d);
                     }
                  }
               }
               break;
            default: 
               MFEM_ABORT("Invalid face_id");
         }
         break;
#ifdef MFEM_DEBUG
         for(int k = 0 ; k < ndofs1d*ndofs1d*ndofs1d ; k++ )
         {
            MFEM_VERIFY( (facemapnor[k] >= ndofs1d*ndofs1d*ndofs1d) or (facemapnor[k] < 0 ) ,
            "Invalid facemapnor values.");
            MFEM_VERIFY( (facemaptan1[k] >= ndofs1d*ndofs1d*ndofs1d) or (facemaptan1[k] < 0 ) ,
            "Invalid facemapnor values.");
            MFEM_VERIFY( (facemaptan2[k] >= ndofs1d*ndofs1d*ndofs1d) or (facemaptan2[k] < 0 ) ,
            "Invalid facemapnor values.");
         }
#endif
   }
}

void GetVectorCoefficients2D(Vector &v1,
                            Vector &v2,
                            Vector &r,
                            Vector &coeffs)
{
   // Solves [v1,v2]*coeffs = r for coeffs

   coeffs(0) = v2(1)*r(0) - v2(0)*r(1);
   coeffs(1) = v1(0)*r(1) - v1(1)*r(0);

#ifdef MFEM_DEBUG
   std::cout << "GetVectorCoefficients2D" << std::endl;
   v1.Print();
   v2.Print();
#endif

   double detLHS = v1(0)*v2(1) - v1(1)*v2(0);

   if( fabs(detLHS) < 1.0e-11 )
   {
      std::cout << "fabs(detLHS) = " << fabs(detLHS) << std::endl;
      v1.Print();
      v2.Print();
      MFEM_ABORT("v1,v2 not linearly independent!");      
   }
   coeffs /= detLHS;
}   

void GetVectorCoefficients3D(Vector &v1,
                            Vector &v2,
                            Vector &v3,
                            Vector &r,
                            Vector &coeffs)
{
   // Solves [v1,v2,v3]*coeffs = r for coeffs

   coeffs(0) = v2(0)*v3(1)*r(2) - v2(0)*v3(2)*r(1) - v2(1)*v3(0)*r(2) + v2(1)*v3(2)*r(0) + v2(2)*v3(0)*r(1) - v2(2)*v3(1)*r(0);
   coeffs(1) = v1(0)*v3(2)*r(1) - v1(0)*v3(1)*r(2) + v1(1)*v3(0)*r(2) - v1(1)*v3(2)*r(0) - v1(2)*v3(0)*r(1) + v1(2)*v3(1)*r(0);
   coeffs(2) = v1(0)*v2(1)*r(2) - v1(0)*v2(2)*r(1) - v1(1)*v2(0)*r(2) + v1(1)*v2(2)*r(0) + v1(2)*v2(0)*r(1) - v1(2)*v2(1)*r(0);

   double detLHS = v1(0)*v2(1)*v3(2) - v1(0)*v2(2)*v3(1) 
                     - v1(1)*v2(0)*v3(2) + v1(1)*v2(2)*v3(0)
                     + v1(2)*v2(0)*v3(1) - v1(2)*v2(1)*v3(0);

#ifdef MFEM_DEBUG
   std::cout << "GetVectorCoefficients3D" << std::endl;
   v1.Print();
   v2.Print();
   v3.Print();
#endif

   if( fabs(detLHS) < 1.0e-11 )
   {
      std::cout << "fabs(detLHS) = " << fabs(detLHS) << std::endl;
      v1.Print();
      v2.Print();
      v3.Print();
      MFEM_ABORT("v1, v2, v3 not linearly independent!");      
   }
   coeffs /= detLHS;
}     

// Generates faceMap, which maps face indices to element indices
// based on Lexicographic ordering
void GetFaceDofs(const int dim, const int face_id,
                 const int ndofs1d, Array<int> &faceMap)
{
   switch (dim)
   {
      case 1:
         switch (face_id)
         {
            case 0: // WEST
               faceMap[0] = 0;
               break;
            case 1: // EAST
               faceMap[0] = ndofs1d-1;
               break;
         }
         break;
      case 2:
         switch (face_id)
         {
            case 0: // SOUTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  faceMap[i] = i;
               }
               break;
            case 1: // EAST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  faceMap[i] = ndofs1d-1 + i*ndofs1d;
               }
               break;
            case 2: // NORTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  faceMap[i] = (ndofs1d-1)*ndofs1d + i;
               }
               break;
            case 3: // WEST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  faceMap[i] = i*ndofs1d;
               }
               break;
         }
         break;
      case 3:
         switch (face_id)
         {
            case 0: // BOTTOM
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     faceMap[i+j*ndofs1d] = i + j*ndofs1d;
                  }
               }
               break;
            case 1: // SOUTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     faceMap[i+j*ndofs1d] = i + j*ndofs1d*ndofs1d;
                  }
               }
               break;
            case 2: // EAST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     faceMap[i+j*ndofs1d] = ndofs1d-1 + i*ndofs1d + j*ndofs1d*ndofs1d;
                  }
               }
               break;
            case 3: // NORTH
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     faceMap[i+j*ndofs1d] = (ndofs1d-1)*ndofs1d + i + j*ndofs1d*ndofs1d;
                  }
               }
               break;
            case 4: // WEST
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     faceMap[i+j*ndofs1d] = i*ndofs1d + j*ndofs1d*ndofs1d;
                  }
               }
               break;
            case 5: // TOP
               for (int i = 0; i < ndofs1d; ++i)
               {
                  for (int j = 0; j < ndofs1d; ++j)
                  {
                     faceMap[i+j*ndofs1d] = (ndofs1d-1)*ndofs1d*ndofs1d + i + j*ndofs1d;
                  }
               }
               break;
         }
         break;
   }
}

H1FaceRestriction::H1FaceRestriction(const FiniteElementSpace &fes,
                                     const ElementDofOrdering e_ordering,
                                     const FaceType type)
   : fes(fes),
     nf(fes.GetNFbyType(type)),
     vdim(fes.GetVDim()),
     byvdim(fes.GetOrdering() == Ordering::byVDIM),
     ndofs(fes.GetNDofs()),
     dof(nf > 0 ? fes.GetFaceElement(0)->GetDof() : 0),
     nfdofs(nf*dof),
     scatter_indices(nf*dof),
     offsets(ndofs+1),
     gather_indices(nf*dof)
{
   if (nf==0) { return; }

#ifdef MFEM_USE_MPI

   // If the underlying finite element space is parallel, ensure the face
   // neighbor information is generated.
   if (const ParFiniteElementSpace *pfes
       = dynamic_cast<const ParFiniteElementSpace*>(&fes))
   {
      pfes->GetParMesh()->ExchangeFaceNbrData();
   }

#endif

   // If fespace == H1
   const FiniteElement *fe = fes.GetFE(0);
   const TensorBasisElement *tfe = dynamic_cast<const TensorBasisElement*>(fe);
   MFEM_VERIFY(tfe != NULL &&
               (tfe->GetBasisType()==BasisType::GaussLobatto ||
                tfe->GetBasisType()==BasisType::Positive),
               "Only Gauss-Lobatto and Bernstein basis are supported in "
               "H1FaceRestriction.");
   MFEM_VERIFY(fes.GetMesh()->Conforming(),
               "Non-conforming meshes not yet supported with partial assembly.");
   // Assuming all finite elements are using Gauss-Lobatto.
   height = vdim*nf*dof;
   width = fes.GetVSize();
   const bool dof_reorder = (e_ordering == ElementDofOrdering::LEXICOGRAPHIC);
   if (dof_reorder && nf > 0)
   {
      for (int f = 0; f < fes.GetNF(); ++f)
      {
         const FiniteElement *fe = fes.GetFaceElement(f);
         const TensorBasisElement* el =
            dynamic_cast<const TensorBasisElement*>(fe);
         if (el) { continue; }
         mfem_error("Finite element not suitable for lexicographic ordering");
      }
      const FiniteElement *fe = fes.GetFaceElement(0);
      const TensorBasisElement* el =
         dynamic_cast<const TensorBasisElement*>(fe);
      const Array<int> &fe_dof_map = el->GetDofMap();
      MFEM_VERIFY(fe_dof_map.Size() > 0, "invalid dof map");
   }
   const TensorBasisElement* el =
      dynamic_cast<const TensorBasisElement*>(fe);
   const int *dof_map = el->GetDofMap().GetData();
   const Table& e2dTable = fes.GetElementToDofTable();
   const int* elementMap = e2dTable.GetJ();
   Array<int> faceMap(dof);
   int e1, e2;
   int inf1, inf2;
   int face_id;
   int orientation;
   const int ndofs1d = fes.GetFE(0)->GetOrder()+1;
   const int elem_dofs = fes.GetFE(0)->GetDof();
   const int dim = fes.GetMesh()->SpaceDimension();
   // Computation of scatter_indices
   int f_ind = 0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      orientation = inf1 % 64;
      face_id = inf1 / 64;
      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (type==FaceType::Boundary && e2<0 && inf2<0) )
      {
         // Assumes Gauss-Lobatto basis
         if (dof_reorder)
         {
            if (orientation != 0)
            {
               mfem_error("FaceRestriction used on degenerated mesh.");
            }
            GetFaceDofs(dim, face_id, ndofs1d, faceMap); // Only for hex
         }
         else
         {
            mfem_error("FaceRestriction not yet implemented for this type of "
                       "element.");
            // TODO Something with GetFaceDofs?
         }
         for (int d = 0; d < dof; ++d)
         {
            const int face_dof = faceMap[d];
            const int did = (!dof_reorder)?face_dof:dof_map[face_dof];
            const int gid = elementMap[e1*elem_dofs + did];
            const int lid = dof*f_ind + d;
            scatter_indices[lid] = gid;
         }
         f_ind++;
      }
   }
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   // Computation of gather_indices
   for (int i = 0; i <= ndofs; ++i)
   {
      offsets[i] = 0;
   }
   f_ind = 0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      orientation = inf1 % 64;
      face_id = inf1 / 64;
      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (type==FaceType::Boundary && e2<0 && inf2<0) )
      {
         GetFaceDofs(dim, face_id, ndofs1d, faceMap);
         for (int d = 0; d < dof; ++d)
         {
            const int face_dof = faceMap[d];
            const int did = (!dof_reorder)?face_dof:dof_map[face_dof];
            const int gid = elementMap[e1*elem_dofs + did];
            ++offsets[gid + 1];
         }
         f_ind++;
      }
   }
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   for (int i = 1; i <= ndofs; ++i)
   {
      offsets[i] += offsets[i - 1];
   }
   f_ind = 0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      orientation = inf1 % 64;
      face_id = inf1 / 64;
      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (type==FaceType::Boundary && e2<0 && inf2<0) )
      {
         GetFaceDofs(dim, face_id, ndofs1d, faceMap);
         for (int d = 0; d < dof; ++d)
         {
            const int face_dof = faceMap[d];
            const int did = (!dof_reorder)?face_dof:dof_map[face_dof];
            const int gid = elementMap[e1*elem_dofs + did];
            const int lid = dof*f_ind + d;
            gather_indices[offsets[gid]++] = lid;
         }
         f_ind++;
      }
   }
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   for (int i = ndofs; i > 0; --i)
   {
      offsets[i] = offsets[i - 1];
   }
   offsets[0] = 0;
}

void H1FaceRestriction::Mult(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_indices = scatter_indices.Read();
   auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
   auto d_y = Reshape(y.Write(), nd, vd, nf);
   MFEM_FORALL(i, nfdofs,
   {
      const int idx = d_indices[i];
      const int dof = i % nd;
      const int face = i / nd;
      for (int c = 0; c < vd; ++c)
      {
         d_y(dof, c, face) = d_x(t?c:idx, t?idx:c);
      }
   });
}

void H1FaceRestriction::AddMultTranspose(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   auto d_offsets = offsets.Read();
   auto d_indices = gather_indices.Read();
   auto d_x = Reshape(x.Read(), nd, vd, nf);
   auto d_y = Reshape(y.ReadWrite(), t?vd:ndofs, t?ndofs:vd);
   MFEM_FORALL(i, ndofs,
   {
      const int offset = d_offsets[i];
      const int nextOffset = d_offsets[i + 1];
      for (int c = 0; c < vd; ++c)
      {
         double dofValue = 0;
         for (int j = offset; j < nextOffset; ++j)
         {
            const int idx_j = d_indices[j];
            dofValue +=  d_x(idx_j % nd, c, idx_j / nd);
         }
         d_y(t?c:i,t?i:c) += dofValue;
      }
   });
}

static int ToLexOrdering2D(const int face_id, const int size1d, const int i)
{
   if (face_id==2 || face_id==3)
   {
      return size1d-1-i;
   }
   else
   {
      return i;
   }
}

static int PermuteFace2D(const int face_id1, const int face_id2,
                         const int orientation,
                         const int size1d, const int index)
{
   int new_index;
   // Convert from lex ordering
   if (face_id1==2 || face_id1==3)
   {
      new_index = size1d-1-index;
   }
   else
   {
      new_index = index;
   }
   // Permute based on face orientations
   if (orientation==1)
   {
      new_index = size1d-1-new_index;
   }
   return ToLexOrdering2D(face_id2, size1d, new_index);
}

/*
static int PermuteDFaceDNorm2D(const int face_id1, const int face_id2,
                                 const int orientation,
                                 const int size1d, const int index)
{
   int new_index;
   // Convert from lex ordering
   if (face_id1==2 || face_id1==3)
   {
      new_index = size1d-1-index;
   }
   else
   {
      new_index = index;
   }
   // Permute based on face orientations
   if (orientation==1)
   {
      new_index = size1d-1-new_index;
   }
   return ToLexOrdering2D(face_id2, size1d, new_index);
}
*/

static int ToLexOrdering3D(const int face_id, const int size1d, const int i,
                           const int j)
{
   if (face_id==2 || face_id==1 || face_id==5)
   {
      return i + j*size1d;
   }
   else if (face_id==3 || face_id==4)
   {
      return (size1d-1-i) + j*size1d;
   }
   else // face_id==0
   {
      return i + (size1d-1-j)*size1d;
   }
}

static int PermuteFace3D(const int face_id1, const int face_id2,
                         const int orientation,
                         const int size1d, const int index)
{
   int i=0, j=0, new_i=0, new_j=0;
   i = index%size1d;
   j = index/size1d;
   // Convert from lex ordering
   if (face_id1==3 || face_id1==4)
   {
      i = size1d-1-i;
   }
   else if (face_id1==0)
   {
      j = size1d-1-j;
   }
   // Permute based on face orientations
   switch (orientation)
   {
      case 0:
         new_i = i;
         new_j = j;
         break;
      case 1:
         new_i = j;
         new_j = i;
         break;
      case 2:
         new_i = j;
         new_j = (size1d-1-i);
         break;
      case 3:
         new_i = (size1d-1-i);
         new_j = j;
         break;
      case 4:
         new_i = (size1d-1-i);
         new_j = (size1d-1-j);
         break;
      case 5:
         new_i = (size1d-1-j);
         new_j = (size1d-1-i);
         break;
      case 6:
         new_i = (size1d-1-j);
         new_j = i;
         break;
      case 7:
         new_i = i;
         new_j = (size1d-1-j);
         break;
   }
   return ToLexOrdering3D(face_id2, size1d, new_i, new_j);
}

// Permute dofs or quads on a face for e2 to match with the ordering of e1
int PermuteFaceL2(const int dim, const int face_id1,
                  const int face_id2, const int orientation,
                  const int size1d, const int index)
{

   return index;

   switch (dim)
   {
      case 1:
         return 0;
      case 2:
         return PermuteFace2D(face_id1, face_id2, orientation, size1d, index);
      case 3:
         return PermuteFace3D(face_id1, face_id2, orientation, size1d, index);
      default:
         mfem_error("Unsupported dimension.");
         return 0;
   }
}

int PermuteFaceNormL2(const int dim, const int face_id1,
                  const int face_id2, const int orientation,
                  const int size1d, const int index)
{
   return index;

   switch (dim)
   {
      case 1:
         return 0;
      case 2:
         switch (face_id1)
         {
            case 1:
            case 2:
               return (size1d-1) - index;
            case 0:
            case 3:
               return index;
            default:
               mfem_error("Invalid face_id1");
         }
      case 3:
         switch (face_id1)
         {
            case 2:
            case 3:
            case 5:
               return (size1d-1) - index;
            case 0:
            case 1:
            case 4:
               return index;
            default:
               mfem_error("Invalid face_id1");
         }
      default:
         mfem_error("Unsupported dimension.");
         return 0;
   }
}

L2FaceRestriction::L2FaceRestriction(const FiniteElementSpace &fes,
                                     const FaceType type,
                                     const L2FaceValues m)
   : fes(fes),
     nf(fes.GetNFbyType(type)),
     ne(fes.GetNE()),
     vdim(fes.GetVDim()),
     byvdim(fes.GetOrdering() == Ordering::byVDIM),
     ndofs(fes.GetNDofs()),
     dof(nf > 0 ?
         fes.GetTraceElement(0, fes.GetMesh()->GetFaceBaseGeometry(0))->GetDof()
         : 0),
     elemDofs(fes.GetFE(0)->GetDof()),
     m(m),
     nfdofs(nf*dof),
     scatter_indices1(nf*dof),
     scatter_indices2(m==L2FaceValues::DoubleValued?nf*dof:0),
     offsets(ndofs+1),
     gather_indices((m==L2FaceValues::DoubleValued? 2 : 1)*nf*dof)
{
}

L2FaceRestriction::L2FaceRestriction(const FiniteElementSpace &fes,
                                     const ElementDofOrdering e_ordering,
                                     const FaceType type,
                                     const L2FaceValues m)
   : L2FaceRestriction(fes, type, m)
{

   // If fespace == L2
   const FiniteElement *fe = fes.GetFE(0);
   const TensorBasisElement *tfe = dynamic_cast<const TensorBasisElement*>(fe);
   MFEM_VERIFY(tfe != NULL &&
               (tfe->GetBasisType()==BasisType::GaussLobatto ||
                tfe->GetBasisType()==BasisType::Positive),
               "Only Gauss-Lobatto and Bernstein basis are supported in "
               "L2FaceRestriction.");
   MFEM_VERIFY(fes.GetMesh()->Conforming(),
               "Non-conforming meshes not yet supported with partial assembly.");
   if (nf==0) { return; }
   height = (m==L2FaceValues::DoubleValued? 2 : 1)*vdim*nf*dof;
   width = fes.GetVSize();
   const bool dof_reorder = (e_ordering == ElementDofOrdering::LEXICOGRAPHIC);
   if (!dof_reorder)
   {
      mfem_error("Non-Tensor L2FaceRestriction not yet implemented.");
   }
   if (dof_reorder && nf > 0)
   {
      for (int f = 0; f < fes.GetNF(); ++f)
      {
         const FiniteElement *fe =
            fes.GetTraceElement(f, fes.GetMesh()->GetFaceBaseGeometry(f));
         const TensorBasisElement* el =
            dynamic_cast<const TensorBasisElement*>(fe);
         if (el) { continue; }
         mfem_error("Finite element not suitable for lexicographic ordering");
      }
   }
   const Table& e2dTable = fes.GetElementToDofTable();
   const int* elementMap = e2dTable.GetJ();
   Array<int> faceMap1(dof), faceMap2(dof);
   int e1, e2;
   int inf1, inf2;
   int face_id1 = -1, face_id2 = -1;
   int orientation = -1;
   const int ndofs1d = fes.GetFE(0)->GetOrder()+1;
   const int elem_dofs = fes.GetFE(0)->GetDof();
   const int dim = fes.GetMesh()->SpaceDimension();
   // Computation of scatter indices
   int f_ind=0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      if (dof_reorder)
      {
         orientation = inf1 % 64; // why 64?
         face_id1 = inf1 / 64;
         GetFaceDofs(dim, face_id1, ndofs1d, faceMap1); // Only for hex
         orientation = inf2 % 64;
         face_id2 = inf2 / 64;
         GetFaceDofs(dim, face_id2, ndofs1d, faceMap2); // Only for hex
      }
      else
      {
         mfem_error("FaceRestriction not yet implemented for this type of "
                    "element.");
         // TODO Something with GetFaceDofs?
      }
      if ((type==FaceType::Interior && e2>=0) ||
          (type==FaceType::Boundary && e2<0))
      {
         for (int d = 0; d < dof; ++d)
         {
            const int face_dof = faceMap1[d];
            const int did = face_dof;
            const int gid = elementMap[e1*elem_dofs + did];
            const int lid = dof*f_ind + d;
            scatter_indices1[lid] = gid;
         }
         if (m==L2FaceValues::DoubleValued)
         {
            for (int d = 0; d < dof; ++d)
            {
               if (type==FaceType::Interior && e2>=0) // interior face
               {
                  const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                               orientation, ndofs1d, d);
                  const int face_dof = faceMap2[pd];
                  const int did = face_dof;
                  const int gid = elementMap[e2*elem_dofs + did];
                  const int lid = dof*f_ind + d;
                  scatter_indices2[lid] = gid;
               }
               else if (type==FaceType::Boundary && e2<0) // true boundary face
               {
                  const int lid = dof*f_ind + d;
                  scatter_indices2[lid] = -1;
               }
            }
         }
         f_ind++;
      }
   }
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   // Computation of gather_indices
   for (int i = 0; i <= ndofs; ++i)
   {
      offsets[i] = 0;
   }
   f_ind = 0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (type==FaceType::Boundary && e2<0 && inf2<0) )
      {
         orientation = inf1 % 64;
         face_id1 = inf1 / 64;
         GetFaceDofs(dim, face_id1, ndofs1d, faceMap1);
         orientation = inf2 % 64;
         face_id2 = inf2 / 64;
         GetFaceDofs(dim, face_id2, ndofs1d, faceMap2);

         for (int d = 0; d < dof; ++d)
         {
            const int did = faceMap1[d];
            const int gid = elementMap[e1*elem_dofs + did];
            ++offsets[gid + 1];
         }
         if (m==L2FaceValues::DoubleValued)
         {
            for (int d = 0; d < dof; ++d)
            {
               if (type==FaceType::Interior && e2>=0) // interior face
               {
                  const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                               orientation, ndofs1d, d);
                  const int did = faceMap2[pd];
                  const int gid = elementMap[e2*elem_dofs + did];
                  ++offsets[gid + 1];
               }
            }
         }
         f_ind++;
      }
   }
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   for (int i = 1; i <= ndofs; ++i)
   {
      offsets[i] += offsets[i - 1];
   }
   f_ind = 0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (type==FaceType::Boundary && e2<0 && inf2<0) )
      {
         orientation = inf1 % 64;
         face_id1 = inf1 / 64;
         GetFaceDofs(dim, face_id1, ndofs1d, faceMap1);
         orientation = inf2 % 64;
         face_id2 = inf2 / 64;
         GetFaceDofs(dim, face_id2, ndofs1d, faceMap2);
         for (int d = 0; d < dof; ++d)
         {
            const int did = faceMap1[d];
            const int gid = elementMap[e1*elem_dofs + did];
            const int lid = dof*f_ind + d;
            // We don't shift lid to express that it's e1 of f
            gather_indices[offsets[gid]++] = lid;
         }
         if (m==L2FaceValues::DoubleValued)
         {
            for (int d = 0; d < dof; ++d)
            {
               if (type==FaceType::Interior && e2>=0) // interior face
               {
                  const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                               orientation, ndofs1d, d);
                  const int did = faceMap2[pd];
                  const int gid = elementMap[e2*elem_dofs + did];
                  const int lid = dof*f_ind + d;
                  // We shift lid to express that it's e2 of f
                  gather_indices[offsets[gid]++] = nfdofs + lid;
               }
            }
         }
         f_ind++;
      }
   }
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   for (int i = ndofs; i > 0; --i)
   {
      offsets[i] = offsets[i - 1];
   }
   offsets[0] = 0;

   std::cout << " scatter_indices1 " << std::endl;
   for (int i = 0; i < scatter_indices1.Size() ; i++)
   {
      std::cout << i << " : " << 
      scatter_indices1[i] << std::endl;
   }   
   std::cout << " scatter_indices2 " << std::endl;
   for (int i = 0; i < scatter_indices2.Size() ; i++)
   {
      std::cout << i << " : " << 
      scatter_indices2[i] << std::endl;
   }   
   std::cout << " offsets post " << std::endl;
   for (int i = 0; i < ndofs+1; i++)
   {
      std::cout << i << " : " << 
      offsets[i]  << std::endl;
   }
   std::cout << " gather_indices " << std::endl;
   for (int i = 0; i < gather_indices.Size() ; i++)
   {
      std::cout << i << " : " << 
      gather_indices[i] << std::endl;
   }
   std::cout << " done " << std::endl;
   //exit(1);

}

void L2FaceRestriction::Mult(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;

   if (m==L2FaceValues::DoubleValued)
   {
      auto d_indices1 = scatter_indices1.Read();
      auto d_indices2 = scatter_indices2.Read();
      auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
      auto d_y = Reshape(y.Write(), nd, vd, 2, nf);
      MFEM_FORALL(i, nfdofs,
      {
         const int dof = i % nd;
         const int face = i / nd;
         const int idx1 = d_indices1[i];
         for (int c = 0; c < vd; ++c)
         {
            d_y(dof, c, 0, face) = d_x(t?c:idx1, t?idx1:c);
         }
         const int idx2 = d_indices2[i];
         for (int c = 0; c < vd; ++c)
         {
            d_y(dof, c, 1, face) = idx2==-1 ? 0.0 : d_x(t?c:idx2, t?idx2:c);
         }
      });
   }
   else
   {
      auto d_indices1 = scatter_indices1.Read();
      auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
      auto d_y = Reshape(y.Write(), nd, vd, nf);
      MFEM_FORALL(i, nfdofs,
      {
         const int dof = i % nd;
         const int face = i / nd;
         const int idx1 = d_indices1[i];
         for (int c = 0; c < vd; ++c)
         {
            d_y(dof, c, face) = d_x(t?c:idx1, t?idx1:c);
         }
      });
   }
}

void L2FaceRestriction::AddMultTranspose(const Vector& x, Vector& y) const
{
   // Assumes all elements have the same number of dofs
   const int nd = dof;
   const int vd = vdim;
   const bool t = byvdim;
   const int dofs = nfdofs;
   auto d_offsets = offsets.Read();
   auto d_indices = gather_indices.Read();

   if (m == L2FaceValues::DoubleValued)
   {
      auto d_x = Reshape(x.Read(), nd, vd, 2, nf);
      auto d_y = Reshape(y.ReadWrite(), t?vd:ndofs, t?ndofs:vd);
      MFEM_FORALL(i, ndofs,
      {
         const int offset = d_offsets[i];
         const int nextOffset = d_offsets[i + 1];
         for (int c = 0; c < vd; ++c)
         {
            double dofValue = 0;
            for (int j = offset; j < nextOffset; ++j)
            {
               int idx_j = d_indices[j];
               bool isE1 = idx_j < dofs;
               idx_j = isE1 ? idx_j : idx_j - dofs;
               dofValue +=  isE1 ?
               d_x(idx_j % nd, c, 0, idx_j / nd)
               :d_x(idx_j % nd, c, 1, idx_j / nd);
            }
            d_y(t?c:i,t?i:c) += dofValue;
         }
      });
   }
   else
   {
      auto d_x = Reshape(x.Read(), nd, vd, nf);
      auto d_y = Reshape(y.ReadWrite(), t?vd:ndofs, t?ndofs:vd);
      MFEM_FORALL(i, ndofs,
      {
         const int offset = d_offsets[i];
         const int nextOffset = d_offsets[i + 1];
         for (int c = 0; c < vd; ++c)
         {
            double dofValue = 0;
            for (int j = offset; j < nextOffset; ++j)
            {
               int idx_j = d_indices[j];
               dofValue +=  d_x(idx_j % nd, c, idx_j / nd);
            }
            d_y(t?c:i,t?i:c) += dofValue;
         }
      });
   }
}

void L2FaceRestriction::FillI(SparseMatrix &mat,
                              const bool keep_nbr_block) const
{
   const int face_dofs = dof;
   auto d_indices1 = scatter_indices1.Read();
   auto d_indices2 = scatter_indices2.Read();
   auto I = mat.ReadWriteI();
   MFEM_FORALL(fdof, nf*face_dofs,
   {
      const int iE1 = d_indices1[fdof];
      const int iE2 = d_indices2[fdof];
      AddNnz(iE1,I,face_dofs);
      AddNnz(iE2,I,face_dofs);
   });
}

void L2FaceRestriction::FillJAndData(const Vector &ea_data,
                                     SparseMatrix &mat,
                                     const bool keep_nbr_block) const
{
   const int face_dofs = dof;
   auto d_indices1 = scatter_indices1.Read();
   auto d_indices2 = scatter_indices2.Read();
   auto I = mat.ReadWriteI();
   auto mat_fea = Reshape(ea_data.Read(), face_dofs, face_dofs, 2, nf);
   auto J = mat.WriteJ();
   auto Data = mat.WriteData();
   MFEM_FORALL(fdof, nf*face_dofs,
   {
      const int f  = fdof/face_dofs;
      const int iF = fdof%face_dofs;
      const int iE1 = d_indices1[f*face_dofs+iF];
      const int iE2 = d_indices2[f*face_dofs+iF];
      const int offset1 = AddNnz(iE1,I,face_dofs);
      const int offset2 = AddNnz(iE2,I,face_dofs);
      for (int jF = 0; jF < face_dofs; jF++)
      {
         const int jE1 = d_indices1[f*face_dofs+jF];
         const int jE2 = d_indices2[f*face_dofs+jF];
         J[offset2+jF] = jE1;
         J[offset1+jF] = jE2;
         Data[offset2+jF] = mat_fea(jF,iF,0,f);
         Data[offset1+jF] = mat_fea(jF,iF,1,f);
      }
   });
}

void L2FaceRestriction::AddFaceMatricesToElementMatrices(Vector &fea_data,
                                                         Vector &ea_data) const
{
   const int face_dofs = dof;
   const int elem_dofs = elemDofs;
   const int NE = ne;
   if (m==L2FaceValues::DoubleValued)
   {
      auto d_indices1 = scatter_indices1.Read();
      auto d_indices2 = scatter_indices2.Read();
      auto mat_fea = Reshape(fea_data.Read(), face_dofs, face_dofs, 2, nf);
      auto mat_ea = Reshape(ea_data.ReadWrite(), elem_dofs, elem_dofs, ne);
      MFEM_FORALL(f, nf,
      {
         const int e1 = d_indices1[f*face_dofs]/elem_dofs;
         const int e2 = d_indices2[f*face_dofs]/elem_dofs;
         for (int j = 0; j < face_dofs; j++)
         {
            const int jB1 = d_indices1[f*face_dofs+j]%elem_dofs;
            for (int i = 0; i < face_dofs; i++)
            {
               const int iB1 = d_indices1[f*face_dofs+i]%elem_dofs;
               AtomicAdd(mat_ea(iB1,jB1,e1), mat_fea(i,j,0,f));
            }
         }
         if (e2 < NE)
         {
            for (int j = 0; j < face_dofs; j++)
            {
               const int jB2 = d_indices2[f*face_dofs+j]%elem_dofs;
               for (int i = 0; i < face_dofs; i++)
               {
                  const int iB2 = d_indices2[f*face_dofs+i]%elem_dofs;
                  AtomicAdd(mat_ea(iB2,jB2,e2), mat_fea(i,j,1,f));
               }
            }
         }
      });
   }
   else
   {
      auto d_indices = scatter_indices1.Read();
      auto mat_fea = Reshape(fea_data.Read(), face_dofs, face_dofs, nf);
      auto mat_ea = Reshape(ea_data.ReadWrite(), elem_dofs, elem_dofs, ne);
      MFEM_FORALL(f, nf,
      {
         const int e = d_indices[f*face_dofs]/elem_dofs;
         for (int j = 0; j < face_dofs; j++)
         {
            const int jE = d_indices[f*face_dofs+j]%elem_dofs;
            for (int i = 0; i < face_dofs; i++)
            {
               const int iE = d_indices[f*face_dofs+i]%elem_dofs;
               AtomicAdd(mat_ea(iE,jE,e), mat_fea(i,j,f));
            }
         }
      });
   }
}

int ToLexOrdering(const int dim, const int face_id, const int size1d,
                  const int index)
{
   switch (dim)
   {
      case 1:
         return 0;
      case 2:
         return ToLexOrdering2D(face_id, size1d, index);
      case 3:
         return ToLexOrdering3D(face_id, size1d, index%size1d, index/size1d);
      default:
         mfem_error("Unsupported dimension.");
         return 0;
   }
}

L2FaceNormalDRestriction::L2FaceNormalDRestriction(const FiniteElementSpace &fes,
                                     const FaceType type,
                                     const L2FaceValues m)
   : fes(fes),
     dim(fes.GetMesh()->SpaceDimension()),
     nf(fes.GetNFbyType(type)),
     ne(fes.GetNE()),
     vdim(fes.GetVDim()),
     byvdim(fes.GetOrdering() == Ordering::byVDIM),
     ndofs(fes.GetNDofs()),
     ndofs1d(fes.GetFE(0)->GetOrder()+1),
     ndofs_face(nf > 0 ?
                fes.GetTraceElement(0, fes.GetMesh()->GetFaceBaseGeometry(0))->GetDof()
                : 0),
     //nfdofs(ndofs_face*nf),
     elemDofs(fes.GetFE(0)->GetDof()),
     m(m),
     numfacedofs(nf*ndofs_face),
     num_values_per_point(2),
     num_faces_per_element( 2*dim ),
     scatter_indices(nf*ndofs_face),
     scatter_indices_nor(nf*ndofs_face*ndofs1d),
     scatter_indices_tan1(nf*ndofs_face*ndofs1d),
     scatter_indices_tan2(nf*ndofs_face*ndofs1d),
     scatter_indices_neighbor(m==L2FaceValues::DoubleValued?nf*ndofs_face:0),
     scatter_indices_neighbor_nor(m==L2FaceValues::DoubleValued?nf*ndofs_face*ndofs1d:0),
     scatter_indices_neighbor_tan1(m==L2FaceValues::DoubleValued?nf*ndofs_face*ndofs1d:0),
     scatter_indices_neighbor_tan2(m==L2FaceValues::DoubleValued?nf*ndofs_face*ndofs1d:0),
     offsets(ndofs+1),
     offsets_nor(ndofs+1),
     offsets_tan1(ndofs+1),
     offsets_tan2(ndofs+1),
     gather_indices((m==L2FaceValues::DoubleValued? 2 : 1)*nf*ndofs_face),
     gather_indices_nor((m==L2FaceValues::DoubleValued? 2 : 1)*nf*ndofs_face*ndofs1d),
     gather_indices_tan1((m==L2FaceValues::DoubleValued? 2 : 1)*nf*ndofs_face*ndofs1d),
     gather_indices_tan2((m==L2FaceValues::DoubleValued? 2 : 1)*nf*ndofs_face*ndofs1d),
     map_elements_to_faces(ne*num_faces_per_element),
     map_elements_to_sides(ne*num_faces_per_element),
     map_side_permutations(ndofs_face*nf),
     needed_elements(ne)
{

#ifdef MFEM_DEBUG   

   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
   for (int i = 0; i < nf*ndofs_face ; i++ )
   {
     scatter_indices[i] = 123456789;
     scatter_indices_neighbor[i] = 123456789;
   } 
   for (int i = 0; i < nf*ndofs_face*ndofs1d ; i++ )
   {
     scatter_indices_nor[i] = 123456789;
     scatter_indices_tan1[i] = 123456789;
     scatter_indices_tan2[i] = 123456789;
     scatter_indices_neighbor_nor[i] = 123456789;
     scatter_indices_neighbor_tan1[i] = 123456789;
     scatter_indices_neighbor_tan2[i] = 123456789;
   } 
   for (int i = 0; i < ndofs+1 ; i++ )
   {
      offsets[i] = 123456789;
      offsets_nor[i] = 123456789;
      offsets_tan1[i] = 123456789;
      offsets_tan2[i] = 123456789;
   }
   for (int i = 0; i < gather_indices.Size()  ; i++ )
   {
      gather_indices[i] = 123456789;
   }
   for (int i = 0; i < gather_indices_nor.Size()  ; i++ )
   {
     gather_indices_nor[i] = 123456789;
     gather_indices_tan1[i] = 123456789;
     gather_indices_tan2[i] = 123456789;
   }
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

#endif

}

L2FaceNormalDRestriction::L2FaceNormalDRestriction(const FiniteElementSpace &fes,
                                     const ElementDofOrdering e_ordering,
                                     const FaceType type,
                                     //const IntegrationRule* ir,
                                     const L2FaceValues m)
   : L2FaceNormalDRestriction(fes, type, m)
{
   const FiniteElement *fe = fes.GetFE(0);
   const TensorBasisElement *tfe = dynamic_cast<const TensorBasisElement*>(fe);

   MFEM_VERIFY(tfe != NULL, 
               "Element type incompatible with partial assembly. ");
   MFEM_VERIFY(tfe->GetBasisType()==BasisType::GaussLobatto ||
               tfe->GetBasisType()==BasisType::Positive,
               "Only Gauss-Lobatto and Bernstein basis are supported in "
               "L2FaceNormalDRestriction.");
   MFEM_VERIFY(fes.GetMesh()->Conforming(),
               "Non-conforming meshes not yet supported with partial assembly.");

   if (nf==0) { return; }

   // Operator parameters
   // double valued? * vector dims * face dofs * stencil dofs * num derivatives
   height = (m==L2FaceValues::DoubleValued? 2 : 1)*vdim*numfacedofs*num_values_per_point;

   width = fes.GetVSize();

   const bool dof_reorder = (e_ordering == ElementDofOrdering::LEXICOGRAPHIC);
   if (!dof_reorder)
   {
      mfem_error("Non-Tensor L2FaceRestriction not yet implemented.");
   }
   if (dof_reorder && nf > 0)
   {
      for (int f = 0; f < fes.GetNF(); ++f)
      {
         const FiniteElement *fe =
            fes.GetTraceElement(f, fes.GetMesh()->GetFaceBaseGeometry(f));
         const TensorBasisElement* el =
            dynamic_cast<const TensorBasisElement*>(fe);
         if (el) { continue; }
         mfem_error("Finite element not suitable for lexicographic ordering");
      }
   }

   const Table& e2dTable = fes.GetElementToDofTable();
   const int* elementMap = e2dTable.GetJ();
   //elementMap = e2dTable.GetJ();
   Array<int> facemapnorself(ndofs_face*ndofs1d), facemapnorneighbor(ndofs_face*ndofs1d);
   Array<int> facemaptan1self(ndofs_face*ndofs1d), facemaptan2self(ndofs_face*ndofs1d);
   Array<int> facemaptan1neighbor(ndofs_face*ndofs1d), facemaptan2neighbor(ndofs_face*ndofs1d);
   int e1, e2;
   int inf1, inf2;
   int face_id1, face_id2;
   int orientation1;
   int orientation2;
   const int elem_dofs = fes.GetFE(0)->GetDof();

   Vector bf;
   Vector gf_nor;
   Vector gf_tan1;
   Vector gf_tan2;
   // Initialize face restriction operators
   bf.SetSize(ndofs1d); 
   gf_nor.SetSize(ndofs1d);
   gf_tan1.SetSize(ndofs1d);
   gf_tan2.SetSize(ndofs1d);
   // Needs to be ndofs1d* dofs_per_face *nf
   Bf.SetSize( ndofs1d*ndofs_face*nf*num_values_per_point, Device::GetMemoryType());
   Gf.SetSize( ndofs1d*ndofs_face*nf*num_values_per_point*3, Device::GetMemoryType());
   bf = 0.;
   gf_nor = 0.;
   gf_tan1 = 0.;
   gf_tan2 = 0.;
   Bf = 0.;
   Gf = 0.;
   IntegrationPoint zero;
   double zeropt[1] = {0};
   zero.Set(zeropt,1);
   auto u_face    = Reshape(Bf.Write(), ndofs1d, ndofs_face, nf, num_values_per_point);
   auto dudn_face = Reshape(Gf.Write(), ndofs1d, ndofs_face, nf, num_values_per_point, 3);

   // re-implementation
   Ge.SetSize(ndofs1d*ndofs1d*ne, Device::GetMemoryType());
   jac_face_factors.SetSize(dim*ndofs_face*num_faces_per_element*ne, Device::GetMemoryType());
   map_elements_to_faces = -1;
   map_elements_to_sides = -1;
   map_side_permutations = -1;
   Ge.SetSize( ndofs1d*ne , Device::GetMemoryType());
   Ge = 0.;
   auto du_element = Reshape(Ge.Write(), ndofs1d, ndofs1d, ne);
   auto jac_face_factor = Reshape( jac_face_factors.Write(), dim, ndofs_face, num_faces_per_element, ne);

   // Computation of scatter indices
   int f_ind=0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);
      if (dof_reorder)
      {
         fes.GetMesh()->GetFaceOrientations(f, &orientation1, &orientation2);
         fes.GetMesh()->GetFaceIds(f, &face_id1, &face_id2);
         Array<int> V, E, Eo;
         fes.GetMesh()->GetElementVertices(e1, V);
         fes.GetMesh()->GetElementEdges(e1, E, Eo);
         GetGradFaceDofStencil(dim, face_id1, ndofs1d, facemapnorself, facemaptan1self, facemaptan2self);
         GetGradFaceDofStencil(dim, face_id2, ndofs1d, facemapnorneighbor, facemaptan1neighbor, facemaptan2neighbor);
         orientation1 = Eo[face_id1];
         orientation2 = Eo[face_id2];
      }
      else
      {
         mfem_error("FaceRestriction not yet implemented for this type of "
                    "element.");
      }

      bool int_face_match = type==FaceType::Interior && e2>=0;
      bool bdy_face_match = type==FaceType::Boundary && e2<0;

      if ( int_face_match || bdy_face_match )
      {

         FaceElementTransformations &Trans0 =
         *fes.GetMesh()->GetFaceElementTransformations(f);

         const FiniteElement &el1 =
         *fes.GetTraceElement(e1, fes.GetMesh()->GetFaceBaseGeometry(f));
         const FiniteElement &el2 =
         *fes.GetTraceElement(e2, fes.GetMesh()->GetFaceBaseGeometry(f));

         const FiniteElement &elf1 = *fes.GetFE(e1);
         const FiniteElement &elf2 = *fes.GetFE(e2);

         const TensorBasisElement* telf1 = dynamic_cast<const TensorBasisElement*>(fes.GetFE(e1));
         const TensorBasisElement* telf2 = dynamic_cast<const TensorBasisElement*>(fes.GetFE(e1));

         Poly_1D::Basis el1_basis1d = telf1->GetBasis1D();
         Poly_1D::Basis el2_basis1d = telf2->GetBasis1D();

         el1_basis1d.Eval(zero.x, bf, gf_nor);

         gf_nor *= -1.0;

/*
         if (ir == NULL)
         {
            int order;
            if (int_face_match)
            {
               order = 2*std::max(el1.GetOrder(), el2.GetOrder());
            }
            else
            {
               order = 2*el1.GetOrder();
            }
            ir = &IntRules.Get(Trans0.GetGeometryType(), order);
         }
         */

         // Get Gauss-Lobatto integration points
         IntegrationRule ir_glob_1d;
         QuadratureFunctions1D quad;
         quad.GaussLobatto(1+el1.GetOrder(),&ir_glob_1d);

         IntegrationRule ir_glob_face;
         IntegrationRule ir_glob_element;
         if( dim == 2 )
         {
            ir_glob_face = IntegrationRule(ir_glob_1d);         
            ir_glob_element = IntegrationRule(ir_glob_1d, ir_glob_1d);         
         }
         else if( dim == 3)
         {
            ir_glob_face = IntegrationRule(ir_glob_1d, ir_glob_1d);         
            ir_glob_element = IntegrationRule(ir_glob_1d, ir_glob_1d, ir_glob_1d);
         }

         // Initialize face restriction operators
/*
         Vector locR;
         locR.SetSize(ndofs1d);

         Vector locR2;
         locR2.SetSize(ndofs1d);
*/
         //int NPe = ir_glob_element.GetNPoints();
         int NPf = ir_glob_face.GetNPoints();
         int NP1d = ir_glob_1d.GetNPoints();

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

         for( int did = 0; did < NP1d; did++ )
         {
            Vector be;
            be.SetSize(NP1d);
            Vector ge;
            ge.SetSize(NP1d);

            el1_basis1d.Eval(ir_glob_1d.IntPoint(did).x, be, ge);

            for( int p1d = 0; p1d < NP1d; p1d++ )
            {
               du_element(did, p1d, e1) = ge(p1d);
            }
            if( int_face_match )
            {
               el2_basis1d.Eval(ir_glob_1d.IntPoint(did).x, be, ge);

               for( int p1d = 0; p1d < NP1d; p1d++ )
               {
                  du_element(did, p1d, e2) = ge(p1d);
               }
            }
         }
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

         // Loop over integration points on the the face
         for (int p = 0; p < NPf; p++) 
         {
            Vector stencil_point;
            stencil_point.SetSize(dim);
            stencil_point = 0.0;

            Vector stencil_point_nor;
            stencil_point_nor.SetSize(dim);
            stencil_point_nor = 0.0;

            Vector stencil_point_tan1;
            stencil_point_tan1.SetSize(dim);
            stencil_point_tan1 = 0.0;

            Vector stencil_point_tan2;
            stencil_point_tan2.SetSize(dim);
            stencil_point_tan2 = 0.0;

            Vector stencil_point_neighbor;
            stencil_point_neighbor.SetSize(dim);
            stencil_point_neighbor = 0.0;

            Vector stencil_point_neighbor_nor;
            stencil_point_neighbor_nor.SetSize(dim);
            stencil_point_neighbor_nor = 0.0;

            Vector stencil_point_neighbor_tan1;
            stencil_point_neighbor_tan1.SetSize(dim);
            stencil_point_neighbor_tan1 = 0.0;

            Vector stencil_point_neighbor_tan2;
            stencil_point_neighbor_tan2.SetSize(dim);
            stencil_point_neighbor_tan2 = 0.0;

            Vector this_stencil_point;
            this_stencil_point.SetSize(dim);
            this_stencil_point = 0.0;

            Vector this_stencil_point_neighbor;
            this_stencil_point.SetSize(dim);
            this_stencil_point = 0.0;

            IntegrationPoint &pb = ir_glob_element.IntPoint(facemapnorself[p*ndofs1d+0]); 

            fes.GetMesh()->GetElementTransformation(e1)->Transform(pb,this_stencil_point);
            if (int_face_match)
            {
               IntegrationPoint &pb_neighbor = ir_glob_element.IntPoint(facemapnorneighbor[p*ndofs1d+0]); 
               fes.GetMesh()->GetElementTransformation(e2)->Transform(pb_neighbor,this_stencil_point_neighbor);
            }

            int tan1_id = p % NP1d; // is this correct?
            IntegrationPoint &ip_loc_tan1 = ir_glob_1d.IntPoint(tan1_id);
            el1_basis1d.Eval(ip_loc_tan1.x, bf, gf_tan1);

            if( dim == 3 )
            {
               int tan2_id = (p/NP1d) % NP1d; // is this correct?
               IntegrationPoint &ip_loc_tan2 = ir_glob_1d.IntPoint(tan2_id);
               el1_basis1d.Eval(ip_loc_tan2.x, bf, gf_tan2);
            }

            for (int l = 0; l < NP1d; l++)
            {
               const int pn  = facemapnorself[p*ndofs1d+l];
               const int pt1 = facemaptan1self[p*ndofs1d+l];
               const int pt2 = (dim==3)? facemaptan2self[p*ndofs1d+l] : 0;

               IntegrationPoint &ip_nor = ir_glob_element.IntPoint(pn);
               IntegrationPoint &ip_tan1 = ir_glob_element.IntPoint(pt1);
               IntegrationPoint &ip_tan2 = ir_glob_element.IntPoint(pt2);

               Vector this_stencil_point_nor;
               this_stencil_point_nor.SetSize(dim);
               this_stencil_point_nor = 0.0;

               Vector this_stencil_point_tan1;
               this_stencil_point_tan1.SetSize(dim);
               this_stencil_point_tan1 = 0.0;

               Vector this_stencil_point_tan2;
               this_stencil_point_tan2.SetSize(dim);
               this_stencil_point_tan2 = 0.0;

               fes.GetMesh()->GetElementTransformation(e1)->Transform(ip_nor , this_stencil_point_nor );
               fes.GetMesh()->GetElementTransformation(e1)->Transform(ip_tan1, this_stencil_point_tan1);
               if( dim == 3 )
               {
                  fes.GetMesh()->GetElementTransformation(e1)->Transform(ip_tan2, this_stencil_point_tan2);
               }
               
               this_stencil_point_nor *= gf_nor(l);
               stencil_point_nor += this_stencil_point_nor;
               
               this_stencil_point_tan1 *= gf_tan1(l);
               stencil_point_tan1 += this_stencil_point_tan1;

               if( dim == 3 )
               {
                  this_stencil_point_tan2 *= gf_tan2(l);
                  stencil_point_tan2 += this_stencil_point_tan2;
               }
               if (int_face_match)
               {
                  const int pn_2  = facemapnorneighbor[p*ndofs1d+l];
                  const int pt1_2 = facemaptan1neighbor[p*ndofs1d+l];
                  const int pt2_2 = (dim == 3)? facemaptan2neighbor[p*ndofs1d+l]:0;

                  IntegrationPoint &ip_nor_2 = ir_glob_element.IntPoint(pn_2); 
                  IntegrationPoint &ip_tan1_2 = ir_glob_element.IntPoint(pt1_2); 
                  IntegrationPoint &ip_tan2_2 = ir_glob_element.IntPoint(pt2_2); 

                  // do these need permutefacel2? 
                  Vector this_stencil_point_neighbor_nor;
                  this_stencil_point_neighbor_nor.SetSize(dim);
                  this_stencil_point_neighbor_nor = 0.0;

                  Vector this_stencil_point_neighbor_tan1;
                  this_stencil_point_neighbor_tan1.SetSize(dim);
                  this_stencil_point_neighbor_tan1 = 0.0;

                  Vector this_stencil_point_neighbor_tan2;
                  this_stencil_point_neighbor_tan2.SetSize(dim);
                  this_stencil_point_neighbor_tan2 = 0.0;

                  fes.GetMesh()->GetElementTransformation(e2)->Transform(ip_nor_2 , this_stencil_point_neighbor_nor );
                  fes.GetMesh()->GetElementTransformation(e2)->Transform(ip_tan1_2, this_stencil_point_neighbor_tan1);
                  if( dim == 3)
                  {
                     fes.GetMesh()->GetElementTransformation(e2)->Transform(ip_tan2_2, this_stencil_point_neighbor_tan2);
                  }

                  this_stencil_point_neighbor_nor *= gf_nor(l);
                  stencil_point_neighbor_nor += this_stencil_point_neighbor_nor;

                  this_stencil_point_neighbor_tan1 *= gf_tan1(l);
                  stencil_point_neighbor_tan1 += this_stencil_point_neighbor_tan1;

                  if( dim == 3)
                  {
                     this_stencil_point_neighbor_tan2 *= gf_tan2(l);
                     stencil_point_neighbor_tan2 += this_stencil_point_neighbor_tan2;
                  }
               }
            }

/*
            stencil_point_nor /= stencil_point_nor.Norml2();
            stencil_point_tan1 /= stencil_point_tan1.Norml2();
            if( dim == 3)
            {
               stencil_point_tan2 /= stencil_point_tan2.Norml2();
            }

            if (int_face_match)
            {
               stencil_point_neighbor_nor /= stencil_point_neighbor_nor.Norml2();
               stencil_point_neighbor_tan1 /= stencil_point_neighbor_tan1.Norml2();
               if( dim == 3)
               {
                  stencil_point_neighbor_tan2 /= stencil_point_neighbor_tan2.Norml2();
               }
            }
            */
            const FaceGeometricFactors *facegeom = fes.GetMesh()->GetFaceGeometricFactors(
                           ir_glob_face,
                           FaceGeometricFactors::DETERMINANTS |
                           FaceGeometricFactors::NORMALS, type);

            auto truenor = Reshape(facegeom->normal.Read(), NPf, dim, nf);

            Vector facenorm; // change this to use gf*pts
            facenorm.SetSize(dim);
            if( dim == 2 )
            {
               facenorm(0) = truenor(p,0,f_ind);
               facenorm(1) = truenor(p,1,f_ind);
            }
            else if( dim == 3 )
            {
               facenorm(0) = truenor(p,0,f_ind);
               facenorm(1) = truenor(p,1,f_ind);
               facenorm(2) = truenor(p,2,f_ind);
            }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
            double detJ = Trans0.Elem1->Jacobian().Det();
            const int lid = GetLid(p, f_ind, ndofs_face);

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

            Vector coeffs;
            coeffs.SetSize(dim);
            if( dim == 2 )
            {
               GetVectorCoefficients2D(stencil_point_nor,
                                       stencil_point_tan1,
                                       facenorm,
                                       coeffs);
            }
            else if( dim == 3 )
            {
               GetVectorCoefficients3D(stencil_point_nor,
                                       stencil_point_tan1,
                                       stencil_point_tan2,
                                       facenorm,
                                       coeffs);
            }

            DenseMatrix adjJ;
            adjJ.SetSize(dim);

            Vector nor;
            nor.SetSize(dim);

            Vector nh;
            nh.SetSize(dim);

            CalcAdjugate(Trans0.Elem1->Jacobian(), adjJ);
            CalcOrtho(Trans0.Jacobian(), nor);
            adjJ.Mult(nor, nh);
            //double detJ = Trans0.Elem1->Jacobian().Det();

            nh /= detJ;
            


#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

            DenseMatrix adjJ1;
            adjJ1.SetSize(dim);

            mfem::MatrixInverse* invJ1 = fes.GetMesh()->GetElementTransformation(e1)->Jacobian().Inverse();

//         this gives the wrong result
//            CalcAdjugate(Trans0.Elem1->Jacobian(), adjJ1);
//            double detJ1 = Trans0.Elem1->Jacobian().Det();
 
            CalcAdjugate( fes.GetMesh()->GetElementTransformation(e1)->Jacobian(), adjJ1);
            double detJ1 = fes.GetMesh()->GetElementTransformation(e1)->Jacobian().Det();
 
            Vector adjf1;
            adjf1.SetSize(dim);
            adjJ1.Mult(facenorm, adjf1);
            adjf1 /= detJ1;

            invJ1->Mult(facenorm, adjf1);

            //adjf1 *= facegeom->detJ(lid);
            jac_face_factor( 0, p, face_id1, e1) = adjf1(0);

            jac_face_factor( 1, p, face_id1, e1) = adjf1(1);
            if(dim==3)
            {
               jac_face_factor( 2, p, face_id1, e1) = adjf1(2);
            }
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

            for( int i = 0 ; i < ndofs1d ; i++ )
            {
               u_face(i,p,f_ind,0) = bf(i);
               dudn_face(i,p,f_ind,0,0) = gf_nor(i)*coeffs(0);
               dudn_face(i,p,f_ind,0,1) = gf_tan1(i)*coeffs(1);
               if(dim==3)
               {
                  dudn_face(i,p,f_ind,0,2) = gf_tan2(i)*coeffs(2);
               }
               

#ifdef MFEM_DEBUG

               std::cout << "------------------------------------ " << std::endl;
               std::cout << "stencil_point_nor.Norml2() =  " << stencil_point_nor.Norml2() << std::endl;
               std::cout << "stencil_point_tan1.Norml2() =  " << stencil_point_tan1.Norml2() << std::endl;
               std::cout << "stencil_point_tan2.Norml2() =  " << stencil_point_tan2.Norml2() << std::endl;

               std::cout << "facenorm.Print() " << std::endl;
               facenorm.Print();
               std::cout << "print coeffs vs nh " << std::endl;
               coeffs.Print();
               nh.Print();
               std::cout << " Trans0.Elem1->Jacobian() = " << std::endl;
               Trans0.Elem1->Jacobian().Print();

               std::cout << " Trans0.Elem1->Jacobian()->det = " << detJ << std::endl;

//               DenseMatrix &Jinv = Trans0.Elem1->Jacobian().Inverse();
  //             std::cout << " J^-1 = " << std::endl;
               //Jinv.Print();
               std::cout << " adjJ = " << std::endl;
               adjJ.Print();
               std::cout << " detJ(lid) = " << facegeom->detJ(lid) << std::endl;
               std::cout << " facedetJ/detJ = " << facegeom->detJ(lid)/detJ << std::endl;
#endif


               if(int_face_match)
               {
                  double detJ = Trans0.Elem2->Jacobian().Det();

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

                  Vector coeffs;
                  coeffs.SetSize(dim);
                  if( dim == 2 )
                  {
                     GetVectorCoefficients2D(stencil_point_neighbor_nor,
                                             stencil_point_neighbor_tan1,
                                             facenorm,
                                             coeffs);
                     coeffs *= -1.0;
                  }
                  else if( dim == 3 )
                  {
                     GetVectorCoefficients3D(stencil_point_neighbor_nor,
                                             stencil_point_neighbor_tan1,
                                             stencil_point_neighbor_tan2,
                                             facenorm,
                                             coeffs);
                     coeffs *= -1.0;
                  }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

                  DenseMatrix adjJ2;
                  adjJ2.SetSize(dim);
//         this gives the wrong result
//                  CalcAdjugate( Trans0.Elem2->Jacobian(), adjJ2);
//                  double detJ2 = Trans0.Elem2->Jacobian().Det();

                  CalcAdjugate( fes.GetMesh()->GetElementTransformation(e2)->Jacobian(), adjJ2);
                  double detJ2 = fes.GetMesh()->GetElementTransformation(e2)->Jacobian().Det();

                  Vector adjf2;
                  adjf2.SetSize(dim);
                  adjJ2.Mult(facenorm, adjf2);
                  adjf2 /= detJ2;
                  //adjf2 *= facegeom->detJ(lid);
                  // negate side 2
                  jac_face_factor( 0, p, face_id2, e2) = adjf2(0);
                  jac_face_factor( 1, p, face_id2, e2) = adjf2(1);
                  if(dim==3)
                  {
                     jac_face_factor( 2, p, face_id2, e2) = adjf2(2);
                  }
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif


#ifdef MFEM_DEBUG
                  std::cout << "------------------------------------ " << std::endl;
#endif
                  u_face(i,p,f_ind,1) = bf(i);
                  dudn_face(i,p,f_ind,1,0) = gf_nor(i)*coeffs(0);
                  dudn_face(i,p,f_ind,1,1) = gf_tan1(i)*coeffs(1);
                  if(dim==3)
                  {
                     dudn_face(i,p,f_ind,1,2) = gf_tan2(i)*coeffs(2);
                  }
               }
            }
         }


#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

         if( int_face_match)            
         {
            map_elements_to_faces[num_faces_per_element*e1 + face_id1] = f_ind;
            map_elements_to_sides[num_faces_per_element*e1 + face_id1] = 0;

            map_elements_to_faces[num_faces_per_element*e2 + face_id2] = f_ind;
            map_elements_to_sides[num_faces_per_element*e2 + face_id2] = 1;

            needed_elements[e1] = 1;
            needed_elements[e2] = 1;

            for( int fdof = 0 ; fdof < NPf ; fdof++ )
            {
               int new_fdof = PermuteFaceL2(dim, face_id1, face_id2, orientation2, ndofs1d, fdof);
               std::cout << 
               " fdof " << fdof << 
               " new_fdof " << new_fdof << 
               " face_id1 " << face_id1 << 
               " face_id2 " << face_id2 << 
               " orientation2 " << orientation2 << 
               std::endl;

               if( new_fdof != fdof )
               {
                  exit(1);
               }

               // Only is used for side 1
               map_side_permutations[ndofs_face*f_ind + fdof] = new_fdof;
            }
         }
         if( bdy_face_match )
         {
            needed_elements[e1] = 1;
            map_elements_to_faces[num_faces_per_element*e1 + face_id1] = f_ind;
            map_elements_to_sides[num_faces_per_element*e1 + face_id1] = 0;
         }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

         // Compute task-local scatter id for each face dof
         for (int d = 0; d < ndofs_face; ++d)
         {
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
            int gid = GetGid(d, ndofs1d, e1, elem_dofs, facemapnorself, elementMap);
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
            int lid = GetLid(d, f_ind, ndofs_face);
            scatter_indices[lid] = gid;
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

            if(m==L2FaceValues::DoubleValued)
            {
               // For double-values face dofs, compute second scatter index
               if (int_face_match) // interior face
               {
                  const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                               orientation2, ndofs1d, d);
                  gid = GetGid(pd, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
                  lid = GetLid(d, f_ind, ndofs_face);
                  scatter_indices_neighbor[lid] = gid;
               }
               else if (bdy_face_match) // true boundary face
               {
                  const int lid = GetLid(d, f_ind, ndofs_face);
                  scatter_indices_neighbor[lid] = -1;
               }
            }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
            for (int k = 0; k < ndofs1d; ++k)
            {
               const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                             orientation2, ndofs1d, d);
               const int pk = PermuteFaceNormL2(dim, face_id1, face_id2,
                                               orientation2, ndofs1d, k);
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
               int gid = GetGid(pd, pk, ndofs1d, e1, elem_dofs, facemapnorself, elementMap);
#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
               int lid = GetLid(d, pk, f_ind, ndofs1d, ndofs_face);
               scatter_indices_nor[lid] = gid;

               gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemaptan1self, elementMap);
               lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
               scatter_indices_tan1[lid] = gid;

               if( dim==3 )
               {
                  const int gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemaptan2self, elementMap);
                  const int lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                  scatter_indices_tan2[lid] = gid;
               }

               if(m==L2FaceValues::DoubleValued)
               {
                  // For double-values face dofs, compute second scatter index
                  if (int_face_match) // interior face
                  {
                     const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                                  orientation2, ndofs1d, d);
                     int gid = GetGid(pd, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);
                     int lid = GetLid(d, f_ind, ndofs_face);
                     scatter_indices_neighbor[lid] = gid;

                     const int pk = PermuteFaceNormL2(dim, face_id1, face_id2,
                                               orientation2, ndofs1d, k);

                     gid = GetGid(pd, pk, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);
                     lid = GetLid(d, pk, f_ind, ndofs1d, ndofs_face);
                     scatter_indices_neighbor_nor[lid] = gid;

                     gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemaptan1neighbor, elementMap);
                     lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                     scatter_indices_neighbor_tan1[lid] = gid;
                     if( dim == 3)
                     {
                        gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemaptan2neighbor, elementMap);
                        lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                        scatter_indices_neighbor_tan2[lid] = gid;
                     }
                  }
                  else if (bdy_face_match) // true boundary face
                  {
                     const int pk = PermuteFaceNormL2(dim, face_id1, face_id2,
                                               orientation2, ndofs1d, k);
                     const int lid = GetLid(d, pk, f_ind, ndofs1d, ndofs_face);
                     scatter_indices_neighbor_nor[lid] = -1;
                     scatter_indices_neighbor_tan1[lid] = -1;
                     scatter_indices_neighbor_tan2[lid] = -1;
                  }
               }
            }
         }
         f_ind++;
      }
   }

   num_needed_elements = 0;
   for( int p = 0; p < ne ; p++ )
   {
     //std::cout << "p = " << p << std::endl;
      bool is_needed = false;
      for( int face_id = 0 ; face_id < num_faces_per_element ; face_id++ )
      {
         is_needed = is_needed || (map_elements_to_sides[num_faces_per_element*p + face_id] > -1 );
      }
      if( is_needed )
      {
         //std::cout << "needed_elements["<<num_needed_elements<<"] = "<< p << std::endl;
         needed_elements[num_needed_elements] = p;
         num_needed_elements++;
      }
    }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   // Computation of gather_indices
   for (int i = 0; i <= ndofs; ++i)
   {
      offsets[i] = 0;
      offsets_nor[i] = 0;
      offsets_tan1[i] = 0;
      if( dim == 3 )
      {
         offsets_tan2[i] = 0;
      }
   }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

   f_ind = 0;
   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);

      bool int_face_match = type==FaceType::Interior && e2>=0;
      bool bdy_face_match = type==FaceType::Boundary && e2<0;

      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (bdy_face_match && inf2<0) )
      {

         fes.GetMesh()->GetFaceOrientations(f, &orientation1, &orientation2);
         fes.GetMesh()->GetFaceIds(f, &face_id1, &face_id2);
         Array<int> V, E, Eo;
         fes.GetMesh()->GetElementVertices(e1, V);
         fes.GetMesh()->GetElementEdges(e1, E, Eo);

         GetGradFaceDofStencil(dim, face_id1, ndofs1d, facemapnorself, facemaptan1self, facemaptan2self);
         GetGradFaceDofStencil(dim, face_id2, ndofs1d, facemapnorneighbor, facemaptan1neighbor, facemaptan2neighbor);
         orientation1 = Eo[face_id1];
         orientation2 = Eo[face_id2];

         for (int d = 0; d < ndofs_face; ++d)
         {
            int gid = GetGid(d, ndofs1d, e1, elem_dofs, facemapnorself, elementMap);                     
            ++offsets[gid + 1];

            for (int k = 0; k < ndofs1d; ++k)
            {
               int gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemapnorself, elementMap);                     
               ++offsets_nor[gid + 1];

               gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemaptan1self, elementMap);     
               ++offsets_tan1[gid + 1];

               if(dim==3)
               {
                  gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemaptan2self, elementMap);                     
                  ++offsets_tan2[gid + 1];
               }
            }
         }
         if (m==L2FaceValues::DoubleValued)
         {
            if(int_face_match) // interior face
            {
               for (int d = 0; d < ndofs_face; ++d)
               {
                  const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                               orientation2, ndofs1d, d);
                  int gid = GetGid(pd, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);                     
                  ++offsets[gid + 1];

                  for (int k = 0; k < ndofs1d; ++k)
                  {
                     int gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);                     
                     ++offsets_nor[gid + 1];

                     gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemaptan1neighbor, elementMap);
                     ++offsets_tan1[gid + 1];

                     if( dim == 3 )
                     {
                        gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemaptan2neighbor, elementMap);
                        ++offsets_tan2[gid + 1];
                     }
                  }
               }
            }
         }


         f_ind++;
      }
   }


#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif

#ifdef MFEM_DEBUG

   std::cout << " scatter_indices " << std::endl;
   for (int i = 0; i < nf*ndofs_face ; i++)
   {
      std::cout << i << " : " << 
      scatter_indices[i] << " " <<
      scatter_indices_neighbor[i] << std::endl;
   }
   for (int i = 0; i < nf*ndofs_face ; i++)
   {
      std::cout << i << " : " << 
      scatter_indices_nor[i] << " " <<
      scatter_indices_tan1[i] << " " <<
      scatter_indices_tan2[i]  << " " <<
      scatter_indices_neighbor_nor[i] << " " <<
      scatter_indices_neighbor_tan1[i] << " " <<
      scatter_indices_neighbor_tan2[i]  << std::endl;
   }
   std::cout << "end scatter_indices " << std::endl;

/*
   std::cout << " offsets pre " << std::endl;
   for (int i = 0; i < ndofs+1; i++)
   {
      std::cout << i << " : " << 
      offsets[i] << " " <<
      //offsets_nor[i] << " " <<
      //offsets_tan1[i] << " " <<
      //offsets_tan2[i] <<  
      std::endl;
   }
   */

//   std::cout << "end offsets pre " << std::endl;
#endif

   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   for (int i = 1; i <= ndofs; ++i)
   {
      offsets[i] += offsets[i - 1];
      offsets_nor[i] += offsets_nor[i - 1];
      offsets_tan1[i] += offsets_tan1[i - 1];
      if( dim == 3 )
      {
         offsets_tan2[i] += offsets_tan2[i - 1];
      }
   }
   f_ind = 0;

#ifdef MFEM_DEBUG
/* 
   std::cout << " offsets post " << std::endl;
   for (int i = 0; i < ndofs+1; i++)
   {
      std::cout << i << " : " << 
      offsets[i] << " " <<
      //offsets_nor[i]  << " " <<
      //offsets_tan1[i] << " " <<
      //offsets_tan2[i] << 
      std::endl;
   }
   */
   std::cout << "end offsets post " << std::endl;
   std::cout << "height " << height << std::endl;
   std::cout << "width " << width << std::endl;
#endif

   for (int f = 0; f < fes.GetNF(); ++f)
   {
      fes.GetMesh()->GetFaceElements(f, &e1, &e2);
      fes.GetMesh()->GetFaceInfos(f, &inf1, &inf2);

      bool int_face_match = type==FaceType::Interior && e2>=0;
      bool bdy_face_match = type==FaceType::Boundary && e2<0;

      if ((type==FaceType::Interior && (e2>=0 || (e2<0 && inf2>=0))) ||
          (bdy_face_match && inf2<0) )
      {

         fes.GetMesh()->GetFaceOrientations(f, &orientation1, &orientation2);
         fes.GetMesh()->GetFaceIds(f, &face_id1, &face_id2);
         Array<int> V, E, Eo;
         fes.GetMesh()->GetElementVertices(e1, V);
         fes.GetMesh()->GetElementEdges(e1, E, Eo);
         GetGradFaceDofStencil(dim, face_id1, ndofs1d, facemapnorself, facemaptan1self, facemaptan2self);
         GetGradFaceDofStencil(dim, face_id2, ndofs1d, facemapnorneighbor, facemaptan1neighbor, facemaptan2neighbor);
         orientation1 = Eo[face_id1];
         orientation2 = Eo[face_id2];

         for (int d = 0; d < ndofs_face; ++d)
         {
            int gid = GetGid(d, ndofs1d, e1, elem_dofs, facemapnorself, elementMap);
            int lid = GetLid(d, f_ind, ndofs_face);
            int offset = offsets[gid];
            gather_indices[offset] = lid;
            offsets[gid]++;

            for (int k = 0; k < ndofs1d; ++k)
            {
               int gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemapnorself, elementMap);
               int lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
               int offset = offsets_nor[gid];
               gather_indices_nor[offset] = lid;
               offsets_nor[gid]++;

               gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemaptan1self, elementMap);
               lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
               offset = offsets_tan1[gid];
               gather_indices_tan1[offset] = lid;
               offsets_tan1[gid]++;
               if( dim == 3 )
               {
                  gid = GetGid(d, k, ndofs1d, e1, elem_dofs, facemaptan2self, elementMap);
                  lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                  offset = offsets_tan2[gid];
                  gather_indices_tan2[offset] = lid;
                  offsets_tan2[gid]++;
               }

            }
         }
         if (m==L2FaceValues::DoubleValued)
         {
            for (int d = 0; d < ndofs_face; ++d)
            {
               if (int_face_match) // interior face
               {
                  const int half = numfacedofs;
                  const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                               orientation2, ndofs1d, d);

                  // double check all of this logic 
                  int gid = GetGid(pd, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);
                  int lid = GetLid(d, f_ind, ndofs_face);
                  int offset = offsets[gid];

                  gather_indices[offset] = half + lid;
                  offsets[gid]++;
               }

               for (int k = 0; k < ndofs1d; ++k)
               {
                  if (int_face_match) // interior face
                  {
                     const int half_grad = ndofs1d*numfacedofs;
                     const int pd = PermuteFaceL2(dim, face_id1, face_id2,
                                                  orientation2, ndofs1d, d);

                     // double check all of this logic 
                     int gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemapnorneighbor, elementMap);
                     int lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                     int offset = offsets_nor[gid];

                     gather_indices_nor[offset] = half_grad + lid;
                     offsets_nor[gid]++;

                     gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemaptan1neighbor, elementMap);
                     lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                     offset = offsets_tan1[gid];
                     gather_indices_tan1[offset] = half_grad + lid;
                     offsets_tan1[gid]++;
                     if( dim == 3 )
                     {
                        gid = GetGid(pd, k, ndofs1d, e2, elem_dofs, facemaptan2neighbor, elementMap);
                        lid = GetLid(d, k, f_ind, ndofs1d, ndofs_face);
                        // We shift lid to express that it's e2 of f
                        offset = offsets_tan2[gid];
                        gather_indices_tan2[offset] = half_grad + lid;
                        offsets_tan2[gid]++;
                     }
                  }
               }
            }
         }
         f_ind++;
      }
   }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
#endif
   MFEM_VERIFY(f_ind>0, "Unexpected number of faces.");
   MFEM_VERIFY(f_ind==nf, "Unexpected number of faces.");
   for (int i = ndofs; i > 0; --i)
   {
      offsets[i] = offsets[i - 1];
      offsets_nor[i] = offsets_nor[i - 1];
      offsets_tan1[i] = offsets_tan1[i - 1];
      if( dim == 3)
      {
         offsets_tan2[i] = offsets_tan2[i - 1];
      }
   }
   offsets[0] = 0;
   offsets_nor[0] = 0;
   offsets_tan1[0] = 0;
   if( dim == 3 )
   {
      offsets_tan2[0] = 0;
   }

#ifdef MFEM_DEBUG
   std::cout << " elementMap " << std::endl;
   for (int i = 0; i < elemDofs*ne ; i++)
   {
      std::cout << i << " : " << 
      elementMap[i] << std::endl;
   }
   std::cout << "end elementMap " << std::endl;

   std::cout << " ndofs_face = " << ndofs_face  << std::endl;
   std::cout << " ndofs1d = " << ndofs1d  << std::endl;
   std::cout << " nf = " << nf  << std::endl;

   std::cout << " gather_indices " << std::endl;
   for (int i = 0; i < gather_indices.Size() ; i++)
   {
      std::cout << i << " : " << 
      gather_indices[i] << std::endl;
   }
   for (int i = 0; i < gather_indices_nor.Size() ; i++)
   {
      std::cout << i << " : " << 
      gather_indices_nor[i] << " " <<
      gather_indices_tan1[i] << " " <<
      gather_indices_tan2[i]  << std::endl;
   }
   std::cout << "end gather_indices " << std::endl;
#endif

#ifdef MFEM_DEBUG

   std::cout << " scatter_indices " << std::endl;
   for (int i = 0; i < scatter_indices.Size() ; i++)
   {
      std::cout << i << " : " << 
      scatter_indices[i] << std::endl;
   }
   std::cout << " scatter_indices_neighbor " << std::endl;
   for (int i = 0; i < scatter_indices_neighbor.Size() ; i++)
   {
      std::cout << i << " : " << 
      scatter_indices_neighbor[i] << std::endl;
   }   
   std::cout << " offsets post " << std::endl;
   for (int i = 0; i < ndofs+1; i++)
   {
      std::cout << i << " : " << 
      offsets[i]  << std::endl;
   }
   std::cout << " gather_indices " << std::endl;
   for (int i = 0; i < gather_indices.Size() ; i++)
   {
      std::cout << i << " : " << 
      gather_indices[i] << std::endl;
   }
   std::cout << " done " << std::endl;
#endif
   //exit(1);

}

void L2FaceNormalDRestriction::Mult(const Vector& x, Vector& y) const
{

#ifdef MFEM_DEBUG
   std::cout << " restrict x" << std::endl;
   x.Print(std::cout,1);
   std::cout << " end restrict x" << std::endl;
#endif

   // Assumes all elements have the same number of dofs, nd
   const int vd = vdim;
   const int dim = fes.GetMesh()->SpaceDimension();
   // is x transposed?
   const bool t = byvdim;
   //auto u_face    = Reshape(Bf.Read(), ndofs1d, ndofs_face, nf, 2);
   auto dudn_face = Reshape(Gf.Read(), ndofs1d, ndofs_face, nf, 2, 3);
   int num_sides = 2; 
   int num_derivatives = 2; 
   y = 0.0;

// -1 old, 0 both, 1 new
#define restrict_use 1

#if restrict_use < 1


   if (m==L2FaceValues::DoubleValued)
   {
      auto d_indices1 = scatter_indices.Read();
      auto d_indicesnorself = scatter_indices_nor.Read();
      auto d_indicestan1self = scatter_indices_tan1.Read();
      auto d_indicestan2self = scatter_indices_tan2.Read();

      auto d_indices2 = scatter_indices_neighbor.Read();
      auto d_indicesnorneighbor = scatter_indices_neighbor_nor.Read();
      auto d_indicestan1neighbor = scatter_indices_neighbor_tan1.Read();
      auto d_indicestan2neighbor = scatter_indices_neighbor_tan2.Read();

      auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
      auto d_y = Reshape(y.Write(), ndofs_face, vd, num_sides, nf, num_derivatives);

      // Loop over all face dofs
      MFEM_FORALL(i, ndofs_face*vd*nf,
      {
         const int fdof = i % ndofs_face;
         const int face = i/ndofs_face;
         const int idx1 = d_indices1[i];
         for (int c = 0; c < vd; ++c)
         {
            d_y( fdof , c, 0, face, 0) += d_x(t?c:idx1, t?idx1:c);
         }

         // neighbor side 
         const int idx2 = d_indices2[i];
         if( idx2==-1 )
         {
            for (int c = 0; c < vd; ++c)
            {
               d_y( fdof , c, 1, face, 0) = 0.0;
            }
         }
         else
         {
            for (int c = 0; c < vd; ++c)
            {
               d_y( fdof , c, 1, face, 0) += d_x(t?c:idx2, t?idx2:c);
            }
         }
      });

      MFEM_FORALL(i, ndofs1d*ndofs_face*vd*nf,
      {
         const int k = i % ndofs1d;
         const int fdof = (i/ndofs1d) % ndofs_face;
         const int face = i / (ndofs1d*ndofs_face);
         const int idx1_nor = d_indicesnorself[i];
         const int idx1_tan1 = d_indicestan1self[i];
         const int idx1_tan2 = (dim == 3)? d_indicestan2self[i] : 0;
         for (int c = 0; c < vd; ++c)
         {
            d_y( fdof , c, 0, face, 1) += d_x(t?c:idx1_nor, t?idx1_nor:c)*dudn_face(k,fdof,face,0,0);
            d_y( fdof , c, 0, face, 1) += d_x(t?c:idx1_tan1, t?idx1_tan1:c)*dudn_face(k,fdof,face,0,1);
            if( dim == 3 )
            {
               d_y( fdof , c, 0, face, 1) += d_x(t?c:idx1_tan2, t?idx1_tan2:c)*dudn_face(k,fdof,face,0,2);
            }
         }

         // neighbor side 
         const int idx2_nor = d_indicesnorneighbor[i];
         const int idx2_tan1 = d_indicestan1neighbor[i];
         const int idx2_tan2 = ( dim==3 ) ? d_indicestan2neighbor[i] : 0;

         if( idx2_nor == -1 )
         {
            for (int c = 0; c < vd; ++c)
            {
               d_y( fdof , c, 1, face, 1) = 0.0;
            }
         }
         else
         {
            for (int c = 0; c < vd; ++c)
            {
               d_y( fdof , c, 1, face, 1) += d_x(t?c:idx2_nor, t?idx2_nor:c)*dudn_face(k,fdof,face,1,0);
               d_y( fdof , c, 1, face, 1) += d_x(t?c:idx2_tan1, t?idx2_tan1:c)*dudn_face(k,fdof,face,1,1);
               if( dim == 3 )
               {
                  d_y( fdof , c, 1, face, 1) += d_x(t?c:idx2_tan2, t?idx2_tan2:c)*dudn_face(k,fdof,face,1,2);
               }
            }
         }
      });
   }
   else
   {

   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
      mfem_error("not yet implemented.");
   }

#endif


#ifdef MFEM_DEBUG
   std::cout << " restrict yold" << std::endl;
   y.Print(std::cout,1);
   std::cout << " end restrict yold" << std::endl;
#endif



#if restrict_use > -1 

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
   Vector y_old;
   y_old = y;
#endif
   y = 0.0;

   int D1D = ndofs1d;
   int end = D1D - 1;
   auto Ge_ = Reshape(Ge.Read(), D1D, D1D, ne);
   int max_D1D = D1D;

   const Table& e2dTable = fes.GetElementToDofTable();
   const int* elementMap = e2dTable.GetJ();

//   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

   if (m==L2FaceValues::DoubleValued)
   {
      auto d_x = Reshape(x.Read(), t?vd:ndofs, t?ndofs:vd);
      auto d_y_new = Reshape(y.Write(), ndofs_face, vd, num_sides, nf, num_derivatives);

//   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

/*
      // Loop over all face dofs
      MFEM_FORALL(i, ndofs_face*vd*nf,
      {
         const int fdof = i % ndofs_face;
         const int face = i/ndofs_face;
         const int idx1 = d_indices1[i];
         for (int c = 0; c < vd; ++c)
         {
            d_y_new( fdof , c, 0, face, 0) += d_x(t?c:idx1, t?idx1:c);
         }
         // neighbor side 
         const int idx2 = d_indices2[i];
         if( idx2==-1 )
         {
            for (int c = 0; c < vd; ++c)
            {
               d_y_new( fdof , c, 1, face, 0) = 0.0;
            }
         }
         else
         {
            for (int c = 0; c < vd; ++c)
            {
               d_y_new( fdof , c, 1, face, 0) += d_x(t?c:idx2, t?idx2:c);
            }
         }
      });
      */


      auto jac_face_factor = Reshape( jac_face_factors.Read(), dim, ndofs_face, num_faces_per_element, ne);
      int elem_dofs = (dim==3)? D1D*D1D*D1D : D1D*D1D;

//  std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

      if(dim == 2)
      {           

         // Loop over all elements
         MFEM_FORALL(e_, num_needed_elements,
         {
            int e = needed_elements[e_];

            /*
            // may not need this
            bool skip = true;
            for( int face_id = 0 ; face_id < num_faces_per_element ; face_id++ )
            {
               if( map_elements_to_sides[num_faces_per_element*e + face_id] != -1 )
               {
                  skip = false;
               }
            }
            if(skip)
            {
               continue;
            }
            */

            double u[max_D1D][max_D1D][vdim];

            // Roll element dofs into tensor form
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int did = d1 + D1D*d2;
                  int idx = elementMap[e*elem_dofs + did];
                  for (int c = 0; c < vdim; c++)
                  {
                     u[d1][d2][c] = d_x(t?c:idx, t?idx:c);
                  }
               }
            }

//   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

            // Restrict to faces
            double R0xu[max_D1D][vdim];
            double R0yu[max_D1D][vdim];

            double R1xu[max_D1D][vdim];
            double R1yu[max_D1D][vdim];

            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int c = 0; c < vdim; c++)
               {
                  // xi = 0 face
                  R0xu[d1][c] = u[0][d1][c];
                  R0yu[d1][c] = u[d1][0][c];

                  // xi = 1 face
                  R1xu[d1][c] = u[end][d1][c];
                  R1yu[d1][c] = u[d1][end][c];
               }
            }

//   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
            
            double R0yGxu[max_D1D][vdim];
            double R0xGyu[max_D1D][vdim];
            double R1yGxu[max_D1D][vdim];
            double R1xGyu[max_D1D][vdim];
            
            // Apply derivative operator to each dimension
            // todo - maybe rewrite this to improve stride
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int c = 0; c < vdim; c++)
               {
                  R0yGxu[d1][c] = 0;
                  R0xGyu[d1][c] = 0;
                  R1yGxu[d1][c] = 0;
                  R1xGyu[d1][c] = 0;

                  for (int p = 0; p < D1D; p++)
                  {
                     double G = Ge_(d1,p,e);
                     // xi = 0 faces
                     R0yGxu[d1][c] += G*R0yu[p][c];
                     R0xGyu[d1][c] += G*R0xu[p][c];
                     
                     // xi = 1 faces
                     R1yGxu[d1][c] += G*R1yu[p][c];
                     R1xGyu[d1][c] += G*R1xu[p][c];
                  }
               }
            }

            double R0xGxu[max_D1D][vdim];
            double R0yGyu[max_D1D][vdim];
            double R1xGxu[max_D1D][vdim];
            double R1yGyu[max_D1D][vdim];

            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int c = 0; c < vdim; c++)
               {
                  R0xGxu[d1][c] = 0;
                  R0yGyu[d1][c] = 0;
                  R1xGxu[d1][c] = 0;
                  R1yGyu[d1][c] = 0;
                  for (int p = 0; p < D1D; p++)
                  {
                     R0xGxu[d1][c] += Ge_(0,p,e)*u[p][d1][c];
                     R0yGyu[d1][c] += Ge_(0,p,e)*u[d1][p][c];
                     R1xGxu[d1][c] += Ge_(end,p,e)*u[p][d1][c];
                     R1yGyu[d1][c] += Ge_(end,p,e)*u[d1][p][c];
                  }
               }
            }

//   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

            // y = 0 face
            int face_id = 0;
            int face = map_elements_to_faces[num_faces_per_element*e + face_id];
            int side = map_elements_to_sides[num_faces_per_element*e + face_id];
            std::cout << "e " << e << "  faceid " << face_id << " face " << face << " side " << side << std::endl;
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               int fdof = d1;
               if(side==1)
               {
                  fdof = map_side_permutations[ndofs_face*face + fdof];
               }
               double sign = (side==1) ? -1.0 : 1.0;
               double face_pt_adj_x = sign*jac_face_factor(0, d1, face_id, e);
               double face_pt_adj_y = sign*jac_face_factor(1, d1, face_id, e);
               for (int c = 0; c < vdim; c++)
               {
                  d_y_new(fdof , c, side, face, 0) += R0yu[d1][c];
                  double dnormal = 0;
                  dnormal += face_pt_adj_x*R0yGxu[d1][c];
                  dnormal += face_pt_adj_y*R0yGyu[d1][c];
                  d_y_new(fdof , c, side, face, 1) += dnormal;
               }
            }

//   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

            // x = 1 face
            face_id = 1;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            std::cout << "e " << e << "  faceid " << face_id << " face " << face << " side " << side << std::endl;
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               int fdof = d1;
               if(side==1)
               {
                  fdof = map_side_permutations[ndofs_face*face + fdof];
               }
               double sign = (side==1) ? -1.0 : 1.0;
               double face_pt_adj_x = sign*jac_face_factor(0, d1, face_id, e);
               double face_pt_adj_y = sign*jac_face_factor(1, d1, face_id, e);
               for (int c = 0; c < vdim; c++)
               {
                  d_y_new(fdof , c, side, face, 0) += R1xu[d1][c];
                  double dnormal = 0;
                  dnormal += face_pt_adj_x*R1xGxu[d1][c];
                  dnormal += face_pt_adj_y*R1xGyu[d1][c];
                  d_y_new(fdof , c, side, face, 1) += dnormal;
               }
            }

            // y = 1 face
            face_id = 2;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];

            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               int fdof = d1;
               if(side==1)
               {
                  fdof = map_side_permutations[ndofs_face*face + fdof];
               }
               double sign = (side==1) ? -1.0 : 1.0;
               double face_pt_adj_x = sign*jac_face_factor(0, d1, face_id, e);
               double face_pt_adj_y = sign*jac_face_factor(1, d1, face_id, e);
               for (int c = 0; c < vdim; c++)
               {
                  d_y_new(fdof , c, side, face, 0) += R1yu[d1][c];
                  double dnormal = 0;
                  dnormal += face_pt_adj_x*R1yGxu[d1][c];
                  dnormal += face_pt_adj_y*R1yGyu[d1][c];
                  d_y_new(fdof , c, side, face, 1) += dnormal;
               }
            }

            // x = 0 face
            face_id = 3;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            std::cout << "e " << e << "  faceid " << face_id << " face " << face << " side " << side << std::endl;
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               int fdof = d1;
               if(side==1)
               {
                  fdof = map_side_permutations[ndofs_face*face + fdof];
               }
               double sign = (side==1) ? -1.0 : 1.0;
               double face_pt_adj_x = sign*jac_face_factor(0, d1, face_id, e);
               double face_pt_adj_y = sign*jac_face_factor(1, d1, face_id, e);
               for (int c = 0; c < vdim; c++)
               {
                  d_y_new(fdof , c, side, face, 0) += R0xu[d1][c];
                  double dnormal = 0;
                  dnormal += face_pt_adj_x*R0xGxu[d1][c];
                  dnormal += face_pt_adj_y*R0xGyu[d1][c];
                  d_y_new(fdof , c, side, face, 1) += dnormal;
               }
            }
         });
      }
      else if(dim == 3)
      {
         // Loop over all elements
         MFEM_FORALL(e_, num_needed_elements,
         {
            int e = needed_elements[e_];

            double u[max_D1D][max_D1D][max_D1D][vdim];

            // Roll element dofs into tensor form
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int d3 = 0; d3 < D1D; d3++)
                  {
                     int did = d1 + D1D*d2 + D1D*D1D*d3;
                     int idx = elementMap[e*elem_dofs + did];
                     for (int c = 0; c < vdim; c++)
                     {
                        u[d1][d2][d3][c] = d_x(t?c:idx, t?idx:c);
                     }
                  }
               }
            }

            // Restrict to faces
            double R0xu[max_D1D][max_D1D][vdim];
            double R0yu[max_D1D][max_D1D][vdim];
            double R0zu[max_D1D][max_D1D][vdim];

            double R1xu[max_D1D][max_D1D][vdim];
            double R1yu[max_D1D][max_D1D][vdim];
            double R1zu[max_D1D][max_D1D][vdim];

            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int c = 0; c < vdim; c++)
                  {
                     // xi = 0 face
                     R0xu[d1][d2][c] = u[0][d1][d2][c];
                     R0yu[d1][d2][c] = u[d1][0][d2][c];
                     R0zu[d1][d2][c] = u[d1][d2][0][c];

                     // xi = 1 face
                     R1xu[d1][d2][c] = u[end][d1][d2][c];
                     R1yu[d1][d2][c] = u[d1][end][d2][c];
                     R1zu[d1][d2][c] = u[d1][d2][end][c];
                  }
               }
            }
            
            double R0yGxu[max_D1D][max_D1D][vdim];
            double R0zGxu[max_D1D][max_D1D][vdim];

            double R0xGyu[max_D1D][max_D1D][vdim];
            double R0zGyu[max_D1D][max_D1D][vdim];

            double R0xGzu[max_D1D][max_D1D][vdim];
            double R0yGzu[max_D1D][max_D1D][vdim];
            
            double R1yGxu[max_D1D][max_D1D][vdim];
            double R1zGxu[max_D1D][max_D1D][vdim];

            double R1xGyu[max_D1D][max_D1D][vdim];
            double R1zGyu[max_D1D][max_D1D][vdim];

            double R1xGzu[max_D1D][max_D1D][vdim];
            double R1yGzu[max_D1D][max_D1D][vdim];

            // Apply derivative operator to each dimension
            // todo - maybe rewrite this to improve stride
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int c = 0; c < vdim; c++)
                  {
                     R0yGxu[d1][d2][c] = 0;
                     R0zGxu[d1][d2][c] = 0;
                     R0xGyu[d1][d2][c] = 0;
                     R0zGyu[d1][d2][c] = 0;
                     R0xGzu[d1][d2][c] = 0;
                     R0yGzu[d1][d2][c] = 0;
                     R1yGxu[d1][d2][c] = 0;
                     R1zGxu[d1][d2][c] = 0;
                     R1xGyu[d1][d2][c] = 0;
                     R1zGyu[d1][d2][c] = 0;
                     R1xGzu[d1][d2][c] = 0;
                     R1yGzu[d1][d2][c] = 0;

                     for (int p = 0; p < D1D; p++)
                     {
                        // xi = 0 faces
                        R0yGxu[d1][d2][c] += Ge_(d1,p,e)*R0yu[p][d2][c];
                        R0zGxu[d1][d2][c] += Ge_(d2,p,e)*R0zu[p][d2][c];

                        R0xGyu[d1][d2][c] += Ge_(d1,p,e)*R0xu[p][d2][c];
                        R0zGyu[d1][d2][c] += Ge_(d2,p,e)*R0zu[d1][p][c];

                        R0xGzu[d1][d2][c] += Ge_(d1,p,e)*R0xu[d1][p][c];
                        R0yGzu[d1][d2][c] += Ge_(d2,p,e)*R0yu[d1][p][c];
                        
                        // xi = 1 faces
                        R1yGxu[d1][d2][c] += Ge_(d1,p,e)*R1yu[p][d2][c];
                        R1zGxu[d1][d2][c] += Ge_(d2,p,e)*R1zu[p][d2][c];

                        R1xGyu[d1][d2][c] += Ge_(d1,p,e)*R1xu[p][d2][c];
                        R1zGyu[d1][d2][c] += Ge_(d2,p,e)*R1zu[d1][p][c];

                        R1xGzu[d1][d2][c] += Ge_(d1,p,e)*R1xu[d1][p][c];
                        R1yGzu[d1][d2][c] += Ge_(d2,p,e)*R1yu[d1][p][c];

                     }
                  }
               }
            }

            double R0xGxu[max_D1D][max_D1D][vdim];
            double R0yGyu[max_D1D][max_D1D][vdim];
            double R0zGzu[max_D1D][max_D1D][vdim];
            double R1xGxu[max_D1D][max_D1D][vdim];
            double R1yGyu[max_D1D][max_D1D][vdim];
            double R1zGzu[max_D1D][max_D1D][vdim];

            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int c = 0; c < vdim; c++)
                  {
                     R0xGxu[d1][d2][c] = 0;
                     R0yGyu[d1][d2][c] = 0;
                     R0zGzu[d1][d2][c] = 0;
                     R1xGxu[d1][d2][c] = 0;
                     R1yGyu[d1][d2][c] = 0;
                     R1zGzu[d1][d2][c] = 0;
                     for (int p = 0; p < D1D; p++)
                     {
                        R0xGxu[d1][d2][c] += Ge_(0,p,e)*u[p][d1][d2][c];
                        R0yGyu[d1][d2][c] += Ge_(0,p,e)*u[d1][p][d2][c];
                        R0zGzu[d1][d2][c] += Ge_(0,p,e)*u[d1][d2][p][c];
                        R1xGxu[d1][d2][c] += Ge_(end,p,e)*u[p][d1][d2][c];
                        R1yGyu[d1][d2][c] += Ge_(end,p,e)*u[d1][p][d2][c];
                        R1zGzu[d1][d2][c] += Ge_(end,p,e)*u[d1][d2][p][c];
                     }
                  }
               }
            }

            // z = 0 face
            int face_id = 0;
            int face = map_elements_to_faces[num_faces_per_element*e + face_id];
            int side = map_elements_to_sides[num_faces_per_element*e + face_id];
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int fdof = d1 + D1D*d2;
                  if(side==1)
                  {
                     fdof = map_side_permutations[ndofs_face*face + fdof];
                  }
                  double sign = (side==1) ? -1.0 : 1.0;
                  double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                  double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                  double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                  for (int c = 0; c < vdim; c++)
                  {
                     d_y_new(fdof , c, side, face, 0) += R0zu[d1][d2][c];
                     double dnormal = 0;
                     dnormal += face_pt_adj_x*R0zGxu[d1][d2][c];
                     dnormal += face_pt_adj_y*R0zGyu[d1][d2][c];
                     dnormal += face_pt_adj_z*R0zGzu[d1][d2][c];
                     d_y_new(fdof , c, side, face, 1) += dnormal;
                  }
               }
            }

            // y = 0 face
            face_id = 1;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int fdof = d1 + D1D*d2;
                  if(side==1)
                  {
                     fdof = map_side_permutations[ndofs_face*face + fdof];
                  }
                  double sign = (side==1) ? -1.0 : 1.0;
                  double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                  double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                  double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                  for (int c = 0; c < vdim; c++)
                  {
                     d_y_new(fdof , c, side, face, 0) += R0yu[d1][d2][c];
                     double dnormal = 0;
                     dnormal += face_pt_adj_x*R0yGxu[d1][d2][c];
                     dnormal += face_pt_adj_y*R0yGyu[d1][d2][c];
                     dnormal += face_pt_adj_z*R0yGzu[d1][d2][c];
                     d_y_new(fdof , c, side, face, 1) += dnormal;
                  }
               }
            }

            // x = 1 face
            face_id = 2;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int fdof = d1 + D1D*d2;
                  if(side==1)
                  {
                     fdof = map_side_permutations[ndofs_face*face + fdof];
                  }
                  double sign = (side==1) ? -1.0 : 1.0;
                  double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                  double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                  double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                  for (int c = 0; c < vdim; c++)
                  {
                     d_y_new(fdof , c, side, face, 0) += R1xu[d1][d2][c];
                     double dnormal = 0;
                     dnormal += face_pt_adj_x*R1xGxu[d1][d2][c];
                     dnormal += face_pt_adj_y*R1xGyu[d1][d2][c];
                     dnormal += face_pt_adj_z*R1xGzu[d1][d2][c];
                     d_y_new(fdof , c, side, face, 1) += dnormal;
                  }
               }
            }

            // y = 1 
            face_id = 3;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int fdof = d1 + D1D*d2;
                  if(side==1)
                  {
                     fdof = map_side_permutations[ndofs_face*face + fdof];
                  }
                  double sign = (side==1) ? -1.0 : 1.0;
                  double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                  double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                  double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                  for (int c = 0; c < vdim; c++)
                  {
                     d_y_new(fdof , c, side, face, 0) += R1yu[d1][d2][c];
                     double dnormal = 0;
                     dnormal += face_pt_adj_x*R1yGxu[d1][d2][c];
                     dnormal += face_pt_adj_y*R1yGyu[d1][d2][c];
                     dnormal += face_pt_adj_z*R1yGzu[d1][d2][c];
                     d_y_new(fdof , c, side, face, 1) += dnormal;
                  }
               }
            }

            // x = 0 face
            face_id = 4;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int fdof = d1 + D1D*d2;
                  if(side==1)
                  {
                     fdof = map_side_permutations[ndofs_face*face + fdof];
                  }
                  double sign = (side==1) ? -1.0 : 1.0;
                  double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                  double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                  double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                  for (int c = 0; c < vdim; c++)
                  {
                     d_y_new(fdof , c, side, face, 0) += R0xu[d1][d2][c];
                     double dnormal = 0;
                     dnormal += face_pt_adj_x*R0xGxu[d1][d2][c];
                     dnormal += face_pt_adj_y*R0xGyu[d1][d2][c];
                     dnormal += face_pt_adj_z*R0xGzu[d1][d2][c];
                     d_y_new(fdof, c, side, face, 1) += dnormal;
                  }
               }
            }

            // z = 1 face
            face_id = 5;
            face = map_elements_to_faces[num_faces_per_element*e + face_id];
            side = map_elements_to_sides[num_faces_per_element*e + face_id];
            if(side>=0)
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int fdof = d1 + D1D*d2;
                  if(side==1)
                  {
                     fdof = map_side_permutations[ndofs_face*face + fdof];
                  }
                  double sign = (side==1) ? -1.0 : 1.0;
                  double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                  double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                  double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                  for (int c = 0; c < vdim; c++)
                  {
                     d_y_new(fdof , c, side, face, 0) += R1zu[d1][d2][c];
                     double dnormal = 0;
                     dnormal += face_pt_adj_x*R1zGxu[d1][d2][c];
                     dnormal += face_pt_adj_y*R1zGyu[d1][d2][c];
                     dnormal += face_pt_adj_z*R1zGzu[d1][d2][c];
                     d_y_new(fdof , c, side, face, 1) += dnormal;
                  }
               }
            }
         });
      }
   }
   else
   {
      MFEM_ABORT("Invalid dim for RestrictionMult");
   }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

   #if restrict_use == 0
   Vector ydiff = y_old;
   ydiff -= y;

   std::cout << "y_old" << std::endl;
   y_old.Print(std::cout,1);

   std::cout << "y_new " << std::endl;
   y.Print(std::cout,1);

   double err = ydiff.Normlinf();
   std::cout << "mult error " << std::endl << err << std::endl;
    if(err > 1.0e-11) 
    {
        exit(1);
    }
    #endif

#endif

#endif


#ifdef MFEM_DEBUG
   std::cout << " restrict y" << std::endl;
   y.Print(std::cout,1);
   std::cout << " end restrict y" << std::endl;
#endif


}

void L2FaceNormalDRestriction::AddMultTranspose(const Vector& x, Vector& y) const
{

#ifdef MFEM_DEBUG
   std::cout << "multT x " << std::endl;
   x.Print(std::cout,1);
   std::cout << "end multT x " << std::endl;
#endif

   auto u_face    = Reshape(Bf.Read(), ndofs1d, ndofs_face, nf, num_values_per_point);
   auto dudn_face = Reshape(Gf.Read(), ndofs1d, ndofs_face, nf, num_values_per_point, 3);

   // Assumes all elements have the same number of dofs
   const int dim = fes.GetMesh()->SpaceDimension();
   const int vd = vdim;
   const bool t = byvdim;
   const int half = numfacedofs;
   const int half_grad = ndofs1d*numfacedofs;
   auto d_offsets = offsets.Read();
   auto d_offsets_nor = offsets_nor.Read();
   auto d_offsets_tan1 = offsets_tan1.Read();
   auto d_offsets_tan2 = offsets_tan2.Read();
   auto d_indices = gather_indices.Read();
   auto d_indices_nor = gather_indices_nor.Read();
   auto d_indices_tan1 = gather_indices_tan1.Read();
   auto d_indices_tan2 = gather_indices_tan2.Read();

// -1 old, 0 both, 1 new 
#define restrict_transpose_use 1

#if restrict_transpose_use < 1

   if (m == L2FaceValues::DoubleValued)
   {
      auto d_x = Reshape(x.Read(), ndofs_face, vd, 2, nf, num_values_per_point);
      auto d_y = Reshape(y.Write(), t?vd:ndofs, t?ndofs:vd);
      MFEM_FORALL(i, ndofs,
      {
         // 
         int offset = d_offsets[i];
         int nextOffset = d_offsets[i + 1];
         //std::cout << "offsets : " << offset << " to " << nextOffset << std::endl;

         for (int c = 0; c < vd; ++c)
         {
            for (int j = offset; j < nextOffset; ++j)
            {
               int idx_j0 = d_indices[j];

               bool isE1 = idx_j0 < half;
               // we use e1 e2 but then use D0 D1 in the PA kernel
               int idx_j = isE1 ? idx_j0 : idx_j0 - half;

               int did, faceid;
               GetFromLid( idx_j, did, faceid, ndofs_face);

               if( isE1 )
               {
                  d_y(t?c:i,t?i:c) += d_x( did, c, 0, faceid, 0);
               }
               else
               {
                  d_y(t?c:i,t?i:c) += d_x( did, c, 1, faceid, 0);
               }
            }
         }

         offset = d_offsets_nor[i];
         nextOffset = d_offsets_nor[i + 1];
         for (int c = 0; c < vd; ++c)
         {
            for (int j = offset; j < nextOffset; ++j)
            {
               int idx_j0 = d_indices_nor[j];

               bool isE1 = idx_j0 < half_grad;
               // we use e1 e2 but then use D0 D1 in the PA kernel
               int idx_j = isE1 ? idx_j0 : idx_j0 - half_grad;

               int s, did, faceid;
               GetFromLid( idx_j, did, s, faceid, ndofs1d, ndofs_face);


               int e = i/elemDofs;
               int face_num = -1;

               for( int face_id = 0 ; face_id < num_faces_per_element ; face_id++ )
               {
                    if( faceid == map_elements_to_faces[num_faces_per_element*e + face_id] )
                    {
                       face_num = face_id;
                    }
               }
               //std::cout << "e " << e << " face_id " << face_num << " face " << faceid << " side " << 1-isE1 << std::endl; 

               //  if( (face_num <= 2 ) )//|| (face_num==3) || (face_num==1) ) 
              if( isE1 )
               {
                  d_y(t?c:i,t?i:c) += d_x( did, c, 0, faceid, 1)*dudn_face(s,did,faceid,0,0);
               }
               else
               {
                  d_y(t?c:i,t?i:c) += d_x( did, c, 1, faceid, 1)*dudn_face(s,did,faceid,1,0);
               }
            }
         }



         offset = d_offsets_tan1[i];
         nextOffset = d_offsets_tan1[i + 1];
         for (int c = 0; c < vd; ++c)
         {
            for (int j = offset; j < nextOffset; ++j)
            {
               int idx_j0 = d_indices_tan1[j];
               bool isE1 = idx_j0 < half_grad;
               // we use e1 e2 but then use D0 D1 in the PA kernel 
               int idx_j = isE1 ? idx_j0 : idx_j0 - half_grad;

               int s, did, faceid;
               GetFromLid( idx_j, did, s, faceid, ndofs1d, ndofs_face);


               int e = i/elemDofs;
               int face_num = -1;

               for( int face_id = 0 ; face_id < num_faces_per_element ; face_id++ )
               {
                    if( faceid == map_elements_to_faces[num_faces_per_element*e + face_id] )
                    {
                       face_num = face_id;
                    }
               }

                //if( (face_num <= 4 ) )//|| (face_num==3) || (face_num==1) ) 
               if( isE1 )
               {
                  d_y(t?c:i,t?i:c) += d_x( did, c, 0, faceid, 1)*dudn_face(s,did,faceid,0,1);
               }
               else
               {
                  d_y(t?c:i,t?i:c) += d_x( did, c, 1, faceid, 1)*dudn_face(s,did,faceid,1,1);
               }
            }
         }


         if( dim >= 3 )
         {
            offset = d_offsets_tan2[i];
            nextOffset = d_offsets_tan2[i + 1];
            for (int c = 0; c < vd; ++c)
            {
               for (int j = offset; j < nextOffset; ++j)
               {
                  int idx_j0 = d_indices_tan2[j];
                  bool isE1 = idx_j0 < half_grad;
                  // we use e1 e2 but then use D0 D1 in the PA kernel 
                  int idx_j = isE1 ? idx_j0 : idx_j0 - half_grad;

                  int s, did, faceid;
                  GetFromLid( idx_j, did, s, faceid, ndofs1d, ndofs_face);

                  if( isE1 )
                  {
                     d_y(t?c:i,t?i:c) += d_x( did, c, 0, faceid, 1)*dudn_face(s,did,faceid,0,2);
                  }
                  else
                  {
                     d_y(t?c:i,t?i:c) += d_x( did, c, 1, faceid, 1)*dudn_face(s,did,faceid,1,2);
                  }
               }
            }
         }


      });
   }
   else
   {

   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

      mfem_error("not yet implemented.");
   }

#endif 


#ifdef MFEM_DEBUG
   std::cout << "multT y " << std::endl;
   y.Print(std::cout,1);
   std::cout << "end multT y " << std::endl;
#endif


#if restrict_transpose_use > -1

   #ifdef MFEM_DEBUG
      std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;
      Vector y_old;
      y_old = y;
   #endif

   y = 0.0;


   int D1D = ndofs1d;
   int end = D1D - 1;
   auto Ge_ = Reshape(Ge.Read(), D1D, D1D, ne);
   int max_D1D = D1D;

   const Table& e2dTable = fes.GetElementToDofTable();
   const int* elementMap = e2dTable.GetJ();

   if (m==L2FaceValues::DoubleValued)
   {
      auto d_x = Reshape(x.Read(), ndofs_face, vd, 2, nf, num_values_per_point);
      auto d_y = Reshape(y.Write(), t?vd:ndofs, t?ndofs:vd);

      auto jac_face_factor = Reshape( jac_face_factors.Read(), dim, ndofs_face, num_faces_per_element, ne);
      int elem_dofs = (dim==3)? D1D*D1D*D1D : D1D*D1D;

      int D1D = ndofs1d;
      int end = D1D - 1;
      auto Get_ = Reshape(Ge.Read(), D1D, D1D, ne);
      int max_D1D = D1D;

      if(dim == 2)
      {
         // Loop over all elements
         MFEM_FORALL(e_, num_needed_elements,
         {
            int e = needed_elements[e_];

            double u[max_D1D][max_D1D][vdim];
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int c = 0; c < vdim; c++)
                  {
                     u[d1][d2][c] = 0.;
                  }
               }
            }

#ifdef MFEM_DEBUG
            for( int d1 = 0 ; d1 < D1D ; d1++ )
            {
                std::cout << "Get_("<<d1<<",:,"<<e<<") = "  ;
                for( int p = 0 ; p < D1D ; p++ )
                {
                 std::cout << Get_(d1,p,e) << "  " ;
                }
                std::cout << std::endl;
            }
#endif

            for( int face_id = 0 ; face_id < num_faces_per_element ; face_id++ )
            {
               int face = map_elements_to_faces[num_faces_per_element*e + face_id];
               int side = map_elements_to_sides[num_faces_per_element*e + face_id];

              // std::cout << "e " << e << " face_id " << face_id << " face " << face << " side " << side << std::endl; 

               if(side>=0)
               {
                  double Ru[max_D1D][vdim];
                  double RGxu[max_D1D][vdim];
                  double RGyu[max_D1D][vdim];
                  for (int d1 = 0; d1 < D1D; d1++)
                  {
                     int fdof = d1;
                     if(side==1)
                     {
                        fdof = map_side_permutations[ndofs_face*face + fdof];
                     }
                     double sign = (side==1) ? -1.0 : 1.0;
                     double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                     double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                     for (int c = 0; c < vdim; c++)
                     {
                        double uf  = d_x(fdof, c, side, face, 0);
                        double duf = d_x(fdof, c, side, face, 1);
                        const int lid1 = GetLid(d1, face, ndofs_face);
                        const int ind1 = Get_dof_from_ijk(d1,D1D-1,0,D1D);
                        if( ind1 != lid1 )
                        {
                           std::cout << "fdof " << fdof << " d1 " << d1 << " id1 " << lid1 << " ind1 " << ind1 << " side " << side  << std::endl;
                           //exit(1);
                        }
                        Ru[d1][c] = uf;
                        RGxu[d1][c] = face_pt_adj_x*duf;
                        RGyu[d1][c] = face_pt_adj_y*duf;
                     }
                  }

                  switch(face_id)
                  {

                     case 0: // y = 0 face
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int c = 0; c < vdim; c++)
                           {
                              u[d1][0][c] += Ru[d1][c];
                              for (int p = 0; p < D1D; p++)
                              {
                                 // tangent
                                 u[d1][0][c] += Get_(d1,p,e)*RGxu[p][c];
                                 // normal
                                 u[d1][p][c] += Get_(0,p,e)*RGyu[d1][c];
                              }
                           }
                        }
                        break;
                     case 1: // x = 1 face
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int c = 0; c < vdim; c++)
                           {
                              u[end][d1][c] += Ru[d1][c];
                              for (int p = 0; p < D1D; p++)
                              {
                                 // normal
                                 u[p][d1][c] += Get_(end,p,e)*RGxu[d1][c];
                                 // tangent
                                 u[end][d1][c] += Get_(p,d1,e)*RGyu[p][c];
                               }
                           }
                        }
                        break;

                     case 2: // y = 1  
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int c = 0; c < vdim; c++)
                           {
                              u[d1][end][c] += Ru[d1][c];
                              for (int p = 0; p < D1D; p++)
                              {
                                 // tangent
                                 u[d1][end][c] += Get_(p,d1,e)*RGxu[p][c];
                                 // normal
                                 u[d1][p][c] += Get_(end,p,e)*RGyu[d1][c];
                              }
                           }
                        }
                        break;
                     case 3: // x = 0 face 
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int c = 0; c < vdim; c++)
                           {
                              u[0][d1][c] += Ru[d1][c];
                              for (int p = 0; p < D1D; p++)
                              {
                                 // normal
                                 u[p][d1][c] += Get_(0,p,e)*RGxu[d1][c];
                                 // tangent
                                 u[0][d1][c] += Get_(d1,p,e)*RGyu[p][c];
                              }
                           }
                        }
                        break;
                  }
               }
            }

            // Unroll element dofs from tensor form
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  int did = d1 + D1D*d2;
                  int idx = elementMap[e*elem_dofs + did];
                  for (int c = 0; c < vdim; c++)
                  {
                     // does this need to be atomic?
                     d_y(t?c:idx, t?idx:c) += u[d1][d2][c];
                  }
               }
            }
         });
      }
      if(dim == 3)
      {
         // Loop over all elements
         MFEM_FORALL(e_, num_needed_elements,
         {
            int e = needed_elements[e_];

            double u[max_D1D][max_D1D][max_D1D][vdim];
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int d3 = 0; d3 < D1D; d3++)
                  {
                     for (int c = 0; c < vdim; c++)
                     {
                        u[d1][d2][d3][c] = 0.;
                     }
                  }     
               }
            }

            for( int face_id = 0 ; face_id < num_faces_per_element ; face_id++ )
            {
               int face = map_elements_to_faces[num_faces_per_element*e + face_id];
               int side = map_elements_to_sides[num_faces_per_element*e + face_id];
               if(side>=0)
               {
                  double Ru[max_D1D][max_D1D][vdim];
                  double RGxu[max_D1D][max_D1D][vdim];
                  double RGyu[max_D1D][max_D1D][vdim];
                  double RGzu[max_D1D][max_D1D][vdim];
                  for (int d1 = 0; d1 < D1D; d1++)
                  {
                     for (int d2 = 0; d2 < D1D; d2++)
                     {
                        int fdof = d1 + D1D*d2;
                        if(side==1)
                        {
                           fdof = map_side_permutations[ndofs_face*face + fdof];
                        }
                        double sign = (side==1) ? -1.0 : 1.0;
                        double face_pt_adj_x = sign*jac_face_factor(0, fdof, face_id, e);
                        double face_pt_adj_y = sign*jac_face_factor(1, fdof, face_id, e);
                        double face_pt_adj_z = sign*jac_face_factor(2, fdof, face_id, e);
                        for (int c = 0; c < vdim; c++)
                        {
                           double u  = d_x(fdof, c, side, face, 0);
                           double du = d_x(fdof, c, side, face, 1);
                           Ru[d1][d2][c] = u;
                           RGxu[d1][d2][c] = face_pt_adj_x*du;
                           RGyu[d1][d2][c] = face_pt_adj_y*du;
                           RGzu[d1][d2][c] = face_pt_adj_z*du;
                        }
                     }
                  }

                  switch(face_id)
                  {
                     case 0: // z = 0 face
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int d2 = 0; d2 < D1D; d2++)
                           {
                              for (int c = 0; c < vdim; c++)
                              {                                 
                                 u[d1][d2][0][c] += Ru[d1][d2][c];
                                 for (int p = 0; p < D1D; p++)
                                 {
                                    u[d1][d2][0][c] += Get_(d1,p,e)*RGxu[p][d2][c];
                                    u[d1][d2][0][c] += Get_(d2,p,e)*RGyu[d1][p][c];
                                    u[d1][d2][p][c] += Get_(0,p,e)*RGzu[d1][d2][c];
                                 }
                              }
                           }
                        }
                        break;
                     case 1: // y = 0 face
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int d2 = 0; d2 < D1D; d2++)
                           {
                              for (int c = 0; c < vdim; c++)
                              {
                                 u[d1][0][d2][c] += Ru[d1][d2][c];
                                 for (int p = 0; p < D1D; p++)
                                 {
                                    u[d1][0][d2][c] += Get_(d1,p,e)*RGxu[p][d2][c];
                                    u[d1][p][d2][c] += Get_(0,p,e)*RGyu[d1][d2][c];
                                    u[d1][0][d2][c] += Get_(d2,p,e)*RGzu[d1][p][c];
                                 }
                              }
                           }
                        }
                        break;
                     case 2: // x = 1 face
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int d2 = 0; d2 < D1D; d2++)
                           {
                              for (int c = 0; c < vdim; c++)
                              {
                                 u[end][d1][d2][c] += Ru[d1][d2][c];
                                 for (int p = 0; p < D1D; p++)
                                 {
                                    u[p][d1][d2][c] += Get_(end,p,e)*RGxu[d1][d2][c];
                                    u[end][d1][d2][c] += Get_(d2,p,e)*RGyu[p][d2][c];
                                    u[end][d1][d2][c] += Get_(d1,p,e)*RGzu[d1][p][c];
                                 }
                              }
                           }
                        }
                        break;
                     case 3: // y = 1  
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int d2 = 0; d2 < D1D; d2++)
                           {
                              for (int c = 0; c < vdim; c++)
                              {
                                 u[d1][end][d2][c] += Ru[d1][d2][c];
                                 for (int p = 0; p < D1D; p++)
                                 {
                                    u[d1][end][d2][c] += Get_(d1,p,e)*RGxu[p][d2][c];
                                    u[d1][p][d2][c] += Get_(end,p,e)*RGyu[d1][d2][c];
                                    u[d1][end][d2][c] += Get_(d2,p,e)*RGzu[d1][p][c];
                                 }
                              }
                           }
                        }
                        break;
                     case 4: // x = 0 face 
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int d2 = 0; d2 < D1D; d2++)
                           {
                              for (int c = 0; c < vdim; c++)
                              {
                                 u[0][d1][d2][c] += Ru[d1][d2][c];
                                 for (int p = 0; p < D1D; p++)
                                 {
                                    u[p][d1][d2][c] += Get_(0,p,e)*RGxu[d1][d2][c];
                                    u[0][d1][d2][c] += Get_(d2,p,e)*RGyu[p][d2][c];
                                    u[0][d1][d2][c] += Get_(d1,p,e)*RGzu[d1][p][c];
                                 }
                              }
                           }
                        }
                        break;
                     case 5: // z = 1 face
                        for (int d1 = 0; d1 < D1D; d1++)
                        {
                           for (int d2 = 0; d2 < D1D; d2++)
                           {
                              for (int c = 0; c < vdim; c++)
                              {
                                 u[d1][d2][end][c] += Ru[d1][d2][c];
                                 for (int p = 0; p < D1D; p++)
                                 {
                                    u[d1][d2][end][c] += Get_(d1,p,e)*RGxu[p][d2][c];
                                    u[d1][d2][end][c] += Get_(d2,p,e)*RGyu[d1][p][c];
                                    u[d1][d2][p][c] += Get_(end,p,e)*RGzu[d1][d2][c];
                                 }
                              }
                           }
                        }
                        break;
                  }
               }
            }

            // Unroll element dofs from tensor form
            for (int d1 = 0; d1 < D1D; d1++)
            {
               for (int d2 = 0; d2 < D1D; d2++)
               {
                  for (int d3 = 0; d3 < D1D; d3++)
                  {
                     int did = d1 + D1D*d2 + D1D*D1D*d3;
                     int idx = elementMap[e*elem_dofs + did];
                     for (int c = 0; c < vdim; c++)
                     {
                        // does this need to be atomic?
                        d_y(t?c:idx, t?idx:c) += u[d1][d2][d3][c];
                     }
                  }
               }
            }
         });
      }
   }
   else
   {
      MFEM_ABORT("Invalid dim for RestrictionMult");
   }

#ifdef MFEM_DEBUG
   std::cout << "% " << __LINE__ << " in " << __FUNCTION__ << " in " << __FILE__ << std::endl;

   #if restrict_transpose_use == 0
   Vector ydiff = y_old;
   ydiff -= y;
   
   std::cout << "y_old transpose" << std::endl;
   y_old.Print(std::cout,1);

   std::cout << "y_new " << std::endl;
   y.Print(std::cout,1);

    double err = ydiff.Normlinf();
   std::cout << "mult transpose error " << std::endl << err << std::endl;
   
    if(err > 1.0e-4) 
    {
        exit(1);
    }

    #endif

#endif

#endif

#ifdef MFEM_DEBUG
   std::cout << " restrict y" << std::endl;
   y.Print(std::cout,1);
   std::cout << " end restrict y" << std::endl;
#endif



}
}
