//Diagonal Source Transfer Preconditioner

#include "DST.hpp"

Sweep::Sweep(int dim_) : dim(dim_)
{
   nsweeps = pow(2,dim);
   sweeps.resize(nsweeps);

   for (int is = 0; is<nsweeps; is++)
   {
      sweeps[is].SetSize(dim);
   }

   switch(dim)
   {
      case 1: 
         sweeps[0][0] =  1; 
         sweeps[1][0] = -1; 
         break;
      case 2: 
         sweeps[0][0] =  1; sweeps[0][1] =  1; 
         sweeps[1][0] = -1; sweeps[1][1] =  1; 
         sweeps[2][0] =  1; sweeps[2][1] = -1; 
         sweeps[3][0] = -1; sweeps[3][1] = -1;
         break;
      default:
         sweeps[0][0] =  1; sweeps[0][1] =  1; sweeps[0][2] =  1; 
         sweeps[1][0] = -1; sweeps[1][1] =  1; sweeps[1][2] =  1; 
         sweeps[2][0] =  1; sweeps[2][1] = -1; sweeps[2][2] =  1; 
         sweeps[3][0] = -1; sweeps[3][1] = -1; sweeps[3][2] =  1;
         sweeps[4][0] =  1; sweeps[4][1] =  1; sweeps[4][2] = -1; 
         sweeps[5][0] = -1; sweeps[5][1] =  1; sweeps[5][2] = -1; 
         sweeps[6][0] =  1; sweeps[6][1] = -1; sweeps[6][2] = -1; 
         sweeps[7][0] = -1; sweeps[7][1] = -1; sweeps[7][2] = -1;
         break;
   }
}


DST::DST(SesquilinearForm * bf_, Array2D<double> & Pmllength_, 
         double omega_, Coefficient * ws_,  int nrlayers_ , int nx_, int ny_, int nz_)
   : Solver(2*bf_->FESpace()->GetTrueVSize(), 2*bf_->FESpace()->GetTrueVSize()), 
     bf(bf_), Pmllength(Pmllength_), omega(omega_), ws(ws_), nrlayers(nrlayers_)
{

   // Indentify problem ... Helmholtz or Maxwell
   int prob_kind = bf->FESpace()->FEColl()->GetContType();

   // StopWatch chrono;
   // chrono.Clear();
   // chrono.Start();

   Mesh * mesh = bf->FESpace()->GetMesh();
   dim = mesh->Dimension();
   int partition_kind = 2;
   nx=nx_; ny=ny_;  nz=nz_;
   ovlpnrlayers = nrlayers+1;
   part = new MeshPartition(mesh, partition_kind,nx,ny,nz, ovlpnrlayers);
   nx = part->nxyz[0]; ny = part->nxyz[1];  nz = part->nxyz[2];
   nrpatch = part->nrpatch;

   // chrono.Stop();
   // double t0 = chrono.RealTime();

   // partition_kind = 1;
   // MeshPartition * part1 = new MeshPartition(mesh, partition_kind,nx,ny,nz);
   // SaveMeshPartition(part1->patch_mesh, "output/mesh3x3.", "output/sol3x3.");
   // SaveMeshPartition(part->patch_mesh, "output/mesh3x3.", "output/sol3x3.");


   // Sweeps info

   // chrono.Clear();
   // chrono.Start();

   swp = new Sweep(dim);

   dmap  = new DofMap(bf->FESpace(),part); 
   MarkOverlapElements();
   MarkOverlapDofs();
   // ComputeOverlapDofMaps();


   // Set up the local patch problems


   sqf.SetSize(nrpatch);
   Optr.SetSize(nrpatch);
   PmlMat.SetSize(nrpatch);
   PmlMatInv.SetSize(nrpatch);
   f_orig.SetSize(nrpatch);
   f_transf.SetSize(nrpatch);
   cout << "nrpatch = " << nrpatch << endl;

   // chrono.Stop();
   // double t1 = chrono.RealTime();

   // chrono.Clear();
   // chrono.Start();


   for (int ip=0; ip<nrpatch; ip++)
   {
      // cout << "Setting up patch ip = " << ip << endl;
      if (prob_kind == 0)
      {
         SetHelmholtzPmlSystemMatrix(ip);
      }
      else if (prob_kind == 1)
      {
         SetMaxwellPmlSystemMatrix(ip);
      }
      PmlMat[ip] = Optr[ip]->As<ComplexSparseMatrix>();

      // cout << "Factorizing patch ip = " << ip << endl;

      PmlMatInv[ip] = new ComplexUMFPackSolver;
      PmlMatInv[ip]->Control[UMFPACK_ORDERING] = UMFPACK_ORDERING_METIS;
      PmlMatInv[ip]->SetOperator(*PmlMat[ip]);

      int ndofs = dmap->Dof2GlobalDof[ip].Size();
      f_orig[ip] = new Vector(ndofs); 
      f_transf[ip].SetSize(swp->nsweeps);
      for (int i=0;i<swp->nsweeps; i++)
      {
         f_transf[ip][i] = new Vector(ndofs);
      }
   }



   // chrono.Stop();
   // double t2 = chrono.RealTime();
   // Allocate space for auxiliary vector used for transfers
   zaux.SetSize(2*bf->FESpace()->GetTrueVSize());

   // cout << "DST: partition time:   " << t0 << endl;
   // cout << "DST: dof map time:     " << t1 << endl;
   // cout << "DST: Assembly & factor " << t2 << endl;
   // cout << "DST: Constructor time: " << t0+t1+t2 << endl;
}

