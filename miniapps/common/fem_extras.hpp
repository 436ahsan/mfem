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

#ifndef MFEM_FEM_EXTRAS
#define MFEM_FEM_EXTRAS

#include "mfem.hpp"
#include <cstddef>

namespace mfem
{

namespace common
{

/** The H1_FESpace class is a FiniteElementSpace which automatically
    allocates and destroys its own FiniteElementCollection, in this
    case an H1_FECollection object.
*/
class H1_FESpace : public FiniteElementSpace
{
public:
   H1_FESpace(Mesh *m,
              const int p, const int space_dim = 3,
              const int type = BasisType::GaussLobatto,
              int vdim = 1, int order = Ordering::byNODES);
   ~H1_FESpace();
private:
   const FiniteElementCollection *FEC_;
};

/** The ND_FESpace class is a FiniteElementSpace which automatically
    allocates and destroys its own FiniteElementCollection, in this
    case an ND_FECollection object.
*/
class ND_FESpace : public FiniteElementSpace
{
public:
   ND_FESpace(Mesh *m, const int p, const int space_dim,
              int vdim = 1, int order = Ordering::byNODES);
   ~ND_FESpace();
private:
   const FiniteElementCollection *FEC_;
};

/** The RT_FESpace class is a FiniteElementSpace which automatically
    allocates and destroys its own FiniteElementCollection, in this
    case an RT_FECollection object.
*/
class RT_FESpace : public FiniteElementSpace
{
public:
   RT_FESpace(Mesh *m, const int p, const int space_dim,
              int vdim = 1, int order = Ordering::byNODES);
   ~RT_FESpace();
private:
   const FiniteElementCollection *FEC_;
};


/** The L2_FESpace class is a FiniteElementSpace which automatically
    allocates and destroys its own FiniteElementCollection, in this
    case an L2_FECollection object.
*/
class L2_FESpace : public FiniteElementSpace
{
public:
   L2_FESpace(Mesh *m, const int p, const int space_dim,
              int vdim = 1, int order = Ordering::byNODES);
   ~L2_FESpace();
private:
   const FiniteElementCollection *FEC_;
};

class DiscreteInterpolationOperator : public DiscreteLinearOperator
{
public:
   DiscreteInterpolationOperator(FiniteElementSpace *dfes,
                                 FiniteElementSpace *rfes)
      : DiscreteLinearOperator(dfes, rfes) {}
   virtual ~DiscreteInterpolationOperator();
};

class DiscreteGradOperator : public DiscreteInterpolationOperator
{
public:
   DiscreteGradOperator(FiniteElementSpace *dfes,
                        FiniteElementSpace *rfes);
};

class DiscreteCurlOperator : public DiscreteInterpolationOperator
{
public:
   DiscreteCurlOperator(FiniteElementSpace *dfes,
                        FiniteElementSpace *rfes);
};

class DiscreteDivOperator : public DiscreteInterpolationOperator
{
public:
   DiscreteDivOperator(FiniteElementSpace *dfes,
                       FiniteElementSpace *rfes);
};

class CoefFactory
{
protected:
   Array<Coefficient*> coefs;                ///< Owned
   Array<GridFunction*> ext_gf;              ///< Not owned
   Array<double (*)(const Vector &)> ext_fn; ///< Not owned

public:
   CoefFactory() {}

   virtual ~CoefFactory();

   // void StealData(Array<Coefficient*> &c) { c = coefs; coefs.LoseData(); }

   int AddExternalGridFunction(GridFunction &gf) { return ext_gf.Append(&gf); }

   int AddExternalFunction(double (*fn)(const Vector &))
   { return ext_fn.Append(fn); }

   virtual Coefficient *operator()(std::istream &input);
   virtual Coefficient *operator()(std::string &coef_name, std::istream &input);
};

/// Visualize the given mesh object, using a GLVis server on the
/// specified host and port. Set the visualization window title, and optionally,
/// its geometry.
void VisualizeMesh(socketstream &sock, const char *vishost, int visport,
                   Mesh &mesh, const char *title,
                   int x = 0, int y = 0, int w = 400, int h = 400,
                   const char *keys = NULL);

/// Visualize the given grid function, using a GLVis server on the
/// specified host and port. Set the visualization window title, and optionally,
/// its geometry.
void VisualizeField(socketstream &sock, const char *vishost, int visport,
                    GridFunction &gf, const char *title,
                    int x = 0, int y = 0, int w = 400, int h = 400,
                    const char *keys = NULL, bool vec = false);

} // namespace common

} // namespace mfem

#endif
