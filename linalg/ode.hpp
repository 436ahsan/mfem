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

#ifndef MFEM_ODE
#define MFEM_ODE

#include "../config/config.hpp"
#include "operator.hpp"

namespace mfem
{

/// Abstract class for solving systems of ODEs: dx/dt = f(x,t)
class ODESolver
{
protected:
   /// Pointer to the associated TimeDependentOperator.
   TimeDependentOperator *f;  // f(.,t) : R^n --> R^n

public:
   ODESolver() : f(NULL) { }

   /// Associate a TimeDependentOperator with the ODE solver.
   /** This method has to be called:
       - Before the first call to Step().
       - When the dimensions of the associated TimeDependentOperator change.
       - When a time stepping sequence has to be restarted.
       - To change the associated TimeDependentOperator. */
   virtual void Init(TimeDependentOperator &f)
   {
      this->f = &f;
   }

   /** @brief Perform a time step from time @a t [in] to time @a t [out] based
       on the requested step size @a dt [in]. */
   /** @param[in,out] x   Approximate solution.
       @param[in,out] t   Time associated with the approximate solution @a x.
       @param[in,out] dt  Time step size.

       The following rules describe the common behavior of the method:
       - The input @a x [in] is the approximate solution for the input time
         @a t [in].
       - The input @a dt [in] is the desired time step size, defining the desired
         target time: t [target] = @a t [in] + @a dt [in].
       - The output @a x [out] is the approximate solution for the output time
         @a t [out].
       - The output @a dt [out] is the last time step taken by the method which
         may be smaller or larger than the input @a dt [in] value, e.g. because
         of time step control.
       - The method may perform more than one time step internally; in this case
         @a dt [out] is the last internal time step size.
       - The output value of @a t [out] may be smaller or larger than
         t [target], however, it is not smaller than @a t [in] + @a dt [out], if
         at least one internal time step was performed.
       - The value @a x [out] may be obtained by interpolation using internally
         stored data.
       - In some cases, the contents of @a x [in] may not be used, e.g. when
         @a x [out] from a previous Step() call was obtained by interpolation.
       - In consecutive calls to this method, the output @a t [out] of one
         Step() call has to be the same as the input @a t [in] to the next
         Step() call.
       - If the previous rule has to be broken, e.g. to restart a time stepping
         sequence, then the ODE solver must be re-initialized by calling Init()
         between the two Step() calls. */
   virtual void Step(Vector &x, double &t, double &dt) = 0;

   /// Perform time integration from time @a t [in] to time @a tf [in].
   /** @param[in,out] x   Approximate solution.
       @param[in,out] t   Time associated with the approximate solution @a x.
       @param[in,out] dt  Time step size.
       @param[in]     tf  Requested final time.

       The default implementation makes consecutive calls to Step() until
       reaching @a tf.
       The following rules describe the common behavior of the method:
       - The input @a x [in] is the approximate solution for the input time
         @a t [in].
       - The input @a dt [in] is the initial time step size.
       - The output @a dt [out] is the last time step taken by the method which
         may be smaller or larger than the input @a dt [in] value, e.g. because
         of time step control.
       - The output value of @a t [out] is not smaller than @a tf [in]. */
   virtual void Run(Vector &x, double &t, double &dt, double tf)
   {
      while (t < tf) { Step(x, t, dt); }
   }

   virtual ~ODESolver() { }
};


/// The ODEController and related classes are based on the algorithms described
/// in "A _PI_ Stepsize Control For The Numerical Solution of Ordinary
/// Differential Equations" by K. Gustafsson, M. Lundh, and G. Soderlind, BIT
/// Numerical Mathematics, Vol. 28, Issue 2, pages 270-287 (1988).

class ODEDifferenceMeasure;
class ODEStepAdjustmentFactor;
class ODEStepAdjustmentLimiter;

/** The ODEController class is designed to adaptively adjust the step size
    used with the ODESolver classes. The goal is to maintain some measure of
    the solution error at a user specified level.
 */
class ODEController
{
protected:
   ODESolver                * sol;
   ODEDifferenceMeasure     * msr;
   ODEStepAdjustmentFactor  * acc;
   ODEStepAdjustmentFactor  * rej;
   ODEStepAdjustmentLimiter * lim;
   double tol;
   double rho;
   double curr_r;
   double min_dt;

   int ofreq;
   int nsteps;

   int nrejs;
   int max_nrejs;

   mutable Vector next_x;
   mutable double dt;

   std::ostream * out;

public:
   ODEController()
      : sol(NULL), msr(NULL), acc(NULL), rej(NULL), lim(NULL),
        tol(-1.0), rho(1.2), curr_r(-1.0), min_dt(-1.0), ofreq(-1), nsteps(0),
        nrejs(0), max_nrejs(1000), dt(1.0), out(NULL) {}

