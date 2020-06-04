//Additive Source Transfer Preconditioner

#include "AdditiveST.hpp"


AdditiveST::AdditiveST(SesquilinearForm * bf_, Array2D<double> & Pmllength_, 
         double omega_, Coefficient * ws_,  int nrlayers_)
   : Solver(2*bf_->FESpace()->GetTrueVSize(), 2*bf_->FESpace()->GetTrueVSize()), 
     bf(bf_), Pmllength(Pmllength_), omega(omega_), ws(ws_), nrlayers(nrlayers_)
{
   Mesh * mesh = bf->FESpace()->GetMesh();
   dim = mesh->Dimension();

   int partition_kind;

   // 1. Ovelapping partition with overlap = 2h 
   partition_kind = 2; // Non Overlapping partition 
   int nx=2;
   int ny=2; 
   int nz=1;
   ovlpnrlayers = nrlayers+2;
   povlp = new MeshPartition(mesh, partition_kind,nx,ny,nz, ovlpnrlayers);

   partition_kind = 1;
   novlp = new MeshPartition(mesh, partition_kind,nx,ny,nz);

   nxyz[0] = povlp->nxyz[0];
   nxyz[1] = povlp->nxyz[1];
   nxyz[2] = povlp->nxyz[2];
   nrpatch = povlp->nrpatch;
   subdomains = povlp->subdomains;


   ovlp_prob  = new DofMap(bf,povlp); 
   nvlp_prob  = new DofMap(bf,novlp); 
   PmlMat.SetSize(nrpatch);
   PmlMatInv.SetSize(nrpatch);
   for (int ip=0; ip<nrpatch; ip++)
   {
      PmlMat[ip] = GetPmlSystemMatrix(ip);
      PmlMatInv[ip] = new KLUSolver;
      PmlMatInv[ip]->SetOperator(*PmlMat[ip]);
   }

   int nsteps = nx + ny - 1;

   f_orig.SetSize(nrpatch);
   usol.SetSize(nrpatch);
   f_s.SetSize(nrpatch);
   f_diag.SetSize(nrpatch);
   // Construct a simple map used for directions of transfer
   for (int ip=0; ip<nrpatch; ip++)
   {
      int n = 2*ovlp_prob->fespaces[ip]->GetTrueVSize(); // (x 2 for complex ) 
      f_orig[ip] = new Vector(n); *f_orig[ip] = 0.0;
      usol[ip] = new Vector(n); *usol[ip] = 0.0;
      f_s[ip].SetSize(nsteps);
      f_diag[ip].SetSize(nsteps);
      for (int i=0;i<nsteps; i++)
      {
         f_s[ip][i] = new Vector(n); 
         f_diag[ip][i] = new Vector(n);
      }
   }
}

