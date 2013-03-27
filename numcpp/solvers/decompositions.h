#ifndef DECOMPOSITIONS
#define DECOMPOSITIONS

//#include "../core.h"
//#include "../../third_party/Eigen/Dense"
//#include "lapack.h"

namespace numcpp
{

/*template<class T>
Vector<T> solve(const Matrix<T>& A, const Vector<T>& b)
{
  size_t M = A.shape(0);
  size_t N = A.shape(1);

  Eigen::Map<Eigen::Matrix<T,Eigen::Dynamic, Eigen::Dynamic>, 0,
             Eigen::Stride<Eigen::Dynamic,Eigen::Dynamic> >
             A_(A.data(), M, N, Eigen::Stride<Eigen::Dynamic,Eigen::Dynamic>(A.strides()[1],A.strides()[0]));
  Eigen::Map<Eigen::Matrix<T,Eigen::Dynamic, 1>> b_(b.data(), M, 1);

  Vector<T> x(N);

  Eigen::Map<Eigen::Matrix<T,Eigen::Dynamic, 1>> x_(x.data(), N, 1);

  x_ = A_.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b_);

  std::cout << "A:" << A_ << std::endl;
  std::cout << "b:" << b_ << std::endl;
  std::cout << "x:" << x_ << std::endl;

  Eigen::Matrix<T,Eigen::Dynamic, 1> x2 = A_.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b_);

std::cout << "x2:" << x2 << std::endl;

  return x;
}*/

/*template<class T>
class SvdData
{
public:
  SvdData(const Matrix<T>& A)
    : M_(A.shape(0))
    , N_(A.shape(1))
    , P_(std::min(M_, N_))
    , svd_( Eigen::Map<Eigen::Matrix<T,Eigen::Dynamic, Eigen::Dynamic>, 0,
             Eigen::Stride<Eigen::Dynamic,Eigen::Dynamic> >
               (A.data(),
                M_,
                N_,
                Eigen::Stride<Eigen::Dynamic,Eigen::Dynamic>(A.strides()[1],A.strides()[0])
               ),
             Eigen::ComputeThinU | Eigen::ComputeThinV)
    , U(svd_.matrixU().data(), false, M_, P_)
    , S(svd_.singularValues().data(), false, P_)
    , V(svd_.matrixV().data(), false, P_, N_)
  {
  }

private:
  size_t M_,N_,P_;
  Eigen::JacobiSVD<Eigen::Matrix<T,Eigen::Dynamic,Eigen::Dynamic> > svd_;
public:
  Matrix<T> U;
  Vector<T> S;
  Matrix<T> V;
};

template<class T>
SvdData<T> svd(const Matrix<T>& A)
{
  return SvdData<T>(A);
}*/

}

#endif
