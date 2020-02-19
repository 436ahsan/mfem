//
// Compile with: make helmholtz
//
// Sample runs:  helmholtz -m ../data/one-hex.mesh
//               helmholtz -m ../data/fichera.mesh
//               helmholtz -m ../data/fichera-mixed.mesh
//
// Description:  This example code demonstrates the use of MFEM to define a
//               simple finite element discretization of the Helmholtz problem
//               -Delta p - omega^2 p = 1 with impedance boundary condition.
//
#include "mfem.hpp"
#include <fstream>
#include <iostream>
#include "complex_additive_schwarzp.hpp"

using namespace std;
using namespace mfem;

// Exact solution and r.h.s., see below for implementation.
double f_exact_Re(const Vector &x);
double f_exact_Im(const Vector &x);

int dim;
double omega;
int sol = 1;
bool pml = false;
double length = 1.0;
double pml_length = 0.25;
bool scatter = false;

#ifndef MFEM_USE_SUPERLU
#error This example requires that MFEM is built with MFEM_USE_PETSC=YES
#endif

int main(int argc, char *argv[])
{

   // 1. Initialise MPI
   int num_procs, myid;
   MPI_Init(&argc, &argv);                    // Initialise MPI
   MPI_Comm_size(MPI_COMM_WORLD,
                 &num_procs); //total number of processors available
   MPI_Comm_rank(MPI_COMM_WORLD, &myid);      // Determine process identifier

   //-----------------------------------------------------------------------------

   // 2. Parse command-line options.
   // geometry file
   const char *mesh_file = "../../data/one-hex.mesh";
   // finite element order of approximation
   int order = 1;
   // static condensation flag
   bool static_cond = false;
   bool visualization = 1;
   // number of wavelengths
   double k = 0.5;
   // number of mg levels
   int ref = 1;
   // number of initial ref
   int initref = 1;
   // dimension
   int nd = 2;

   // optional command line inputs
   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree) or -1 for"
                  " isoparametric space.");
   args.AddOption(&nd, "-nd", "--dim","Problem space dimension");
   args.AddOption(&sol, "-sol", "--exact",
                  "Exact solution flag - 0:polynomial, 1: plane wave, -1: unknown exact");
   args.AddOption(&k, "-k", "--wavelengths",
                  "Number of wavelengths.");
   args.AddOption(&pml, "-pml", "--pml", "-no-pml",
                  "--no-pml", "Enable PML.");
   args.AddOption(&pml_length, "-pml_length", "--pml_length",
                  "Length of the PML region in each direction");
   args.AddOption(&length, "-length", "--length",
                  "length of the domainin in each direction.");
   args.AddOption(&ref, "-ref", "--ref",
                  "Number of Refinements.");
   args.AddOption(&initref, "-initref", "--initref",
                  "Number of initial refinements.");
   args.AddOption(&static_cond, "-sc", "--static-condensation", "-no-sc",
                  "--no-static-condensation", "Enable static condensation.");
   args.AddOption(&scatter, "-scat", "--scattering-prob", "-no-scat",
                  "--no-scattering", "Solve a scattering problem");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.Parse();
   // check if the inputs are correct
   if (!args.Good())
   {
      if (myid == 0)
      {
         args.PrintUsage(cout);
      }
      MPI_Finalize();
      return 1;
   }
   if (myid == 0)
   {
      args.PrintOptions(cout);
   }
   // Angular frequency
   omega = 2.0 * M_PI * k;

   // 3. Read the mesh from the given mesh file.
   Mesh *mesh;

   if (nd == 2)
   {
      mesh = new Mesh(1, 1, Element::QUADRILATERAL, true, length, length, false);
   }
   else
   {
      mesh = new Mesh(1, 1, 1, Element::HEXAHEDRON, true, length, length, length,
                      false);
   }

   // 3. Executing uniform h-refinement
   for (int i = 0; i < initref; i++ )
   {
      mesh->UniformRefinement();
   }
   dim = mesh->Dimension();

   // 5. Define a parallel mesh and delete the serial mesh.
   ParMesh *pmesh = new ParMesh(MPI_COMM_WORLD, *mesh);
   delete mesh;

   // ----------------------------------------------------------------------------

   for (int i = 0; i < ref; i++)
   {
      pmesh->UniformRefinement();
   }

   // 6. Define a finite element space on the mesh.
   FiniteElementCollection *fec = new H1_FECollection(order, dim);
   ParFiniteElementSpace *fespace = new ParFiniteElementSpace(pmesh, fec);

   // 6. Set up the linear form (Real and Imaginary part)
   FunctionCoefficient f_Re(f_exact_Re);
   FunctionCoefficient f_Im(f_exact_Im);

   // ParLinearForm *b_Re(new ParLinearForm);
   ParComplexLinearForm b(fespace, ComplexOperator::HERMITIAN);
   b.AddDomainIntegrator(new DomainLFIntegrator(f_Re),
                         new DomainLFIntegrator(f_Im));
   b.real().Vector::operator=(0.0);
   b.imag().Vector::operator=(0.0);
   b.Assemble();

   // 7. Set up the bilinear form (Real and Imaginary part)
   ConstantCoefficient one(1.0);
   ConstantCoefficient sigma(-pow(omega, 2));

   ParSesquilinearForm * a = new ParSesquilinearForm(fespace,
                                                     ComplexOperator::HERMITIAN);
   ConstantCoefficient impedance(omega);


   Array<int> bdr_attr(pmesh->bdr_attributes.Max());
   bdr_attr = 1;
   RestrictedCoefficient imp_rest(impedance,bdr_attr);
   a->AddDomainIntegrator(new DiffusionIntegrator(one),NULL);
   a->AddDomainIntegrator(new MassIntegrator(sigma),NULL);
   a->AddBoundaryIntegrator(NULL,new BoundaryMassIntegrator(imp_rest));
   a->Assemble();
   a->Finalize();

   Array<int> ess_tdof_list;
   Array<int> ess_bdr(pmesh->bdr_attributes.Max());
   ess_bdr = 0;
   fespace->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);

   // Solution grid function
   ParComplexGridFunction p_gf(fespace);
   ParComplexGridFunction p_gf_ex(fespace);

   OperatorHandle Ah;
   Vector X, B;

   a->FormLinearSystem(ess_tdof_list, p_gf, b, Ah, X, B);

   ComplexHypreParMatrix * AZ = Ah.As<ComplexHypreParMatrix>();
   HypreParMatrix * A = AZ->GetSystemMatrix();


   if (myid == 0)
   {
      cout << "Size of fine grid system: "
           << A->GetGlobalNumRows() << " x " << A->GetGlobalNumCols() << endl;
   }

   SuperLURowLocMatrix * Arow = new SuperLURowLocMatrix(*A);
   SuperLUSolver * superlu = new SuperLUSolver(MPI_COMM_WORLD);
   superlu->SetPrintStatistics(false);
   superlu->SetSymmetricPattern(true);
   superlu->SetColumnPermutation(superlu::PARMETIS);
   superlu->SetOperator(*Arow);
   superlu->Mult(B,X);

   a->RecoverFEMSolution(X,B,p_gf);



   ComplexParAddSchwarz * test = new ComplexParAddSchwarz(a);
   delete test;


   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;
      string keys;
      if (dim ==2 )
      {
         keys = "keys mrRljc\n";
      }
      else
      {
         keys = "keys mc\n";
      }
      socketstream sol_sock_re(vishost, visport);
      sol_sock_re << "parallel " << num_procs << " " << myid << "\n";
      sol_sock_re.precision(8);
      sol_sock_re << "solution\n" << *pmesh << p_gf.real() <<
                  "window_title 'Numerical Pressure (real part)' "
                  << keys << flush;
   }

   delete a;
   delete fespace;
   delete fec;
   delete pmesh;
   MPI_Finalize();
   return 0;
}


