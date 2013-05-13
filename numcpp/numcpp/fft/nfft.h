#ifndef NUMCPP_NFFT_H
#define NUMCPP_NFFT_H

#include "fftw.h"
#include "../graphics.h"

namespace numcpp
{

template<class T, int D>
Vector< std::complex<T> > ndft(const Array<std::complex<T>,D>& f, const Matrix<T>& x)
{
  size_t M = shape(x,0);

  Vector< std::complex<T> > g = zeros(M);

  for(size_t k=0; k<M; k++)
  {
    Iterator<D> it(f.shape());
    for(size_t l=0; l<f.size(); l++, it++)
    {
      T arg =0;
      for(int d=0; d<D; d++)
        arg += x(k,d)*( (*it)[d] - shape(f,d)/2.0);
      g(k) += f[*it] * exp(-2*pi*I*arg);
    }
  }
  return g;
}

template<class T>
Vector< std::complex<T> > ndft(const Vector<std::complex<T>>& f, const Vector<T>& x)
{
  return ndft(f, reshape(x,size(x),1));
}

template<size_t D, class T>
Array<std::complex<T>,D> ndftAdjoint(const Vector< std::complex<T>>& fHat, const Matrix<T>& x,
                                     const std::array<size_t,D> &N)
{
  size_t M = shape(x,0);

  Array< std::complex<T>, D > g = zeros<D>(N);

  for(size_t k=0; k<M; k++)
  {
    Iterator<D> it(g.shape());
    for(size_t l=0; l<g.size(); l++, it++)
    {
      T arg =0;
      for(int d=0; d<D; d++)
        arg += x(k,d)*( (*it)[d] - shape(g,d)/2.0);
      g[*it] += fHat(k) * exp(2*pi*I*arg);
    }
  }
  return g;
}

template<class T>
Vector< std::complex<T> > ndftAdjoint(const Vector<std::complex<T>>& fHat, const Vector<T>& x, size_t N)
{
  return ndftAdjoint<1>(fHat, reshape(x,size(x),1), {N});
}

template<class T>
T nfft_window_hat(T k, size_t n, size_t m, T sigma)
{
  T b = pi*(2-1.0/sigma);
  return besseli0(m*sqrt(b*b-pow(2*pi*k/n,2)));
}

template<class T>
T nfft_window(T x, size_t n, size_t m, T sigma)
{
  T b = pi*(2-1.0/sigma);
  T arg = m*m - n*n*x*x;
  if(abs(x) < m/static_cast<T>(n))
    return sinh(b*sqrt(arg)) / sqrt(arg)/pi;
  else if(abs(x) > m/static_cast<T>(n))
    return 0;//sin(b*sqrt(-m^2+n^2*x^2))/sqrt(-m^2+n^2*x^2)/pi;
  else
    return b / pi;
}


template<class T, int D>
class NFFTPlan
{
public:

  NFFTPlan(const Matrix<T>& x, std::array<size_t,D> N, size_t m, T sigma)
   : N(N), sigma(sigma), M(shape(x,0)), m(m), x(x)
  {
    auto t = tic();

    for(int d=0; d<D; d++)
      n[d] = (size_t)round(sigma*N[d]);

    tmpVec = zeros<D>(n);

    // initialization of LUTs
    for(int d=0; d<D; d++)
    {
      windowLUT[d] = zeros(10000);
      #ifdef _OPENMP
      #pragma omp parallel for
      #endif
      for(size_t l=0; l<size(windowLUT[d]); l++)
      {
        T y = (l / static_cast<T>(size(windowLUT[d])-1) ) * m/static_cast<T>(n[d]) ;
        windowLUT[d](l) = nfft_window(y, n[d], m, sigma);
      }
    }

    for(int d=0; d<D; d++)
    {
      windowHatInvLUT[d] = zeros(N[d]);
      #ifdef _OPENMP
      #pragma omp parallel for
      #endif
      for(size_t k=0; k<N[0]; k++)
      {
        windowHatInvLUT[d](k) = 1. / nfft_window_hat(k-N[d]/2.0, n[d], m, sigma);
      }
    }

    toc(t);
  }

  NFFTPlan(const Vector<T>& x, size_t N, size_t m, T sigma)
   : NFFTPlan(reshape(x,x.size(),1), {N}, m, sigma)
  {
  }

