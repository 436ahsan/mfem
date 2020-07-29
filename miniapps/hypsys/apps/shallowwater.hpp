#ifndef HYPSYS_SHALLOWWATER
#define HYPSYS_SHALLOWWATER

#include "hyperbolic_system.hpp"

class ShallowWater : public HyperbolicSystem
{
public:
   explicit ShallowWater(FiniteElementSpace *fes_, BlockVector &u_block,
                         Configuration &config_);
   ~ShallowWater() { };

   virtual void EvaluateFlux(const Vector &u, DenseMatrix &FluxEval,
                             int e, int k, int i = -1) const;
   virtual double GetWaveSpeed(const Vector &u, const Vector n, int e, int k,
                               int i) const;
   virtual void CheckAdmissibility(const Vector &u) const override;
   virtual void SetBdrCond(const Vector &y1, Vector &y2, const Vector &normal,
                           int attr) const override;
   virtual void ComputeDerivedQuantities(const GridFunction &u, GridFunction &d1, GridFunction &d2) const override;
   virtual void ComputeErrors(Array<double> &errors, const GridFunction &u,
                              double DomainSize, double t) const override;
};

#endif
