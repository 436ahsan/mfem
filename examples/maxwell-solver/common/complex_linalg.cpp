

#include "complex_linalg.hpp"


ComplexDenseMatrix::ComplexDenseMatrix(){}

ComplexDenseMatrix::ComplexDenseMatrix(int s) 
{
   MFEM_ASSERT(s >= 0, "invalid ComplexDenseMatrix size: " << s);
   height = s;
   width  = s;
   cout << "s = " << s << endl;
   if (s > 0)
   {
      data = new complex<double>[s*s];
      *this = 0.0; // init with zeroes
   }
}


ComplexDenseMatrix::ComplexDenseMatrix(int m, int n) 
{
   MFEM_VERIFY(m >= 0 && n >= 0,
               "invalid DenseMatrix size: " << m << " x " << n);
   const int s = m*n;
   height = m;
   width  = n;
   if (s > 0)
   {
      data = new complex<double>[s];
      *this = 0.0; // init with zeroes
   }
}

void ComplexDenseMatrix::SetSize(int h, int w)
{
   MFEM_VERIFY(h >= 0 && w >= 0,
               "invalid ComplexDenseMatrix size: " << h << " x " << w);
   if (Height() == h && Width() == w)
   {
      return;
   }
   height = h;
   width = w;
   const int hw = h*w;
   delete data;
   data = new complex<double>[hw];
   *this = 0.0; // init with zeroes
}

ComplexDenseMatrix &ComplexDenseMatrix::operator=(double c)
{
   const int s = Height()*Width();
   for (int i = 0; i < s; i++)
   {
      data[i] = c;
   }
   return *this;
}

ComplexDenseMatrix &ComplexDenseMatrix::operator=(complex<double> c)
{
   const int s = Height()*Width();
   for (int i = 0; i < s; i++)
   {
      data[i] = c;
   }
   return *this;
}


std::complex<double> ComplexDenseMatrix::Det() const
{
   MFEM_ASSERT(Height() == Width() && Height() > 0,
               "The matrix must be square and "
               << "sized larger than zero to compute the determinant."
               << "  Height() = " << Height()
               << ", Width() = " << Width());

   switch (Height())
   {
      case 1:
         return data[0];

      case 2:
         return data[0] * data[3] - data[1] * data[2];

      case 3:
      {
         const complex<double> *d = data;
         return
            d[0] * (d[4] * d[8] - d[5] * d[7]) +
            d[3] * (d[2] * d[7] - d[1] * d[8]) +
            d[6] * (d[1] * d[5] - d[2] * d[4]);
      }
      default:
      {
         MFEM_ABORT("dim>3 not supported yet");         
         return 0;
      }
   }
}

void ComplexDenseMatrix::Print(std::ostream &out, int width_) const
{
   // save current output flags
   ios::fmtflags old_flags = out.flags();
   // output flags = scientific + show sign
   out << setiosflags(ios::scientific | ios::showpos);
   for (int i = 0; i < height; i++)
   {
      out << "[row " << i << "]\n";
      for (int j = 0; j < width; j++)
      {
         out << (*this)(i,j);
         if (j+1 == width || (j+1) % width_ == 0)
         {
            out << '\n';
         }
         else
         {
            out << ' ';
         }
      }
   }
   // reset output flags to original values
   out.flags(old_flags);
}

void ComplexDenseMatrix::PrintMatlab(std::ostream &out) const
{
   // save current output flags
   ios::fmtflags old_flags = out.flags();
   // output flags = scientific + show sign
   out << setiosflags(ios::scientific | ios::showpos);
   for (int i = 0; i < height; i++)
   {
      cout << "i = " << i << endl;
      for (int j = 0; j < width; j++)
      {
         cout << "j = " << j << endl;
         out << (*this)(i,j);
         out << ' ';
      }
      out << "\n";
   }
   // reset output flags to original values
   out.flags(old_flags);
}

ComplexDenseMatrixInverse::ComplexDenseMatrixInverse(const ComplexDenseMatrix & A) : ComplexDenseMatrix(A.Height())
{
   MFEM_VERIFY(A.Height() == A.Width(), "The matrix is not square");
   MFEM_VERIFY(A.Height() < 4, "dim > 3 is not supported yet");

   std::complex<double> detA = A.Det();
   MFEM_VERIFY(abs(A.Det())>1e-14, "The given matrix is singular");

   std::complex<double> * d = this->Data();
   std::complex<double> *dA = A.GetData();
   switch (A.Height())
   {
   case 1:
      d[0] = 1.0/dA[0];
      break;
   case 2:
      d[0] =  1.0/detA * dA[3];
      d[1] = -1.0/detA * dA[1];
      d[2] = -1.0/detA * dA[2];
      d[3] =  1.0/detA * dA[0];
      break;   
   case 3:
      d[0] =   1.0/detA*(dA[3]*dA[8] - dA[5]*dA[7]);
      d[1] =  -1.0/detA*(dA[1]*dA[8] - dA[2]*dA[7]);
      d[2] =   1.0/detA*(dA[1]*dA[5] - dA[2]*dA[4]);
      d[3] =  -1.0/detA*(dA[3]*dA[7] - dA[4]*dA[6]);
      d[4] =   1.0/detA*(dA[0]*dA[8] - dA[2]*dA[6]);
      d[5] =  -1.0/detA*(dA[0]*dA[5] - dA[2]*dA[3]);
      d[6] =   1.0/detA*(dA[3]*dA[7] - dA[4]*dA[6]);
      d[7] =  -1.0/detA*(dA[0]*dA[7] - dA[1]*dA[6]);
      d[8] =   1.0/detA*(dA[0]*dA[4] - dA[1]*dA[3]);
      break;      
   default:
      // Should be unreachable 
      break;
   }
}