  Vector<std::complex<T> > trafo(const Vector<std::complex<T>>& f )
  {
    tmpVec = 0;

    // apodization
    auto t = tic();
    for(size_t l=0; l<N[0]; l++)
    {
      tmpVec(l + (n[0] - N[0]) / 2) = f(l) * windowHatInvLUT[0](l);
    }
    toc(t);

    // fft
    t = tic();
    tmpVec = fftshift(fft(fftshift(tmpVec)));
    toc(t);

    // convolution
    t = tic();
    Vector< std::complex<T> > fHat = zeros(M);
    #ifdef _OPENMP
    #pragma omp parallel for
    #endif
    for(size_t k=0; k<M; k++) // loop over nonequispaced nodes
    {
      auto c = std::floor(x(k,0)*n[0]);
      for(ptrdiff_t l=(c-m); l<(c+m+1); l++) // loop over nonzero elements
      {
        ptrdiff_t idx = (l+n[0]/2) % n[0];

        // Direct evaluation of window:
        //tmpVec(idx) += fHat(k) * nfft_window(x(k) - l/static_cast<T>(n), n, m, sigma);

        // Use linear interpolation and a LUT
        T idx2 = abs(((x(k,0)*n[0] - l)/m )*(size(windowLUT[0])-1));
        size_t idx2L = std::floor(idx2);
        size_t idx2H = std::ceil(idx2);
        fHat(k) += tmpVec(idx) * (windowLUT[0](idx2L) + ( idx2-idx2L ) * (windowLUT[0](idx2H) - windowLUT[0](idx2L) ) );
      }
    }
    toc(t);

    return fHat;
  }

  Vector<std::complex<T> > trafo(const Matrix<std::complex<T>>& f )
  {
    tmpVec = 0;

    // apodization
    auto t = tic();
    for(size_t lx=0; lx<N[0]; lx++)
    {
      for(size_t ly=0; ly<N[1]; ly++)
      {
        tmpVec(lx + (n[0] - N[0]) / 2, ly + (n[1] - N[1]) / 2) =
           f(lx,ly) * windowHatInvLUT[0](lx) * windowHatInvLUT[1](ly);
      }
    }
    toc(t);

    // fft
    t = tic();
    tmpVec = fftshift(fft(fftshift(tmpVec)));
    toc(t);

    // convolution
    t = tic();
    Vector< std::complex<T> > fHat = zeros(M);

    std::array<ptrdiff_t,2> l;
    std::array<size_t, 2> idx;
    std::array<T, 2> c;
    for(size_t k=0; k<M; k++) // loop over nonequispaced nodes
    {
      for(int d=0; d<2; d++)
        c[d] = std::floor(x(k,d)*n[d]);

      for(l[0]=(c[0]-m); l[0]<(c[0]+m+1); l[0]++) // loop over nonzero elements
      {
        idx[0] = (l[0]+n[0]/2) % n[0];
        for(l[1]=(c[1]-m); l[1]<(c[1]+m+1); l[1]++)
        {
          idx[1] = (l[1]+n[1]/2) % n[1];

          // Use linear interpolation and a LUT
          auto tmp = tmpVec[idx];
          for(int d=0; d<2; d++)
          {
            T idx2 = abs(((x(k,d)*n[d] - l[d])/m )*(size(windowLUT[d])-1));
            size_t idx2L = std::floor(idx2);
            size_t idx2H = std::ceil(idx2);
            tmp *= (windowLUT[d](idx2L) + ( idx2-idx2L ) * (windowLUT[d](idx2H) - windowLUT[d](idx2L) ) );
          }

          fHat(k) += tmp;
        }
      }
    }
    toc(t);

    return fHat;
  }

  Vector<std::complex<T>> trafoND(const Array< std::complex<T>, D>& f)
  {
    tmpVec = 0;
    std::array<ptrdiff_t,D> l;
    std::array<size_t, D> idx, P;
    std::array<T, D> c;

    // apodization
    auto t = tic();
    Iterator<D> it(N);

    for(size_t j=0; j<prod(N); j++, it++)
    {
      T windowHatInvLUTProd = 1;
      for(int d=0; d<D; d++)
      {
        idx[d] = (*it)[d] + (n[d] - N[d]) / 2;
        windowHatInvLUTProd *= windowHatInvLUT[d]((*it)[d]);
      }
      tmpVec[idx] = f[*it] * windowHatInvLUTProd;

    }
    toc(t);

    // fft
    t = tic();
    tmpVec = fftshift(fft(fftshift(tmpVec)));
    toc(t);

    // convolution
    t = tic();
    Vector< std::complex<T> > fHat = zeros(M);

    for(size_t k=0; k<M; k++) // loop over nonequispaced nodes
    {
      for(int d=0; d<D; d++)
      {
        c[d] = std::floor(x(k,d)*n[d]);
        P[d] = 2*m + 1;
      }

      Iterator<D> it(P);
      for(size_t j=0; j<prod(P); j++, it++)
      {
          for(int d=0; d<D; d++)
          {
            l[d] = c[d]-m+(*it)[d];
            idx[d] = (l[d]+n[d]/2) % n[d];
          }

          // Use linear interpolation and a LUT
          auto tmp = tmpVec[idx];
          for(int d=0; d<D; d++)
          {
            T idx2 = abs(((x(k,d)*n[d] - l[d])/m )*(size(windowLUT[d])-1));
            size_t idx2L = std::floor(idx2);
            size_t idx2H = std::ceil(idx2);
            tmp *= (windowLUT[d](idx2L) + ( idx2-idx2L ) * (windowLUT[d](idx2H) - windowLUT[d](idx2L) ) );
          }

          fHat(k) += tmp;
      }
    }
    toc(t);

    return fHat;
  }

