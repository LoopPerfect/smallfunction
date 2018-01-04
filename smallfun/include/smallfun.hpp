#ifndef SMALLFUNCTION_SMALLFUNCTION_HPP
#define SMALLFUNCTION_SMALLFUNCTION_HPP

#include <type_traits>

namespace smallfun {


template<class ReturnType, class...Xs>
struct SFConcept {
  virtual ReturnType operator()(Xs...)const = 0;
  virtual ReturnType operator()(Xs...) = 0;
  virtual void copy(void*)const = 0;
  virtual ~SFConcept() {};
};

template<class F, class ReturnType, class...Xs>
struct SFModel final
  : SFConcept<ReturnType, Xs...> {
  F f;

  SFModel(F const& f)
    : f(f)
  {}

  virtual void copy(void* memory)const {
    new (memory) SFModel<F, ReturnType, Xs...>(f);
  }

  virtual ReturnType operator()(Xs...xs)const {
    return f(xs...);
  }

  virtual ReturnType operator()(Xs...xs) {
    return f(xs...);
  }

  virtual ~SFModel() {}
};



template<class Signature, unsigned size=128>
struct SmallFun;

template<class ReturnType, class...Xs, unsigned size>
class SmallFun<ReturnType(Xs...), size> {
  char memory[size];

  bool allocated = 0;
  using concept = SFConcept<ReturnType, Xs...>;
public:
  SmallFun(){}

  template<class F,
    std::enable_if_t<(sizeof(SFModel<F, ReturnType, Xs...>)<=size), bool> = 0 >
  SmallFun(F const&f)
    : allocated(sizeof(SFModel<F, ReturnType, Xs...>)) {
    new (memory) SFModel<F, ReturnType, Xs...>(f);
  }

  template<unsigned s,
    std::enable_if_t<(s <= size), bool> = 0>
  SmallFun(SmallFun<ReturnType(Xs...), s> const& sf)
    : allocated(sf.allocated) {
    sf.copy(memory);
  }


  template<unsigned s,
    std::enable_if_t<(s <= size), bool> = 0>
  SmallFun& operator=(SmallFun<ReturnType(Xs...), s> const& sf) {
    clean();
    allocated = sf.allocated;
    sf.copy(memory);
    return *this;
  }

  void clean() {
    if (allocated) {
      ((concept*)memory)->~concept();
      allocated = 0;
    }
  }

  ~SmallFun() {
    if (allocated) {
      ((concept*)memory)->~concept();
    }
  }

  template<class...Ys>
  ReturnType operator()(Ys&&...ys) {
    return (*(concept*)memory)(std::forward<Ys>(ys)...);
  }

  template<class...Ys>
  ReturnType operator()(Ys&&...ys)const {
    return (*(concept*)memory)(std::forward<Ys>(ys)...);
  }

  void copy(void* data)const {
    if (allocated) {
      ((concept*)memory)->copy(data);
    }
  }
};

}

#endif
