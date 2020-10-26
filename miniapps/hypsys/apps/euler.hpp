#ifndef HYPSYS_EULER
#define HYPSYS_EULER

#include "hyperbolic_system.hpp"

class Euler : public HyperbolicSystem
{
public:
   explicit Euler(FiniteElementSpace *fes_, BlockVector &u_block,
                  Configuration &config_);
   ~Euler() { };

   virtual double EvaluatePressure(const Vector &u) const;

   virtual void EvaluateFlux(const Vector &u, DenseMatrix &FluxEval,
                             int e, int k, int i = -1) const;
   virtual double GetGMS(const Vector &uL, const Vector &uR,
                         const Vector &normal) const override;
   virtual double GetWaveSpeed(const Vector &u, const Vector n, int e, int k,
                               int i) const;
   virtual void CheckAdmissibility(const Vector &u) const override;
   virtual void SetBdrCond(const Vector &y1, Vector &y2, const Vector &normal,
                           int attr) const override;
   virtual void ComputeDerivedQuantities(const GridFunction &u, GridFunction &d1,
                                         GridFunction &d2) const override;
   virtual void ComputeErrors(Array<double> &errors, const GridFunction &u,
                              double DomainSize, double t) const override;
};

#endif