   /// Define the particulars of the ODE step-size control process
   /** The various pieces are:
        sol - Computes a possible update of the field at the next time step
        msr - Computes relative change in the field from two sucessive steps
        acc - Computes a new step size when the previous step was accepted
        rej - Computes a new step size when the previous step was rejected
        lim - Imposes limits on the next time step
   */
   void Init(ODESolver &sol, ODEDifferenceMeasure &msr,
             ODEStepAdjustmentFactor &acc, ODEStepAdjustmentFactor &rej,
             ODEStepAdjustmentLimiter &lim)
   {
      this->sol = &sol; this->msr = &msr;
      this->acc = &acc; this->rej = &rej;
      this->lim = &lim;
   }

   // Returns the current time step
   double GetTimeStep() const { return this->dt; }

   /// Sets (or resets) the initial time step
   void SetTimeStep(double dt) { this->dt = dt; }

   /// Sets the minimum allowable time step
   void SetMinTimeStep(double min_dt) { this->min_dt = min_dt; }

   /// Sets the error target for the control process
   void SetTolerance(double tol);

   /// Sets the threshold for rejection of a time step to be rho * tol
   void SetRejectionLimit(double rho) { this->rho = rho; }

   /// Sets the maximum number of successively rejected steps
   void SetMaxRejectCount(int max_nrejs) { this->max_nrejs = max_nrejs; }

   void SetOutputFrequency(int ofreq) { this->ofreq = ofreq; }

   void SetOutput(std::ostream & os) { this->out = &os; }

   virtual void Step(Vector &x, double &t, double delta_t);

   /// Advances the solution vector x from time t to tf
   virtual void Run(Vector &x, double &t, double tf);
};

/// Computes a measure of the difference between two successive field values
class ODEDifferenceMeasure
{
protected:
   ODEDifferenceMeasure() {}

public:
   virtual ~ODEDifferenceMeasure() {}

   virtual double Eval(Vector &u0, Vector &u1) = 0;
};

class ODEStepAdjustmentFactor
{
protected:
   double tol;

   ODEStepAdjustmentFactor() : tol(-1.0) {}

public:
   virtual ~ODEStepAdjustmentFactor() {}

   void SetTolerance(double tol) { this->tol = tol; }

   virtual double operator()(double err, double dt) const = 0;
};

class ODEStepAdjustmentLimiter
{
protected:
   ODEStepAdjustmentLimiter() {}

public:
   virtual ~ODEStepAdjustmentLimiter() {}

   virtual double operator()(double theta) const = 0;
};

/// The classical forward Euler method
class ForwardEulerSolver : public ODESolver
{
private:
   Vector dxdt;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/** A family of explicit second-order RK2 methods. Some choices for the
    parameter 'a' are:
    a = 1/2 - the midpoint method
    a =  1  - Heun's method
    a = 2/3 - default, has minimal truncation error. */
class RK2Solver : public ODESolver
{
private:
   double a;
   Vector dxdt, x1;

public:
   RK2Solver(const double _a = 2./3.) : a(_a) { }

   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/// Third-order, strong stability preserving (SSP) Runge-Kutta method
class RK3SSPSolver : public ODESolver
{
private:
   Vector y, k;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/// The classical explicit forth-order Runge-Kutta method, RK4
class RK4Solver : public ODESolver
{
private:
   Vector y, k, z;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/** An explicit Runge-Kutta method corresponding to a general Butcher tableau
    +--------+----------------------+
    | c[0]   | a[0]                 |
    | c[1]   | a[1] a[2]            |
    | ...    |    ...               |
    | c[s-2] | ...   a[s(s-1)/2-1]  |
    +--------+----------------------+
    |        | b[0] b[1] ... b[s-1] |
    +--------+----------------------+ */
class ExplicitRKSolver : public ODESolver
{
private:
   int s;
   const double *a, *b, *c;
   Vector y, *k;

public:
   ExplicitRKSolver(int _s, const double *_a, const double *_b,
                    const double *_c);

   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);