void DST::Mult(const Vector &r, Vector &z) const
{
   // StopWatch chrono;
   // chrono.Clear();
   // chrono.Start();
   // char vishost[] = "localhost";
   // int  visport   = 19916;
   // Init
   // cout << "DST: in mult " << endl;



   for (int ip=0; ip<nrpatch; ip++)
   {
      *f_orig[ip] = 0.0;
      for (int i=0;i<swp->nsweeps; i++)
      {
         *f_transf[ip][i] = 0.0;
      }
   }
   for (int ip=0; ip<nrpatch; ip++)
   {
      Array<int> * Dof2GlobalDof = &dmap->Dof2GlobalDof[ip];
      r.GetSubVector(*Dof2GlobalDof,*f_orig[ip]);

      int i,j,k;
      Getijk(ip,i,j,k);
      Array<int> ijk(dim);
      ijk[0] = i;
      ijk[1] = j;
      if (dim == 3) ijk[2] = k;
      Array2D<int> direct(dim,2); direct = 0;
      for (int d=0;d<dim; d++)
      {
         if (ijk[d] > 0) direct[d][0] = 1; 
         if (ijk[d] < part->nxyz[d]-1) direct[d][1] = 1; 
      }
      // Vector faux(*f_orig[ip]);
      // *f_orig[ip] = 0.0;
      // GetChiRes(faux,*f_orig[ip],ip,direct,ovlpnrlayers);
      GetChiRes(*f_orig[ip],ip,direct);
   }

   // chrono.Stop();
   // double t0 = chrono.RealTime();

   // cout << "Mult: Init:   " << t0 << endl;


   z = 0.0; 


   int nsteps;
   switch(dim)
   {
      case 1: nsteps = nx; break;
      case 2: nsteps = nx+ny-1; break;
      default: nsteps = nx+ny+nz-2; break;
   }
   int nsweeps = swp->nsweeps;



   // chrono.Clear();
   // chrono.Start();

   // for (int l=0; l<1; l++)
   // StopWatch chrono1;
   // StopWatch chrono2;

   for (int l=0; l<nsweeps; l++)
   {  
      // chrono1.Clear();
      // chrono1.Start();
      // cout << " l = " << l << endl;
      for (int s = 0; s<nsteps; s++)
      // for (int s = 0; s<1; s++)
      {
         // chrono2.Clear();
         // chrono2.Start();
         // cout << " s = " << s << endl;
         Array2D<int> subdomains;
         GetStepSubdomains(l,s,subdomains);

         int nsubdomains = subdomains.NumRows();
         // cout << "nsubdomains = " << nsubdomains << endl;
         for (int sb=0; sb< nsubdomains; sb++)
         // for (int sb=0; sb< 1; sb++)
         {
            Array<int> ijk(dim);
            for (int d=0; d<dim; d++) ijk[d] = subdomains[sb][d]; 
            int ip = GetPatchId(ijk);

            Array<int> * Dof2GlobalDof = &dmap->Dof2GlobalDof[ip];
            int ndofs = Dof2GlobalDof->Size();

            // cout << "ndofs = " << ndofs << endl;

            Vector sol_local(ndofs); 
            Vector res_local(ndofs); res_local = 0.0;
            if (l==0) res_local += *f_orig[ip];
            res_local += *f_transf[ip][l];
            // if (res_local.Norml2() < 1e-8) continue;
            StopWatch chrono3;
            // chrono3.Clear();
            // chrono3.Start();
            PmlMatInv[ip]->Mult(res_local, sol_local);
            // chrono3.Stop();
            // cout << "solve time " << chrono.RealTime() << endl;
            TransferSources(l,ip, sol_local);
            // Array2D<int> direct(dim,2); direct = 0;
            // for (int d=0;d<dim; d++)
            // {
            //    if (ijk[d] > 0) direct[d][0] = 1; 
            //    if (ijk[d] < part->nxyz[d]-1) direct[d][1] = 1; 
            // }

            // Vector cfsol_local;
            // GetCutOffSolution(sol_local,cfsol_local,ip,direct,ovlpnrlayers,true);
            z.AddElementVector(*Dof2GlobalDof, sol_local);
         }
         // chrono2.Stop();
         // cout << "Mult: step time:   " << chrono2.RealTime() << endl; // cin.get();

      }
      // chrono1.Stop();
      // cout << "Mult: sweep time:   " << chrono1.RealTime() << endl; // cin.get();

   }
   // chrono.Stop();
   // double t1 = chrono.RealTime();

   // cout << "Mult: Apply:   " << t1 << endl; 
}


