#ifndef DDMESH_HPP
#define DDMESH_HPP

#include "mfem.hpp"

using namespace mfem;
using namespace std;

/*
void TestSerialMeshLinearSystem(Mesh *mesh)
{
  const int order = 1;
  const int dim = mesh->Dimension();

  FiniteElementCollection *fec = new H1_FECollection(order, dim);
  FiniteElementSpace *fespace = new FiniteElementSpace(mesh, fec);

  Array<int> ess_tdof_list;

  BilinearForm *a = new BilinearForm(fespace);
  ConstantCoefficient one(1.0);
  a->AddDomainIntegrator(new DiffusionIntegrator(one));

  a->Assemble();

  SparseMatrix A;
  a->FormSystemMatrix(ess_tdof_list, A);

  delete a;
  delete fespace;
  delete fec;
}

void TestParallelMeshLinearSystem(ParMesh *pmesh)
{
  const int order = 1;
  const int dim = pmesh->Dimension();

  //FiniteElementCollection *fec = new H1_FECollection(order, dim);
  FiniteElementCollection *fec = new ND_FECollection(order, dim);
  ParFiniteElementSpace *fespace = new ParFiniteElementSpace(pmesh, fec);

  Array<int> ess_tdof_list;

  //ParBilinearForm *a = new ParBilinearForm(fespace);
  ParBilinearForm a(fespace);
  
  ConstantCoefficient one(1.0);
  //a->AddDomainIntegrator(new DiffusionIntegrator(one));
  a.AddDomainIntegrator(new VectorFEMassIntegrator(one));
  a.AddDomainIntegrator(new CurlCurlIntegrator(one));

  a.Assemble();

  HypreParMatrix *A = new HypreParMatrix();
  a.FormSystemMatrix(ess_tdof_list, *A);
  
  //delete a;
  delete fespace;
  delete fec;
  delete A;
}
*/

class AdjacencyInterpolator : public DiscreteInterpolator
{
public:
  virtual void AssembleElementMatrix2(const FiniteElement &dom_fe,
				      const FiniteElement &ran_fe,
				      ElementTransformation &Trans,
				      DenseMatrix &elmat)
  { elmat.SetSize(ran_fe.GetDof(), dom_fe.GetDof()); elmat = 1.0; }
};

class SubdomainInterface
{
  friend class SubdomainParMeshGenerator;
  
private:
  int sd0, sd1;  // Indices of the two neighboring subdomains, with sd0 < sd1.

  int globalIndex;

protected:
  
public:
  SubdomainInterface(const int sd0_, const int sd1_) : sd0(sd0_), sd1(sd1_)
  {
    MFEM_VERIFY(sd0 < sd1, "");
    globalIndex = -1;
  }
  
  std::set<int> vertices, edges, faces;  // local pmesh vertex, edges, and face indices
  
  void InsertVertexIndex(const int v)
  {
    vertices.insert(v);
  }

  void InsertEdgeIndex(const int e)
  {
    edges.insert(e);
  }

  void InsertFaceIndex(const int f)
  {
    faces.insert(f);
  }

  int FirstSubdomain() const
  {
    return sd0;
  }

  int SecondSubdomain() const
  {
    return sd1;
  }

  int SetGlobalIndex(const int numSubdomains)
  {
    globalIndex = (numSubdomains * sd0) + sd1;
    return globalIndex;
  }

  int GetGlobalIndex()
  {
    return globalIndex;
  }
  
  int NumVertices() const
  {
    return vertices.size();
  }

  int NumFaces() const
  {
    return faces.size();
  }

  void PrintVertices(const ParMesh *pmesh) const
  {
    for (std::set<int>::const_iterator it = vertices.begin(); it != vertices.end(); ++it)
      {
	const double* c = pmesh->GetVertex(*it);
	cout << *it << ": " << c[0] << ", " << c[1] << ", " << c[2] << endl;
      }
  }
};

