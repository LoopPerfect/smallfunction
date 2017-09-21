# smallfunction

`SmallFun` is an alternative to `std::function`, which implements *fixed size capture optimization* (a form of small buffer optimization). In some benchmarks, it is 3-5x faster than `std::function`. 

## Background

`std::function` is a convenient way to store lambdas with closures (also known as captures), whilst providing a unified interface.
Before `std::function` and lambdas, we would create a hand-crafted functor object like this:

```c++=
struct Functor {
  // The context, or capture
  // For example, an int and an unsigned
  int i;
  unsigned N;

  // The lambda
  int operator() (int j) const {
    // For example, a small math function
    return i * j + N;
  }
};
```

This repository compares `std::function`, the hand-crafted `Functor` and `SmallFun`. We find that `SmallFun` performs better then `std::function` by being slighly less generic.


## What is std::function's Missed Opportunity?

`std::function` uses a [PImpl pattern](http://en.cppreference.com/w/cpp/language/pimpl) to provide an unified interface aross all functors for a given signature. 

For example, these two instances `f` and `g` have the same size, despite having different captures: 

```c++=
int x = 2;
int y = 9;
int z = 4;

// f captures nothing
std::function<int(int)> f = [](int i) {
  return i + 1;
};

// g captures x, y and z
std::function<int(int)> g = [=](int i) {
  return (i * (x + z)) + y;
};
```

This is because `std::function` stores the capture on the **heap**. This unifies the size of all instances, but it is also an opportunity for optimization! 

## How?

Instead of dynamically allocating memory on the **heap**, we can place the function object (including its virtual table) into a preallocated location on the **stack**. 

This is how we implemented `SmallFun`, which is used much like `std::function`: 

```c++=
// A SmallFun with capture size of 64 bytes
SmallFun<unsigned(int const j), 64> f = [i, N] (int j) {
  return i * j + N;
};
```


## Benchmarks

| test          |time(g++6)| time clang++6 & libc++ | note |
|---------------|-----------|---------|------------------------------|
| functor       |    191 ns | 120 ns  | baseline that's the best we could do: a hand crafted functor |
| sf32          |    312 ns | 300 ns  | This is big enough to store 2 ints   |
| sf64          |    369 ns | 310 ns  | |
| sf128         |    346 ns | 333 ns  | |
| sf256         |    376 ns | 320 ns  | |
| sf512         |    503 ns | 450 ns  | |
| sf1024        |    569 ns | 512 ns  | |
| sf2048        |    870 ns | 709 ns  | |
| std::function |   1141 ns | 1511 ns | That's how std::function performs     |


### The Test

To test how quickly we can allocate and call functors, we will be saving all the many instances in a vector and executing them in a loop. The results are saved into another vector to ensure that the optimizer does not optimize away what we are testing. 

```c++=
 void stdFunction(benchmark::State& state) {
  
  unsigned N = 100;
  
  std::vector<std::function<unsigned(int const j)>> fs(N);
  std::vector<int> r(N);
  
  while (state.KeepRunning()) {

    for (int i = 0; i < N; ++i) {
      fs[i] = [i, N] (int j) { // assign to the type erased container
        return i * j + N;
      };
    };

    int j = 0;
    std::transform(fs.begin(), fs.end(), r.begin(),  [&](auto const& f) {
      return f(j++); // eval the function objects
    });
  }
}
```


# SmallFun Implementation Details

We need to combine three C++ patterns: type-erasure, PImpl and placement-new.

## Type Erasure

Type Erasure unifies many implementations into one interface. In our case, every lambda or functor has a custom call operator and destructor. We need to automatically generate an implementation for any type the API consumer will be using.

This shall be our public interface:

```c++=
template<class ReturnType, class...Xs>
struct Concept {
  virtual ReturnType operator()(Xs...) const = 0;
  virtual ReturnType operator()(Xs...) = 0;
  virtual ~Concept() {};
};
```

And for any callable type with a given signature:

```c++=
template<class F, class ReturnType, class...Xs>
struct Model final
  : Concept<ReturnType, Xs...> {
  F f;

  Model(F const& f)
    : f(f)
  {}

  virtual ReturnType operator()(Xs...xs) const {
    return f(xs...);
  }

  virtual ReturnType operator()(Xs...xs) {
    return f(xs...);
  }

  virtual ~Model() {}
};
```

Now we can use it the following way

```c++=
auto lambda = [](int x) { return x; };
using lambdaType = decltype(lambda);

SFConcept<int, int>* functor = new Model<lambdaType, int, int>(lambda);
```

This is quite cumbersome and error prone. The next step will be a container.


## PImpl

PImpl seperates, hides, manages the lifetime of an actual implementation and exposes a limited public API.

A straightforward implementation could look like this:

```c++=
template<class ReturnType, class...Xs>
class Function<ReturnType(Xs...)> {
  std::shared_ptr<Concept<ReturnType,Xs...>> pimpl;

public:
  Function() {}

  template<class F>
  Function(F const& f)
    : pimpl(new SFModel<F, ReturnType, Xs...> )  // heap allocation
  {}
  
  ~Function() = default;
};
```

This is more or less how `std::function` is implemented.

So how do we get rid of the heap allocation?

## placement-new

Placement-new  allocates memory at a given address. For example: 

```c++=
char memorypool[64];
int* a = new (memorypool) int[4];
int* b = new (memorypool + sizeof(int) * 4 ) int[4];
assert( (void*)a[0] == (void*)memorypool[0] );
assert( (void*)b[0] == (void*)memorypool[32] );
```

## Putting it All Together

Now we only need to do minor changes to remove the heap allocation:

```c++=
template<class ReturnType, class...Xs>
class SmallFun<ReturnType(Xs...)> {
  char memory[SIZE];
public:
  template<class F>
  SmallFun(F const& f) 
    : new (memory) Model<F, ReturnType, Xs...>  {
    assert( sizeof(Model<F, ReturnType, Xs...>) < SIZE ); 
  }
  
  ~SmallFun() {
    if (allocated) {
      ((concept*)memory)->~concept();
    }
  } 
};
```

As you may noticed, if the `Model<...>`'s size is greater than `SIZE` bad bad things will happen and an assert will only catch this at run-time when it is to late... Luckily, this can be catched at compile-time using `enable_if_t`. 

But first what about the copy constructor?


## Copy Constructor

Unlike the implementation of `std::function`, we cannot just copy nor move a `std::shared_ptr`. We also cannot just copy bitwise the memory as the lambda may manage a resource that can only be released once or has a side-effect. Therefore, we need to make the model able to copy-construct itself for a given memory location: 

We just need to add:

```c++=
  // ...

  virtual void copy(void* memory) const {
    new (memory) Model<F, ReturnType, Xs...>(f);
  }


  template<unsigned rhsSize,
    std::enable_if_t<(rhsSize <= size), bool> = 0>
  SmallFun(SmallFun<ReturnType(Xs...), rhsSize> const& rhs) {
    rhs.copy(memory);
  }
  
  // ...
```


# Further Remarks

- As we saw, we can verify at compile-time if a Lambda will fit in our memory.
If it does not, we could provide a fallback to heap allocation. 

- A more generic implementation of `SmallFun` would take a generic allocator.

- We noticed that we cannot copy the memory just by copying the memory bitwise. However using type-traits, we could check if 
the underlying data-type is POD and then copy bitwise.