void DST::Getijk(int ip, int & i, int & j, int & k) const
{
   k = ip/(nx*ny);
   j = (ip-k*nx*ny)/nx;
   i = (ip-k*nx*ny)%nx;
}

int DST::GetPatchId(const Array<int> & ijk) const
{
   int d=ijk.Size();
   int z = (d==2)? 0 : ijk[2];
   return part->subdomains(ijk[0],ijk[1],z);
}


void DST::TransferSources(int s, int ip0, Vector & sol0) const
{
//   Find all neighbors of patch ip0
   int i0, j0, k0;
   Getijk(ip0, i0,j0,k0);
   Array<int> directions(dim);   
   for (int i=-1; i<2; i++)
   {
      int i1 = i0 + i;
      if (i1 <0 || i1>=nx) continue;
      directions[0] = i;
      for (int j=-1; j<2; j++)
      {
         int j1 = j0 + j;
         if (j1 <0 || j1>=ny) continue;
         directions[1] = j;
         int kbeg = (dim == 2) ? 0 : -1;
         int kend = (dim == 2) ? 1 :  2;
         for (int k=kbeg; k<kend; k++)
         {
            int k1 = k0 + k;
            if (k1 <0 || k1>=nz) continue;
            if (dim == 3) directions[2] = k;

            if (i==0 && j==0 && k==0) continue;

            Array<int> ijk1(dim); 
            ijk1[0] = i1; ijk1[1]=j1; 
            if (dim ==3 ) ijk1[2]=k1;
            int ip1 = GetPatchId(ijk1);


            int l = GetSweepToTransfer(s,directions);

            if (l == -1) continue;
   //          // Array2D<int> direct(dim,2); direct = 0;
   //          // for (int d=0; d<dim; d++)
   //          // {
   //          //    if (directions[d] == -1) direct[d][0] = 1;
   //          //    if (directions[d] ==  1) direct[d][1] = 1;
   //          // }
   //          // Vector cfsol0;
   //          // GetCutOffSolution(sol0,cfsol0,ip0,direct,ovlpnrlayers,true);
            Vector raux;
            SourceTransfer(sol0,directions,ip0,raux);
   //          // SourceTransfer(cfsol0,directions,ip0,raux);
            *f_transf[ip1][l]-=raux;
         }
      }  
   }
}