void AdditiveST::Mult(const Vector &r, Vector &z) const
{
   char vishost[] = "localhost";
   int  visport   = 19916;
   for (int ip=0; ip<nrpatch; ip++)
   {
      *f_orig[ip] = 0.0;
      *usol[ip] = 0.0;
      for (int i=0;i< f_s[ip].Size(); i++)
      {
         *f_s[ip][i] = 0.0;
         *f_diag[ip][i] = 0.0;
      }
   }
   socketstream res_sock(vishost, visport);
   Vector res(r);
   PlotSolution(res,res_sock,0,false); 
   // for (int ip=0; ip<nrpatch; ip++)
   for (int ip=nrpatch-1; ip>=0; ip--)
   {
      Array<int> * Dof2GlobalDof = &ovlp_prob->Dof2GlobalDof[ip];
      r.GetSubVector(*Dof2GlobalDof,*f_orig[ip]);

      // make sure that f_ij is compactly supported in \Omega_ij (non overlapping)
      int i,j,k;
      Getijk(ip,i,j,k);
      Array<int> directions(2); directions = 0;
      int nx = nxyz[0];
      int ny = nxyz[1];
      if (i+1<nx) directions[0] = 1;
      if (j+1<ny) directions[1] = 1;
      Vector faux(f_orig[ip]->Size());
      GetChiRes(*f_orig[ip],faux,ip,directions,ovlpnrlayers);
      directions = 0.0;
      if (i>0) directions[0] = -1;
      if (j>0) directions[1] = -1;
      *f_orig[ip] = 0.0;
      GetChiRes(faux,*f_orig[ip],ip,directions,ovlpnrlayers);
      // Array<int> * nDof2GlobalDof = &nvlp_prob->Dof2GlobalDof[ip];
      // Vector faux(nDof2GlobalDof->Size());
      // r.GetSubVector(*nDof2GlobalDof,faux);
      // res = 0.0;
      // res.SetSubVector(*nDof2GlobalDof,faux);
      // res.GetSubVector(*Dof2GlobalDof,*f_orig[ip]);
   }

   z = 0.0; 
   Vector znew(z);

   // --------------------------------------------
   //       Sweep in the direction (1,1)
   // --------------------------------------------
   int nx = nxyz[0];
   int ny = nxyz[1];
   int nsteps = (nx + ny - 1);

   for (int s = 0; s<nsteps; s++)
   {
      for (int ip = 0; ip<nrpatch; ip++)
      {
         Array<int> * Dof2GlobalDof = &ovlp_prob->Dof2GlobalDof[ip];
         int ndofs = Dof2GlobalDof->Size();

         Vector sol_local(ndofs); sol_local = 0.0;
         Vector res_local(ndofs); res_local = 0.0;
         if (s==0) 
         {
            res_local = *f_orig[ip];
         }
         else if (s == 1)
         {
            res_local = *f_s[ip][s-1];
         }
         else
         {
            res_local = *f_s[ip][s-1];
            res_local += *f_diag[ip][s-2];
         }
         // cout << "reslocal norm = " << res_local.Norml2() << endl;
         if (res_local.Norml2() < 1e-12) continue;
         PmlMatInv[ip]->Mult(res_local, sol_local);
         AdditiveTransferSources(s, ip, sol_local);
         *usol[ip] += sol_local;

         // Array<int>directions(2); directions = 0; 
         // int i,j,k;
         // Getijk(ip,i,j,k);
         // if (i+1<nx) directions[0] = 1;
         // if (j+1<ny) directions[1] = 1;
         // Vector cfsol_local;
         // GetCutOffSolution(sol_local,cfsol_local,ip,directions,ovlpnrlayers,true);
         // sol_local = cfsol_local;
         // directions = 0.0;
         // if (i>0) directions[0] = -1;
         // if (j>0) directions[1] = -1;
         // GetCutOffSolution(sol_local,cfsol_local,ip,directions,ovlpnrlayers,true);
         // znew = 0.0;
         // znew.SetSubVector(*Dof2GlobalDof, cfsol_local);
         // z+=znew;
      }
      // socketstream sol1_sock(vishost, visport);
      // PlotSolution(z,sol1_sock,0,false); cin.get();
   }
   for (int ip = 0; ip<nrpatch; ip++)
   {
      Array<int> * Dof2GlobalDof = &ovlp_prob->Dof2GlobalDof[ip];

      Array<int>directions(2); directions = 0; 
      int i,j,k;
      Getijk(ip,i,j,k);
      if (i+1<nx) directions[0] = 1;
      if (j+1<ny) directions[1] = 1;
      Vector cfsol_local;
      GetCutOffSolution(*usol[ip],cfsol_local,ip,directions,ovlpnrlayers,true);
      *usol[ip] = cfsol_local;
      directions = 0.0;
      if (i>0) directions[0] = -1;
      if (j>0) directions[1] = -1;
      GetCutOffSolution(*usol[ip],cfsol_local,ip,directions,ovlpnrlayers,true);
      znew = 0.0;
      znew.SetSubVector(*Dof2GlobalDof, cfsol_local);
      z+=znew;
   }
}