  Vector<std::complex<T> > adjoint(const Vector<std::complex<T>>& fHat )
  {
    tmpVec = 0;

    // convolution
    auto t = tic();
    #ifdef _OPENMP
    #pragma omp parallel for
    #endif
    for(size_t k=0; k<M; k++) // loop over nonequispaced nodes
    {
      auto c = std::floor(x(k,0)*n[0]);
      for(ptrdiff_t l=(c-m); l<(c+m+1); l++) // loop over nonzero elements
      {
        ptrdiff_t idx = (l+n[0]/2) % n[0];

        // Direct evaluation of window:
        //tmpVec(idx) += fHat(k) * nfft_window(x(k) - l/static_cast<T>(n), n, m, sigma);

        // Use linear interpolation and a LUT
        T idx2 = abs(((x(k,0)*n[0] - l)/m )*(size(windowLUT[0])-1));
        size_t idx2L = std::floor(idx2);
        size_t idx2H = std::ceil(idx2);
        tmpVec(idx) += fHat(k) * (windowLUT[0](idx2L) + ( idx2-idx2L ) * (windowLUT[0](idx2H) - windowLUT[0](idx2L) ) );
      }
    }
    toc(t);

    // fft
    t = tic();
    tmpVec = fftshift(ifft(fftshift(tmpVec))) * n[0];
    toc(t);

    // apodization
    t = tic();
    Vector< std::complex<T> > f = zeros(N[0]);

    for(size_t l=0; l<N[0]; l++)
    {
      f(l) = tmpVec(l + (n[0] - N[0]) / 2) * windowHatInvLUT[0](l);
    }

/*    for(size_t l=0; l<N[0]/2; l++)
    {
      f(l) = tmpVec(l + (n[0] - N[0] / 2)) * windowHatInvLUT[0](l+N[0]/2);
      f(l+N[0]/2) = tmpVec(l) * windowHatInvLUT[0](l);
    }*/


    toc(t);

    return f;
  }

  Matrix< std::complex<T> > adjoint2D(const Vector<std::complex<T>>& fHat)
  {
    tmpVec = 0;

    // convolution
    auto t = tic();
    std::array<ptrdiff_t,2> l;
    std::array<size_t, 2> idx;
    std::array<T, 2> c;
    for(size_t k=0; k<M; k++) // loop over nonequispaced nodes
    {
      for(int d=0; d<2; d++)
        c[d] = std::floor(x(k,d)*n[d]);

      for(l[0]=(c[0]-m); l[0]<(c[0]+m+1); l[0]++) // loop over nonzero elements
      {
        idx[0] = (l[0]+n[0]/2) % n[0];
        for(l[1]=(c[1]-m); l[1]<(c[1]+m+1); l[1]++)
        {
          idx[1] = (l[1]+n[1]/2) % n[1];

          // Use linear interpolation and a LUT
          auto tmp = fHat(k);
          for(int d=0; d<2; d++)
          {
            T idx2 = abs(((x(k,d)*n[d] - l[d])/m )*(size(windowLUT[d])-1));
            size_t idx2L = std::floor(idx2);
            size_t idx2H = std::ceil(idx2);
            tmp *= (windowLUT[d](idx2L) + ( idx2-idx2L ) * (windowLUT[d](idx2H) - windowLUT[d](idx2L) ) );
          }

          tmpVec[idx] += tmp;
        }
      }
    }
    toc(t);

    // fft
    t = tic();
    tmpVec = fftshift(ifft(fftshift(tmpVec))) * n[0] * n[1];
    toc(t);

    // apodization
    t = tic();
    Matrix< std::complex<T> > f = zeros<2>(N);
    for(size_t lx=0; lx<N[0]; lx++)
    {
      for(size_t ly=0; ly<N[1]; ly++)
      {
        f(lx,ly) = tmpVec(lx + (n[0] - N[0]) / 2, ly + (n[1] - N[1]) / 2)
            * windowHatInvLUT[0](lx) * windowHatInvLUT[1](ly);
      }
    }
    toc(t);

    return f;
  }