void DST::SetHelmholtzPmlSystemMatrix(int ip)
{
   Mesh * mesh = part->patch_mesh[ip];
   // double h = GetUniformMeshElementSize(mesh);
   double h = part->MeshSize;
   // cout << "h = " << h << endl;
   Array2D<double> length(dim,2);
   length = h*(nrlayers);

   int i,j,k;
   Getijk(ip,i,j,k);
   if (i == 0 )    length[0][0] = Pmllength[0][0];
   if (i == nx-1 ) length[0][1] = Pmllength[0][1];
   if (dim  > 1)
   {
      if (j == 0 )    length[1][0] = Pmllength[1][0];
      if (j == ny-1 ) length[1][1] = Pmllength[1][1];
   }
   if (dim  == 3)
   {
      if (k == 0 )    length[2][0] = Pmllength[2][0];
      if (k == nz-1 ) length[2][1] = Pmllength[2][1];
   }
   
   CartesianPML pml(mesh, length);
   pml.SetOmega(omega);

   Array <int> ess_tdof_list;
   if (mesh->bdr_attributes.Size())
   {
      Array<int> ess_bdr(mesh->bdr_attributes.Max());
      ess_bdr = 1;
      dmap->fespaces[ip]->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);
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
   sqf[ip] = new SesquilinearForm (dmap->fespaces[ip],ComplexOperator::HERMITIAN);

   sqf[ip]->AddDomainIntegrator(new DiffusionIntegrator(c1_re),
                         new DiffusionIntegrator(c1_im));
   sqf[ip]->AddDomainIntegrator(new MassIntegrator(c2_re),
                         new MassIntegrator(c2_im));
   sqf[ip]->Assemble();

   Optr[ip] = new OperatorPtr;
   sqf[ip]->FormSystemMatrix(ess_tdof_list,*Optr[ip]);
   // SparseMatrix * Mat = AZ_ext->GetSystemMatrix();
   // Mat->SortColumnIndices();
   // Mat->Threshold(1e-9);
   // return Mat;
}

void DST::SetMaxwellPmlSystemMatrix(int ip)
{
   Mesh * mesh = part->patch_mesh[ip];
   // double h = GetUniformMeshElementSize(mesh);
   double h = part->MeshSize;
   // cout << "h = " << h << endl;
   Array2D<double> length(dim,2);
   length = h*(nrlayers);

   int i,j,k;
   Getijk(ip,i,j,k);
   if (i == 0 )    length[0][0] = Pmllength[0][0];
   if (i == nx-1 ) length[0][1] = Pmllength[0][1];
   if (dim  > 1)
   {
      if (j == 0 )    length[1][0] = Pmllength[1][0];
      if (j == ny-1 ) length[1][1] = Pmllength[1][1];
   }
   if (dim  == 3)
   {
      if (k == 0 )    length[2][0] = Pmllength[2][0];
      if (k == nz-1 ) length[2][1] = Pmllength[2][1];
   }
   
   CartesianPML pml(mesh, length);
   pml.SetOmega(omega);
   Array <int> ess_tdof_list;
   if (mesh->bdr_attributes.Size())
   {
      Array<int> ess_bdr(mesh->bdr_attributes.Max());
      ess_bdr = 1;
      dmap->fespaces[ip]->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);
   }

   ConstantCoefficient omeg(-pow(omega, 2));
   int cdim = (dim == 2) ? 1 : dim;

   PmlMatrixCoefficient pml_c1_Re(cdim,detJ_inv_JT_J_Re, &pml);
   PmlMatrixCoefficient pml_c1_Im(cdim,detJ_inv_JT_J_Im, &pml);

   PmlMatrixCoefficient pml_c2_Re(dim, detJ_JT_J_inv_Re,&pml);
   PmlMatrixCoefficient pml_c2_Im(dim, detJ_JT_J_inv_Im,&pml);
   ScalarMatrixProductCoefficient c2_Re0(omeg,pml_c2_Re);
   ScalarMatrixProductCoefficient c2_Im0(omeg,pml_c2_Im);
   ScalarMatrixProductCoefficient c2_Re(*ws,c2_Re0);
   ScalarMatrixProductCoefficient c2_Im(*ws,c2_Im0);

   sqf[ip] = new SesquilinearForm(dmap->fespaces[ip],ComplexOperator::HERMITIAN);

   sqf[ip]->AddDomainIntegrator(new CurlCurlIntegrator(pml_c1_Re),
                         new CurlCurlIntegrator(pml_c1_Im));
   sqf[ip]->AddDomainIntegrator(new VectorFEMassIntegrator(c2_Re),
                         new VectorFEMassIntegrator(c2_Im));
   sqf[ip]->Assemble();

   Optr[ip] = new OperatorPtr;
   sqf[ip]->FormSystemMatrix(ess_tdof_list,*Optr[ip]);
   // SparseMatrix * Mat = AZ_ext->GetSystemMatrix();
   // Mat->SortColumnIndices();
   // Mat->Threshold(1e-9);
   // return Mat;

}

