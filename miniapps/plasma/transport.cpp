//
// ./transport -s 12 -v 1 -vs 5 -tol 1e-3 -tf 4
//
#include "mfem.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

#include "../common/pfem_extras.hpp"
#include "transport_solver.hpp"

using namespace std;
using namespace mfem;
using namespace mfem::miniapps;
using namespace mfem::plasma;

int problem_;

// Equation constant parameters.
/*
int num_species_ = -1;
int num_equations_ = -1;
const double specific_heat_ratio_ = 1.4;
const double gas_constant_ = 1.0;
*/
// Scalar coefficient for diffusion of momentum
static double diffusion_constant_ = 0.1;
static double dg_sigma_ = -1.0;
static double dg_kappa_ = -1.0;

static double T_max_ = 10.0;
static double T_min_ =  1.0;

static double B_max_ = 5.0;
static double v_max_ = 1e3;

// Maximum characteristic speed (updated by integrators)
static double max_char_speed_;

// Background fields and initial conditions
static int prob_ = 4;
static int gamma_ = 10;
static double alpha_ = NAN;
static double chi_max_ratio_ = 1.0;
static double chi_min_ratio_ = 1.0;

void ChiFunc(const Vector &x, DenseMatrix &M)
{
   M.SetSize(2);

   switch (prob_)
   {
      case 1:
      {
         double cx = cos(M_PI * x[0]);
         double cy = cos(M_PI * x[1]);
         double sx = sin(M_PI * x[0]);
         double sy = sin(M_PI * x[1]);

         double den = cx * cx * sy * sy + sx * sx * cy * cy;

         M(0,0) = chi_max_ratio_ * sx * sx * cy * cy + sy * sy * cx * cx;
         M(1,1) = chi_max_ratio_ * sy * sy * cx * cx + sx * sx * cy * cy;

         M(0,1) = (1.0 - chi_max_ratio_) * cx * cy * sx * sy;
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 2:
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double den = pow(b * b * x[0], 2) + pow(a * a * x[1], 2);

         M(0,0) = chi_max_ratio_ * pow(a * a * x[1], 2) + pow(b * b * x[0], 2);
         M(1,1) = chi_max_ratio_ * pow(b * b * x[0], 2) + pow(a * a * x[1], 2);

         M(0,1) = (1.0 - chi_max_ratio_) * pow(a * b, 2) * x[0] * x[1];
         M(1,0) = M(0,1);

         M *= 1.0e-2 / den;
      }
      break;
      case 3:
      {
         double ca = cos(alpha_);
         double sa = sin(alpha_);

         M(0,0) = 1.0 + (chi_max_ratio_ - 1.0) * ca * ca;
         M(1,1) = 1.0 + (chi_max_ratio_ - 1.0) * sa * sa;

         M(0,1) = (chi_max_ratio_ - 1.0) * ca * sa;
         M(1,0) = (chi_max_ratio_ - 1.0) * ca * sa;
      }
      break;
   }
}

double TFunc(const Vector &x, double t)
{
   double a = 0.4;
   double b = 0.8;

   double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);

   return T_min_ + (T_max_ - T_min_) * cos(0.5 * M_PI * sqrt(r));
}

double TeFunc(const Vector &x, double t)
{
   double a = 0.4;
   double b = 0.8;

   double r = pow(x[0] / a, 2) + pow(x[1] / b, 2);
   double rs = pow(x[0] - 0.5 * a, 2) + pow(x[1] - 0.5 * b, 2);
   return T_min_ +
          (T_max_ - T_min_) * (cos(0.5 * M_PI * sqrt(r)) + 0.5 * exp(-400.0 * rs));
}

void bFunc(const Vector &x, Vector &B)
{
   B.SetSize(2);

   double a = 0.4;
   double b = 0.8;

   B[0] =  a * x[1] / (b * b);
   B[1] = -x[0] / a;

   B *= B_max_;
}