class SubdomainInterfaceGenerator
{
private:
  int numSubdomains;
  ParMesh *pmesh;  // global mesh
  const int d;  // Mesh dimension

public:
  SubdomainInterfaceGenerator(const int numSubdomains_, ParMesh *pmesh_) :
    numSubdomains(numSubdomains_), pmesh(pmesh_), d(pmesh_->Dimension())
  {

  }

  ~SubdomainInterfaceGenerator()
  {

  }

  void CreateInterfaces(std::vector<SubdomainInterface>& interfaces)
  {
    MFEM_VERIFY(d == 3, "");

    interfaces.clear();
    
    L2_FECollection elem_fec(0, pmesh->Dimension());
    H1_FECollection vert_fec(1, pmesh->Dimension());
    RT_FECollection face_fec(0, pmesh->Dimension());
    
    ParFiniteElementSpace elem_fes(pmesh, &elem_fec);
    ParFiniteElementSpace vert_fes(pmesh, &vert_fec);
    ParFiniteElementSpace face_fes(pmesh, &face_fec);
    
    ParDiscreteLinearOperator vert_elem_oper(&vert_fes, &elem_fes); // maps vert_fes to elem_fes
    vert_elem_oper.AddDomainInterpolator(new AdjacencyInterpolator);
    vert_elem_oper.Assemble();
    vert_elem_oper.Finalize();
    
    HypreParMatrix *vert_elem = vert_elem_oper.ParallelAssemble();

    Vector elem_marker(elem_fes.GetTrueVSize());
    Vector vert_marker(vert_fes.GetTrueVSize());

    ParGridFunction vert_marker_gf(&vert_fes);

    MFEM_VERIFY(elem_marker.Size() == pmesh->GetNE(), "");
    MFEM_VERIFY(vert_marker_gf.Size() == pmesh->GetNV(), "");

    map<int,int> interfaceGlobalToLocal;
    
    for (int s=0; s<numSubdomains; ++s)
      {
	// Find all interfaces between subdomain s and subdomains t != s.
	
	// Mark all elements in subdomain s.
	elem_marker = 0.0;
	for (int i=0; i<pmesh->GetNE(); ++i)
	  {
	    if (pmesh->GetAttribute(i) == s+1)
	      elem_marker[i] = 1.0;
	  }

	// Find all elements neighboring subdomain s.
	vert_elem->MultTranspose(elem_marker, vert_marker);
	vert_elem->Mult(vert_marker, elem_marker);

	vert_marker_gf.SetFromTrueDofs(vert_marker);
	
	// If elem_marker(j) > 0.0, then j is a neighbor of an element marked above, or j was marked above.
	for (int i=0; i<pmesh->GetNE(); ++i)
	  {
	    if (pmesh->GetAttribute(i) != s+1 && elem_marker[i] > 0.1)
	      {
		// Element i is in a subdomain with index other than s.
		const int neighborSD = pmesh->GetAttribute(i) - 1;

		const int sd0 = std::min(s, neighborSD);
		const int sd1 = std::max(s, neighborSD);

		const int gi = (numSubdomains * sd0) + sd1;  // Global index of interface
		
		{
		  map<int, int>::iterator it = interfaceGlobalToLocal.find(gi);
		  if (it == interfaceGlobalToLocal.end())  // Create a new interface
		    {
		      interfaceGlobalToLocal[gi] = interfaces.size();
		      interfaces.push_back(SubdomainInterface(sd0, sd1));
		    }
		}
		
		const int interfaceIndex = interfaceGlobalToLocal[gi];

		Array<int> v, f, e, fcor, ecor;
		pmesh->GetElementVertices(i, v);
		pmesh->GetElementEdges(i, e, ecor);
		pmesh->GetElementFaces(i, f, fcor);
		
		for (int j = 0; j < v.Size(); j++)
		  {
		    if (vert_marker_gf[v[j]] > 0.1)
		      interfaces[interfaceIndex].InsertVertexIndex(v[j]);
		  }

		for (int k = 0; k < e.Size(); ++k)
		  {
		    Array<int> ev;
		    pmesh->GetEdgeVertices(e[k], ev);
		    bool edgeOn = true;
		    for (int j=0; j<ev.Size(); ++j)
		      {
			if (vert_marker_gf[ev[j]] < 0.1)
			  edgeOn = false;
		      }

		    if (edgeOn)
		      {
			interfaces[interfaceIndex].InsertEdgeIndex(e[k]);
		      }
		  }
		
		for (int k = 0; k < f.Size(); ++k)
		  {
		    Array<int> fv;
		    pmesh->GetFaceVertices(f[k], fv);
		    bool faceOn = true;
		    //bool yhalf = true;
		    for (int j=0; j<fv.Size(); ++j)
		      {
			if (vert_marker_gf[fv[j]] < 0.1)
			  faceOn = false;

			//if (fabs(pmesh->GetVertex(fv[j])[1] - 0.5) > 1.0e-8)
			//yhalf = false;
		      }

		    //MFEM_VERIFY(faceOn == yhalf, "yhalf");
		    
		    if (faceOn)
		      {
			// Also check whether the face is owned by this process, by checking whether the only DOF in face_fes is true.
			Array<int> fdof;
			face_fes.GetFaceDofs(f[k], fdof);

			MFEM_VERIFY(fdof.Size() == 1, "");

			const int fdof0 = (fdof[0] >= 0) ? fdof[0] : -1 - fdof[0];
			const int tdof = face_fes.GetLocalTDofNumber(fdof0);
			if (tdof >= 0)  // if this is a true DOF
			  {
			    interfaces[interfaceIndex].InsertFaceIndex(f[k]);
			  }
		      }
		  }
	      }
	  }
      }

    delete vert_elem;
  }

