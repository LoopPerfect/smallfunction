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


# Implementation

We need to combine three c++ patterns: type erasure, pimpl and placement new.

## Type Erasure

Type Erasure unifies many implementations into one interface.
In our case every Lambda / Functor has a custom call operator and destructor.
We need to automatically generate an implementation for any type the API consumer will be using.


This shall be our public interface:

```c++

template<class ReturnType, class...Xs>
struct Concept {
  virtual ReturnType operator()(Xs...)const = 0;
  virtual ReturnType operator()(Xs...) = 0;
  virtual ~Concept() {};
};
```

And for any callable type with for a given signature:

```c++
template<class F, class ReturnType, class...Xs>
struct Model final
  : Concept<ReturnType, Xs...> {
  F f;

  Model(F const& f)
    : f(f)
  {}

  virtual ReturnType operator()(Xs...xs)const {
    return f(xs...);
  }

  virtual ReturnType operator()(Xs...xs) {
    return f(xs...);
  }

  virtual ~Model() {}
};
```

Now we can use it the following way

```c++

auto lambda = [](int x) { return x; };
using lambdaType = decltype(lambda);

SFConcept<int,int>* functor = new Model<lambdaType, int, int>(lambda);

```

We obviusly see this is quite cumbersome and error prone. The next step will be a container.


## Pimpl

Pimpl (private implementation) seperates, hides, manages the lifetime of the used implementation and exposes a limited public API.

A straightforward implementation could look like this:

```c++

template<class ReturnType, class...Xs>
class Function<ReturnType(Xs...)> {
  std::shared_ptr<Concept<ReturnType,Xs...>> pimpl;

public:
  Function(){}

  template<class F>
  Function(F const& f)
    : pimpl(new SFModel<F, ReturnType, Xs...> )  // heap allocation
  {}
  
  ~Function() = default;

};
```

This is more or less how std::function is implemented.
So how do we get rid of the heap allocation ?

## placement new

`placement new`  allocates memory at a given address.  

### Example:

```c++
char memorypool[64];
int* a = new (memorypool) int[4];
int* b = new (memorypool + sizeof(int)*4 ) int[4];
assert( (void*)a[0] == (void*)memorypool[0] );
assert( (void*)b[0] == (void*)memorypool[32] );
```

## Putting it all togeather:

We now just need to do minor changes to remove the heap allocation:

```c++
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

As you may noticed if the Model<...>'s size is greater than SIZE bad bad things will happen...
An assert will only catch this at runntime when it is to late...
Luckily this can be catched at CompileTime using `enable_if_t`

But first what about the copy constructor ?

## Copy Constructor

Unlike the implementation of `std::function` we cannot just copy nor move a shared_ptr.
We also cant just copy bitwise the memory as the lambda may manage a ressource that can only be released once or has a sideeffect on sth. else.
Therefore we need to make the model be able to copy-construct itself for a given memory location: 

We just need to add:

```c++
  virtual void copy(void* memory)const {
    new (memory) Model<F, ReturnType, Xs...>(f);
  }


  template<unsigned rhsSize,
    std::enable_if_t<(rhsSize <= size), bool> = 0>
  SmallFun(SmallFun<ReturnType(Xs...), rhsSize> const& rhs) {
    rhs.copy(memory);
  }
  
```

# Further remarks:

- As we saw we can test at compile time if a Lambda could fit in our memory.
If It does not, we could fallback to allocating on the heap. 

- A more generic implementation of SmallFuncton would take a generic allocator.

- We noticed that we cannot copy the memory just by copying the memory bitwise. However using typetraits, we could check if 
the underlying Datatype is a POD and than copy bitwise.