void DST::SourceTransfer(const Vector & Psi0, Array<int> direction, int ip0, Vector & Psi1) const
{
   int i0,j0,k0;
   Getijk(ip0,i0,j0,k0);

   int i1 = i0+direction[0];   
   int j1 = j0+direction[1];   
   int k1;
   if (dim==3) k1 = k0+direction[2];   
   Array<int> ijk(dim); ijk[0]=i1; ijk[1]=j1; 
   if (dim == 3 ) ijk[2]=k1;
   int ip1 = GetPatchId(ijk);
   
   // MFEM_VERIFY(i1 < nx && i1>=0, "SourceTransfer: i1 out of bounds");
   // MFEM_VERIFY(j1 < ny && j1>=0, "SourceTransfer: j1 out of bounds");
   // if (dim==3)
   // {
   //    MFEM_VERIFY(k1 < nz && k1>=0, "SourceTransfer: k1 out of bounds");
   // }

   Array<int> * Dof2GlobalDof0 = &dmap->Dof2GlobalDof[ip0];
   Array<int> * Dof2GlobalDof1 = &dmap->Dof2GlobalDof[ip1];
   zaux.SetSubVector(*Dof2GlobalDof1,0.0);
   zaux.SetSubVector(*Dof2GlobalDof0,Psi0);
   Psi1.SetSize(Dof2GlobalDof1->Size());
   Vector zloc(Psi1.Size()); 
   zaux.GetSubVector(*Dof2GlobalDof1,zloc);
   // Vector Psi(Dof2GlobalDof1->Size()); 
   PmlMat[ip1]->Mult(zloc,Psi1);

   Array2D<int> direct(dim,2); direct = 0;
   for (int d = 0; d<dim; d++)
   {
      if (direction[d]==1) direct[d][0] = 1;
      if (direction[d]==-1) direct[d][1] = 1;
   }

   // GetChiRes(Psi, Psi1,ip1,direct, ovlpnrlayers);
   GetChiRes(Psi1,ip1,direct);
}


