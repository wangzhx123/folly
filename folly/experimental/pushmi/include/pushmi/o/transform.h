// clang-format off
// clang format does not support the '<>' in the lambda syntax yet.. []<>()->{}
#pragma once
// Copyright (c) 2018-present, Facebook, Inc.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "../single.h"
#include "../many.h"
#include "submit.h"
#include "extension_operators.h"

namespace pushmi {

namespace detail {

template<class F, class Tag>
struct transform_on;

template<class F>
struct transform_on<F, is_single<>> {
  F f_;
  transform_on() = default;
  constexpr explicit transform_on(F f)
    : f_(std::move(f)) {}
  template<class Out>
  auto operator()(Out out) const {
    return make_single(std::move(out), on_value(*this));
  }
  template<class Out, class V>
  auto operator()(Out& out, V&& v) {
    using Result = decltype(f_((V&&) v));
    static_assert(::pushmi::SemiMovable<Result>,
      "none of the functions supplied to transform can convert this value");
    static_assert(::pushmi::SingleReceiver<Out, Result>,
      "Result of value transform cannot be delivered to Out");
    ::pushmi::set_value(out, f_((V&&) v));
  }
};

template<class F>
struct transform_on<F, is_many<>> {
  F f_;
  transform_on() = default;
  constexpr explicit transform_on(F f)
    : f_(std::move(f)) {}
  template<class Out>
  auto operator()(Out out) const {
    return make_many(std::move(out), on_next(*this));
  }
  template<class Out, class V>
  auto operator()(Out& out, V&& v) {
    using Result = decltype(f_((V&&) v));
    static_assert(::pushmi::SemiMovable<Result>,
      "none of the functions supplied to transform can convert this value");
    static_assert(::pushmi::ManyReceiver<Out, Result>,
      "Result of value transform cannot be delivered to Out");
    ::pushmi::set_next(out, f_((V&&) v));
  }
};

struct transform_fn {
  template <class... FN>
  auto operator()(FN... fn) const;
};

template <class... FN>
auto transform_fn::operator()(FN... fn) const {
  auto f = ::pushmi::overload(std::move(fn)...);
  return ::pushmi::constrain(::pushmi::lazy::Sender<::pushmi::_1>, [f = std::move(f)](auto in) {
    using In = decltype(in);
    // copy 'f' to allow multiple calls to connect to multiple 'in'
    using F = decltype(f);
    using Cardinality = property_set_index_t<properties_t<In>, is_silent<>>;
    return ::pushmi::detail::deferred_from<In>(
      std::move(in),
      ::pushmi::detail::submit_transform_out<In>(
        transform_on<F, Cardinality>{f}
      )
    );
  });
}

} // namespace detail

namespace operators {
PUSHMI_INLINE_VAR constexpr detail::transform_fn transform{};
} // namespace operators

} // namespace pushmi
