# smallfunction

stack allocated and type erased functors

Optimized `std::function` alternative using "fixed size capture optimization"
A 3-5x faster `std::function`.

`std::function` is a very convinient way to store lambdas with closures (captures) while providing a unified interface.
Before `std::function` and lambdas we would create a hand crafted functor object like this:

```c++
struct Functor {
  // the context / capture
  int i;
  unsigned N;

  // the lambda
  int operator()(int j)const {
    return i*j+N;
  }
};
```

This repository compares `std::function` with this Functor and provides an alternative implementaion
that performs better by being slighly less generic.


## What is std::functions missed opportunity ?

std::function uses a pimpl pattern to provide an unified interface aross all functors for a given signature.
However, the std implementation - as it makes no asumptions on the objects size - stores the object on the **heap**.
In most cases however we can do better!

## How ?

Instead dynamically allocate memory on the **heap** - we can place the function object including it's virtual table onto a preallocated location on the **stack**.


## Benchmarks



| test          |    time   | note |
|---------------|-----------|---------------------------------------|
| functor       |    191 ns | baseline thats the best we could do; hand crafted functor |
| sf32          |    312 ns | This is big enough to store 2 ints   |
| sf64          |    369 ns | |
| sf128         |    346 ns | |
| sf256         |    376 ns | |
| sf512         |    503 ns | |
| sf1024        |    569 ns | |
| sf2048        |    870 ns | |
| std::function |   1141 ns | Thats how std::function performs     |


### The Test

We will be testing how quickly we can allocate and call functors.
We will be saving all the functors into a vector and execute.
We will save the results into a result vector to make sure
that the optimizer does not optimize away the thing we test.

```c++
 void stdFunction(benchmark::State& state) {
  unsigned N = 100;
  std::vector<std::function<unsigned(int const j)>> fs(N);
  std::vector<int> r(N);
  while(state.KeepRunning()) {

    for(int i=0; i < N; ++i) {
      fs[i] = [i, N] (int j) { // assign to the type erased container
        return i*j+N;
      };
    };

    int j = 0;
    std::transform(fs.begin(), fs.end(), r.begin(),  [&](auto const& f) {
      return f(j++); // eval the function objects
    });
  }
}
```