  int* GetInterfaceGlobalIndices(std::vector<SubdomainInterface>& interfaces)
  {
    const int numInterfaces = interfaces.size();

    if (numInterfaces == 0)
      return NULL;

    int* globalId = new int[numInterfaces];

    for (int i=0; i<numInterfaces; ++i)
      {
	globalId[i] = interfaces[i].SetGlobalIndex(numSubdomains);
      }

    return globalId;
  }

  int GlobalToLocalInterfaceMap(std::vector<SubdomainInterface>& localInterfaces, std::vector<int>& globalToLocal,
				std::vector<int>& globalIndices)
  {
    int *interfaceGlobalId = GetInterfaceGlobalIndices(localInterfaces);

    const int numLocalInterfaces = localInterfaces.size();
    
    int num_procs = -1;
    int myid = -1;

    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    int *cts = new int [num_procs];
    int *offsets = new int [num_procs];

    MPI_Allgather(&numLocalInterfaces, 1, MPI_INT, cts, 1, MPI_INT, MPI_COMM_WORLD);

    int sumcts = cts[0];
    offsets[0] = 0;
    for (int i = 1; i < num_procs; ++i)
      {
	offsets[i] = offsets[i-1] + cts[i-1];
	sumcts += cts[i];
      }
    
    int numGI = 0;
    
    if (sumcts > 0)
      {
	int *allGlobalId = new int [sumcts];
	MPI_Allgatherv(interfaceGlobalId, numLocalInterfaces, MPI_INT,
		       allGlobalId, cts, offsets, MPI_INT, MPI_COMM_WORLD);

	std::set<int> allGI;
	for (int i=0; i<sumcts; ++i)
	  allGI.insert(allGlobalId[i]);

	numGI = allGI.size();
	globalToLocal.resize(numGI);
	globalIndices.resize(numGI);
	
	int giPrev = -1;
	int cnt = 0;
	for (std::set<int>::const_iterator it = allGI.begin(); it != allGI.end(); ++it, ++cnt)
	  {
	    const int gi = *it;
	    
	    MFEM_VERIFY(giPrev < gi, "Global indices are not sorted in ascending order.");
	    giPrev = gi;
	    
	    globalIndices[cnt] = gi;
	    
	    globalToLocal[cnt] = -1;
	    for (int i=0; i<numLocalInterfaces; ++i)
	      {
		if (gi == interfaceGlobalId[i])
		  globalToLocal[cnt] = i;
	      }
	  }

	MFEM_VERIFY(cnt == numGI, "");

	delete [] allGlobalId;
      }
    
    if (interfaceGlobalId != NULL)
      delete [] interfaceGlobalId;

    delete [] cts;
    delete [] offsets;

    return numGI;
  }
};