   virtual ~ExplicitRKSolver();
};


/** An 8-stage, 6th order RK method. From Verner's "efficient" 9-stage 6(5)
    pair. */
class RK6Solver : public ExplicitRKSolver
{
private:
   static const double a[28], b[8], c[7];

public:
   RK6Solver() : ExplicitRKSolver(8, a, b, c) { }
};


/** A 12-stage, 8th order RK method. From Verner's "efficient" 13-stage 8(7)
    pair. */
class RK8Solver : public ExplicitRKSolver
{
private:
   static const double a[66], b[12], c[11];

public:
   RK8Solver() : ExplicitRKSolver(12, a, b, c) { }
};


/// Backward Euler ODE solver. L-stable.
class BackwardEulerSolver : public ODESolver
{
protected:
   Vector k;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/// Implicit midpoint method. A-stable, not L-stable.
class ImplicitMidpointSolver : public ODESolver
{
protected:
   Vector k;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/** Two stage, singly diagonal implicit Runge-Kutta (SDIRK) methods;
    the choices for gamma_opt are:
    0 - 3rd order method, not A-stable
    1 - 3rd order method, A-stable, not L-stable (default)
    2 - 2nd order method, L-stable
    3 - 2nd order method, L-stable (has solves outside [t,t+dt]). */
class SDIRK23Solver : public ODESolver
{
protected:
   double gamma;
   Vector k, y;

public:
   SDIRK23Solver(int gamma_opt = 1);

   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/** Three stage, singly diagonal implicit Runge-Kutta (SDIRK) method of
    order 4. A-stable, not L-stable. */
class SDIRK34Solver : public ODESolver
{
protected:
   Vector k, y, z;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/** Three stage, singly diagonal implicit Runge-Kutta (SDIRK) method of
    order 3. L-stable. */
class SDIRK33Solver : public ODESolver
{
protected:
   Vector k, y;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/// Generalized-alpha ODE solver from "A generalized-α method for integrating
/// the filtered Navier–Stokes equations with a stabilized finite element
/// method" by K.E. Jansen, C.H. Whiting and G.M. Hulbert.
class GeneralizedAlphaSolver : public ODESolver
{
protected:
   Vector xdot,k,y;
   double alpha_f, alpha_m, gamma;
   bool first;

   void SetRhoInf(double rho_inf);
   void PrintProperties(std::ostream &out = mfem::out);
public:

   GeneralizedAlphaSolver(double rho = 1.0) { SetRhoInf(rho); };

   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/// IMEX Backward-Forward Euler ODE solver
class IMEX_BE_FE : public ODESolver
{
protected:
   Vector k_exp, k_imp, y;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};

/** Second-order IMEX (2,3,2) method, from "Implicit-explicit Runge-Kutta
    methods for time-dependent partial differential equations" by Ascher, Ruuth
    and Spiteri, Applied Numerical Mathematics (1997). */
class IMEXRK2 : public ODESolver
{
protected:
   Vector k_exp, k_imp, y, z;

public:
   virtual void Init(TimeDependentOperator &_f);

   virtual void Step(Vector &x, double &t, double &dt);
};


/// The SIASolver class is based on the Symplectic Integration Algorithm
/// described in "A Symplectic Integration Algorithm for Separable Hamiltonian
/// Functions" by J. Candy and W. Rozmus, Journal of Computational Physics,
/// Vol. 92, pages 230-256 (1991).

/** The Symplectic Integration Algorithm (SIA) is designed for systems of first
    order ODEs derived from a Hamiltonian.
       H(q,p,t) = T(p) + V(q,t)
    Which leads to the equations:
       dq/dt = dT/dp
       dp/dt = -dV/dq
    In the integrator the operators P and F are defined to be:
       P = dT/dp
       F = -dV/dq
 */
class SIASolver
{
public:
   SIASolver() : F_(NULL), P_(NULL) {}

   virtual void Init(Operator &P, TimeDependentOperator & F);

   virtual void Step(Vector &q, Vector &p, double &t, double &dt) = 0;

   virtual void Run(Vector &q, Vector &p, double &t, double &dt, double tf)
   {
      while (t < tf) { Step(q, p, t, dt); }
   }

   virtual ~SIASolver() {}

protected:
   TimeDependentOperator * F_; // p_{i+1} = p_{i} + dt F(q_{i})
   Operator              * P_; // q_{i+1} = q_{i} + dt P(p_{i+1})

   mutable Vector dp_;
   mutable Vector dq_;
};

// First Order Symplectic Integration Algorithm
class SIA1Solver : public SIASolver
{
public:
   SIA1Solver() {}
   void Step(Vector &q, Vector &p, double &t, double &dt);
};

// Second Order Symplectic Integration Algorithm
class SIA2Solver : public SIASolver
{
public:
   SIA2Solver() {}
   void Step(Vector &q, Vector &p, double &t, double &dt);
};

// Variable order Symplectic Integration Algorithm (orders 1-4)
class SIAVSolver : public SIASolver
{
public:
   SIAVSolver(int order);
   void Step(Vector &q, Vector &p, double &t, double &dt);

private:
   int order_;