void DST::GetCutOffSolution(const Vector & sol, Vector & cfsol, 
                  int ip, Array2D<int> direct, int nlayers, bool local) const
{

   Mesh * mesh = dmap->fespaces[ip]->GetMesh();
   
   Vector pmin, pmax;
   mesh->GetBoundingBox(pmin, pmax);
   // double h = GetUniformMeshElementSize(mesh);
   double h = part->MeshSize;


   int i, j, k;
   Getijk(ip,i,j,k);

   Array2D<double> pmlh(dim,2); pmlh = 0.0;
   for (int i=0; i<dim; i++)
   {
      if (direct[i][0]==1) pmin[i] += h*nrlayers; 
      if (direct[i][1]==1) pmax[i] -= h*nrlayers; 
      for (int j=0; j<2; j++)
      {
         if (direct[i][j]==1)
         {
            pmlh[i][j] = h*(nlayers-nrlayers-1);
         }
      }  
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
      fes = dmap->fespaces[ip];
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


void DST::GetChiRes(const Vector & res, Vector & cfres, 
                    int ip, Array2D<int> direct, int nlayers) const
{
   FiniteElementSpace * fes = dmap->fespaces[ip];
   Mesh * mesh = fes->GetMesh();
   Vector pmin, pmax;
   mesh->GetBoundingBox(pmin, pmax);
   double h = part->MeshSize;
   
   Array2D<double> pmlh(dim,2); pmlh = 0.0;

   for (int i=0; i<dim; i++)
   {
      if (direct[i][0]==1) pmin[i] += h*(nlayers-1); 
      if (direct[i][1]==1) pmax[i] -= h*(nlayers-1); 
      for (int j=0; j<2; j++)
      {
         if (direct[i][j]==1)
         {
            pmlh[i][j] = h;
         }
      }  
   }

   CutOffFnCoefficient cf(ChiFncn, pmin, pmax, pmlh);

   double * data = res.GetData();
   
   int n = fes->GetTrueVSize();

   GridFunction solgf_re(fes, data);
   GridFunction solgf_im(fes, &data[n]);

   GridFunctionCoefficient coeff1_re(&solgf_re);
   GridFunctionCoefficient coeff1_im(&solgf_im);

   ProductCoefficient prod_re(coeff1_re, cf);
   ProductCoefficient prod_im(coeff1_im, cf);

   ComplexGridFunction gf(fes);
   gf.ProjectCoefficient(prod_re,prod_im);

   cfres.SetSize(res.Size());
   cfres = gf;
   
}

void DST::GetChiRes(Vector & res, int ip, Array2D<int> direct) const
{
   for (int d=0; d<dim; d++)
   {
      // negative direction
      if (direct[d][0]==1) res.SetSubVector(NovlpDofs1[ip][d],0.0);
      // possitive direction
      if (direct[d][1]==1) res.SetSubVector(NovlpDofs1[ip][d+dim],0.0);
   }
}


void DST::GetStepSubdomains(const int sweep, const int step, Array2D<int> & subdomains) const
{
   Array<int> aux;
   switch(dim)
   {
      case 2: 
      for (int i=nx-1;i>=0; i--)
      {  
         int j;
         switch (sweep)
         {
            case 0:  j = step-i;         break;
            case 1:  j = step-nx+i+1;    break;
            case 2:  j = nx+i-step-1;    break;
            default: j = nx+ny-i-step-2; break;
         }
         if (j<0 || j>=ny) continue;
         aux.Append(i); aux.Append(j);
      }
      break; 
      default:
      for (int i=nx-1;i>=0; i--)
      {  
         for (int j=ny-1;j>=0; j--)
         {
            int k;
            switch (sweep)
            {
               case 0:  k = step-i-j;            break;
               case 1:  k = step-nx+i+1-j;       break;
               case 2:  k = step-ny+j+1-i;       break;
               case 3:  k = step-nx-ny+i+j+2;    break;
               case 4:  k = i+j+nz-1-step;       break;
               case 5:  k = nx+nz-i+j-step-2;    break;
               case 6:  k = ny+nz+i-j-step-2;    break;
               default: k = nx+ny+nz-i-j-step-3; break;
            }
            if (k<0 || k>=nz) continue;   
            aux.Append(i); aux.Append(j); aux.Append(k); 
         }
      }
      break;
   }

   int nrows = aux.Size()/dim;
   int ncols = dim;

   subdomains.SetSize(nrows,ncols);
   for (int r=0;r<nrows; r++)
   {
      for (int c=0; c<ncols; c++)
      {
         int k = r*ncols + c;
         subdomains[r][c] = aux[k];
      }
   }
}


int DST::GetSweepToTransfer(const int s, Array<int> directions) const
{
   int l1=-1;
   int nsweeps = swp->nsweeps;
   Array<int> sweep0;   
   swp->GetSweep(s,sweep0);


   switch (dim)
   {
      case 2:
      for (int l=s; l<nsweeps; l++)
      {
         // Rule 1: the transfer source direction has to be similar with 
         // the sweep direction
         Array<int> sweep1;   
         swp->GetSweep(l,sweep1);
         int ddot = 0;
         for (int d=0; d<dim; d++) ddot+= sweep1[d] * directions[d];
         if (ddot <= 0) continue;

         // Rule 2: The horizontal or vertical transfer source cannot be used
         // Case of horizontal or vertical transfer source 
         // (it can't be both 0 cause it's skipped)
         if (directions[0]==0 || directions[1] == 0) 
         {
            if (sweep0[0] == -sweep1[0] && sweep0[1] == -sweep1[1]) continue;
         }
         l1 = l;
         break;
      }
      break;
      default:
      for (int l=s; l<nsweeps; l++)
      {
         // Rule 1: (similar directions) the transfer source direction has to be similar with 
         // the sweep direction
         Array<int> sweep1;   
         swp->GetSweep(l,sweep1);
         int ddot = 0;
         bool similar = true;
         for (int d=0; d<dim; d++) 
         {
            if (sweep1[d] * directions[d] < 0) similar = false;
            ddot+= sweep1[d] * directions[d];
         }
         if (!similar || ddot<=0) continue; // not similar

         // Rule 2: (oposite directions) the transfer source direction has to be similar with 
         // the sweep direction
         // 
         // check any of the projections onto the planes
         // (xy, xz, yz)

         if ( (directions[0]==0 && directions[1] != 0) || 
              (directions[0]!=0 && directions[1] == 0) ||
              (directions[0]==0 && directions[2] != 0) || 
              (directions[0]!=0 && directions[2] == 0) ||
              (directions[2]==0 && directions[1] != 0) || 
              (directions[2]!=0 && directions[1] == 0) ) 
         {
            if (sweep0[0] == -sweep1[0] && 
                sweep0[1] == -sweep1[1] && 
                sweep0[2] == -sweep1[2]) continue;
         }

         l1 = l;
         break;
      }
      break;
   }

   return l1;   
}


DST::~DST()
{
   for (int ip=0; ip<nrpatch; ip++)
   {
      for (int i=0;i<swp->nsweeps; i++)
      {
         delete f_transf[ip][i];
      }
      delete f_orig[ip];
      delete PmlMatInv[ip];
      delete Optr[ip];
      delete sqf[ip];
      // delete PmlMat[ip];
   }
   delete dmap;
   delete part; 
}

void DST::PlotSolution(Vector & sol, socketstream & sol_sock, int ip, 
                  bool localdomain) const
{
   FiniteElementSpace * fes;
   if (!localdomain)
   {
      fes = bf->FESpace();
   }
   else
   {
      fes = dmap->fespaces[ip];
   }
   Mesh * mesh = fes->GetMesh();
   GridFunction gf(fes);
   double * data = sol.GetData();
   gf.SetData(data);
   
   string keys;
   // if (ip == 0) 
   keys = "keys mrRljc\n";
   sol_sock << "solution\n" << *mesh << gf << keys << "valuerange -0.05 0.05 \n"  << flush;
   // sol_sock << "solution\n" << *mesh << gf << keys << flush;
}


   void DST::MarkOverlapElements()
   {
      cout<< "Compute Overlap Elements (in each possible direction) " << endl;
      
      
      // Lists of elements
      // x,y,z = +/- 1 ovlp 

      ovlpelems.resize(nrpatch);
      NovlpElems.resize(nrpatch);

      for (int ip = 0; ip<nrpatch; ip++)
      {
         int i,j,k;
         Getijk(ip,i,j,k);
         int ijk[dim]; ijk[0] = i; ijk[1]=j;
         if (dim==3) ijk[2] = k;
         int nxyz[dim]; nxyz[0] = nx; nxyz[1]=ny; nxyz[2]=nz;

         FiniteElementSpace * fes = dmap->fespaces[ip];
         Mesh * mesh = fes->GetMesh();
         int nrelems = mesh->GetNE();
         ovlpelems[ip].SetSize(2*dim,nrelems); ovlpelems[ip] = 1;
         NovlpElems[ip].resize(2*dim);

         Vector pmin, pmax;
         mesh->GetBoundingBox(pmin,pmax);
         // double h = GetUniformMeshElementSize(mesh);
         double h = part->MeshSize;
         // Loop through elements
         for (int iel=0; iel<mesh->GetNE(); iel++)
         {
            // Get element center
            Vector center(dim);
            int geom = mesh->GetElementBaseGeometry(iel);
            ElementTransformation * tr = mesh->GetElementTransformation(iel);
            tr->Transform(Geometries.GetCenter(geom),center);

            // Assign elements to the appropriate lists
            for (int d=0;d<dim; d++)
            {
               if (ijk[d]>0)
               {
                  if (center[d] < pmin[d]+h*ovlpnrlayers) 
                  {
                     ovlpelems[ip][d][iel] = 0;
                  }
                  else
                  {
                     NovlpElems[ip][d].Append(iel);
                  }
               }
               else
               {
                  NovlpElems[ip][d].Append(iel);
               }
               
               if (ijk[d]<nxyz[d]-1)
               {
                  if (center[d] > pmax[d]-h*ovlpnrlayers) 
                  {
                     ovlpelems[ip][dim+d][iel] = 0;
                  } 
                  else
                  {
                     NovlpElems[ip][dim+d].Append(iel);
                  }
               }
               else
               {
                  NovlpElems[ip][dim+d].Append(iel);
               }
               
            }
         }
         /*
         cout << "Subdomain (" << i << "," <<j<<")" << endl;
         for (int d=0;d<dim; d++)
         {
            cout << "d = " << d << endl;

            if (d==0) cout << "-x: " ;
            if (d==1) cout << "-y: " ;
            if (d==2) cout << "-z: " ;
            cout << "1:int elems = " ;
            for (int l=0; l<nrelems; l++)
            {
               if (ovlpelems[ip][d][l] == 1)
               {
                  // cout << part->element_map[ip][l] << " ";
                  cout << l << " ";
               }
            }
            cout << endl;
            cout << "    2:int elems = " ;
            NovlpElems[ip][d].Print(cout, NovlpElems[ip][d].Size());
            // NovlpElems[ip][d].Print();

            if (d==0) cout << "+x: " ;
            if (d==1) cout << "+y: " ;
            if (d==2) cout << "+z: " ;
            cout << "1:int elems = " ;
            for (int l=0; l<nrelems; l++)
            {
               if (ovlpelems[ip][d+dim][l] == 1)
               {
                  // cout << part->element_map[ip][l] << " ";
                  cout << l << " ";
               }
            }
            cout<< endl;
            cout << "    2:int elems = " ;
            NovlpElems[ip][d+dim].Print(cout, NovlpElems[ip][d+dim].Size());

            cout << endl;
            // cout << "    all elems = " ;
            // part->element_map[ip].Print(cout, nrelems);
            cin.get();
         }
         */
      }   
   }



void DST::MarkOverlapDofs()
{
   cout<< "Compute Overlap dofs (in each possible direction) " << endl;
   NovlpDofs.resize(nrpatch);
   NovlpDofs1.resize(nrpatch);
   for (int ip = 0; ip<nrpatch; ip++)
   {
      FiniteElementSpace * fes = dmap->fespaces[ip];
      // Loop through the marked elements
      NovlpDofs[ip].resize(2*dim);
      NovlpDofs1[ip].resize(2*dim);

      for (int d=0;d<2*dim; d++)
      {
         NovlpDofs[ip][d].SetSize(2*fes->GetTrueVSize());
         NovlpDofs[ip][d]=0;
         int melems = NovlpElems[ip][d].Size(); 
         Array<int> temp;
         for (int iel=0; iel<melems; iel++)
         {
            Array<int> ElemDofs;
            int el =  NovlpElems[ip][d][iel];
            fes->GetElementDofs(el,ElemDofs);
            int ndof = ElemDofs.Size();
            for (int i = 0; i<ndof; ++i)
            {
               int eldof = ElemDofs[i];
               int tdof = (eldof >= 0) ? eldof : abs(eldof) - 1; 
               NovlpDofs[ip][d][tdof] = 1;
               NovlpDofs[ip][d][tdof+fes->GetTrueVSize()] = 1;
               // NovlpDofs1[ip][d].Append(tdof);
               // NovlpDofs1[ip][d].Append(tdof+fes->GetTrueVSize());
            }
         }
         for (int i = 0; i<2*fes->GetTrueVSize(); i++)
         {
            if (NovlpDofs[ip][d][i]==0) NovlpDofs1[ip][d].Append(i);
         }
      }
   }
}


void DST::ComputeOverlapDofMaps()
{
   // cout<< "Compute Overlap dofs Maps" << endl;
   // OvlpDofMap.resize(nrpatch);
   // for (int ip = 0; ip<nrpatch; ip++)
   // {
   //    int nrneighbors = pow(3,dim)-1;
   //    OvlpDofMap[ip].resize(nrneighbors);
   //    int i0,j0,k0;
   //    Getijk(ip, i0,j0,k0);
   //    // loop over neighbors
   //    for (int i = -1; i<=1; i++)
   //    {
   //       for (int j = -1; j<=1; j++)
   //       {
   //          if (i==0 && j==0) continue;

   //       }
   //    }
   // }
}