class SubdomainParMeshGenerator
{
private:
  int numSubdomains;
  const ParMesh *pmesh;  // global mesh

  int num_procs = -1;
  int myid = -1;
  int *cts, *offsets;
  int *sdPartition;
  int sdPartitionAlloc;
  const int d;  // Mesh dimension
  int numElVert;
  
  vector<int> procNumElems;
  vector<int> sdProcId;
  vector<int> element_vgid;
  vector<double> element_coords;
  vector<int> element_pmeshId;
  
  H1_FECollection *h1_coll;
  ParFiniteElementSpace *H1_space;

  enum MeshType { SubdomainMesh, InterfaceMesh };
  MeshType mode;
  
  int NumberOfLocalElementsForSubdomain(const int attribute)
  {
    int numElements = 0;  // Number of elements on this process (in pmesh) in the subdomain (i.e. with the given attribute).
    for (int i=0; i<pmesh->GetNE(); ++i)
      {
	if (pmesh->GetAttribute(i) == attribute)
	  numElements++;
      }

    return numElements;
  }
  
  // Gather subdomain or interface mesh data to all processes.
  void GatherSubdomainOrInterfaceMeshData(const int attribute, const int numLocalElements,
					  int& subdomainNumElements, const SubdomainInterface *interface)
  {
    MPI_Allgather(&numLocalElements, 1, MPI_INT, cts, 1, MPI_INT, MPI_COMM_WORLD);

    subdomainNumElements = 0;
    offsets[0] = 0;
    for (int i = 0; i < num_procs; ++i) {
      subdomainNumElements += cts[i];
      procNumElems[i] = cts[i];
      cts[i] *= 2;
      if (i > 0)
	offsets[i] = offsets[i-1] + cts[i-1];
    }

    MFEM_VERIFY(subdomainNumElements > 0, "");
    
    // Assumption: all elements are of the same geometric type.
    Array<int> elVert;

    if (mode == SubdomainMesh)
      pmesh->GetElementVertices(0, elVert);
    else
      {
	std::set<int>::const_iterator it = interface->faces.begin();
	pmesh->GetFaceVertices(*it, elVert);
      }
    
    numElVert = elVert.Size();  // number of vertices per element
    MFEM_VERIFY(numElVert > 0, "");

    // TODO: reduce allocations by storing these in the class and only reallocating if a larger size is needed.
    vector<int> my_element_vgid(std::max(numElVert*numLocalElements, 1));  // vertex global indices, for each element
    vector<double> my_element_coords(std::max(d*numElVert*numLocalElements, 1));

    vector<int> my_element_pmeshId(std::max(numLocalElements, 1));
    
    int conn_idx = 0;
    int coords_idx = 0;
    int locElemCount = 0;

    if (mode == SubdomainMesh)
      {
	for (int elId=0; elId<pmesh->GetNE(); ++elId)
	  {
	    if (pmesh->GetAttribute(elId) == attribute)
	      {
		pmesh->GetElementVertices(elId, elVert);
		MFEM_VERIFY(numElVert == elVert.Size(), "");  // Assuming a uniform element type in the mesh.
		// NOTE: to be very careful, it should be verified that this is the same across all processes.

		Array<int> dofs;
		H1_space->GetElementDofs(elId, dofs);
		MFEM_VERIFY(numElVert == dofs.Size(), "");  // Assuming a bijection between vertices and H1 dummy space DOF's.

		my_element_pmeshId[locElemCount] = elId;
		locElemCount++;
		
		for (int i = 0; i < numElVert; ++i)
		  {
		    my_element_vgid[conn_idx++] = H1_space->GetGlobalTDofNumber(dofs[i]);
		    const double* coords = pmesh->GetVertex(elVert[i]);
		    for (int j=0; j<d; ++j)
		      my_element_coords[coords_idx++] = coords[j];
		  }
	      }
	  }

	MFEM_VERIFY(locElemCount == numLocalElements, "");
      }
    else
      {
	int prev = -1;
	for (std::set<int>::const_iterator it = interface->faces.begin(); it != interface->faces.end(); ++it)
	  {
	    pmesh->GetFaceVertices(*it, elVert);

	    MFEM_VERIFY(prev < *it, "Verify faces are in ascending order");
	    prev = *it;
	    
	    MFEM_VERIFY(numElVert == elVert.Size(), "");  // Assuming a uniform element type in the mesh.
	    // NOTE: to be very careful, it should be verified that this is the same across all processes.

	    Array<int> dofs;
	    H1_space->GetFaceDofs(*it, dofs);
	    MFEM_VERIFY(numElVert == dofs.Size(), "");  // Assuming a bijection between vertices and H1 dummy space DOF's.
    
	    for (int i = 0; i < numElVert; ++i)
	      {
		my_element_vgid[conn_idx++] = H1_space->GetGlobalTDofNumber(dofs[i]);
		const double* coords = pmesh->GetVertex(elVert[i]);
		for (int j=0; j<d; ++j)
		  my_element_coords[coords_idx++] = coords[j];
	      }
	  }
      }
    
    MFEM_VERIFY(coords_idx == d*numElVert*numLocalElements, "");
    MFEM_VERIFY(conn_idx == numElVert*numLocalElements, "");

    // Gather all the element connectivities from all processors.
    offsets[0] = 0;
    cts[0] = numElVert*procNumElems[0];
    for (int i = 1; i < num_procs; ++i) {
      cts[i] = numElVert*procNumElems[i];
      offsets[i] = offsets[i-1] + cts[i-1];
    }
    
    // TODO: resize only if a larger size is needed
    element_vgid.resize(numElVert*subdomainNumElements);
    element_coords.resize(d*numElVert*subdomainNumElements);
    element_pmeshId.resize(subdomainNumElements);

    int sendSize = numElVert*numLocalElements;
    
    MPI_Allgatherv(&(my_element_vgid[0]), sendSize, MPI_INT,
		   &(element_vgid[0]), cts, offsets, MPI_INT, MPI_COMM_WORLD);
    
    // Gather all the element coordinates from all processors.
    offsets[0] = 0;
    cts[0] = d*cts[0];
    for (int i = 1; i < num_procs; ++i) {
      cts[i] = d*cts[i];
      offsets[i] = offsets[i-1] + cts[i-1];
    }

    sendSize = d*numElVert*numLocalElements;
    
    MPI_Allgatherv(&(my_element_coords[0]), sendSize, MPI_DOUBLE,
		   &(element_coords[0]), cts, offsets, MPI_DOUBLE, MPI_COMM_WORLD);

    if (mode == SubdomainMesh)
      {
	sendSize = numLocalElements;

	cts[0] = procNumElems[0];

	for (int i = 1; i < num_procs; ++i) {
	  cts[i] = procNumElems[i];
	  offsets[i] = offsets[i-1] + cts[i-1];
	}

	MPI_Allgatherv(&(my_element_pmeshId[0]), sendSize, MPI_INT,
		       &(element_pmeshId[0]), cts, offsets, MPI_INT, MPI_COMM_WORLD);
      }
  }

