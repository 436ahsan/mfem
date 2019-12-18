
#include "mfem.hpp"
#include <fstream>
#include <iostream>
#include "additive_schwarz.hpp"
#include "../as/schwarz.hpp"

using namespace std;
using namespace mfem;

int main(int argc, char *argv[])
{
   // 1. Parse command-line options.
   const char *mesh_file = "../../../data/star.mesh";
   // const char *mesh_file = "../../../data/beam-quad.mesh";
   int order = 1;
   int ref_levels = 1;
   bool visualization = true;
   StopWatch chrono;

   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree) or -1 for"
                  " isoparametric space.");
   args.AddOption(&ref_levels, "-ref", "--ref_levels",
                  "Number of uniform h-refinements");               
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.Parse();
   if (!args.Good())
   {
      args.PrintUsage(cout);
      return 1;
   }
   args.PrintOptions(cout);


   Mesh *mesh;
   mesh = new Mesh(mesh_file, 1, 1);
   // mesh = new Mesh(1, 1, Element::QUADRILATERAL, true, 1, 1, false);
   int dim = mesh->Dimension();

   for (int l = 0; l < ref_levels; l++)
   {
      mesh->UniformRefinement();
   }

   FiniteElementCollection *fec = new H1_FECollection(order, dim);
   // FiniteElementCollection *fec = new ND_FECollection(order, dim);
   FiniteElementSpace * fespace = new FiniteElementSpace(mesh, fec);
   Array<int> ess_tdof_list;
   if (mesh->bdr_attributes.Size())
   {
      Array<int> ess_bdr(mesh->bdr_attributes.Max());
      ess_bdr = 1;
      fespace->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);
   }

   // 7. Set up the linear form b(.) which corresponds to the right-hand side of
   //    the FEM linear system, which in this case is (1,phi_i) where phi_i are
   //    the basis functions in the finite element fespace.
   LinearForm *b = new LinearForm(fespace);
   ConstantCoefficient one(1.0);
   b->AddDomainIntegrator(new DomainLFIntegrator(one));
   b->Assemble();

   // 8. Define the solution vector x as a finite element grid function
   //    corresponding to fespace. Initialize x with initial guess of zero,
   //    which satisfies the boundary conditions.
   GridFunction x(fespace);
   x = 0.0;

   // 9. Set up the bilinear form a(.,.) on the finite element space
   //    corresponding to the Laplacian operator -Delta, by adding the Diffusion
   //    domain integrator.
   BilinearForm *a = new BilinearForm(fespace);
   a->SetDiagonalPolicy(mfem::Matrix::DIAG_ONE);
   a->AddDomainIntegrator(new DiffusionIntegrator(one));

   // 10. Assemble the bilinear form and the corresponding linear system,
   //     applying any necessary transformations such as: eliminating boundary
   //     conditions, applying conforming constraints for non-conforming AMR,
   //     static condensation, etc.
   a->Assemble();

   OperatorPtr A;
   Vector B, X;
   a->FormLinearSystem(ess_tdof_list, x, *b, A, X, B);

   cout << "Size of linear system: " << A->Height() << endl;

   // Use a simple symmetric Gauss-Seidel preconditioner with PCG.
   // GSSmoother M((SparseMatrix&)(*A));
   Array<double> times;
   GSSmoother M((SparseMatrix&)(*A));
   chrono.Clear();
   chrono.Start();
   AddSchwarz * S1 = new AddSchwarz(a);
   S1->SetOperator((SparseMatrix&)(*A));
   S1->SetDumpingParam(2.0/3.0);
   S1->SetNumSmoothSteps(3);
   chrono.Stop();
   times.Append(chrono.RealTime());

   chrono.Clear();
   chrono.Start();
   SchwarzSmoother * S2 = new SchwarzSmoother(mesh, 0, fespace,&(SparseMatrix&)(*A), ess_tdof_list);
   S2->SetDumpingParam(2.0/3.0);
   S2->SetNumSmoothSteps(3);
   chrono.Stop();
   times.Append(chrono.RealTime());
   int maxit = 2000;
   double rtol = 1e-8;
   double atol = 1e-8;
   CGSolver pcg;
   pcg.SetPrintLevel(1);
   pcg.SetMaxIter(maxit);
   pcg.SetRelTol(rtol);
   pcg.SetAbsTol(atol);
   pcg.SetOperator((SparseMatrix&)(*A));
   X = 0.0;
   // pcg.SetPreconditioner(M);
   // pcg.Mult(B, X);
   // X = 0.0;
   chrono.Clear();
   chrono.Start();
   pcg.SetPreconditioner(*S1);
   pcg.Mult(B, X);
   chrono.Stop();
   times.Append(chrono.RealTime());
   X = 0.0;

   chrono.Clear();
   chrono.Start();
   pcg.SetPreconditioner(*S2);
   pcg.Mult(B, X);
   chrono.Stop();
   times.Append(chrono.RealTime());

   cout << "S1 Times: " << times[0] << ", " << times[2] << endl;
   cout << "S2 Times: " << times[1] << ", " << times[3] << endl;



   // 12. Recover the solution as a finite element grid function.
   a->RecoverFEMSolution(X, *b, x);

   // 14. Send the solution by socket to a GLVis server.
   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;
      socketstream sol_sock(vishost, visport);
      sol_sock.precision(8);
      sol_sock << "mesh\n" << *mesh << flush;
   }

   // 15. Free the used memory.
   delete S1;
   delete S2;
   delete a;
   delete b;
   delete fespace;
   delete fec;
   delete mesh;

   return 0;
}
