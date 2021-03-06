#include <numcpp.h>

using namespace numcpp;

int main()
{
  auto N = 100000;
  auto M = 1000;

  {
    auto t = tic();

    EnableCaching();

    Array<cdouble> x = zeros(N);
    for(auto i=0; i<M; i++)
    {
      Array<cdouble> y = eval(linspace(0,1,N));//fft(eval(linspace(0,1,N)*i));
      x += i*exp(2*pi*I*i*y);
    }

    DisableCaching();

    toc(t);
  }

  //std::cout << MemoryBlock::numAllocations << std::endl;

  {
    auto t = tic();

    Array<cdouble> x = zeros(N);
    for(auto i=0; i<M; i++)
    {
      Array<cdouble> y = eval(linspace(0,1,N));//fft(eval(linspace(0,1,N)*i));
      x += i*exp(2*pi*I*i*y);
    }

    toc(t);
  }

  //std::cout << MemoryBlock::numAllocations << std::endl;

  return 0;
}