void bbTFunc(const Vector &x, DenseMatrix &M)
{
   M.SetSize(2);

   switch (prob_)
   {
      case 1:
      {
         double cx = cos(M_PI * x[0]);
         double cy = cos(M_PI * x[1]);
         double sx = sin(M_PI * x[0]);
         double sy = sin(M_PI * x[1]);

         double den = cx * cx * sy * sy + sx * sx * cy * cy;

         M(0,0) = sx * sx * cy * cy;
         M(1,1) = sy * sy * cx * cx;

         M(0,1) =  -1.0 * cx * cy * sx * sy;
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 2:
      case 4:
      {
         double a = 0.4;
         double b = 0.8;

         double den = pow(b * b * x[0], 2) + pow(a * a * x[1], 2);

         M(0,0) = pow(a * a * x[1], 2);
         M(1,1) = pow(b * b * x[0], 2);

         M(0,1) = -1.0 * pow(a * b, 2) * x[0] * x[1];
         M(1,0) = M(0,1);

         M *= 1.0 / den;
      }
      break;
      case 3:
      {
         double ca = cos(alpha_);
         double sa = sin(alpha_);

         M(0,0) = ca * ca;
         M(1,1) = sa * sa;

         M(0,1) = ca * sa;
         M(1,0) = ca * sa;
      }
      break;
   }
}

void vFunc(const Vector &x, Vector &V)
{
   bFunc(x, V);
   V *= -v_max_ / B_max_;
}

class NormedDifferenceMeasure : public ODEDifferenceMeasure
{
private:
   MPI_Comm comm_;
   const Operator * M_;
   Vector du_;
   Vector Mu_;

public:
   NormedDifferenceMeasure(MPI_Comm comm) : comm_(comm), M_(NULL) {}

   void SetOperator(const Operator & op)
   { M_ = &op; du_.SetSize(M_->Width()); Mu_.SetSize(M_->Height()); }

   double Eval(Vector &u0, Vector &u1)
   {
      M_->Mult(u0, Mu_);
      double nrm0 = InnerProduct(comm_, u0, Mu_);
      add(u1, -1.0, u0, du_);
      M_->Mult(du_, Mu_);
      return sqrt(InnerProduct(comm_, du_, Mu_) / nrm0);
   }
};

// Initial condition
void AdaptInitialMesh(MPI_Session &mpi,
                      ParMesh &pmesh, ParFiniteElementSpace &fespace,
                      ParGridFunction & gf, Coefficient &coef,
                      int p, double tol, bool visualization = false);

void InitialCondition(const Vector &x, Vector &y);

int main(int argc, char *argv[])
{
   // 1. Initialize MPI.
   MPI_Session mpi(argc, argv);

   // 2. Parse command-line options.
   problem_ = 1;
   const char *mesh_file = "ellipse_origin_h0pt0625_o3.mesh";
   int ser_ref_levels = 0;
   int par_ref_levels = 0;
   int nc_limit = 3;         // maximum level of hanging nodes
   double max_elem_error = -1.0;
   double hysteresis = 0.25; // derefinement safety coefficient
   int order = 3;

   DGParams dg;
   dg.sigma = -1.0;
   dg.kappa = -1.0;

   int ode_solver_type = 2;
   bool   imex = true;
   double tol_ode = 1e-3;
   double rej_ode = 1.2;
   double kP_acc = 0.13;
   double kI_acc = 1.0 / 15.0;
   double kD_acc = 0.2;
   double kI_rej = 0.2;
   double lim_max = 2.0;

   double tol_init = 1e-5;
   double t_init = 0.0;
   double t_final = -1.0;
   double dt = -0.01;
   // double dt_rel_tol = 0.1;
   double cfl = 0.3;
   bool visualization = true;
   bool visit = false;
   bool binary = false;
   int vis_steps = 10;

   Array<int> ion_charges;
   Vector ion_masses;

   int precision = 8;
   cout.precision(precision);

   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&problem_, "-p", "--problem",
                  "Problem setup to use. See options in velocity_function().");
   args.AddOption(&ser_ref_levels, "-rs", "--refine-serial",
                  "Number of times to refine the mesh uniformly before parallel"
                  " partitioning, -1 for auto.");
   args.AddOption(&par_ref_levels, "-rp", "--refine-parallel",
                  "Number of times to refine the mesh uniformly after parallel"
                  " partitioning.");
   args.AddOption(&max_elem_error, "-e", "--max-err",
                  "Maximum element error");
   args.AddOption(&hysteresis, "-y", "--hysteresis",
                  "Derefinement safety coefficient.");
   args.AddOption(&nc_limit, "-l", "--nc-limit",
                  "Maximum level of hanging nodes.");
   args.AddOption(&order, "-o", "--order",
                  "Order (degree) of the finite elements.");
   args.AddOption(&dg.sigma, "-dgs", "--dg-sigma",
                  "One of the two DG penalty parameters, typically +1/-1."
                  " See the documentation of class DGDiffusionIntegrator.");
   args.AddOption(&dg.kappa, "-dgk", "--dg-kappa",
                  "One of the two DG penalty parameters, should be positive."
                  " Negative values are replaced with (order+1)^2.");
   args.AddOption(&tol_init, "-tol0", "--initial-tolerance",
                  "Error tolerance for initial condition.");
   args.AddOption(&tol_ode, "-tol", "--ode-tolerance",
                  "Difference tolerance for ODE integration.");
   args.AddOption(&ode_solver_type, "-s", "--ode-solver",
                  "ODE Implicit solver: "
                  "            IMEX methods\n\t"
                  "            1 - IMEX BE/FE, 2 - IMEX RK2,\n\t"
                  "            L-stable methods\n\t"
                  "            11 - Backward Euler,\n\t"
                  "            12 - SDIRK23, 13 - SDIRK33,\n\t"
                  "            A-stable methods (not L-stable)\n\t"
                  "            22 - ImplicitMidPointSolver,\n\t"
                  "            23 - SDIRK23, 34 - SDIRK34.");
   args.AddOption(&t_final, "-tf", "--t-final",
                  "Final time; start time is 0.");
   args.AddOption(&dt, "-dt", "--time-step",
                  "Time step. Positive number skips CFL timestep calculation.");
   // args.AddOption(&dt_rel_tol, "-dttol", "--time-step-tolerance",
   //                "Time step will only be adjusted if the relative difference "
   //                "exceeds dttol.");
   args.AddOption(&cfl, "-c", "--cfl-number",
                  "CFL number for timestep calculation.");
   args.AddOption(&ion_charges, "-qi", "--ion-charges",
                  "Charges of the various species "
                  "(in units of electron charge)");
   args.AddOption(&ion_masses, "-mi", "--ion-masses",
                  "Masses of the various species (in amu)");
   args.AddOption(&diffusion_constant_, "-nu", "--diffusion-constant",
                  "Diffusion constant used in momentum equation.");
   args.AddOption(&dg_sigma_, "-dgs", "--sigma",
                  "One of the two DG penalty parameters, typically +1/-1."
                  " See the documentation of class DGDiffusionIntegrator.");
   args.AddOption(&dg_kappa_, "-dgk", "--kappa",
                  "One of the two DG penalty parameters, should be positive."
                  " Negative values are replaced with (order+1)^2.");
   args.AddOption(&B_max_, "-B", "--B-magnitude",
                  "");
   args.AddOption(&v_max_, "-v", "--velocity",
                  "");
   args.AddOption(&chi_max_ratio_, "-chi-max", "--chi-max-ratio",
                  "Ratio of chi_max_parallel/chi_perp.");
   args.AddOption(&chi_min_ratio_, "-chi-min", "--chi-min-ratio",
                  "Ratio of chi_min_parallel/chi_perp.");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.AddOption(&visit, "-visit", "--visit-datafiles", "-no-visit",
                  "--no-visit-datafiles",
                  "Save data files for VisIt (visit.llnl.gov) visualization.");
   args.AddOption(&binary, "-binary", "--binary-datafiles", "-ascii",
                  "--ascii-datafiles",
                  "Use binary (Sidre) or ascii format for VisIt data files.");
   args.AddOption(&vis_steps, "-vs", "--visualization-steps",
                  "Visualize every n-th timestep.");

   args.Parse();
   if (!args.Good())
   {
      if (mpi.Root()) { args.PrintUsage(cout); }
      return 1;
   }
   if (dg.kappa < 0.0)
   {
      dg.kappa = (double)(order+1)*(order+1);
   }
   /*
   if (ode_exp_solver_type < 0)
   {
      ode_exp_solver_type = ode_split_solver_type;
   }
   if (ode_imp_solver_type < 0)
   {
      ode_imp_solver_type = ode_split_solver_type;
   }
   */
   imex = ode_solver_type < 10;

   if (ion_charges.Size() == 0)
   {
      ion_charges.SetSize(1);
      ion_charges[0] =  1.0;
   }
   if (ion_masses.Size() == 0)
   {
      ion_masses.SetSize(1);
      ion_masses[0] = 2.01410178;
   }
   if (dg_kappa_ < 0)
   {
      dg_kappa_ = (order+1)*(order+1);
   }
   if (t_final < 0.0)
   {
      if (strcmp(mesh_file, "../data/periodic-hexagon.mesh") == 0)
      {
         t_final = 3.0;
      }
      else if (strcmp(mesh_file, "../data/periodic-square.mesh") == 0)
      {
         t_final = 2.0;
      }
      else
      {
         t_final = 1.0;
      }
   }
   if (mpi.Root()) { args.PrintOptions(cout); }

   // 3. Read the mesh from the given mesh file. This example requires a 2D
   //    periodic mesh, such as ../data/periodic-square.mesh.
   Mesh *mesh = new Mesh(mesh_file, 1, 1);
   const int dim = mesh->Dimension();
   const int sdim = mesh->SpaceDimension();

   MFEM_ASSERT(dim == 2, "Need a two-dimensional mesh for the problem definition");

   // 4. Refine the serial mesh on all processors to increase the resolution.
   //    Also project a NURBS mesh to a piecewise-quadratic curved mesh. Make
   //    sure that the mesh is non-conforming.
   if (mesh->NURBSext)
   {
      mesh->UniformRefinement();
      mesh->SetCurvature(2);
   }
   mesh->EnsureNCMesh();

   // num_species_   = ion_charges.Size();
   // num_equations_ = (num_species_ + 1) * (dim + 2);

   // 5. Refine the mesh in serial to increase the resolution. In this example
   //    we do 'ser_ref_levels' of uniform refinement, where 'ser_ref_levels' is
   //    a command-line parameter.
   for (int lev = 0; lev < ser_ref_levels; lev++)
   {
      mesh->UniformRefinement();
   }

   // 6. Define a parallel mesh by a partitioning of the serial mesh. Refine
   //    this mesh further in parallel to increase the resolution. Once the
   //    parallel mesh is defined, the serial mesh can be deleted.
   ParMesh pmesh(MPI_COMM_WORLD, *mesh);
   delete mesh;
   for (int lev = 0; lev < par_ref_levels; lev++)
   {
      pmesh.UniformRefinement();
   }

   // 7. Define the discontinuous DG finite element space of the given
   //    polynomial order on the refined mesh.
   DG_FECollection fec(order, dim);
   // Finite element space for a scalar (thermodynamic quantity)
   ParFiniteElementSpace fespace(&pmesh, &fec);

   // Adaptively refine mesh to accurately represent a given coefficient
   {
      ParGridFunction coef_gf(&fespace);

      FunctionCoefficient coef(TeFunc);

      AdaptInitialMesh(mpi, pmesh, fespace, coef_gf, coef,
                       2, tol_init, visualization);
   }
   /*
   // Finite element space for a mesh-dim vector quantity (momentum)
   ParFiniteElementSpace vfes(&pmesh, &fec, dim, Ordering::byNODES);
   // Finite element space for all variables together (full thermodynamic state)
   ParFiniteElementSpace ffes(&pmesh, &fec, num_equations_, Ordering::byNODES);

   RT_FECollection fec_rt(order, dim);
   // Finite element space for the magnetic field
   ParFiniteElementSpace fes_rt(&pmesh, &fec_rt);

   // This example depends on this ordering of the space.
   MFEM_ASSERT(ffes.GetOrdering() == Ordering::byNODES, "");

   HYPRE_Int glob_size_sca = fespace.GlobalTrueVSize();
   HYPRE_Int glob_size_tot = ffes.GlobalTrueVSize();
   HYPRE_Int glob_size_rt  = fes_rt.GlobalTrueVSize();
   if (mpi.Root())
   { cout << "Number of unknowns per field: " << glob_size_sca << endl; }
   if (mpi.Root())
   { cout << "Total number of unknowns:     " << glob_size_tot << endl; }
   if (mpi.Root())
   { cout << "Number of magnetic field unknowns: " << glob_size_rt << endl; }

   //ConstantCoefficient nuCoef(diffusion_constant_);
   // MatrixFunctionCoefficient nuCoef(dim, ChiFunc);

   // 8. Define the initial conditions, save the corresponding mesh and grid
   //    functions to a file. This can be opened with GLVis with the -gc option.

   // The solution u has components {particle density, x-velocity,
   // y-velocity, temperature} for each species (species loop is the outermost).
   // These are stored contiguously in the BlockVector u_block.
   Array<int> offsets(num_equations_ + 1);
   for (int k = 0; k <= num_equations_; k++)
   {
      offsets[k] = k * fespace.GetNDofs();
   }
   BlockVector u_block(offsets);

   Array<int> n_offsets(num_species_ + 2);
   for (int k = 0; k <= num_species_ + 1; k++)
   {
      n_offsets[k] = offsets[k];
   }
   BlockVector n_block(u_block, n_offsets);
   */
   // Momentum and Energy grid functions on for visualization.
   /*
   ParGridFunction density(&fes, u_block.GetData());
   ParGridFunction velocity(&dfes, u_block.GetData() + offsets[1]);
   ParGridFunction temperature(&fes, u_block.GetData() + offsets[dim+1]);
   */
   /*
   // Initialize the state.
   VectorFunctionCoefficient u0(num_equations_, InitialCondition);
   ParGridFunction sol(&ffes, u_block.GetData());
   sol.ProjectCoefficient(u0);

   VectorFunctionCoefficient BCoef(dim, bFunc);
   ParGridFunction B(&fes_rt);
   B.ProjectCoefficient(BCoef);
   */
   // Output the initial solution.
   /*
   {
      ostringstream mesh_name;
      mesh_name << "transport-mesh." << setfill('0') << setw(6)
      << mpi.WorldRank();
      ofstream mesh_ofs(mesh_name.str().c_str());
      mesh_ofs.precision(precision);
      mesh_ofs << pmesh;

      for (int i = 0; i < num_species_; i++)
   for (int j = 0; j < dim + 2; j++)
   {
      int k = 0;
      ParGridFunction uk(&sfes, u_block.GetBlock(k));
      ostringstream sol_name;
      sol_name << "species-" << i << "-field-" << j << "-init."
          << setfill('0') << setw(6) << mpi.WorldRank();
      ofstream sol_ofs(sol_name.str().c_str());
      sol_ofs.precision(precision);
      sol_ofs << uk;
   }
   }
   */

   // 9. Set up the nonlinear form corresponding to the DG discretization of the
   //    flux divergence, and assemble the corresponding mass matrix.
   /*
   MixedBilinearForm Aflux(&dfes, &fes);
   Aflux.AddDomainIntegrator(new DomainIntegrator(dim, num_equations_));
   Aflux.Assemble();

   ParNonlinearForm A(&vfes);
   RiemannSolver rsolver(num_equations_, specific_heat_ratio_);
   A.AddInteriorFaceIntegrator(new FaceIntegrator(rsolver, dim,
                    num_equations_));

   // 10. Define the time-dependent evolution operator describing the ODE
   //     right-hand side, and perform time-integration (looping over the time
   //     iterations, ti, with a time-step dt).
   AdvectionTDO adv(vfes, A, Aflux.SpMat(), num_equations_,
                    specific_heat_ratio_);
   DiffusionTDO diff(fes, dfes, vfes, nuCoef, dg_sigma_, dg_kappa_);
   */
   // TransportSolver transp(ode_imp_solver, ode_exp_solver, sfes, vfes, ffes,
   //                     n_block, B, ion_charges, ion_masses);
   /*
   // Visualize the density, momentum, and energy
   vector<socketstream> dout(num_species_+1), vout(num_species_+1),
          tout(num_species_+1), xout(num_species_+1), eout(num_species_+1);

   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;

      int Wx = 0, Wy = 0; // window position
      int Ww = 275, Wh = 250; // window size
      int offx = Ww + 3, offy = Wh + 25; // window offsets

      MPI_Barrier(pmesh.GetComm());

      for (int i=0; i<=num_species_; i++)
      {
         int doff = offsets[i];
         int voff = offsets[i * dim + num_species_ + 1];
         int toff = offsets[i + (num_species_ + 1) * (dim + 1)];
         double * u_data = u_block.GetData();
         ParGridFunction density(&fespace, u_data + doff);
         ParGridFunction velocity(&vfes, u_data + voff);
         ParGridFunction temperature(&fespace, u_data + toff);
   */
   /*
        ParGridFunction chi_para(&sfes);
        ParGridFunction eta_para(&sfes);
        if (i==0)
        {
           ChiParaCoefficient chiParaCoef(n_block, ion_charges);
           chiParaCoef.SetT(temperature);
           chi_para.ProjectCoefficient(chiParaCoef);

           EtaParaCoefficient etaParaCoef(n_block, ion_charges);
           etaParaCoef.SetT(temperature);
           eta_para.ProjectCoefficient(etaParaCoef);
        }
        else
        {
           ChiParaCoefficient chiParaCoef(n_block, i - 1,
                                          ion_charges,
                                          ion_masses);
           chiParaCoef.SetT(temperature);
           chi_para.ProjectCoefficient(chiParaCoef);

           EtaParaCoefficient etaParaCoef(n_block, i - 1,
                                          ion_charges,
                                          ion_masses);
           etaParaCoef.SetT(temperature);
           eta_para.ProjectCoefficient(etaParaCoef);
        }
   */
   /*
         ostringstream head;
         if (i==0)
         {
            head << "Electron";
         }
         else
         {
            head << "Species " << i;
         }

         ostringstream doss;
         doss << head.str() << " Density";
         VisualizeField(dout[i], vishost, visport,
                        density, doss.str().c_str(),
                        Wx, Wy, Ww, Wh);
         Wx += offx;

         ostringstream voss; voss << head.str() << " Velocity";
         VisualizeField(vout[i], vishost, visport,
                        velocity, voss.str().c_str(),
                        Wx, Wy, Ww, Wh, NULL, true);
         Wx += offx;

         ostringstream toss; toss << head.str() << " Temperature";
         VisualizeField(tout[i], vishost, visport,
                        temperature, toss.str().c_str(),
                        Wx, Wy, Ww, Wh);
   */
   /*
   Wx += offx;

   ostringstream xoss; xoss << head.str() << " Chi Parallel";
   VisualizeField(xout[i], vishost, visport,
                  chi_para, xoss.str().c_str(),
                  Wx, Wy, Ww, Wh);

   Wx += offx;

   ostringstream eoss; eoss << head.str() << " Eta Parallel";
   VisualizeField(eout[i], vishost, visport,
                  eta_para, eoss.str().c_str(),
                  Wx, Wy, Ww, Wh);

   Wx -= 4 * offx;
   */
   /*
         Wx -= 2 * offx;
         Wy += offy;
      }
   }
   */

   // Determine the minimum element size.
   double hmin;
   if (cfl > 0)
   {
      double my_hmin = pmesh.GetElementSize(0, 1);
      for (int i = 1; i < pmesh.GetNE(); i++)
      {
         my_hmin = min(pmesh.GetElementSize(i, 1), my_hmin);
      }
      // Reduce to find the global minimum element size
      MPI_Allreduce(&my_hmin, &hmin, 1, MPI_DOUBLE, MPI_MIN, pmesh.GetComm());

      double dt_diff = hmin * hmin / chi_max_ratio_;
      double dt_adv  = hmin / max(v_max_, DBL_MIN);

      if (mpi.Root())
      {
         cout << "Maximum advection time step: " << dt_adv << endl;
         cout << "Maximum diffusion time step: " << dt_diff << endl;
      }

      // dt = cfl * min(dt_diff, dt_adv);
      dt = cfl * dt_adv;
   }

   ODEController ode_controller;
   PIDAdjFactor dt_acc(kP_acc, kI_acc, kD_acc);
   IAdjFactor   dt_rej(kI_rej);
   MaxLimiter   dt_max(lim_max);

   ODESolver * ode_solver = NULL;
   switch (ode_solver_type)
   {
      // Implicit-Explicit methods
      case 1:  ode_solver = new IMEX_BE_FE; break;
      case 2:  ode_solver = new IMEXRK2; break;
      // Implicit L-stable methods
      case 11: ode_solver = new BackwardEulerSolver; break;
      case 12: ode_solver = new SDIRK23Solver(2); break;
      case 13: ode_solver = new SDIRK33Solver; break;
      // Implicit A-stable methods (not L-stable)
      case 22: ode_solver = new ImplicitMidpointSolver; break;
      case 23: ode_solver = new SDIRK23Solver; break;
      case 34: ode_solver = new SDIRK34Solver; break;
      default:
         if (mpi.Root())
         {
            cout << "Unknown Implicit ODE solver type: "
                 << ode_solver_type << '\n';
         }
         return 3;
   }

   ParBilinearForm m(&fespace);
   m.AddDomainIntegrator(new MassIntegrator);
   m.Assemble();
   m.Finalize();

   NormedDifferenceMeasure ode_diff_msr(MPI_COMM_WORLD);
   ode_diff_msr.SetOperator(m);

   ConstantCoefficient one(1.0);
   FunctionCoefficient u0Coef(TeFunc);
   MatrixFunctionCoefficient DCoef(dim, ChiFunc);
   VectorFunctionCoefficient VCoef(dim, vFunc);

   DGAdvectionDiffusionTDO oper(dg, fespace, one, imex);

   oper.SetDiffusionCoefficient(DCoef);
   oper.SetAdvectionCoefficient(VCoef);

   Array<int> dbcAttr(pmesh.bdr_attributes.Max());
   dbcAttr = 1;
   oper.SetDirichletBC(dbcAttr, u0Coef);

   oper.SetTime(0.0);
   ode_solver->Init(oper);

   ode_controller.Init(*ode_solver, ode_diff_msr,
                       dt_acc, dt_rej, dt_max);

   ode_controller.SetOutputFrequency(vis_steps);
   ode_controller.SetTimeStep(dt);
   ode_controller.SetTolerance(tol_ode);
   ode_controller.SetRejectionLimit(rej_ode);

   ParGridFunction u(&fespace);
   u.ProjectCoefficient(u0Coef);

   L2_FECollection fec_l2_o0(0, dim);
   // Finite element space for a scalar (thermodynamic quantity)
   ParFiniteElementSpace fespace_l2_o0(&pmesh, &fec_l2_o0);

   ParGridFunction err(&fespace_l2_o0, (double *)NULL);

   // 11. Set up an error estimator. Here we use the Zienkiewicz-Zhu estimator
   //     with L2 projection in the smoothing step to better handle hanging
   //     nodes and parallel partitioning. We need to supply a space for the
   //     discontinuous flux (L2) and a space for the smoothed flux (H(div) is
   //     used here).
   L2_FECollection flux_fec(order, dim);
   ParFiniteElementSpace flux_fes(&pmesh, &flux_fec, sdim);
   RT_FECollection smooth_flux_fec(order-1, dim);
   ParFiniteElementSpace smooth_flux_fes(&pmesh, &smooth_flux_fec);
   // Another possible option for the smoothed flux space:
   // H1_FECollection smooth_flux_fec(order, dim);
   // ParFiniteElementSpace smooth_flux_fes(&pmesh, &smooth_flux_fec, dim);
   DiffusionIntegrator integ(DCoef);
   L2ZienkiewiczZhuEstimator estimator(integ, u, flux_fes, smooth_flux_fes);

   if (max_elem_error < 0.0)
   {
      const Vector init_errors = estimator.GetLocalErrors();

      double loc_max_error = init_errors.Max();
      double loc_min_error = init_errors.Min();

      double glb_max_error = -1.0;
      double glb_min_error = -1.0;

      MPI_Allreduce(&loc_max_error, &glb_max_error, 1,
                    MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
      MPI_Allreduce(&loc_min_error, &glb_min_error, 1,
                    MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);

      if (mpi.Root())
      {
         cout << "Range of error estimates for initial condition: "
              << glb_min_error << " < elem err < " << glb_max_error << endl;
      }

      max_elem_error = glb_max_error;

   }

   // 12. A refiner selects and refines elements based on a refinement strategy.
   //     The strategy here is to refine elements with errors larger than a
   //     fraction of the maximum element error. Other strategies are possible.
   //     The refiner will call the given error estimator.
   ThresholdRefiner refiner(estimator);
   // refiner.SetTotalErrorFraction(0.7);
   refiner.SetTotalErrorFraction(0.0); // use purely local threshold
   refiner.SetLocalErrorGoal(max_elem_error);
   refiner.PreferConformingRefinement();
   refiner.SetNCLimit(nc_limit);

   // 12. A derefiner selects groups of elements that can be coarsened to form
   //     a larger element. A conservative enough threshold needs to be set to
   //     prevent derefining elements that would immediately be refined again.
   ThresholdDerefiner derefiner(estimator);
   derefiner.SetThreshold(hysteresis * max_elem_error);
   derefiner.SetNCLimit(nc_limit);

   const int max_dofs = 100000;

   // Start the timer.
   tic_toc.Clear();
   tic_toc.Start();

   socketstream sout, eout;
   char vishost[] = "localhost";
   int  visport   = 19916;

   int Wx = 278, Wy = 0; // window position
   int Ww = 275, Wh = 250; // window size

   DataCollection *dc = NULL;
   if (visit)
   {
      if (binary)
      {
#ifdef MFEM_USE_SIDRE
         dc = new SidreDataCollection("Transport-Parallel", &pmesh);
#else
         MFEM_ABORT("Must build with MFEM_USE_SIDRE=YES for binary output.");
#endif
      }
      else
      {
         dc = new VisItDataCollection("Transport-Parallel", &pmesh);
         dc->SetPrecision(precision);
         // To save the mesh using MFEM's parallel mesh format:
         // dc->SetFormat(DataCollection::PARALLEL_FORMAT);
      }
      dc->RegisterField("solution", &u);
      dc->SetCycle(0);
      dc->SetTime(t_init);
      dc->Save();
   }

   int cycle = 0;
   int amr_it = 0;
   int ref_it = 0;
   int dref_it = 0;

   double t = t_init;

   if (mpi.Root()) { cout << "\nBegin time stepping at t = " << t << endl; }
   while (t < t_final)
   {
      ode_controller.Run(u, t, t_final);

      if (mpi.Root()) { cout << "Time stepping paused at t = " << t << endl; }

      if (visualization)
      {
         ostringstream oss;
         oss << "Field at time " << t;
         VisualizeField(sout, vishost, visport, u, oss.str().c_str(),
                        Wx, Wy, Ww, Wh);
      }

      if (visit)
      {
         cycle++;
         dc->SetCycle(cycle);
         dc->SetTime(t);
         dc->Save();
      }

      if (t_final - t > 1e-8 * (t_final - t_init))
      {
         HYPRE_Int global_dofs = fespace.GlobalTrueVSize();

         if (global_dofs > max_dofs)
         {
            continue;
         }

         // Make sure errors will be recomputed in the following.
         if (mpi.Root())
         {
            cout << "\nEstimating errors." << endl;
         }
         refiner.Reset();
         derefiner.Reset();

         // 20. Call the refiner to modify the mesh. The refiner calls the error
         //     estimator to obtain element errors, then it selects elements to be
         //     refined and finally it modifies the mesh. The Stop() method can be
         //     used to determine if a stopping criterion was met.

         if (visualization)
         {
            err.MakeRef(&fespace_l2_o0,
                        const_cast<double*>(&(estimator.GetLocalErrors())[0]));
            ostringstream oss;
            oss << "Error estimate at time " << t;
            VisualizeField(eout, vishost, visport, err, oss.str().c_str(),
                           2*Wx, Wy, Ww, Wh);
         }

         refiner.Apply(pmesh);

         if (refiner.Stop())
         {
            if (mpi.Root())
            {
               cout << "No refinements necessary." << endl;
            }
            // continue;
         }
         else
         {
            ref_it++;
            if (mpi.Root())
            {
               cout << "Refining elements (iteration " << ref_it << ")" << endl;
            }

            // 21. Update the finite element space (recalculate the number of DOFs,
            //     etc.) and create a grid function update matrix. Apply the matrix
            //     to any GridFunctions over the space. In this case, the update
            //     matrix is an interpolation matrix so the updated GridFunction will
            //     still represent the same function as before refinement.
            fespace.Update();
            fespace_l2_o0.Update();
            u.Update();

            // 22. Load balance the mesh, and update the space and solution. Currently
            //     available only for nonconforming meshes.
            if (pmesh.Nonconforming())
            {
               pmesh.Rebalance();

               // Update the space and the GridFunction. This time the update matrix
               // redistributes the GridFunction among the processors.
               fespace.Update();
               fespace_l2_o0.Update();
               u.Update();
            }
            m.Update(); m.Assemble(); m.Finalize();
            ode_diff_msr.SetOperator(m);
            oper.Update();
            ode_solver->Init(oper);
         }
         if (derefiner.Apply(pmesh))
         {
            dref_it++;
            if (mpi.Root())
            {
               cout << "Derefining elements (iteration " << dref_it << ")" << endl;
            }

            // 24. Update the space and the solution, rebalance the mesh.
            cout << "fespace.Update();" << endl;
            fespace.Update();
            cout << "fespace_l2_o0.Update();" << endl;
            fespace_l2_o0.Update();
            cout << "u.Update();" << endl;
            u.Update();
            cout << "m.Update();" << endl;
            m.Update(); m.Assemble(); m.Finalize();
            ode_diff_msr.SetOperator(m);
            cout << "oper.Update();" << endl;
            oper.Update();
            ode_solver->Init(oper);
         }
         else
         {
            if (mpi.Root())
            {
               cout << "No derefinements needed." << endl;
            }
         }

         amr_it++;

         global_dofs = fespace.GlobalTrueVSize();
         if (mpi.Root())
         {
            cout << "\nAMR iteration " << amr_it << endl;
            cout << "Number of unknowns: " << global_dofs << endl;
         }

      }
   }

   tic_toc.Stop();
   if (mpi.Root())
   {
      cout << "\nTime stepping done after " << tic_toc.RealTime() << "s.\n";
   }
   /*
   // 11. Save the final solution. This output can be viewed later using GLVis:
   //     "glvis -np 4 -m transport-mesh -g species-0-field-0-final".
   {
      int k = 0;
      for (int i = 0; i < num_species_; i++)
         for (int j = 0; j < dim + 2; j++)
         {
            ParGridFunction uk(&fespace, u_block.GetBlock(k));
            ostringstream sol_name;
            sol_name << "species-" << i << "-field-" << j << "-final."
                     << setfill('0') << setw(6) << mpi.WorldRank();
            ofstream sol_ofs(sol_name.str().c_str());
            sol_ofs.precision(precision);
            sol_ofs << uk;
            k++;
         }
   }

   // 12. Compute the L2 solution error summed for all components.
   if ((t_final == 2.0 &&
        strcmp(mesh_file, "../data/periodic-square.mesh") == 0) ||
       (t_final == 3.0 &&
        strcmp(mesh_file, "../data/periodic-hexagon.mesh") == 0))
   {
      const double error = sol.ComputeLpError(2, u0);
      if (mpi.Root()) { cout << "Solution error: " << error << endl; }
   }
   */
   // Free the used memory.
   delete ode_solver;
   // delete ode_imp_solver;
   delete dc;

   return 0;
}

// Initial condition
void AdaptInitialMesh(MPI_Session &mpi,
                      ParMesh &pmesh, ParFiniteElementSpace &fespace,
                      ParGridFunction & gf, Coefficient &coef,
                      int p, double tol, bool visualization)
{
   LpErrorEstimator estimator(p, coef, gf);

   ThresholdRefiner refiner(estimator);
   refiner.SetTotalErrorFraction(0);
   refiner.SetTotalErrorNormP(p);
   refiner.SetLocalErrorGoal(tol);

   socketstream sout;
   char vishost[] = "localhost";
   int  visport   = 19916;

   int Wx = 0, Wy = 0; // window position
   int Ww = 275, Wh = 250; // window size

   const int max_dofs = 100000;
   for (int it = 0; ; it++)
   {
      HYPRE_Int global_dofs = fespace.GlobalTrueVSize();
      if (mpi.Root())
      {
         cout << "\nAMR iteration " << it << endl;
         cout << "Number of unknowns: " << global_dofs << endl;
      }

      // 19. Send the solution by socket to a GLVis server.
      gf.ProjectCoefficient(coef);

      if (visualization)
      {
         VisualizeField(sout, vishost, visport, gf, "Initial Condition",
                        Wx, Wy, Ww, Wh);
      }

      if (global_dofs > max_dofs)
      {
         if (mpi.Root())
         {
            cout << "Reached the maximum number of dofs. Stop." << endl;
         }
         break;
      }

      // 20. Call the refiner to modify the mesh. The refiner calls the error
      //     estimator to obtain element errors, then it selects elements to be
      //     refined and finally it modifies the mesh. The Stop() method can be
      //     used to determine if a stopping criterion was met.
      refiner.Apply(pmesh);
      if (refiner.Stop())
      {
         if (mpi.Root())
         {
            cout << "Stopping criterion satisfied. Stop." << endl;
         }
         break;
      }

      // 21. Update the finite element space (recalculate the number of DOFs,
      //     etc.) and create a grid function update matrix. Apply the matrix
      //     to any GridFunctions over the space. In this case, the update
      //     matrix is an interpolation matrix so the updated GridFunction will
      //     still represent the same function as before refinement.
      fespace.Update();
      gf.Update();

      // 22. Load balance the mesh, and update the space and solution. Currently
      //     available only for nonconforming meshes.
      if (pmesh.Nonconforming())
      {
         pmesh.Rebalance();

         // Update the space and the GridFunction. This time the update matrix
         // redistributes the GridFunction among the processors.
         fespace.Update();
         gf.Update();
      }
   }
}

void InitialCondition(const Vector &x, Vector &y)
{
   MFEM_ASSERT(x.Size() == 2, "");
   /*
   double radius = 0, Minf = 0, beta = 0;
   if (problem_ == 1)
   {
      // "Fast vortex"
      radius = 0.2;
      Minf = 0.5;
      beta = 1. / 5.;
   }
   else if (problem_ == 2)
   {
      // "Slow vortex"
      radius = 0.2;
      Minf = 0.05;
      beta = 1. / 50.;
   }
   else
   {
      mfem_error("Cannot recognize problem."
                 "Options are: 1 - fast vortex, 2 - slow vortex");
   }

   const double xc = 0.0, yc = 0.0;

   // Nice units
   const double vel_inf = 1.;
   const double den_inf = 1.;

   // Derive remainder of background state from this and Minf
   const double pres_inf = (den_inf / specific_heat_ratio_) * (vel_inf / Minf) *
                           (vel_inf / Minf);
   const double temp_inf = pres_inf / (den_inf * gas_constant_);

   double r2rad = 0.0;
   r2rad += (x(0) - xc) * (x(0) - xc);
   r2rad += (x(1) - yc) * (x(1) - yc);
   r2rad /= (radius * radius);

   const double shrinv1 = 1.0 / (specific_heat_ratio_ - 1.);

   const double velX = vel_inf * (1 - beta * (x(1) - yc) / radius * exp(
                                     -0.5 * r2rad));
   const double velY = vel_inf * beta * (x(0) - xc) / radius * exp(-0.5 * r2rad);
   const double vel2 = velX * velX + velY * velY;

   const double specific_heat = gas_constant_ * specific_heat_ratio_ * shrinv1;
   const double temp = temp_inf - 0.5 * (vel_inf * beta) *
                       (vel_inf * beta) / specific_heat * exp(-r2rad);

   const double den = den_inf * pow(temp/temp_inf, shrinv1);
   const double pres = den * gas_constant_ * temp;
   const double energy = shrinv1 * pres / den + 0.5 * vel2;

   y(0) = den;
   y(1) = den * velX;
   y(2) = den * velY;
   y(3) = den * energy;
   */
   // double VMag = 1e2;
   /*
   if (y.Size() != num_equations_) { cout << "y is wrong size!" << endl; }

   int dim = 2;
   double a = 0.4;
   double b = 0.8;

   Vector V(2);
   bFunc(x, V);
   V *= (v_max_ / B_max_) * sqrt(pow(x[0]/a,2)+pow(x[1]/b,2));

   double den = 1.0e18;
   for (int i=1; i<=num_species_; i++)
   {
      y(i) = den;
      y(i * dim + num_species_ + 1) = V(0);
      y(i * dim + num_species_ + 2) = V(1);
      y(i + (num_species_ + 1) * (dim + 1)) = 10.0 * TFunc(x, 0.0);
   }

   // Impose neutrality
   y(0) = 0.0;
   for (int i=1; i<=num_species_; i++)
   {
      y(0) += y(i);
   }
   y(num_species_ + 1) = V(0);
   y(num_species_ + 2) = V(1);
   y((num_species_ + 1) * (dim + 1)) = 5.0 * TFunc(x, 0.0);

   // y.Print(cout, dim+2); cout << endl;
   */
}