//calculate RHS from exact solution f = - \Delta u
double f_exact_Re(const Vector &x)
{
   double f_re = 0.0;
   double x0 = length/2.0;
   double x1 = length/2.0;
   double x2 = length/2.0;
   double alpha,beta;
   double n = 5.0 * omega/M_PI;
   double coeff = pow(n,2)/M_PI;
   beta = pow(x0-x(0),2) + pow(x1-x(1),2);
   if (dim == 3) { beta += pow(x2-x(2),2); }
   alpha = -pow(n,2) * beta;
   f_re = coeff*exp(alpha);
   return f_re;
}
double f_exact_Im(const Vector &x)
{
   double p_re, d2p_re, p_im, d2p_im;
   double dp_re[3], dp_im[3];
   double f_im;
   f_im = 0.0;
   return f_im;
}




























// int ndofs = nodes->FESpace()->GetNDofs();
//    Vector xcoords(ndofs), ycoords(ndofs), zcoords(ndofs);

//    for (int comp = 0; comp < nodes->FESpace()->GetVDim(); comp++)
//    {
//       for (int i = 0; i < ndofs; i++)
//       {
//          if (comp == 0)
//          {
//             xcoords(i) = *nodes[nodes->FESpace()->DofToVDof(i, comp)];
//          }
//          else if (comp == 1)
//          {
//             ycoords(i) = *nodes[nodes->FESpace()->DofToVDof(i, comp)];
//          }
//          else if (comp == 2)
//          {
//             zcoords(i) = *nodes[nodes->FESpace()->DofToVDof(i, comp)];
//          }
//       }
//    }