  // This is a serial function, which should be called only processes touching the subdomain.
  Mesh* BuildSerialMesh(const int attribute, const int subdomainNumElements, const SubdomainInterface *interface)
  {
    // element_vgid holds vertices as global ids.  Vertices may be shared
    // between elements so we don't know the number of unique vertices in the
    // subdomain mesh.  Find all the unique vertices and construct the map of
    // global dof ids to local dof ids (vertices).  Keep track of the number of
    // unique vertices.

    set<int> unique_gdofs;
    map<int, int> unique_gdofs_first_appearance;
    for (int i = 0; i < numElVert*subdomainNumElements; ++i) {
      int gdof = element_vgid[i];
      if (unique_gdofs.insert(gdof).second) {
	unique_gdofs_first_appearance.insert(make_pair(gdof, i));
      }
    }
    
    int subdomainNumVertices = unique_gdofs.size();
    map<int, int> unique_gdofs_2_vertex;
    map<int, int> vertex_2_unique_gdofs;
    int idx = 0;
    for (set<int>::iterator it = unique_gdofs.begin();
	 it != unique_gdofs.end(); ++it) {
      unique_gdofs_2_vertex.insert(make_pair(*it, idx));
      vertex_2_unique_gdofs.insert(make_pair(idx, *it));
      ++idx;
    }

    const int dim = (interface == NULL) ? d : d-1;
    Mesh *smesh = new Mesh(dim, subdomainNumVertices, subdomainNumElements, 0, d);
    
    // For each unique vertex, add its coordinates to the subdomain mesh.
    for (int vert = 0; vert < subdomainNumVertices; ++vert)
      {
	int unique_gdof = vertex_2_unique_gdofs[vert];
	int first_conn_ref = unique_gdofs_first_appearance[unique_gdof];
	int coord_loc = d*first_conn_ref;
	smesh->AddVertex(&element_coords[coord_loc]);
      }

    if (subdomainNumElements > sdPartitionAlloc)
      {
	if (sdPartition)
	  delete [] sdPartition;

	sdPartitionAlloc = subdomainNumElements;
	sdPartition = new int[sdPartitionAlloc];
      }

    // Set the subdomain mesh communicator process indices.
    int sdProcCount = 0;
    for (int p=0; p<num_procs; ++p)
      {
	if (procNumElems[p] > 0)
	  {
	    sdProcId[p] = sdProcCount;
	    sdProcCount++;
	  }
	else
	  sdProcId[p] = -1;
      }
    
    // Now add each element and give it its attributes and connectivity.
    const int elGeom = (mode == SubdomainMesh) ? pmesh->GetElementBaseGeometry(0) : pmesh->GetFaceBaseGeometry(0);
    idx = 0;
    int ielem = 0;
    for (int p=0; p<num_procs; ++p)
      {
	for (int i=0; i<procNumElems[p]; ++i, ++ielem)
	  {
	    Element* sel = smesh->NewElement(elGeom);
	    if (mode == SubdomainMesh)
	      sel->SetAttribute(element_pmeshId[ielem] + 1);  // Add 1 to ensure positive attribute, then subtract 1 later.
	    else
	      sel->SetAttribute(attribute);
	    
	    Array<int> sv(numElVert);
	    for (int vert = 0; vert < numElVert; ++vert) {
	      sv[vert] = unique_gdofs_2_vertex[element_vgid[idx++]];
	    }
	    sel->SetVertices(sv);

	    smesh->AddElement(sel);

	    sdPartition[ielem] = sdProcId[p];
	  }
      }

    MFEM_VERIFY(ielem == subdomainNumElements, "");
    MFEM_VERIFY(idx == numElVert*subdomainNumElements, "");
    
    smesh->FinalizeTopology();
  
    return smesh;
  }
  
