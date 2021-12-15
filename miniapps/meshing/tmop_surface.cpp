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
//
//    ---------------------------------------------------------------------
//    Mesh Optimizer Miniapp: Optimize high-order meshes - Parallel Version
//    ---------------------------------------------------------------------
//
// This miniapp performs mesh optimization using the Target-Matrix Optimization
// Paradigm (TMOP) by P.Knupp et al., and a global variational minimization
// approach. It minimizes the quantity sum_T int_T mu(J(x)), where T are the
// target (ideal) elements, J is the Jacobian of the transformation from the
// target to the physical element, and mu is the mesh quality metric. This
// metric can measure shape, size or alignment of the region around each
// quadrature point. The combination of targets & quality metrics is used to
// optimize the physical node positions, i.e., they must be as close as possible
// to the shape / size / alignment of their targets. This code also demonstrates
// a possible use of nonlinear operators (the class TMOP_QualityMetric, defining
// mu(J), and the class TMOP_Integrator, defining int mu(J)), as well as their
// coupling to Newton methods for solving minimization problems. Note that the
// utilized Newton methods are oriented towards avoiding invalid meshes with
// negative Jacobian determinants. Each Newton step requires the inversion of a
// Jacobian matrix, which is done through an inner linear solver.
//
// Compile with: make pmesh-optimizer
//
// Sample runs:
//   Boundary fitting
//   mpirun -np 4 tmop_surface -m sqdiscquad.mesh -o 2 -rs 2 -mid 1 -tid 1 -vl 2 -sfc 1e2 -rtol 1e-12 -ni 1 -sni 10 -oi 10 -ae 1 -fix-bnd -sbgmesh -slstype 2 -sapp 2 -smtype 2
//   mpirun -np 4 tmop_surface -m sqdisc.mesh -o 2 -rs 0 -mid 1 -tid 1 -vl 2 -sfc 1e2 -rtol 1e-12 -ni 20 -sni 10 -oi 3 -ae 1 -fix-bnd -sbgmesh -slstype 2 -sapp 2 -smtype 2
//   mpirun -np 4 tmop_surface -m sqdiscquad.mesh -o 4 -rs 2 -mid 1 -tid 1 -vl 2 -sfc 1e2 -rtol 1e-12 -ni 10 -sni 10 -oi 5 -ae 1 -fix-bnd -sbgmesh -slstype 2 -sapp 2 -smtype 2

//   mpirun -np 4 tmop_surface -m sqdiscquad.mesh -o 2 -rs 2 -mid 1 -tid 1 -vl 2 -sfc 1e4 -ni 20 -ae 1 -fix-bnd -sbgmesh -slstype 2 -sapp 1 -smtype 2

//  Interface fitting
//  mpirun -np 4 tmop_surface -m square01.mesh -o 3 -rs 1 -mid 58 -tid 1 -ni 200 -vl 1 -sfc 1e4 -rtol 1e-5 -nor -sapp 1 -slstype 1
//  mpirun -np 4 tmop_surface -m square01-tri.mesh -o 2 -rs 1 -mid 58 -tid 1 -ni 200 -vl 1 -sfc 1e4 -rtol 1e-5 -nor -sapp 1 -slstype 1
//  mpirun -np 4 tmop_surface -m square01-tri.mesh -o 2 -rs 2 -mid 58 -tid 1 -ni 200 -vl 1 -sfc 1e4 -rtol 1e-5 -nor -sapp 1 -slstype 3
#include "mfem.hpp"
#include "../common/mfem-common.hpp"
#include <iostream>
#include <fstream>
#include "mesh-optimizer.hpp"

using namespace mfem;
using namespace std;