  Array< std::complex<T>, D > adjointND(const Vector<std::complex<T>>& fHat)
  {
    tmpVec = 0;

    // convolution
    auto t = tic();
    std::array<ptrdiff_t,D> l;
    std::array<size_t, D> idx, P;
    std::array<T, D> c;
    for(size_t k=0; k<M; k++) // loop over nonequispaced nodes
    {
      for(int d=0; d<D; d++)
      {
        c[d] = std::floor(x(k,d)*n[d]);
        P[d] = 2*m + 1;
      }

      Iterator<D> it(P);
      for(size_t j=0; j<prod(P); j++, it++) // loop over nonzero elements
      {
          // Use linear interpolation and a LUT
          auto tmp = fHat(k);
          for(int d=0; d<D; d++)
          {
            l[d] = c[d]-m+(*it)[d];
            idx[d] = (l[d]+n[d]/2) % n[d];
            T idx2 = abs(((x(k,d)*n[d] - l[d])/m )*(size(windowLUT[d])-1));
            size_t idx2L = std::floor(idx2);
            size_t idx2H = std::ceil(idx2);
            tmp *= (windowLUT[d](idx2L) + ( idx2-idx2L ) * (windowLUT[d](idx2H) - windowLUT[d](idx2L) ) );
          }

          tmpVec[idx] += tmp;
      }
    }
    toc(t);

    // fft
    t = tic();
    tmpVec = fftshift(ifft(fftshift(tmpVec))) * prod(n);
    toc(t);

    // apodization
    t = tic();
    Array< std::complex<T>, D > f = zeros<D>(N);
    Iterator<D> it(N);

    for(size_t j=0; j<prod(N); j++, it++)
    {
      T windowHatInvLUTProd = 1;
      for(int d=0; d<D; d++)
      {
        idx[d] = (*it)[d] + (n[d] - N[d]) / 2;
        windowHatInvLUTProd *= windowHatInvLUT[d]((*it)[d]);
      }
      f[*it] = tmpVec[idx] * windowHatInvLUTProd;

    }

    toc(t);

    return f;
  }


private:
  std::array<size_t,D> N;
  double sigma;
  std::array<size_t,D> n;
  size_t M;
  size_t m;

  const Matrix<T> x;
  std::array<Vector<T>,D> windowLUT;
  std::array<Vector<T>,D> windowHatInvLUT;
  Array< std::complex<T>, D > tmpVec;
};





template<class T>
Vector<std::complex<T> > nfft(const Vector<std::complex<T>>& f, const Vector<T>& x, size_t m=2, T sigma=2.0 )
{
  NFFTPlan<T,1> p(x, size(f), m, sigma);
  return p.trafo(f);
}

template<class T>
Vector<std::complex<T> > nfft(const Matrix<std::complex<T>>& f, const Matrix<T>& x, size_t m=2, T sigma=2.0 )
{
  NFFTPlan<T,2> p(x, f.shape(), m, sigma);
  return p.trafo(f);
}

template<class T, int D>
Vector<std::complex<T> > nfft(const Array<std::complex<T>,D>& f, const Matrix<T>& x, size_t m=2, T sigma=2.0 )
{
  NFFTPlan<T,D> p(x, f.shape(), m, sigma);
  return p.trafoND(f);
}

template<class T>
Vector<std::complex<T> > nfftAdjoint(const Vector<std::complex<T>>& fHat, const Vector<T>& x, size_t N, size_t m=2, T sigma=2.0 )
{
  NFFTPlan<T,1> p(x, N, m, sigma);
  return p.adjoint(fHat);
  //return p.adjointND(fHat);
}

template<class T>
Matrix<std::complex<T> > nfftAdjoint(const Vector<std::complex<T>>& fHat, const Matrix<T>& x, std::array<size_t,2> N,
                                     size_t m=2, T sigma=2.0 )
{
  NFFTPlan<T,2> p(x, N, m, sigma);
  return p.adjoint2D(fHat);
  //return p.adjointND(fHat);
}

template<size_t D, class T>
Array<std::complex<T>,D > nfftAdjoint(const Vector<std::complex<T>>& fHat, const Matrix<T>& x, std::array<size_t,D> N,
                                     size_t m=2, T sigma=2.0 )
{
  NFFTPlan<T,D> p(x, N, m, sigma);
  return p.adjointND(fHat);
}

}

#endif