  Mesh* CreateSerialSubdomainOrInterfaceMesh(const int attribute, const SubdomainInterface *interface)
  {
    int numLocalElements = (interface == NULL) ? NumberOfLocalElementsForSubdomain(attribute) : interface->NumFaces();

    int subdomainNumElements = 0;
    GatherSubdomainOrInterfaceMeshData(attribute, numLocalElements, subdomainNumElements, interface);

    // Now we have enough data to build the subdomain mesh.
    Mesh *serialMesh = NULL;

    if (numLocalElements > 0)
      serialMesh = BuildSerialMesh(attribute, subdomainNumElements, interface);
    
    return serialMesh;
  }
  
public:
  SubdomainParMeshGenerator(const int numSubdomains_, ParMesh *pmesh_) :
    numSubdomains(numSubdomains_), pmesh(pmesh_), d(pmesh_->Dimension())
  {
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    cts = new int [num_procs];
    offsets = new int [num_procs];

    sdPartition = NULL;
    sdPartitionAlloc = 0;
    
    procNumElems.resize(num_procs);
    sdProcId.resize(num_procs);
    element_vgid.resize(1);  // Just to make it nonempty
    element_coords.resize(1);  // Just to make it nonempty

    // Define a superfluous finite element space, merely to get global vertex indices for the sample mesh construction.
    h1_coll = new H1_FECollection(1, d);  // Must be first order, to get a bijection between vertices and DOF's.
    H1_space = new ParFiniteElementSpace(pmesh_, h1_coll);  // This constructor effectively sets vertex (DOF) global indices.
  }
  