void AdditiveST::GetCutOffSolution(const Vector & sol, Vector & cfsol, 
                  int ip, Array<int> directions, int nlayers, bool local) const
{

   // int d = directions.Size();
   // int directx = directions[0]; // 1,0,-1
   // int directy = directions[1]; // 1,0,-1
   // int directz;
   // if (d ==3) directz = directions[2];

   Mesh * mesh = ovlp_prob->fespaces[ip]->GetMesh();
   
   Vector pmin, pmax;
   mesh->GetBoundingBox(pmin, pmax);
   double h = GetUniformMeshElementSize(povlp->patch_mesh[ip]);

   int i, j, k;
   Getijk(ip,i,j,k);
   // int nx = nxyz[0];
   // int ny = nxyz[1];
   if (directions[0]==1) pmax[0] -= h*nrlayers; 
   if (directions[1]==1) pmax[1] -= h*nrlayers; 

   if (directions[0]==-1) pmin[0] += h*nrlayers; 
   if (directions[1]==-1) pmin[1] += h*nrlayers; 

   Array2D<double> pmlh(dim,2); pmlh = 0.0;
   
   if (directions[0]==1)
   {
      pmlh[0][1] = h*(nlayers-nrlayers-1);
   }
   if (directions[0]==-1)
   {
      pmlh[0][0] = h*(nlayers-nrlayers-1);
   }
   if (directions[1]==1)
   {
      pmlh[1][1] = h*(nlayers-nrlayers-1);
   }
   if (directions[1]==-1)
   {
      pmlh[1][0] = h*(nlayers-nrlayers-1);
   }

   CutOffFnCoefficient cf(CutOffFncn, pmin, pmax, pmlh);
   double * data = sol.GetData();
   FiniteElementSpace * fes;
   if (!local)
   {
      fes = bf->FESpace();
   }
   else
   {
      fes = ovlp_prob->fespaces[ip];
   }
   int n = fes->GetTrueVSize();
   GridFunction solgf_re(fes, data);
   GridFunction solgf_im(fes, &data[n]);

   GridFunctionCoefficient coeff1_re(&solgf_re);
   GridFunctionCoefficient coeff1_im(&solgf_im);

   ProductCoefficient prod_re(coeff1_re, cf);
   ProductCoefficient prod_im(coeff1_im, cf);

   ComplexGridFunction gf(fes);
   gf.ProjectCoefficient(prod_re,prod_im);

   cfsol.SetSize(sol.Size());
   cfsol = gf;
}


AdditiveST::~AdditiveST()
{
}


void AdditiveST::Getijk(int ip, int & i, int & j, int & k) const
{
   k = ip/(nxyz[0]*nxyz[1]);
   j = (ip-k*nxyz[0]*nxyz[1])/nxyz[0];
   i = (ip-k*nxyz[0]*nxyz[1])%nxyz[0];
}

int AdditiveST::GetPatchId(const Array<int> & ijk) const
{
   int d=ijk.Size();
   int z = (d==2)? 0 : ijk[2];
   return subdomains(ijk[0],ijk[1],z);
}



void AdditiveST::AdditiveTransferSources(int s, int ip0, Vector & sol0) const
{
 // Find all neighbors of patch ip0
   int nx = nxyz[0];
   int ny = nxyz[1];
   int i0, j0, k0;
   Getijk(ip0, i0,j0,k0);
   for (int i=-1; i<2; i++)
   {
      int i1 = i0 + i;
      if (i1 <0 || i1>=nx) continue;
      for (int j=-1; j<2; j++)
      {
         if (i==0 && j==0) continue;

         int j1 = j0 + j;
         if (j1 <0 || j1>=ny) continue;
         Array<int> ij1(2); ij1[0] = i1; ij1[1]=j1;
         int ip1 = GetPatchId(ij1);

         Array<int> directions(2);
         directions[0] = i;
         directions[1] = j;
         Vector cfsol0;
         GetCutOffSolution(sol0,cfsol0,ip0,directions,ovlpnrlayers,true);

         Vector raux;
         SourceTransfer(cfsol0,directions,ip0,raux);
         if (abs(i)+abs(j) == 2)
         {
            *f_diag[ip1][s]+=raux;
         }
         else
         {
            *f_s[ip1][s]+=raux;
         }
      }
   }
}

