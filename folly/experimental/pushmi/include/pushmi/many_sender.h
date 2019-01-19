#pragma once
// Copyright (c) 2018-present, Facebook, Inc.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "many.h"
#include "executor.h"
#include "trampoline.h"

namespace pushmi {

template <class V, class E = std::exception_ptr>
class any_many_sender {
  union data {
    void* pobj_ = nullptr;
    char buffer_[sizeof(V)]; // can hold a V in-situ
  } data_{};
  template <class Wrapped>
  static constexpr bool insitu() {
    return sizeof(Wrapped) <= sizeof(data::buffer_) &&
        std::is_nothrow_move_constructible<Wrapped>::value;
  }
  struct vtable {
    static void s_op(data&, data*) {}
    static any_time_executor<E /* hmm, TP will be invasive */> s_executor(data&) { return {}; }
    static void s_submit(data&, many<V, E>) {}
    void (*op_)(data&, data*) = vtable::s_op;
    any_time_executor<E> (*executor_)(data&) = vtable::s_executor;
    void (*submit_)(data&, many<V, E>) = vtable::s_submit;
  };
  static constexpr vtable const noop_ {};
  vtable const* vptr_ = &noop_;
  template <class Wrapped>
  any_many_sender(Wrapped obj, std::false_type) : any_many_sender() {
    struct s {
      static void op(data& src, data* dst) {
        if (dst)
          dst->pobj_ = std::exchange(src.pobj_, nullptr);
        delete static_cast<Wrapped const*>(src.pobj_);
      }
      static any_time_executor<E> executor(data& src) {
        return any_time_executor<E>{::pushmi::executor(*static_cast<Wrapped*>(src.pobj_))};
      }
      static void submit(data& src, many<V, E> out) {
        ::pushmi::submit(*static_cast<Wrapped*>(src.pobj_), std::move(out));
      }
    };
    static const vtable vtbl{s::op, s::executor, s::submit};
    data_.pobj_ = new Wrapped(std::move(obj));
    vptr_ = &vtbl;
  }
  template <class Wrapped>
  any_many_sender(Wrapped obj, std::true_type) noexcept
      : any_many_sender() {
    struct s {
      static void op(data& src, data* dst) {
        if (dst)
          new (dst->buffer_) Wrapped(
              std::move(*static_cast<Wrapped*>((void*)src.buffer_)));
        static_cast<Wrapped const*>((void*)src.buffer_)->~Wrapped();
      }
      static any_time_executor<E> executor(data& src) {
        return any_time_executor<E>{::pushmi::executor(*static_cast<Wrapped*>((void*)src.buffer_))};
      }
      static void submit(data& src, many<V, E> out) {
        ::pushmi::submit(
            *static_cast<Wrapped*>((void*)src.buffer_), std::move(out));
      }
    };
    static const vtable vtbl{s::op, s::executor, s::submit};
    new (data_.buffer_) Wrapped(std::move(obj));
    vptr_ = &vtbl;
  }
  template <class T, class U = std::decay_t<T>>
  using wrapped_t =
    std::enable_if_t<!std::is_same<U, any_many_sender>::value, U>;
 public:
  using properties = property_set<is_sender<>, is_many<>>;

  any_many_sender() = default;
  any_many_sender(any_many_sender&& that) noexcept
      : any_many_sender() {
    that.vptr_->op_(that.data_, &data_);
    std::swap(that.vptr_, vptr_);
  }

  PUSHMI_TEMPLATE(class Wrapped)
    (requires SenderTo<wrapped_t<Wrapped>, many<V, E>, is_many<>>)
  explicit any_many_sender(Wrapped obj) noexcept(insitu<Wrapped>())
    : any_many_sender{std::move(obj), bool_<insitu<Wrapped>()>{}} {}
  ~any_many_sender() {
    vptr_->op_(data_, nullptr);
  }
  any_many_sender& operator=(any_many_sender&& that) noexcept {
    this->~any_many_sender();
    new ((void*)this) any_many_sender(std::move(that));
    return *this;
  }
  any_time_executor<E> executor() {
    return vptr_->executor_(data_);
  }
  void submit(many<V, E> out) {
    vptr_->submit_(data_, std::move(out));
  }
};

// Class static definitions:
template <class V, class E>
constexpr typename any_many_sender<V, E>::vtable const
  any_many_sender<V, E>::noop_;

template <class SF, class EXF>
class many_sender<SF, EXF> {
  SF sf_;
  EXF exf_;

 public:
  using properties = property_set<is_sender<>, is_many<>>;

  constexpr many_sender() = default;
  constexpr explicit many_sender(SF sf)
      : sf_(std::move(sf)) {}
  constexpr many_sender(SF sf, EXF exf)
      : sf_(std::move(sf)), exf_(std::move(exf)) {}