int main (int argc, char *argv[])
{
   // Initialize MPI.
   int num_procs, myid;
   MPI_Init(&argc, &argv);
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
   MPI_Comm_rank(MPI_COMM_WORLD, &myid);

   // Set the method's default parameters.
   const char *mesh_file    = "icf.mesh";
   int mesh_poly_deg        = 1;
   int rs_levels         = 0;
   int rp_levels         = 0;
   double jitter         = 0.0;
   int metric_id         = 1;
   int target_id         = 1;
   double lim_const      = 0.0;
   double adapt_lim_const   = 0.0;
   double surface_fit_const = 0.0;
   int quad_type         = 1;
   int quad_order        = 8;
   int solver_type       = 0;
   int solver_iter       = 20;
   int outer_iter         = 20;
   int surf_solver_iter   = 20;
   double solver_rtol    = 1e-10;
   int solver_art_type   = 0;
   int lin_solver        = 2;
   int max_lin_iter      = 100;
   bool move_bnd         = true;
   int combomet          = 0;
   bool normalization    = false;
   bool visualization    = true;
   int verbosity_level   = 0;
   bool fdscheme         = false;
   int adapt_eval        = 0;
   bool exactaction      = false;
   const char *devopt    = "cpu";
   bool pa               = false;
   bool surf_bg_mesh     = false;
   int surf_approach     = 0;
   int surf_ls_type      = 1;
   int marking_type      = 1;

   // Parse command-line options.
   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&mesh_poly_deg, "-o", "--order",
                  "Polynomial degree of mesh finite element space.");
   args.AddOption(&rs_levels, "-rs", "--refine-serial",
                  "Number of times to refine the mesh uniformly in serial.");
   args.AddOption(&rp_levels, "-rp", "--refine-parallel",
                  "Number of times to refine the mesh uniformly in parallel.");
   args.AddOption(&jitter, "-ji", "--jitter",
                  "Random perturbation scaling factor.");
   args.AddOption(&metric_id, "-mid", "--metric-id",
                  "Mesh optimization metric:\n\t"
                  "T-metrics\n\t"
                  "1  : |T|^2                          -- 2D shape\n\t"
                  "2  : 0.5|T|^2/tau-1                 -- 2D shape (condition number)\n\t"
                  "7  : |T-T^-t|^2                     -- 2D shape+size\n\t"
                  "9  : tau*|T-T^-t|^2                 -- 2D shape+size\n\t"
                  "14 : |T-I|^2                        -- 2D shape+size+orientation\n\t"
                  "22 : 0.5(|T|^2-2*tau)/(tau-tau_0)   -- 2D untangling\n\t"
                  "50 : 0.5|T^tT|^2/tau^2-1            -- 2D shape\n\t"
                  "55 : (tau-1)^2                      -- 2D size\n\t"
                  "56 : 0.5(sqrt(tau)-1/sqrt(tau))^2   -- 2D size\n\t"
                  "58 : |T^tT|^2/(tau^2)-2*|T|^2/tau+2 -- 2D shape\n\t"
                  "77 : 0.5(tau-1/tau)^2               -- 2D size\n\t"
                  "80 : (1-gamma)mu_2 + gamma mu_77    -- 2D shape+size\n\t"
                  "85 : |T-|T|/sqrt(2)I|^2             -- 2D shape+orientation\n\t"
                  "98 : (1/tau)|T-I|^2                 -- 2D shape+size+orientation\n\t"
                  // "211: (tau-1)^2-tau+sqrt(tau^2+eps)  -- 2D untangling\n\t"
                  // "252: 0.5(tau-1)^2/(tau-tau_0)       -- 2D untangling\n\t"
                  "301: (|T||T^-1|)/3-1              -- 3D shape\n\t"
                  "302: (|T|^2|T^-1|^2)/9-1          -- 3D shape\n\t"
                  "303: (|T|^2)/3*tau^(2/3)-1        -- 3D shape\n\t"
                  // "311: (tau-1)^2-tau+sqrt(tau^2+eps)-- 3D untangling\n\t"
                  "313: (|T|^2)(tau-tau0)^(-2/3)/3   -- 3D untangling\n\t"
                  "315: (tau-1)^2                    -- 3D size\n\t"
                  "316: 0.5(sqrt(tau)-1/sqrt(tau))^2 -- 3D size\n\t"
                  "321: |T-T^-t|^2                   -- 3D shape+size\n\t"
                  // "352: 0.5(tau-1)^2/(tau-tau_0)     -- 3D untangling\n\t"
                  "A-metrics\n\t"
                  "11 : (1/4*alpha)|A-(adjA)^T(W^TW)/omega|^2 -- 2D shape\n\t"
                  "36 : (1/alpha)|A-W|^2                      -- 2D shape+size+orientation\n\t"
                  "107: (1/2*alpha)|A-|A|/|W|W|^2             -- 2D shape+orientation\n\t"
                  "126: (1-gamma)nu_11 + gamma*nu_14a         -- 2D shape+size\n\t"
                 );
   args.AddOption(&target_id, "-tid", "--target-id",
                  "Target (ideal element) type:\n\t"
                  "1: Ideal shape, unit size\n\t"
                  "2: Ideal shape, equal size\n\t"
                  "3: Ideal shape, initial size\n\t"
                  "4: Given full analytic Jacobian (in physical space)\n\t"
                  "5: Ideal shape, given size (in physical space)");
   args.AddOption(&lim_const, "-lc", "--limit-const", "Limiting constant.");
   args.AddOption(&adapt_lim_const, "-alc", "--adapt-limit-const",
                  "Adaptive limiting coefficient constant.");
   args.AddOption(&surface_fit_const, "-sfc", "--surface-fit-const",
                  "Surface preservation constant.");
   args.AddOption(&quad_type, "-qt", "--quad-type",
                  "Quadrature rule type:\n\t"
                  "1: Gauss-Lobatto\n\t"
                  "2: Gauss-Legendre\n\t"
                  "3: Closed uniform points");
   args.AddOption(&quad_order, "-qo", "--quad_order",
                  "Order of the quadrature rule.");
   args.AddOption(&solver_type, "-st", "--solver-type",
                  " Type of solver: (default) 0: Newton, 1: LBFGS");
   args.AddOption(&solver_iter, "-ni", "--newton-iters",
                  "Maximum number of Newton iterations.");
   args.AddOption(&outer_iter, "-oi", "--newton-iters",
                  "Maximum number of Newton iterations.");
   args.AddOption(&solver_rtol, "-rtol", "--newton-rel-tolerance",
                  "Relative tolerance for the Newton solver.");
   args.AddOption(&solver_art_type, "-art", "--adaptive-rel-tol",
                  "Type of adaptive relative linear solver tolerance:\n\t"
                  "0: None (default)\n\t"
                  "1: Eisenstat-Walker type 1\n\t"
                  "2: Eisenstat-Walker type 2");
   args.AddOption(&lin_solver, "-ls", "--lin-solver",
                  "Linear solver:\n\t"
                  "0: l1-Jacobi\n\t"
                  "1: CG\n\t"
                  "2: MINRES\n\t"
                  "3: MINRES + Jacobi preconditioner\n\t"
                  "4: MINRES + l1-Jacobi preconditioner");
   args.AddOption(&max_lin_iter, "-li", "--lin-iter",
                  "Maximum number of iterations in the linear solve.");
   args.AddOption(&move_bnd, "-bnd", "--move-boundary", "-fix-bnd",
                  "--fix-boundary",
                  "Enable motion along horizontal and vertical boundaries.");
   args.AddOption(&combomet, "-cmb", "--combo-type",
                  "Combination of metrics options:\n\t"
                  "0: Use single metric\n\t"
                  "1: Shape + space-dependent size given analytically\n\t"
                  "2: Shape + adapted size given discretely; shared target");
   args.AddOption(&normalization, "-nor", "--normalization", "-no-nor",
                  "--no-normalization",
                  "Make all terms in the optimization functional unitless.");
   args.AddOption(&fdscheme, "-fd", "--fd_approximation",
                  "-no-fd", "--no-fd-approx",
                  "Enable finite difference based derivative computations.");
   args.AddOption(&exactaction, "-ex", "--exact_action",
                  "-no-ex", "--no-exact-action",
                  "Enable exact action of TMOP_Integrator.");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.AddOption(&verbosity_level, "-vl", "--verbosity-level",
                  "Set the verbosity level - 0, 1, or 2.");
   args.AddOption(&adapt_eval, "-ae", "--adaptivity-evaluator",
                  "0 - Advection based (DEFAULT), 1 - GSLIB.");
   args.AddOption(&devopt, "-d", "--device",
                  "Device configuration string, see Device::Configure().");
   args.AddOption(&pa, "-pa", "--partial-assembly", "-no-pa",
                  "--no-partial-assembly", "Enable Partial Assembly.");
   args.AddOption(&surf_solver_iter, "-sni", "--newton-iters",
                  "Maximum number of Newton iterations.");
   args.AddOption(&surf_approach, "-sapp", "--surf-approach",
                  "1 - Use 1 Integrator and balance shape + fitting (DEFAULT),"
                  "2 - iterate between shape optimization and surface fitting.");
   args.AddOption(&surf_bg_mesh, "-sbgmesh", "--surf-bg-mesh",
                  "-no-sbgmesh","--no-surf-bg-mesh", "Use background mesh for surface fitting.");
   args.AddOption(&surf_ls_type, "-slstype", "--surf-ls-type",
                  "1 - Circle (DEFAULT), 2 - Squircle, 3 - Butterfly.");
   args.AddOption(&marking_type, "-smtype", "--surf-marking-type",
                  "1 - Interface (DEFAULT), 2 - Boundary attribute.");
   args.Parse();
   if (!args.Good())
   {
      if (myid == 0) { args.PrintUsage(cout); }
      return 1;
   }
   if (myid == 0) { args.PrintOptions(cout); }

   Device device(devopt);
   if (myid == 0) { device.Print();}

   // Initialize and refine the starting mesh.
   Mesh *mesh = new Mesh(mesh_file, 1, 1, false);
   for (int lev = 0; lev < rs_levels; lev++)
   {
      mesh->UniformRefinement();
   }
   const int dim = mesh->Dimension();

   ParMesh *pmesh = new ParMesh(MPI_COMM_WORLD, *mesh);
   delete mesh;

   for (int lev = 0; lev < rp_levels; lev++)
   {
      pmesh->UniformRefinement();
   }

   // Setup background mesh for surface fitting
   ParMesh *pmesh_surf_fit_bg = NULL;
   if (surf_bg_mesh)
   {
      Mesh *mesh_surf_fit_bg =  new Mesh("../../data/inline-quad.mesh", 1, 1, false);
      for (int lev = 0; lev < 5; lev++) { mesh_surf_fit_bg->UniformRefinement(); }
      pmesh_surf_fit_bg = new ParMesh(MPI_COMM_WORLD, *mesh_surf_fit_bg);
      delete mesh_surf_fit_bg;
   }


   // Define a finite element space on the mesh. Here we use vector finite
   // elements which are tensor products of quadratic finite elements. The
   // number of components in the vector finite element space is specified by
   // the last parameter of the FiniteElementSpace constructor.
   FiniteElementCollection *fec;
   if (mesh_poly_deg <= 0)
   {
      fec = new QuadraticPosFECollection;
      mesh_poly_deg = 2;
   }
   else { fec = new H1_FECollection(mesh_poly_deg, dim); }
   ParFiniteElementSpace *pfespace = new ParFiniteElementSpace(pmesh, fec, dim);

   // Make the mesh curved based on the above finite element space. This
   // means that we define the mesh elements through a fespace-based
   // transformation of the reference element.
   pmesh->SetNodalFESpace(pfespace);

   // 6. Set up an empty right-hand side vector b, which is equivalent to b=0.
   Vector b(0);

   // 7. Get the mesh nodes (vertices and other degrees of freedom in the finite
   //    element space) as a finite element grid function in fespace. Note that
   //    changing x automatically changes the shapes of the mesh elements.
   ParGridFunction x(pfespace);
   pmesh->SetNodalGridFunction(&x);

   // 8. Define a vector representing the minimal local mesh size in the mesh
   //    nodes. We index the nodes using the scalar version of the degrees of
   //    freedom in pfespace. Note: this is partition-dependent.
   //
   //    In addition, compute average mesh size and total volume.
   Vector h0(pfespace->GetNDofs());
   h0 = infinity();
   double vol_loc = 0.0;
   Array<int> dofs;
   for (int i = 0; i < pmesh->GetNE(); i++)
   {
      // Get the local scalar element degrees of freedom in dofs.
      pfespace->GetElementDofs(i, dofs);
      // Adjust the value of h0 in dofs based on the local mesh size.
      const double hi = pmesh->GetElementSize(i);
      for (int j = 0; j < dofs.Size(); j++)
      {
         h0(dofs[j]) = min(h0(dofs[j]), hi);
      }
      vol_loc += pmesh->GetElementVolume(i);
   }
   double volume;
   MPI_Allreduce(&vol_loc, &volume, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   const double small_phys_size = pow(volume, 1.0 / dim) / 100.0;

   // 9. Add a random perturbation to the nodes in the interior of the domain.
   //    We define a random grid function of fespace and make sure that it is
   //    zero on the boundary and its values are locally of the order of h0.
   //    The latter is based on the DofToVDof() method which maps the scalar to
   //    the vector degrees of freedom in pfespace.
   ParGridFunction rdm(pfespace);
   rdm.Randomize();
   rdm -= 0.25; // Shift to random values in [-0.5,0.5].
   rdm *= jitter;
   rdm.HostReadWrite();
   // Scale the random values to be of order of the local mesh size.
   for (int i = 0; i < pfespace->GetNDofs(); i++)
   {
      for (int d = 0; d < dim; d++)
      {
         rdm(pfespace->DofToVDof(i,d)) *= h0(i);
      }
   }
   Array<int> vdofs;
   for (int i = 0; i < pfespace->GetNBE(); i++)
   {
      // Get the vector degrees of freedom in the boundary element.
      pfespace->GetBdrElementVDofs(i, vdofs);
      // Set the boundary values to zero.
      for (int j = 0; j < vdofs.Size(); j++) { rdm(vdofs[j]) = 0.0; }
   }
   x -= rdm;
   // Set the perturbation of all nodes from the true nodes.
   x.SetTrueVector();
   x.SetFromTrueVector();

   // 10. Save the starting (prior to the optimization) mesh to a file. This
   //     output can be viewed later using GLVis: "glvis -m perturbed -np
   //     num_mpi_tasks".
   {
      ostringstream mesh_name;
      mesh_name << "perturbed.mesh";
      ofstream mesh_ofs(mesh_name.str().c_str());
      mesh_ofs.precision(8);
      pmesh->PrintAsOne(mesh_ofs);
   }

   // 11. Store the starting (prior to the optimization) positions.
   ParGridFunction x0(pfespace);
   x0 = x;

   // 12. Form the integrator that uses the chosen metric and target.
   double tauval = -0.1;
   TMOP_QualityMetric *metric = NULL;
   TMOP_QualityMetric *surf_metric = NULL;
   switch (metric_id)
   {
      // T-metrics
      case 1: metric = new TMOP_Metric_001; break;
      case 2: metric = new TMOP_Metric_002; break;
      case 7: metric = new TMOP_Metric_007; break;
      case 9: metric = new TMOP_Metric_009; break;
      case 14: metric = new TMOP_Metric_014; break;
      case 22: metric = new TMOP_Metric_022(tauval); break;
      case 50: metric = new TMOP_Metric_050; break;
      case 55: metric = new TMOP_Metric_055; break;
      case 56: metric = new TMOP_Metric_056; break;
      case 58: metric = new TMOP_Metric_058; break;
      case 77: metric = new TMOP_Metric_077; break;
      case 80: metric = new TMOP_Metric_080(0.5); break;
      case 85: metric = new TMOP_Metric_085; break;
      case 98: metric = new TMOP_Metric_098; break;
      // case 211: metric = new TMOP_Metric_211; break;
      // case 252: metric = new TMOP_Metric_252(tauval); break;
      case 301: metric = new TMOP_Metric_301; break;
      case 302: metric = new TMOP_Metric_302; break;
      case 303: metric = new TMOP_Metric_303; break;
      // case 311: metric = new TMOP_Metric_311; break;
      case 313: metric = new TMOP_Metric_313(tauval); break;
      case 315: metric = new TMOP_Metric_315; break;
      case 316: metric = new TMOP_Metric_316; break;
      case 321: metric = new TMOP_Metric_321; break;
      // case 352: metric = new TMOP_Metric_352(tauval); break;
      // A-metrics
      case 11: metric = new TMOP_AMetric_011; break;
      case 36: metric = new TMOP_AMetric_036; break;
      case 107: metric = new TMOP_AMetric_107a; break;
      case 126: metric = new TMOP_AMetric_126(0.9); break;
      default:
         if (myid == 0) { cout << "Unknown metric_id: " << metric_id << endl; }
         return 3;
   }
   if (surf_approach == 2)
   {
      surf_metric = new TMOP_Metric_000;
   }

   TargetConstructor::TargetType target_t;
   TargetConstructor *target_c = NULL;
   HessianCoefficient *adapt_coeff = NULL;
   H1_FECollection ind_fec(mesh_poly_deg, dim);
   ParFiniteElementSpace ind_fes(pmesh, &ind_fec);
   ParFiniteElementSpace ind_fesv(pmesh, &ind_fec, dim);
   ParGridFunction size(&ind_fes), aspr(&ind_fes), disc(&ind_fes), ori(&ind_fes);
   ParGridFunction aspr3d(&ind_fesv);

   const AssemblyLevel al =
      pa ? AssemblyLevel::PARTIAL : AssemblyLevel::LEGACY;

   switch (target_id)
   {
      case 1: target_t = TargetConstructor::IDEAL_SHAPE_UNIT_SIZE; break;
      case 2: target_t = TargetConstructor::IDEAL_SHAPE_EQUAL_SIZE; break;
      case 3: target_t = TargetConstructor::IDEAL_SHAPE_GIVEN_SIZE; break;
      case 4:
      {
         target_t = TargetConstructor::GIVEN_FULL;
         AnalyticAdaptTC *tc = new AnalyticAdaptTC(target_t);
         adapt_coeff = new HessianCoefficient(dim, metric_id);
         tc->SetAnalyticTargetSpec(NULL, NULL, adapt_coeff);
         target_c = tc;
         break;
      }
      default:
         if (myid == 0) { cout << "Unknown target_id: " << target_id << endl; }
         return 3;
   }

   if (target_c == NULL)
   {
      target_c = new TargetConstructor(target_t, MPI_COMM_WORLD);
   }
   target_c->SetNodes(x0);

   TMOP_Integrator *he_nlf_integ = new TMOP_Integrator(metric, target_c);
   TMOP_Integrator *surf_integ = NULL;
   if (surf_approach == 2)
   {
      surf_integ = new TMOP_Integrator(surf_metric, target_c);
   }

   // Finite differences for computations of derivatives.
   if (fdscheme)
   {
      MFEM_VERIFY(pa == false, "PA for finite differences is not implemented.");
      he_nlf_integ->EnableFiniteDifferences(x);
   }
   he_nlf_integ->SetExactActionFlag(exactaction);
   if (surf_approach == 2)
   {
      surf_integ->SetExactActionFlag(exactaction);
   }

   // Setup the quadrature rules for the TMOP integrator.
   IntegrationRules *irules = NULL;
   switch (quad_type)
   {
      case 1: irules = &IntRulesLo; break;
      case 2: irules = &IntRules; break;
      case 3: irules = &IntRulesCU; break;
      default:
         if (myid == 0) { cout << "Unknown quad_type: " << quad_type << endl; }
         return 3;
   }
   he_nlf_integ->SetIntegrationRules(*irules, quad_order);
   if (surf_approach == 2)
   {
      surf_integ->SetIntegrationRules(*irules, quad_order);
   }
   if (myid == 0 && dim == 2)
   {
      cout << "Triangle quadrature points: "
           << irules->Get(Geometry::TRIANGLE, quad_order).GetNPoints()
           << "\nQuadrilateral quadrature points: "
           << irules->Get(Geometry::SQUARE, quad_order).GetNPoints() << endl;
   }
   if (myid == 0 && dim == 3)
   {
      cout << "Tetrahedron quadrature points: "
           << irules->Get(Geometry::TETRAHEDRON, quad_order).GetNPoints()
           << "\nHexahedron quadrature points: "
           << irules->Get(Geometry::CUBE, quad_order).GetNPoints()
           << "\nPrism quadrature points: "
           << irules->Get(Geometry::PRISM, quad_order).GetNPoints() << endl;
   }

   ConstantCoefficient lim_coeff(lim_const);
   ConstantCoefficient coef_zeta(adapt_lim_const);
   GridFunction zeta_0(&ind_fes);

   // Surface fitting.
   L2_FECollection mat_coll(0, dim);
   H1_FECollection sigma_fec(mesh_poly_deg, dim);
   ParFiniteElementSpace sigma_fes(pmesh, &sigma_fec);
   ParFiniteElementSpace mat_fes(pmesh, &mat_coll);
   ParGridFunction mat(&mat_fes);
   ParGridFunction marker_gf(&sigma_fes);
   ParGridFunction ls_0(&sigma_fes);
   Array<bool> marker(ls_0.Size());
   ConstantCoefficient coef_ls(surface_fit_const);
   AdaptivityEvaluator *adapt_surface = NULL;

   // Background mesh FECollection, FESpace, and GridFunction
   H1_FECollection *sigma_bg_fec = NULL;
   ParFiniteElementSpace *sigma_bg_fes = NULL;
   ParGridFunction *ls_bg_0 = NULL;
   ParFiniteElementSpace *ls_bg_grad_fes = NULL;
   ParGridFunction *ls_bg_grad = NULL;
   ParFiniteElementSpace *sigma_grad_fes = NULL;
   ParGridFunction *sigma_grad = NULL;
   ParFiniteElementSpace *ls_bg_hess_fes = NULL;
   ParGridFunction *ls_bg_hess = NULL;
   ParFiniteElementSpace *sigma_hess_fes = NULL;
   ParGridFunction *sigma_hess = NULL;
   if (surf_bg_mesh)
   {
      pmesh_surf_fit_bg->SetCurvature(mesh_poly_deg);

      sigma_bg_fec = new H1_FECollection(mesh_poly_deg, dim);
      sigma_bg_fes = new ParFiniteElementSpace(pmesh_surf_fit_bg, sigma_bg_fec);
      ls_bg_0 = new ParGridFunction(sigma_bg_fes);

      ls_bg_grad_fes = new ParFiniteElementSpace(pmesh_surf_fit_bg, sigma_bg_fec,
                                                 pmesh_surf_fit_bg->Dimension());
      ls_bg_grad = new ParGridFunction(ls_bg_grad_fes);
      sigma_grad_fes = new ParFiniteElementSpace(pmesh, &sigma_fec,
                                                 pmesh->Dimension());
      sigma_grad = new ParGridFunction(sigma_grad_fes);

      int n_hessian_bg = pmesh_surf_fit_bg->Dimension()
                         *pmesh_surf_fit_bg->Dimension();
      ls_bg_hess_fes = new ParFiniteElementSpace(pmesh_surf_fit_bg, sigma_bg_fec,
                                                 n_hessian_bg);
      ls_bg_hess = new ParGridFunction(ls_bg_hess_fes);
      sigma_hess_fes = new ParFiniteElementSpace(pmesh, &sigma_fec,
                                                 pmesh->Dimension()*pmesh->Dimension());
      sigma_hess = new ParGridFunction(sigma_hess_fes);
   }

   FunctionCoefficient *ls_coeff = NULL;
   if (surface_fit_const > 0.0)
   {
      MFEM_VERIFY(pa == false,
                  "Surface fitting with PA is not implemented yet.");

      if (surf_ls_type == 1) //Circle
      {
         ls_coeff = new FunctionCoefficient(circle_level_set);
      }
      else if (surf_ls_type == 2) // Squircle
      {
         ls_coeff = new FunctionCoefficient(squircle_level_set);
      }
      else if (surf_ls_type == 3) // Butterfly
      {
         ls_coeff = new FunctionCoefficient(butterfly_level_set);
      }
      else
      {
         MFEM_ABORT("Surface fitting level set type not implemented yet.")
      }
      ls_0.ProjectCoefficient(*ls_coeff);
      if (surf_bg_mesh)
      {
         ls_bg_0->ProjectCoefficient(*ls_coeff);

         //Setup gradient of the background mesh
         ls_bg_grad->ReorderByNodes();
         for (int d = 0; d < pmesh_surf_fit_bg->Dimension(); d++)
         {
            ParGridFunction ls_bg_grad_comp(sigma_bg_fes,
                                            ls_bg_grad->GetData()+d*ls_bg_0->Size());
            ls_bg_0->GetDerivative(1, d, ls_bg_grad_comp);
         }

         //Setup Hessian on background mesh
         ls_bg_hess->ReorderByNodes();
         int id = 0;
         for (int d = 0; d < pmesh_surf_fit_bg->Dimension(); d++)
         {
            for (int idir = 0; idir < pmesh_surf_fit_bg->Dimension(); idir++)
            {
               ParGridFunction ls_bg_grad_comp(sigma_bg_fes,
                                               ls_bg_grad->GetData()+d*ls_bg_0->Size());
               ParGridFunction ls_bg_hess_comp(sigma_bg_fes,
                                               ls_bg_hess->GetData()+id*ls_bg_0->Size());
               ls_bg_grad_comp.GetDerivative(1, idir, ls_bg_hess_comp);
               id++;
            }
         }

      }

      for (int i = 0; i < pmesh->GetNE(); i++)
      {
         mat(i) = material_id(i, ls_0);
         pmesh->SetAttribute(i, mat(i) + 1);
      }

      GridFunctionCoefficient coeff_mat(&mat);
      marker_gf.ProjectDiscCoefficient(coeff_mat, GridFunction::ARITHMETIC);

      if (marking_type == 1)
      {
         for (int j = 0; j < marker.Size(); j++)
         {
            if (marker_gf(j) > 0.1 && marker_gf(j) < 0.9)
            {
               marker[j] = true;
               marker_gf(j) = 1.0;
            }
            else
            {
               marker[j] = false;
               marker_gf(j) = 0.0;
            }
         }
      }
      else if (marking_type == 2)
      {
         for (int j = 0; j < marker.Size(); j++)
         {
            marker[j] = false;
         }
         marker_gf = 0.0;
         for (int i = 0; i < pmesh->GetNBE(); i++)
         {
            const int attr = pmesh->GetBdrElement(i)->GetAttribute();
            if (attr == 3)
            {
               sigma_fes.GetBdrElementVDofs(i, vdofs);
               for (int j = 0; j < vdofs.Size(); j++)
               {
                  marker[vdofs[j]] = true;
                  marker_gf(vdofs[j]) = 1.0;
               }
            }
         }
      }

      if (adapt_eval == 0)
      {
         adapt_surface = new AdvectorCG;
      }
      else if (adapt_eval == 1)
      {
#ifdef MFEM_USE_GSLIB
         adapt_surface = new InterpolatorFP;
#else
         MFEM_ABORT("MFEM is not built with GSLIB support!");
#endif
      }
      else { MFEM_ABORT("Bad interpolation option."); }

      TMOP_Integrator *surface_fitting_integrator = surf_approach == 1 ?
                                                    he_nlf_integ :
                                                    surf_integ;

      if (!surf_bg_mesh)
      {
         surface_fitting_integrator->EnableSurfaceFitting(ls_0, marker, coef_ls,
                                                          *adapt_surface);
      }
      else
      {
         surface_fitting_integrator->EnableSurfaceFittingFromSource(*ls_bg_0, ls_0,
                                                                    marker, coef_ls,
                                                                    *adapt_surface,
                                                                    *ls_bg_grad, *sigma_grad,
                                                                    *ls_bg_hess, *sigma_hess);

      }

      if (visualization)
      {
         socketstream vis1, vis2, vis3, vis4, vis5;
         common::VisualizeField(vis1, "localhost", 19916, ls_0, "Level Set 0",
                                300, 600, 300, 300);
         common::VisualizeField(vis2, "localhost", 19916, mat, "Materials",
                                600, 600, 300, 300);
         common::VisualizeField(vis3, "localhost", 19916, marker_gf, "Dofs to Move",
                                900, 600, 300, 300);
         if (surf_bg_mesh)
         {
            common::VisualizeField(vis4, "localhost", 19916, *ls_bg_0,
                                   "Level Set 0 Source",
                                   300, 600, 300, 300);
            common::VisualizeField(vis5, "localhost", 19916, *ls_bg_grad,
                                   "Level Set Gradient",
                                   600, 600, 300, 300);
         }
      }
   }

   // Has to be after the enabling of the limiting / alignment, as it computes
   // normalization factors for these terms as well.
   if (normalization) { he_nlf_integ->ParEnableNormalization(x0); }

   // 13. Setup the final NonlinearForm (which defines the integral of interest,
   //     its first and second derivatives). Here we can use a combination of
   //     metrics, i.e., optimize the sum of two integrals, where both are
   //     scaled by used-defined space-dependent weights.  Note that there are
   //     no command-line options for the weights and the type of the second
   //     metric; one should update those in the code.
   ParNonlinearForm a(pfespace);
   if (pa) { a.SetAssemblyLevel(AssemblyLevel::PARTIAL); }
   a.AddDomainIntegrator(he_nlf_integ);

   ParNonlinearForm *a_surf = NULL;
   if (surf_approach == 2)
   {
      a_surf = new ParNonlinearForm(pfespace);
      a_surf->AddDomainIntegrator(surf_integ);
   }

   // Compute the minimum det(J) of the starting mesh.
   tauval = infinity();
   const int NE = pmesh->GetNE();
   for (int i = 0; i < NE; i++)
   {
      const IntegrationRule &ir =
         irules->Get(pfespace->GetFE(i)->GetGeomType(), quad_order);
      ElementTransformation *transf = pmesh->GetElementTransformation(i);
      for (int j = 0; j < ir.GetNPoints(); j++)
      {
         transf->SetIntPoint(&ir.IntPoint(j));
         tauval = min(tauval, transf->Jacobian().Det());
      }
   }
   double minJ0;
   MPI_Allreduce(&tauval, &minJ0, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
   tauval = minJ0;
   if (myid == 0)
   { cout << "Minimum det(J) of the original mesh is " << tauval << endl; }

   if (tauval < 0.0 && metric_id != 22 && metric_id != 211 && metric_id != 252
       && metric_id != 311 && metric_id != 313 && metric_id != 352)
   {
      MFEM_ABORT("The input mesh is inverted! Try an untangling metric.");
   }
   if (tauval < 0.0)
   {
      MFEM_VERIFY(target_t == TargetConstructor::IDEAL_SHAPE_UNIT_SIZE,
                  "Untangling is supported only for ideal targets.");

      const DenseMatrix &Wideal =
         Geometries.GetGeomToPerfGeomJac(pfespace->GetFE(0)->GetGeomType());
      tauval /= Wideal.Det();

      double h0min = h0.Min(), h0min_all;
      MPI_Allreduce(&h0min, &h0min_all, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
      // Slightly below minJ0 to avoid div by 0.
      tauval -= 0.01 * h0min_all;
   }

   // For HR tests, the energy is normalized by the number of elements.
   const double init_energy = a.GetParGridFunctionEnergy(x);
   double init_metric_energy = init_energy;
   if (lim_const > 0.0 || adapt_lim_const > 0.0 || surface_fit_const > 0.0)
   {
      lim_coeff.constant = 0.0;
      coef_zeta.constant = 0.0;
      coef_ls.constant   = 0.0;
      init_metric_energy = a.GetParGridFunctionEnergy(x);
      lim_coeff.constant = lim_const;
      coef_zeta.constant = adapt_lim_const;
      coef_ls.constant   = surface_fit_const;
   }

   // Visualize the starting mesh and metric values.
   // Note that for combinations of metrics, this only shows the first metric.
   if (visualization)
   {
      char title[] = "Initial metric values";
      vis_tmop_metric_p(mesh_poly_deg, *metric, *target_c, *pmesh, title, 0);
   }

   // 14. Fix all boundary nodes, or fix only a given component depending on the
   //     boundary attributes of the given mesh.  Attributes 1/2/3 correspond to
   //     fixed x/y/z components of the node.  Attribute 4 corresponds to an
   //     entirely fixed node.  Other boundary attributes do not affect the node
   //     movement boundary conditions.
   if (move_bnd == false)
   {
      Array<int> ess_bdr(pmesh->bdr_attributes.Max());
      ess_bdr = 1;
      if (surf_approach == 1)
      {
         ess_bdr[2] = 0;
      }
      a.SetEssentialBC(ess_bdr);
      if (surf_approach == 2)
      {
         ess_bdr[2] = 0.0;
         a_surf->SetEssentialBC(ess_bdr);
      }
   }
   else
   {
      int n = 0;
      for (int i = 0; i < pmesh->GetNBE(); i++)
      {
         const int nd = pfespace->GetBE(i)->GetDof();
         const int attr = pmesh->GetBdrElement(i)->GetAttribute();
         MFEM_VERIFY(!(dim == 2 && attr == 3),
                     "Boundary attribute 3 must be used only for 3D meshes. "
                     "Adjust the attributes (1/2/3/4 for fixed x/y/z/all "
                     "components, rest for free nodes), or use -fix-bnd.");
         if (attr == 1 || attr == 2 || attr == 3) { n += nd; }
         if (attr == 4) { n += nd * dim; }
      }
      Array<int> ess_vdofs(n), vdofs;
      n = 0;
      for (int i = 0; i < pmesh->GetNBE(); i++)
      {
         const int nd = pfespace->GetBE(i)->GetDof();
         const int attr = pmesh->GetBdrElement(i)->GetAttribute();
         pfespace->GetBdrElementVDofs(i, vdofs);
         if (attr == 1) // Fix x components.
         {
            for (int j = 0; j < nd; j++)
            { ess_vdofs[n++] = vdofs[j]; }
         }
         else if (attr == 2) // Fix y components.
         {
            for (int j = 0; j < nd; j++)
            { ess_vdofs[n++] = vdofs[j+nd]; }
         }
         else if (attr == 3) // Fix z components.
         {
            for (int j = 0; j < nd; j++)
            { ess_vdofs[n++] = vdofs[j+2*nd]; }
         }
         else if (attr == 4) // Fix all components.
         {
            for (int j = 0; j < vdofs.Size(); j++)
            { ess_vdofs[n++] = vdofs[j]; }
         }
      }
      a.SetEssentialVDofs(ess_vdofs);
   }

   // 15. As we use the Newton method to solve the resulting nonlinear system,
   //     here we setup the linear solver for the system's Jacobian.
   Solver *S = NULL, *S_prec = NULL;
   const double linsol_rtol = 1e-12;
   if (lin_solver == 0)
   {
      S = new DSmoother(1, 1.0, max_lin_iter);
   }
   else if (lin_solver == 1)
   {
      CGSolver *cg = new CGSolver(MPI_COMM_WORLD);
      cg->SetMaxIter(max_lin_iter);
      cg->SetRelTol(linsol_rtol);
      cg->SetAbsTol(0.0);
      cg->SetPrintLevel(verbosity_level >= 2 ? 3 : -1);
      S = cg;
   }
   else
   {
      MINRESSolver *minres = new MINRESSolver(MPI_COMM_WORLD);
      minres->SetMaxIter(max_lin_iter);
      minres->SetRelTol(linsol_rtol);
      minres->SetAbsTol(0.0);
      if (verbosity_level > 2) { minres->SetPrintLevel(1); }
      else { minres->SetPrintLevel(verbosity_level == 2 ? 3 : -1); }
      if (lin_solver == 3 || lin_solver == 4)
      {
         if (pa)
         {
            MFEM_VERIFY(lin_solver != 4, "PA l1-Jacobi is not implemented");
            S_prec = new OperatorJacobiSmoother;
         }
         else
         {
            HypreSmoother *hs = new HypreSmoother;
            hs->SetType((lin_solver == 3) ? HypreSmoother::Jacobi
                        : HypreSmoother::l1Jacobi, 1);
            S_prec = hs;
         }
         minres->SetPreconditioner(*S_prec);
      }
      S = minres;
   }

   // Perform the nonlinear optimization.
   const IntegrationRule &ir =
      irules->Get(pfespace->GetFE(0)->GetGeomType(), quad_order);
   TMOPNewtonSolver solver(pfespace->GetComm(), ir, solver_type);
   solver.SetIntegrationRules(*irules, quad_order);
   if (solver_type == 0)
   {
      solver.SetPreconditioner(*S);
   }
   // For untangling, the solver will update the min det(T) values.
   if (tauval < 0.0) { solver.SetMinDetPtr(&tauval); }
   solver.SetMaxIter(solver_iter);
   solver.SetRelTol(solver_rtol);
   solver.SetAbsTol(0.0);
   if (solver_art_type > 0)
   {
      solver.SetAdaptiveLinRtol(solver_art_type, 0.5, 0.9);
   }
   solver.SetPrintLevel(verbosity_level >= 1 ? 1 : -1);
   solver.SetOperator(a);

   TMOPNewtonSolver *solver_surf = NULL;
   if (surf_approach == 2)
   {
      solver_surf = new TMOPNewtonSolver(pfespace->GetComm(), ir, solver_type);
      solver_surf->SetIntegrationRules(*irules, quad_order);
      if (solver_type == 0)
      {
         solver_surf->SetPreconditioner(*S);
      }
      // For untangling, the solver will update the min det(T) values.
      if (tauval < 0.0) { solver_surf->SetMinDetPtr(&tauval); }
      solver_surf->SetMaxIter(surf_solver_iter);
      solver_surf->SetRelTol(solver_rtol);
      solver_surf->SetAbsTol(0.0);
      if (solver_art_type > 0)
      {
         solver_surf->SetAdaptiveLinRtol(solver_art_type, 0.5, 0.9);
      }
      solver_surf->SetPrintLevel(verbosity_level >= 1 ? 1 : -1);
      solver_surf->SetOperator(*a_surf);
   }

   if (surf_approach == 1)
   {
      solver.Mult(b, x.GetTrueVector());
      x.SetFromTrueVector();
   }
   else
   {
      for (int j = 0; j < outer_iter; j++)
      {
         if (myid == 0)
         {
            cout << "Outer iteration: " << j << std::endl;
         }
         solver.Mult(b, x.GetTrueVector());
         x.SetFromTrueVector();
         solver_surf->Mult(b, x.GetTrueVector());
         x.SetFromTrueVector();
         if ( j == 0)
         {
            solver.SetAbsTol(solver.GetNormGoal());
            solver_surf->SetAbsTol(solver_surf->GetNormGoal());
         }
      }
      solver.SetMaxIter(1*solver_iter);
      solver.Mult(b, x.GetTrueVector());
      x.SetFromTrueVector();

      GridFunction *sigma_int = surf_integ->GetSigma();
      ls_0 = *sigma_int;
      if (visualization)
      {
         socketstream vis1;
         common::VisualizeField(vis1, "localhost", 19916, ls_0,
                                "Level Set interpolated on mesh",
                                300, 600, 300, 300);
      }
   }

   // 16. Save the  optimized mesh to a file. This output can be viewed later
   //     using GLVis: "glvis -m optimized -np num_mpi_tasks".
   {
      ostringstream mesh_name;
      mesh_name << "optimized.mesh";
      ofstream mesh_ofs(mesh_name.str().c_str());
      mesh_ofs.precision(8);
      pmesh->PrintAsOne(mesh_ofs);
   }

   // Compute the final energy of the functional.
   const double fin_energy = a.GetParGridFunctionEnergy(x);
   double fin_metric_energy = fin_energy;
   if (lim_const > 0.0 || adapt_lim_const > 0.0 || surface_fit_const > 0.0)
   {
      lim_coeff.constant = 0.0;
      coef_zeta.constant = 0.0;
      coef_ls.constant   = 0.0;
      fin_metric_energy  = a.GetParGridFunctionEnergy(x);
      lim_coeff.constant = lim_const;
      coef_zeta.constant = adapt_lim_const;
      coef_ls.constant   = surface_fit_const;
   }
   if (myid == 0)
   {
      cout << std::scientific << std::setprecision(4);
      cout << "Initial strain energy: " << init_energy
           << " = metrics: " << init_metric_energy
           << " + extra terms: " << init_energy - init_metric_energy << endl;
      cout << "  Final strain energy: " << fin_energy
           << " = metrics: " << fin_metric_energy
           << " + extra terms: " << fin_energy - fin_metric_energy << endl;
      cout << "The strain energy decreased by: "
           << (init_energy - fin_energy) * 100.0 / init_energy << " %." << endl;
   }

   // Visualize the final mesh and metric values.
   if (visualization)
   {
      char title[] = "Final metric values";
      vis_tmop_metric_p(mesh_poly_deg, *metric, *target_c, *pmesh, title, 600);
   }

   if (surface_fit_const > 0.0)
   {

      double err_avg, err_max;
      he_nlf_integ->GetSurfaceFittingErrors(err_avg, err_max);
      if (myid == 0)
      {
         cout << "Avg fitting error: " << err_avg << std::endl
              << "Max fitting error: " << err_max << std::endl;
      }

      ls_0.ProjectCoefficient(*ls_coeff);
      if (visualization)
      {
         socketstream vis1, vis2, vis3;
         common::VisualizeField(vis1, "localhost", 19916, ls_0, "Level Set 0 final mesh",
                                300, 600, 300, 300);
         common::VisualizeField(vis2, "localhost", 19916, mat,
                                "Materials", 600, 900, 300, 300);
         common::VisualizeField(vis3, "localhost", 19916, marker_gf,
                                "Surface dof", 900, 900, 300, 300);
      }
   }

   // 19. Visualize the mesh displacement.
   if (visualization)
   {
      socketstream sock;
      x0 -= x;
      if (myid == 0)
      {
         sock.open("localhost", 19916);
         sock << "solution\n";
      }
      pmesh->PrintAsOne(sock);
      x0.SaveAsOne(sock);
      if (myid == 0)
      {
         sock << "window_title 'Displacements'\n"
              << "window_geometry "
              << 300 << " " << 0 << " " << 600 << " " << 600 << "\n"
              << "keys jRmclA" << endl;
      }
   }

   // 20. Free the used memory.
   delete solver_surf;
   delete S;
   delete S_prec;
   delete a_surf;
   delete adapt_surface;
   delete ls_coeff;
   delete sigma_hess;
   delete sigma_hess_fes;
   delete ls_bg_hess;
   delete ls_bg_hess_fes;
   delete sigma_grad;
   delete sigma_grad_fes;
   delete ls_bg_grad;
   delete ls_bg_grad_fes;
   delete ls_bg_0;
   delete sigma_bg_fes;
   delete sigma_bg_fec;
   delete target_c;
   delete adapt_coeff;
   delete surf_metric;
   delete metric;
   delete pfespace;
   delete fec;
   delete pmesh;

   MPI_Finalize();
   return 0;
}
