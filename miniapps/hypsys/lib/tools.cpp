#include "tools.hpp"

const IntegrationRule* GetElementIntegrationRule(FiniteElementSpace *fes,
                                                 bool NodalQuadRule)
{
   const FiniteElement *el = fes->GetFE(0);
   if (NodalQuadRule)
   {
      return &(el->GetNodes());
   }
   ElementTransformation *eltrans = fes->GetElementTransformation(0);
   int order = eltrans->OrderGrad(el) + eltrans->Order() + el->GetOrder();
   return &IntRules.Get(el->GetGeomType(), order);
}

const IntegrationRule *GetFaceIntegrationRule(FiniteElementSpace *fes,
                                              bool NodalQuadRule)
{
   int i, order;
   // Use the first mesh face and element as indicator.
   const FaceElementTransformations *Trans =
      fes->GetMesh()->GetFaceElementTransformations(0);

   if  (NodalQuadRule)
   {
      return &IntRules.Get(Trans->FaceGeom, 1);
   }

   const FiniteElement *el = fes->GetFE(0);

   if (Trans->Elem2No >= 0)
   {
      order = min(Trans->Elem1->OrderW(), Trans->Elem2->OrderW())
              + 2*el->GetOrder();
   }
   else
   {
      order = Trans->Elem1->OrderW() + 2*el->GetOrder();
   }
   if (el->Space() == FunctionSpace::Pk)
   {
      order++;
   }
   return &IntRules.Get(Trans->FaceGeom, order);
}

void VisualizeField(socketstream &sock, const char *vishost, int visport,
                    string ProblemName, GridFunction &gf, string glvis_scale, bool vec)
{
   Mesh &mesh = *gf.FESpace()->GetMesh();

   bool newly_opened = false;

   if (!sock.is_open() && sock)
   {
      sock.open(vishost, visport);
      sock.precision(8);
      newly_opened = true;
   }
   sock << "solution\n";
   mesh.Print(sock);
   gf.Save(sock);

   if (newly_opened)
   {
      sock << "window_title '" << ProblemName << "'\n"
           << "window_geometry "
           << 0 << " " << 0 << " " << 1080 << " " << 1080 << "\n"
           << "autoscale " << glvis_scale << "\n"
           << "keys mcjlppppppppppppppppppppppppppp66666666666666666666666"
           << "66666666666666666666666666666666666666666666666662222222222";
      if ( vec ) { sock << "vvv"; }
      sock << endl;
   }
}