  auto executor() { return exf_(); }
  PUSHMI_TEMPLATE(class Out)
    (requires PUSHMI_EXP(lazy::Receiver<Out, is_many<>> PUSHMI_AND lazy::Invocable<SF&, Out>))
  void submit(Out out) {
    sf_(std::move(out));
  }
};

template <PUSHMI_TYPE_CONSTRAINT(Sender<is_many<>>) Data, class DSF, class DEXF>
class many_sender<Data, DSF, DEXF> {
  Data data_;
  DSF sf_;
  DEXF exf_;

 public:
  using properties = property_set_insert_t<properties_t<Data>, property_set<is_sender<>, is_many<>>>;

  constexpr many_sender() = default;
  constexpr explicit many_sender(Data data)
      : data_(std::move(data)) {}
  constexpr many_sender(Data data, DSF sf)
      : data_(std::move(data)), sf_(std::move(sf)) {}
  constexpr many_sender(Data data, DSF sf, DEXF exf)
      : data_(std::move(data)), sf_(std::move(sf)), exf_(std::move(exf)) {}

  auto executor() { return exf_(data_); }
  PUSHMI_TEMPLATE(class Out)
    (requires PUSHMI_EXP(lazy::Receiver<Out, is_many<>> PUSHMI_AND
        lazy::Invocable<DSF&, Data&, Out>))
  void submit(Out out) {
    sf_(data_, std::move(out));
  }
};

////////////////////////////////////////////////////////////////////////////////
// make_many_sender
PUSHMI_INLINE_VAR constexpr struct make_many_sender_fn {
  inline auto operator()() const {
    return many_sender<ignoreSF, trampolineEXF>{};
  }
  PUSHMI_TEMPLATE(class SF)
    (requires True<> PUSHMI_BROKEN_SUBSUMPTION(&& not Sender<SF>))
  auto operator()(SF sf) const {
    return many_sender<SF, trampolineEXF>{std::move(sf)};
  }
  PUSHMI_TEMPLATE(class SF, class EXF)
    (requires True<> && Invocable<EXF&> PUSHMI_BROKEN_SUBSUMPTION(&& not Sender<SF>))
  auto operator()(SF sf, EXF exf) const {
    return many_sender<SF, EXF>{std::move(sf), std::move(exf)};
  }
  PUSHMI_TEMPLATE(class Data)
    (requires True<> && Sender<Data, is_many<>>)
  auto operator()(Data d) const {
    return many_sender<Data, passDSF, passDEXF>{std::move(d)};
  }
  PUSHMI_TEMPLATE(class Data, class DSF)
    (requires Sender<Data, is_many<>>)
  auto operator()(Data d, DSF sf) const {
    return many_sender<Data, DSF, passDEXF>{std::move(d), std::move(sf)};
  }
  PUSHMI_TEMPLATE(class Data, class DSF, class DEXF)
    (requires Sender<Data, is_many<>> && Invocable<DEXF&, Data&>)
  auto operator()(Data d, DSF sf, DEXF exf) const {
    return many_sender<Data, DSF, DEXF>{std::move(d), std::move(sf), std::move(exf)};
  }
} const make_many_sender {};

////////////////////////////////////////////////////////////////////////////////
// deduction guides
#if __cpp_deduction_guides >= 201703
many_sender() -> many_sender<ignoreSF, trampolineEXF>;

PUSHMI_TEMPLATE(class SF)
  (requires True<> PUSHMI_BROKEN_SUBSUMPTION(&& not Sender<SF>))
many_sender(SF) -> many_sender<SF, trampolineEXF>;

PUSHMI_TEMPLATE(class SF, class EXF)
  (requires True<> && Invocable<EXF&> PUSHMI_BROKEN_SUBSUMPTION(&& not Sender<SF>))
many_sender(SF, EXF) -> many_sender<SF, EXF>;

PUSHMI_TEMPLATE(class Data)
  (requires True<> && Sender<Data, is_many<>>)
many_sender(Data) -> many_sender<Data, passDSF, passDEXF>;

PUSHMI_TEMPLATE(class Data, class DSF)
  (requires Sender<Data, is_many<>>)
many_sender(Data, DSF) -> many_sender<Data, DSF, passDEXF>;

PUSHMI_TEMPLATE(class Data, class DSF, class DEXF)
  (requires Sender<Data, is_many<>> && Invocable<DEXF&, Data&>)
many_sender(Data, DSF, DEXF) -> many_sender<Data, DSF, DEXF>;
#endif

template<>
struct construct_deduced<many_sender> : make_many_sender_fn {};

// template <
//     class V,
//     class E = std::exception_ptr,
//     SenderTo<many<V, E>, is_many<>> Wrapped>
// auto erase_cast(Wrapped w) {
//   return many_sender<V, E>{std::move(w)};
// }

} // namespace pushmi