SparseMatrix * AdditiveST::GetPmlSystemMatrix(int ip)
{
   double h = GetUniformMeshElementSize(povlp->patch_mesh[ip]);
   Array2D<double> length(dim,2);
   length = h*(nrlayers);

   int i,j,k;
   int nx = nxyz[0];
   int ny = nxyz[1];
   Getijk(ip,i,j,k);
   if (i == 0 ) length[0][0] = Pmllength[0][0];
   if (j == 0 ) length[1][0] = Pmllength[1][0];
   if (i == nx-1 ) length[0][1] = Pmllength[0][1];
   if (j == ny-1 ) length[1][1] = Pmllength[1][1];

   CartesianPML pml(povlp->patch_mesh[ip], length);
   pml.SetOmega(omega);

   Array <int> ess_tdof_list;
   if (povlp->patch_mesh[ip]->bdr_attributes.Size())
   {
      Array<int> ess_bdr(povlp->patch_mesh[ip]->bdr_attributes.Max());
      ess_bdr = 1;
      ovlp_prob->fespaces[ip]->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);
   }

   ConstantCoefficient one(1.0);
   ConstantCoefficient sigma(-pow(omega, 2));
   PmlMatrixCoefficient c1_re(dim,pml_detJ_JT_J_inv_Re,&pml);
   PmlMatrixCoefficient c1_im(dim,pml_detJ_JT_J_inv_Im,&pml);
   PmlCoefficient detJ_re(pml_detJ_Re,&pml);
   PmlCoefficient detJ_im(pml_detJ_Im,&pml);
   ProductCoefficient c2_re0(sigma, detJ_re);
   ProductCoefficient c2_im0(sigma, detJ_im);
   ProductCoefficient c2_re(c2_re0, *ws);
   ProductCoefficient c2_im(c2_im0, *ws);
   SesquilinearForm a(ovlp_prob->fespaces[ip],ComplexOperator::HERMITIAN);

   a.AddDomainIntegrator(new DiffusionIntegrator(c1_re),
                         new DiffusionIntegrator(c1_im));
   a.AddDomainIntegrator(new MassIntegrator(c2_re),
                         new MassIntegrator(c2_im));
   a.Assemble();

   OperatorPtr Alocal;
   a.FormSystemMatrix(ess_tdof_list,Alocal);
   ComplexSparseMatrix * AZ_ext = Alocal.As<ComplexSparseMatrix>();
   SparseMatrix * Mat = AZ_ext->GetSystemMatrix();
   Mat->Threshold(1e-13);
   return Mat;
}

void AdditiveST::PlotSolution(Vector & sol, socketstream & sol_sock, int ip, 
                  bool localdomain) const
{
   FiniteElementSpace * fes;
   if (!localdomain)
   {
      fes = bf->FESpace();
   }
   else
   {
      fes = ovlp_prob->fespaces[ip];
   }
   Mesh * mesh = fes->GetMesh();
   GridFunction gf(fes);
   double * data = sol.GetData();
   gf.SetData(data);
   
   string keys;
   keys = "keys mrRljc\n";
   sol_sock << "solution\n" << *mesh << gf << keys << flush;
}

void AdditiveST::PlotMesh(socketstream & mesh_sock, int ip) const
{
   FiniteElementSpace * fes = ovlp_prob->fespaces[ip];
   Mesh * mesh = fes->GetMesh();
   mesh_sock << "mesh\n" << *mesh << flush;
}

void AdditiveST::SaveSolution(Vector & sol, int ip, bool localdomain) const
{
   FiniteElementSpace * fes;
   if (!localdomain)
   {
      fes = bf->FESpace();
   }
   else
   {
      // fes = ovlp_prob->fespaces[ip];
      fes = nvlp_prob->fespaces[ip];
   }
   Mesh * mesh = fes->GetMesh();
   int n = fes->GetTrueVSize();
   GridFunction gf_re(fes);
   GridFunction gf_im(fes);
   double * data = sol.GetData();
   gf_re.SetData(data);
   gf_im.SetData(&data[n]);
   cout << "saving mesh no " << ip << endl;
   string mfilename = "output/mesh_nvlp.";
   ostringstream mesh_name;
   mesh_name << mfilename << setfill('0') << setw(6) << ip;
   ofstream mesh_ofs(mesh_name.str().c_str());
   mesh_ofs.precision(8);
   mesh->Print(mesh_ofs);

   string sfilename_re = "output/sol_nvlp.";

   ostringstream solre_name;
   solre_name << sfilename_re << setfill('0') << setw(6) << ip;
   ofstream solre_ofs(solre_name.str().c_str());
   gf_re.Save(solre_ofs);

}

