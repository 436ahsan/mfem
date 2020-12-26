//            MFEM Example for cutting an H1 space along select faces.
//
// Compile with: make cutH1
//
// Sample runs:
//

#include "mfem.hpp"

using namespace std;
using namespace mfem;

double surface_level_set(const Vector &x)
{
   const double sine = 0.25 * std::sin(4 * M_PI * x(0));
   return (x(1) >= sine + 0.5) ? 0.0 : 1.0;
}

// Used for debugging the elem-to-dof tables when the elements' attributes
// are associated with materials. Options for lvl:
// 0 - only duplicated materials per DOF.
// 1 - all materials per DOF .
// 2 - full element/material output per DOF.
void PrintDofElemTable(const Table &elem_dof, const ParMesh &pmesh, int lvl = 0)
{
   Table dof_elem;
   Transpose(elem_dof, dof_elem);

   const int nrows = dof_elem.Size();
   std::cout << "Total DOFs: " << nrows << std::endl;
   Array<int> dof_elements;
   for (int dof = 0; dof < nrows; dof++)
   {
      // Find the materials that share the current dof.
      std::set<int> dof_materials;
      dof_elem.GetRow(dof, dof_elements);
      if (lvl == 2) { std::cout << "Elements for DOF " << dof << ": \n"; }
      for (int e = 0; e < dof_elements.Size(); e++)
      {
         const int mat_id = pmesh.GetAttribute(dof_elements[e]);

         if (lvl == 2) { cout << dof_elements[e] << "(" << mat_id << ") "; }

         dof_materials.insert(mat_id);
      }
      if (lvl == 2) { std::cout << std::endl; }

      if (lvl == 2) { continue; }
      if (lvl == 0 && dof_materials.size() < 2) { continue; }

      std::cout << "Materials for DOF " << dof << ": " << std::endl;
      for (auto it = dof_materials.cbegin(); it != dof_materials.cend(); it++)
      { std::cout << *it << ' '; }
      std::cout << std::endl;
   }
}

