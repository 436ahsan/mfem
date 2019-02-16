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

#include "tmop_tools.hpp"
#include "../fem/pnonlinearform.hpp"
#include "../general/osockstream.hpp"

namespace mfem
{

using namespace mfem;

void AdvectorCG::SetInitialField(const Vector &init_nodes,
                                 const Vector &init_field)
{
   nodes0 = init_nodes;
   field0 = init_field;
}

void AdvectorCG::ComputeAtNewPosition(const Vector &new_nodes,
                                      Vector &new_field)
{
   // This will be used to move the positions.
   GridFunction *mesh_nodes = pmesh->GetNodes();
   *mesh_nodes = nodes0;
   new_field = field0;

   // Velocity of the positions.
   GridFunction u(mesh_nodes->FESpace());
   subtract(new_nodes, nodes0, u);

   // This must be the fes of the ind, associated with the object's mesh.
   AdvectorCGOperator oper(nodes0, u, *mesh_nodes, *pfes);
   ode_solver.Init(oper);

   // Compute some time step [mesh_size / speed].
   double min_h = std::numeric_limits<double>::infinity();
   for (int i = 0; i < pmesh->GetNE(); i++)
   {
      min_h = std::min(min_h, pmesh->GetElementSize(i));
   }
   double v_max = 0.0;
   const int s = u.FESpace()->GetVSize() / 2;
   for (int i = 0; i < s; i++)
   {
      const double vel = std::sqrt( u(i) * u(i) + u(i+s) * u(i+s) + 1e-14);
      v_max = std::max(v_max, vel);
   }
   double dt = 0.5 * min_h / v_max;
   double glob_dt;
   MPI_Allreduce(&dt, &glob_dt, 1, MPI_DOUBLE, MPI_MIN, pfes->GetComm());

   int myid;
   MPI_Comm_rank(pfes->GetComm(), &myid);
   double t = 0.0;
   bool last_step = false;

   for (int ti = 1; !last_step; ti++)
   {
      if (t + glob_dt >= 1.0)
      {
         if (myid == 0)
         {
            mfem::out << "Remap with dt = " << glob_dt
                      << " took " << ti << " steps." << std::endl;
         }
         glob_dt = 1.0 - t;
         last_step = true;
      }
      ode_solver.Step(new_field, t, glob_dt);
   }

   nodes0 = new_nodes;
   field0 = new_field;
}

AdvectorCGOperator::AdvectorCGOperator(const Vector &x_start,
                                       GridFunction &vel,
                                       Vector &xn,
                                       ParFiniteElementSpace &pfes)
   : TimeDependentOperator(pfes.GetVSize()),
     x0(x_start), x_now(xn), u(vel), u_coeff(&u), M(&pfes), K(&pfes)
{
   ConvectionIntegrator *Kinteg = new ConvectionIntegrator(u_coeff);
   K.AddDomainIntegrator(Kinteg);
   K.Assemble(0);
   K.Finalize(0);

   MassIntegrator *Minteg = new MassIntegrator;
   M.AddDomainIntegrator(Minteg);
   M.Assemble();
   M.Finalize();
}

void AdvectorCGOperator::Mult(const Vector &ind, Vector &di_dt) const
{
   const double t = GetTime();

   // Move the mesh.
   add(x0, t, u, x_now);

   // Assemble on the new mesh.
   K.BilinearForm::operator=(0.0);
   K.Assemble();
   ParGridFunction rhs(K.ParFESpace());
   K.Mult(ind, rhs);
   M.BilinearForm::operator=(0.0);
   M.Assemble();

   HypreParVector *RHS = rhs.ParallelAssemble();
   HypreParVector *X   = rhs.ParallelAverage();
   HypreParMatrix *Mh  = M.ParallelAssemble();
/*
   CGSolver cg(M.ParFESpace()->GetParMesh()->GetComm());
   HypreSmoother prec;
   prec.SetType(HypreSmoother::Jacobi, 1);
   cg.SetPreconditioner(prec);
   cg.SetOperator(*Mh);
   cg.SetRelTol(1e-12); cg.SetAbsTol(0.0);
   cg.SetMaxIter(100);
   cg.SetPrintLevel(0);
   cg.Mult(*RHS, *X);
   K.ParFESpace()->Dof_TrueDof_Matrix()->Mult(*X, di_dt);
*/
   GMRESSolver gmres(M.ParFESpace()->GetParMesh()->GetComm());
   HypreSmoother prec;
   prec.SetType(HypreSmoother::Jacobi, 1);
   gmres.SetPreconditioner(prec);
   gmres.SetOperator(*Mh);
   gmres.SetRelTol(1e-12); gmres.SetAbsTol(0.0);
   gmres.SetMaxIter(100);
   gmres.SetPrintLevel(0);
   gmres.Mult(*RHS, *X);
   K.ParFESpace()->Dof_TrueDof_Matrix()->Mult(*X, di_dt);

   delete Mh;
   delete X;
   delete RHS;
}

double TMOPNewtonSolver::ComputeScalingFactor(const Vector &x,
                                              const Vector &b) const
{
   const ParNonlinearForm *nlf = dynamic_cast<const ParNonlinearForm *>(oper);
   MFEM_VERIFY(nlf != NULL, "Invalid Operator subclass.");
   const bool have_b = (b.Size() == Height());

   const int NE = pfes->GetParMesh()->GetNE(), dim = pfes->GetFE(0)->GetDim(),
             dof = pfes->GetFE(0)->GetDof(), nsp = ir.GetNPoints();
   Array<int> xdofs(dof * dim);
   DenseMatrix Jpr(dim), dshape(dof, dim), pos(dof, dim);
   Vector posV(pos.Data(), dof * dim);

   Vector x_out(x.Size());
   bool x_out_ok = false;
   const double energy_in = nlf->GetEnergy(x);
   double scale = 1.0, energy_out;
   double norm0 = Norm(r);
   x_gf.MakeTRef(pfes, x_out, 0);

   // Decreases the scaling of the update until the new mesh is valid.
   for (int i = 0; i < 12; i++)
   {
      add(x, -scale, c, x_out);
      x_gf.SetFromTrueVector();

      energy_out = nlf->GetParGridFunctionEnergy(x_gf);
      if (energy_out > 1.2*energy_in || isnan(energy_out) != 0)
      {
         if (print_level >= 0)
         { mfem::out << "Scale = " << scale << " Increasing energy.\n"; }
         scale *= 0.5; continue;
      }

      int jac_ok = 1;
      for (int i = 0; i < NE; i++)
      {
         pfes->GetElementVDofs(i, xdofs);
         x_gf.GetSubVector(xdofs, posV);
         for (int j = 0; j < nsp; j++)
         {
            pfes->GetFE(i)->CalcDShape(ir.IntPoint(j), dshape);
            MultAtB(pos, dshape, Jpr);
            if (Jpr.Det() <= 0.0) { jac_ok = 0; goto break2; }
         }
      }
   break2:
      int jac_ok_all;
      MPI_Allreduce(&jac_ok, &jac_ok_all, 1, MPI_INT, MPI_LAND,
                    pfes->GetComm());

      if (jac_ok_all == 0)
      {
         if (print_level >= 0)
         { mfem::out << "Scale = " << scale << " Neg det(J) found.\n"; }
         scale *= 0.5; continue;
      }

      oper->Mult(x_out, r);
      if (have_b) { r -= b; }
      double norm = Norm(r);

      if (norm > 1.2*norm0)
      {
         if (print_level >= 0)
         { mfem::out << "Scale = " << scale << " Norm increased.\n"; }
         scale *= 0.5; continue;
      }
      else { x_out_ok = true; break; }
   }

   if (print_level >= 0)
   {
      mfem::out << "Energy decrease: "
                << (energy_in - energy_out) / energy_in * 100.0
                << "% with " << scale << " scaling.\n";
   }

   if (x_out_ok == false) { scale = 0.0; }

   return scale;
}

void TMOPNewtonSolver::ProcessNewState(const Vector &x) const
{
   if (discr_tc)
   {
      x_gf.Distribute(x);
      discr_tc->UpdateTargetSpecification(x_gf);
   }
}

double TMOPDescentNewtonSolver::ComputeScalingFactor(const Vector &x,
                                                     const Vector &b) const
{
   const ParNonlinearForm *nlf = dynamic_cast<const ParNonlinearForm *>(oper);
   MFEM_VERIFY(nlf != NULL, "invalid Operator subclass");

   const int NE = pfes->GetParMesh()->GetNE(), dim = pfes->GetFE(0)->GetDim(),
             dof = pfes->GetFE(0)->GetDof(), nsp = ir.GetNPoints();
   Array<int> xdofs(dof * dim);
   DenseMatrix Jpr(dim), dshape(dof, dim), pos(dof, dim);
   Vector posV(pos.Data(), dof * dim);

   x_gf.MakeTRef(pfes, x.GetData());
   x_gf.SetFromTrueVector();

   if (print_level >= 0)
   {
      double min_detJ = infinity();
      for (int i = 0; i < NE; i++)
      {
         pfes->GetElementVDofs(i, xdofs);
         x_gf.GetSubVector(xdofs, posV);
         for (int j = 0; j < nsp; j++)
         {
            pfes->GetFE(i)->CalcDShape(ir.IntPoint(j), dshape);
            MultAtB(pos, dshape, Jpr);
            min_detJ = std::min(min_detJ, Jpr.Det());
         }
      }
      double min_detJ_all;
      MPI_Allreduce(&min_detJ, &min_detJ_all, 1, MPI_DOUBLE, MPI_MIN,
                    pfes->GetComm());
      mfem::out << "Minimum det(J) = " << min_detJ_all << '\n';
   }

   Vector x_out(x.Size());
   bool x_out_ok = false;
   const double energy_in = nlf->GetParGridFunctionEnergy(x_gf);
   double scale = 1.0, energy_out;

   for (int i = 0; i < 7; i++)
   {
      add(x, -scale, c, x_out);

      energy_out = nlf->GetEnergy(x_out);
      if (energy_out > energy_in || isnan(energy_out) != 0)
      {
         scale *= 0.5;
      }
      else { x_out_ok = true; break; }
   }

   if (print_level >= 0)
   {
      mfem::out << "Energy decrease: "
                << (energy_in - energy_out) / energy_in * 100.0
                << "% with " << scale << " scaling.\n";
   }

   if (x_out_ok == false) { return 0.0; }

   return scale;
}

// Metric values are visualized by creating an L2 finite element functions and
// computing the metric values at the nodes.
void vis_tmop_metric(int order, TMOP_QualityMetric &qm,
                     const TargetConstructor &tc, ParMesh &pmesh,
                     char *title, int position)
{
   L2_FECollection fec(order, pmesh.Dimension(), BasisType::GaussLobatto);
   ParFiniteElementSpace fes(&pmesh, &fec, 1);
   ParGridFunction metric(&fes);
   InterpolateTMOP_QualityMetric(qm, tc, pmesh, metric);
   socketstream sock;
   if (pmesh.GetMyRank() == 0)
   {
      sock.open("localhost", 19916);
      sock << "solution\n";
   }
   pmesh.PrintAsOne(sock);
   metric.SaveAsOne(sock);
   if (pmesh.GetMyRank() == 0)
   {
      sock << "window_title '"<< title << "'\n"
           << "window_geometry "
           << position << " " << 0 << " " << 600 << " " << 600 << "\n"
           << "keys jRmclA\n";
   }
}

}
