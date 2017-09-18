#include <iostream>
#include <memory>
#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <type_traits>
#include <smallfun/smallfun.hpp>
#include <benchmark/benchmark.h>

using namespace smallfun;

struct Functor {
  int i;
  unsigned N;

  constexpr int operator()(int j)const {
    return i*j+N;
  }
};

void functor(benchmark::State& state) {
  unsigned N = 100;
  std::vector<Functor> fs(N);
  std::vector<int> r(N);

  while(state.KeepRunning()) {

    for(int i=0; i < N; ++i) {
      fs[i] = Functor{i, N};
    };

    int j = 0;
    std::transform(fs.begin(), fs.end(), r.begin(),  [&](auto const& f) {
      return f(j++); // execute the functor
    });
  }

  if( r[N-1] - r[0] != 9801 ) {
    // lets make sure the optimizer does not optimizes away the thing we want to test
    std::cout << r[N-1] - r[0]  << std::endl;
  }
}

template<unsigned B>
void smallFunction(benchmark::State& state) {
  unsigned N = 100;

  using sf = SmallFun<unsigned(int const j), B >;
  std::vector<sf> fs(N);
  std::vector<int> r(N);
  while(state.KeepRunning()) {
    for(int i=0; i < N; ++i) {
      fs[i] = [i, N] (int j) {
        return i*j+N;
      };
    };

    int j = 0;
    std::transform(fs.begin(), fs.end(), r.begin(),  [&](auto const& f) {
      return f(j++);
    });
  }

  if( r[N-1] - r[0] != 9801 ) {
    std::cout << r[N-1] - r[0]  << std::endl;
  }
}


void stdFunction(benchmark::State& state) {
  unsigned N = 100;
  std::vector<std::function<unsigned(int const j)>> fs(N);
  std::vector<int> r(N);
  while(state.KeepRunning()) {
    for(int i=0; i < N; ++i) {
      fs[i] = [i, N] (int j) {
        return i*j+N;
      };
    };

    int j = 0;
    std::transform(fs.begin(), fs.end(), r.begin(),  [&](auto const& f) {
      return f(j++);
    });
  }

  if( r[N-1] - r[0] != 9801 ) {
    std::cout << r[N-1] - r[0]  << std::endl;
  }
}


auto sf32 = smallFunction<32>;
auto sf64 = smallFunction<64>;
auto sf128 = smallFunction<128>;
auto sf256 = smallFunction<256>;
auto sf512 = smallFunction<512>;
auto sf1024 = smallFunction<1024>;
auto sf2048 = smallFunction<2048>;

BENCHMARK(functor);
BENCHMARK(sf32);
BENCHMARK(sf64);
BENCHMARK(sf128);
BENCHMARK(sf256);
BENCHMARK(sf512);
BENCHMARK(sf1024);
BENCHMARK(sf2048);
BENCHMARK(stdFunction);

BENCHMARK_MAIN();
