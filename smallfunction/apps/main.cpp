#include <iostream>
#include <memory>
#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <type_traits>
#include <benchmark/benchmark.h>

template<class T, unsigned size=128>
struct SmallFunction;

template<class ReturnType, class...Xs>
struct Concept {
  virtual ReturnType operator()(Xs...)const = 0;
  virtual ReturnType operator()(Xs...) = 0;
  virtual void copy(void*)const = 0;
  virtual ~Concept() {};
};

template<class F, class ReturnType, class...Xs>
struct Model
  : Concept<ReturnType, Xs...> {
  F f;

  Model(F const& f)
    : f(f)
  {}

  virtual void copy(void* memory)const {
    new (memory) Model<F, ReturnType, Xs...>(f);
  }

  virtual ReturnType operator()(Xs...xs)const {
    return f(xs...);
  }

  virtual ReturnType operator()(Xs...xs) {
    return f(xs...);
  }

  virtual ~Model() {}
};


template<class ReturnType, class...Xs, unsigned size>
struct SmallFunction<ReturnType(Xs...), size> {
  char memory[size];

  bool allocated = 0;
  using memType = decltype(memory);
  using trait = Concept<ReturnType, Xs...>;

  SmallFunction(){}

  template<class F,
    std::enable_if_t<(sizeof(Model<F, ReturnType, Xs...>)<=size), bool> = 0 >
  SmallFunction(F const&f)
    : allocated(sizeof(Model<F, ReturnType, Xs...>)) {
    new (memory) Model<F, ReturnType, Xs...>(f);
  }

  template<unsigned s,
    std::enable_if_t<(s <= size), bool> = 0>
  SmallFunction(SmallFunction<ReturnType(Xs...), s> const& sf)
    : allocated(sf.allocated) {
    sf.copy(memory);
  }


  template<unsigned s,
    std::enable_if_t<(s <= size), bool> = 0>
  SmallFunction& operator=(SmallFunction<ReturnType(Xs...), s> const& sf) {
    clean();
    allocated = sf.allocated;
    sf.copy(memory);
  }

  void clean() {
    if (allocated) {
      ((trait*)memory)->~trait();
      allocated = 0;
    }
  }

  ~SmallFunction() {
    if (allocated) {
      ((trait*)memory)->~trait();
    }
  }

  template<class...Ys>
  ReturnType operator()(Ys&&...ys) {
    return (*(trait*)memory)(std::forward<Ys>(ys)...);
  }

  template<class...Ys>
  ReturnType operator()(Ys&&...ys)const {
    return (*(trait*)memory)(std::forward<Ys>(ys)...);
  }

  void copy(void* data)const {
    if (allocated) {
      ((trait*)memory)->copy(data);
    }
  }
};

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
  std::vector<SmallFunction<unsigned(int const j), B >> fs(N);
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