int main(int argc, char *argv[])
{
   // 1. Initialize MPI.
   int num_procs, myid;
   MPI_Init(&argc, &argv);
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
   MPI_Comm_rank(MPI_COMM_WORLD, &myid);

   // 2. Parse command-line options.
   const char *mesh_file = "../../data/inline-quad.mesh";
   int problem = 0;
   int rs_levels = 0;
   int order = 2;
   const char *device_config = "cpu";
   bool visualization = true;

   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&problem, "-p", "--problem",
                  "Problem type:\n\t"
                  "0: exact alignment with the mesh boundary\n\t"
                  "1: zero level set enclosing a volume");
   args.AddOption(&rs_levels, "-rs", "--refine-serial",
                  "Number of times to refine the mesh uniformly in serial.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree) or -1 for"
                  " isoparametric space.");
   args.AddOption(&device_config, "-d", "--device",
                  "Device configuration string, see Device::Configure().");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.Parse();
   if (!args.Good())
   {
      if (myid == 0)
      {
         args.PrintUsage(cout);
      }
      MPI_Finalize();
      return 1;
   }
   if (myid == 0) { args.PrintOptions(cout); }

   // 3. Enable hardware devices such as GPUs, and programming models such as
   //    CUDA, OCCA, RAJA and OpenMP based on command line options.
   Device device(device_config);
   if (myid == 0) { device.Print(); }

   // Refine the mesh.
   Mesh mesh(mesh_file, 1, 1);
   const int dim = mesh.Dimension();
   for (int lev = 0; lev < rs_levels; lev++) { mesh.UniformRefinement(); }

   ParMesh pmesh(MPI_COMM_WORLD, mesh);
   mesh.Clear();

   Coefficient *ls_coeff;
   if (problem == 0)
   {
      ls_coeff = new DeltaCoefficient(0.75, 0.625, 1.0);
   }
   else
   {
      ls_coeff = new FunctionCoefficient(surface_level_set);
   }

   H1_FECollection fec(order, dim);
   ParFiniteElementSpace pfes(&pmesh, &fec);
   ParGridFunction x(&pfes);

   // Assign material indices to the element attributes.
   const int NE = pmesh.GetNE();
   Array<int> dofs;
   Vector x_loc(pfes.GetFE(0)->GetDof());
   for (int i = 0; i < NE; i++)
   {
      Vector center;
      pmesh.GetElementCenter(i, center);
      Element *el = pmesh.GetElement(i);
      if (center(0) <= 0.5 || center(1) >= 0.5)
      {
         el->SetAttribute(0);
      }
      else { el->SetAttribute(1); }

      int mat_id = el->GetAttribute();
      (mat_id == 0) ? x_loc = 0.0 : x_loc = 1.0;
      pfes.GetElementVDofs(i, dofs);
      x.SetSubVector(dofs, x_loc);
   }

   // Compute the multiplicity of each DOF.
   const Table &elem_dof = pfes.GetElementToDofTable();
   Table dof_elem;
   Table new_elem_dof(elem_dof);
   Transpose(elem_dof, dof_elem);
   dof_elem.Finalize();
   const int nrows = dof_elem.Size();
   int ndofs = nrows;
   Array<int> dof_elements;
   for (int dof = 0; dof < nrows; dof++)
   {
      // Check which materials share the current dof.
      std::set<int> dof_materials;
      dof_elem.GetRow(dof, dof_elements);
      for (int e = 0; e < dof_elements.Size(); e++)
      {
         const int mat_id = pmesh.GetAttribute(dof_elements[e]);
         dof_materials.insert(mat_id);
      }
      // Count the materials for the current DOF.
      const int dof_mat_cnt = dof_materials.size();

      // Duplicate the dof if it is shared between materials.
      if (dof_mat_cnt > 1)
      {
         // The material with the lowest index keeps the old DOF id.
         // All other materials duplicate the dof.
         auto mat = dof_materials.cbegin();
         mat++;
         while(mat != dof_materials.cend())
         {
            // Replace in all elements with material mat.
            for (int e = 0; e < dof_elements.Size(); e++)
            {
               if (pmesh.GetAttribute(dof_elements[e]) == *mat)
               {
                  const int new_dof_id = ndofs;
                  std::cout << "Replacing DOF "
                            << dof << " -> " << new_dof_id
                            << " in EL " << dof_elements[e] << std::endl;
                  new_elem_dof.ReplaceConnection(dof_elements[e], dof, new_dof_id);
               }
            }
            ndofs++;
            mat++;
         }
      }

      // Used only for visualization.
      x(dof) = dof_mat_cnt;
   }

   // Send the solution by socket to a GLVis server.
   if (visualization)
   {
      int size = 500;
      char vishost[] = "localhost";
      int  visport   = 19916;

      socketstream sol_sock_x(vishost, visport);
      sol_sock_x << "parallel " << num_procs << " " << myid << "\n";
      sol_sock_x.precision(8);
      sol_sock_x << "solution\n" << pmesh << x;
      sol_sock_x << "window_geometry " << 0 << " " << 0 << " "
                                       << size << " " << size << "\n"
                 << "window_title '" << "X" << "'\n"
                 << "keys mRjlc\n" << flush;
   }

   PrintDofElemTable(elem_dof, pmesh, 0);
   PrintDofElemTable(new_elem_dof, pmesh, 0);

   pfes.ReplaceElemDofTable(new_elem_dof, ndofs);

   // Set face_attribute = 77 to faces that are on the material interface.
   for (int f = 0; f < pmesh.GetNumFaces(); f++)
   {
      auto *ftr = pmesh.GetFaceElementTransformations(f, 3);
      if (ftr->Elem2No > 0 &&
          pmesh.GetAttribute(ftr->Elem1No) != pmesh.GetAttribute(ftr->Elem2No))
      {
         std::cout << ftr->Elem1No << " " << ftr->Elem2No << std::endl;
         std::cout << pmesh.GetAttribute(ftr->Elem1No) << " "
                   << pmesh.GetAttribute(ftr->Elem2No) << std::endl;
         std::cout << "Setting face " << f << std::endl;
         pmesh.SetFaceAttribute(f, 77);
      }
   }

   // Simple Dirichlet BC.
   Array<int> ess_tdof_list;
   if (pmesh.bdr_attributes.Size())
   {
      Array<int> ess_bdr(pmesh.bdr_attributes.Max());
      ess_bdr = 1;
      pfes.FiniteElementSpace::GetEssentialTrueDofs(ess_bdr, ess_tdof_list);
   }

   // RHS.
   ParLinearForm b(&pfes);
   ConstantCoefficient one(1.0);
   b.AddDomainIntegrator(new DomainLFIntegrator(one));
   b.Assemble();

   // LHS.
   BilinearForm a(&pfes);
   a.AddDomainIntegrator(new DiffusionIntegrator(one));
   Array<int> cut_face_attributes(1);
   cut_face_attributes[0] = 77;
   const double sigma = -1.0, kappa = -1.0;
   a.AddInteriorFaceIntegrator(new DGDiffusionIntegrator(one, sigma, kappa),
                               nullptr);
   a.Assemble();

   // Form the system.
   ParGridFunction u(&pfes);
   u = 0.0;
   OperatorPtr A;
   Vector B, X;
   a.FormLinearSystem(ess_tdof_list, u, b, A, X, B);

   // Solve.
   GSSmoother M((SparseMatrix&)(*A));
   //PCG(*A, M, B, X, 1, 200, 1e-12, 0.0);
   CG(*A, B, X, 1, 200, 1e-12, 0.0);
   a.RecoverFEMSolution(X, b, u);

   if (visualization)
   {
      int size = 500;
      char vishost[] = "localhost";
      int  visport   = 19916;

      socketstream sol_sock(vishost, visport);
      sol_sock << "parallel " << num_procs << " " << myid << "\n";
      sol_sock.precision(8);
      sol_sock << "solution\n" << pmesh << u;
      sol_sock << "window_geometry " << size << " " << 0 << " "
                                     << size << " " << size << "\n"
               << "window_title '" << "Y" << "'\n"
               << "keys mRjlc\n" << flush;
   }

   MPI_Finalize();
   return 0;
}