  ~SubdomainParMeshGenerator()
  {
    delete [] cts;
    delete [] offsets;
    
    if (sdPartition)
      delete [] sdPartition;
    
    delete h1_coll;
    delete H1_space;
  }
  
  ParMesh** CreateParallelSubdomainMeshes()
  {
    ParMesh **pmeshSD = new ParMesh*[numSubdomains];

    mode = SubdomainMesh;
    
    for (int s=0; s<numSubdomains; ++s)  // Loop over subdomains
      {
	Mesh *sdmesh = CreateSerialSubdomainOrInterfaceMesh(s+1, NULL);

	MPI_Comm sd_com;
	int color = (sdmesh == NULL);
	const int status = MPI_Comm_split(MPI_COMM_WORLD, color, myid, &sd_com);
	MFEM_VERIFY(status == MPI_SUCCESS, "Construction of comm failed");

	if (sdmesh != NULL)
	  {
	    //TestSerialMeshLinearSystem(sdmesh);
	    
	    pmeshSD[s] = new ParMesh(sd_com, *sdmesh, sdPartition);
	    delete sdmesh;

	    //TestParallelMeshLinearSystem(pmeshSD[s]);
	    
	    cout << myid << ": Subdomain mesh NBE " << pmeshSD[s]->GetNBE() << endl;
	  }
	else
	  pmeshSD[s] = NULL;
      }
    
    return pmeshSD;
  }
  
  ParMesh* CreateParallelInterfaceMesh(SubdomainInterface& interface)
  {
    mode = InterfaceMesh;

    // interface.vertices is a std::set<int> of local pmesh vertex indices
    // interface.faces is a std::set<int> of local pmesh face indices

    MFEM_VERIFY(d == 3, "");
    
    // Add all faces in interface.faces as elements in a new serial Mesh.

    Mesh *ifmesh = CreateSerialSubdomainOrInterfaceMesh(interface.GetGlobalIndex(), &interface);

    // Create a parallel mesh from ifmesh.

    MPI_Comm if_com;
    int color = (ifmesh == NULL);
    const int status = MPI_Comm_split(MPI_COMM_WORLD, color, myid, &if_com);
    MFEM_VERIFY(status == MPI_SUCCESS, "Construction of comm failed");

    if (ifmesh != NULL)
      {
	ParMesh *ifParMesh = new ParMesh(if_com, *ifmesh, sdPartition);
	delete ifmesh;

	return ifParMesh;
      }

    return NULL;
  }
  
};

#endif // DDMESH_HPP