   Array<double> a_;
   Array<double> b_;
};

/// Computes the largest absolute/relative difference in a pair of vectors
/** Computes the maximum of the following ratio:
        max_i |u1_i - u0_i| / |u0_i + eta_i|
    Where eta can either be a single constant or a vector of non-zero values
    with the same length as u0 and u1.

    Note: this class is designed for use on a single processor.  To take the
    maximum across multiple processors use the ParMaxAbsRelDiffMeasure class.
*/
class MaxAbsRelDiffMeasure : public ODEDifferenceMeasure
{
private:
   Vector * etaVec;
   double   etaConst;

public:
   MaxAbsRelDiffMeasure(double eta) : etaVec(NULL), etaConst(eta) {}
   MaxAbsRelDiffMeasure(Vector &eta) : etaVec(&eta), etaConst(-1.0) {}

   double Eval(Vector &u0, Vector &u1);
};

class L2AbsRelDiffMeasure : public ODEDifferenceMeasure
{
private:
   Vector * etaVec;
   double   etaConst;

public:
   L2AbsRelDiffMeasure(double eta) : etaVec(NULL), etaConst(eta) {}
   L2AbsRelDiffMeasure(Vector &eta) : etaVec(&eta), etaConst(-1.0) {}

   double Eval(Vector &u0, Vector &u1);
};

#ifdef MFEM_USE_MPI

/// Computes the largest absolute/relative difference in a pair of vectors
/** Computes the maximum of the following ratio:
        max_i |u1_i - u0_i| / |u0_i + eta_i|
    Where eta can either be a single constant or a vector of non-zero values
    with the same length as u0 and u1.

    Note: this class is designed for use on multiple processors.  For a
    serial implementation see the MaxAbsRelDiffMeasure class.
*/
class ParMaxAbsRelDiffMeasure : public ODEDifferenceMeasure
{
private:
   MPI_Comm comm;
   Vector * etaVec;
   double   etaConst;

public:
   ParMaxAbsRelDiffMeasure(MPI_Comm comm, double eta)
      : comm(comm), etaVec(NULL), etaConst(eta) {}

   ParMaxAbsRelDiffMeasure(MPI_Comm comm, Vector &eta)
      : comm(comm), etaVec(&eta), etaConst(-1.0) {}

   double Eval(Vector &u0, Vector &u1);
};

class ParL2AbsRelDiffMeasure : public ODEDifferenceMeasure
{
private:
   MPI_Comm comm;
   Vector * etaVec;
   double   etaConst;

public:
   ParL2AbsRelDiffMeasure(MPI_Comm comm, double eta)
      : comm(comm), etaVec(NULL), etaConst(eta) {}

   ParL2AbsRelDiffMeasure(MPI_Comm comm, Vector &eta)
      : comm(comm), etaVec(&eta), etaConst(-1.0) {}

   double Eval(Vector &u0, Vector &u1);
};
#endif

class StdAdjFactor : public ODEStepAdjustmentFactor
{
private:
   double gamma;
   double kI;

public:
   StdAdjFactor(double gamma, double kI) : gamma(gamma), kI(kI) {}

   double operator()(double err, double dt) const;
};

class IntegralAdjFactor : public StdAdjFactor
{
public:
   IntegralAdjFactor(double kI) : StdAdjFactor(1.0, kI) {}
};

typedef IntegralAdjFactor IAdjFactor;

class PIAdjFactor : public ODEStepAdjustmentFactor
{
private:
   double kI;
   double kP;
   mutable double prev_dt;
   mutable double prev_err;

public:
   PIAdjFactor(double kP, double kI)
      : kI(kI), kP(kP), prev_dt(-1.0), prev_err(-1.0) {}

   double operator()(double err, double dt) const;
};

class PIDAdjFactor : public ODEStepAdjustmentFactor
{
private:
   double kI;
   double kP;
   double kD;
   mutable double prev_dt1;
   mutable double prev_dt2;
   mutable double prev_err1;
   mutable double prev_err2;

public:
   PIDAdjFactor(double kP, double kI, double kD)
      : kI(kI), kP(kP), kD(kD),
        prev_dt1(-1.0), prev_dt2(-1.0), prev_err1(-1.0), prev_err2(-1.0) {}

   double operator()(double err, double dt) const;
};

class DeadZoneLimiter : public ODEStepAdjustmentLimiter
{
private:
   double lo;
   double hi;
   double mx;

public:
   DeadZoneLimiter(double lo, double hi, double mx)
      : lo(lo), hi(hi), mx(mx) {}

   double operator()(double theta) const
   { return std::min(mx, ((lo <= theta && theta <= hi) ? 1.0 : theta)); }
};

class MaxLimiter : public ODEStepAdjustmentLimiter
{
private:
   double mx;

public:
   MaxLimiter(double mx) : mx(mx) {}

   double operator()(double theta) const
   { return std::min(mx, theta); }
};

}

#endif