int AdditiveST::SourceTransfer(const Vector & Psi0, Array<int> direction, int ip0, Vector & Psi1) const
{
   int i0,j0,k0;
   Getijk(ip0,i0,j0,k0);

   int i1 = i0+direction[0];   
   int j1 = j0+direction[1];   
   Array<int> ij(2); ij[0]=i1; ij[1]=j1;
   int ip1 = GetPatchId(ij);

   MFEM_VERIFY(i1 < nxyz[0] && i1>=0, "SourceTransfer: i1 out of bounds");
   MFEM_VERIFY(j1 < nxyz[1] && j1>=0, "SourceTransfer: j1 out of bounds");

   Array<int> * Dof2GlobalDof0 = &ovlp_prob->Dof2GlobalDof[ip0];
   Array<int> * Dof2GlobalDof1 = &ovlp_prob->Dof2GlobalDof[ip1];
   Psi1.SetSize(Dof2GlobalDof1->Size()); Psi1=0.0;
   Vector r(2*bf->FESpace()->GetTrueVSize());
   r = 0.0;
   r.SetSubVector(*Dof2GlobalDof0,Psi0);
   Vector zloc(Psi1.Size()); zloc = 0.0;
   r.GetSubVector(*Dof2GlobalDof1,zloc);

   Vector Psi(Dof2GlobalDof1->Size()); Psi=0.0;

   PmlMat[ip1]->Mult(zloc,Psi);
   Psi *=-1.0;

   Array<int> direct(2); direct = 0;
   direct[0] = -direction[0];
   direct[1] = -direction[1];
   GetChiRes(Psi, Psi1,ip1,direct, ovlpnrlayers);

   return ip1;
}




void AdditiveST::GetChiRes(const Vector & res, Vector & cfres, 
                    int ip, Array<int> directions, int nlayers) const
{
   // int l,k;
   // int d = directions.Size();
   // int directx = directions[0]; // 1,0,-1
   // int directy = directions[1]; // 1,0,-1
   // int directz;
   // if (d ==3) directz = directions[2];

   Mesh * mesh = ovlp_prob->fespaces[ip]->GetMesh();
   double h = GetUniformMeshElementSize(mesh);
   
   Vector pmin, pmax;
   mesh->GetBoundingBox(pmin, pmax);
   Array2D<double> pmlh(dim,2); pmlh = 0.0;
   int i,j,k;
   Getijk(ip,i,j,k);
   if (directions[0]==-1)
   {
      pmlh[0][0] = h;
      pmin[0] += h*(nlayers-1);
   }
   if (directions[0]==1)
   {
      pmlh[0][1] = h;
      pmax[0] -= h*(nlayers-1);

   }
   if (directions[1]==-1)
   {
      pmlh[1][0] = h;
      pmin[1] += h*(nlayers-1);
   }
   if (directions[1]==1)
   {
      pmlh[1][1] = h;
      pmax[1] -= h*(nlayers-1);
   }
   CutOffFnCoefficient cf(ChiFncn, pmin, pmax, pmlh);

   double * data = res.GetData();

   FiniteElementSpace * fespace;
   fespace = ovlp_prob->fespaces[ip];
   
   int n = fespace->GetTrueVSize();

   GridFunction solgf_re(fespace, data);
   GridFunction solgf_im(fespace, &data[n]);

   GridFunctionCoefficient coeff1_re(&solgf_re);
   GridFunctionCoefficient coeff1_im(&solgf_im);

   ProductCoefficient prod_re(coeff1_re, cf);
   ProductCoefficient prod_im(coeff1_im, cf);

   ComplexGridFunction gf(fespace);
   gf.ProjectCoefficient(prod_re,prod_im);

   cfres.SetSize(res.Size());
   cfres = gf;
}
