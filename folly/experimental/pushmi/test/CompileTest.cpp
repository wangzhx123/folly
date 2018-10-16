#include <type_traits>

#include <chrono>
using namespace std::literals;

#include "pushmi/flow_single_deferred.h"
#include "pushmi/o/empty.h"
#include "pushmi/o/just.h"
#include "pushmi/o/on.h"
#include "pushmi/o/transform.h"
#include "pushmi/o/tap.h"
#include "pushmi/o/via.h"
#include "pushmi/o/submit.h"
#include "pushmi/o/extension_operators.h"

#include "pushmi/trampoline.h"
#include "pushmi/new_thread.h"

using namespace pushmi::aliases;

void none_test() {
  auto out0 = pushmi::none{};
  auto out1 = pushmi::none{pushmi::abortEF{}};
  auto out2 = pushmi::none{pushmi::abortEF{}, pushmi::ignoreDF{}};
  auto out3 = pushmi::none{[](auto e) noexcept{ e.get(); }};
  auto out5 = pushmi::none{
      pushmi::on_error{[](auto e)noexcept {
        e.get(); }, [](std::exception_ptr e) noexcept{}
      }};
  auto out6 = pushmi::none{
      pushmi::on_done{[]() {  }}};

  using Out0 = decltype(out0);

  auto proxy0 = pushmi::none{out0};
  auto proxy2 = pushmi::none{out0, pushmi::passDEF{}};
  auto proxy3 = pushmi::none{
      out0, pushmi::passDEF{}, pushmi::passDDF{}};
  auto proxy4 = pushmi::none{out0, [](auto d, auto e)noexcept {
    d.error(e.get());
  }};
  auto proxy5 = pushmi::none{
      out0,
      pushmi::on_error{[](Out0&, auto e) noexcept{ e.get(); },
                       [](Out0&, std::exception_ptr e) noexcept{}}};
  auto proxy6 = pushmi::none{
      out0,
      pushmi::on_done{[](Out0&) { }}};

  std::promise<void> p0;
  auto promise0 = pushmi::none{std::move(p0)};
  promise0.done();

  std::promise<void> p1;

  auto any0 = pushmi::any_none<>(std::move(p1));
  auto any1 = pushmi::any_none<>(std::move(promise0));
  auto any2 = pushmi::any_none<>(out0);
  auto any3 = pushmi::any_none<>(proxy0);
}

void deferred_test(){
  auto in0 = pushmi::deferred{};
  auto in1 = pushmi::deferred{pushmi::ignoreSF{}};
  auto in3 = pushmi::deferred{[&](auto out){
    in0.submit(pushmi::none{std::move(out), [](auto d, auto e) noexcept {
      pushmi::set_error(d, e);
    }});
  }};
  in3.submit(pushmi::none{});

  std::promise<void> p0;
  auto promise0 = pushmi::none{std::move(p0)};
  in0 | ep::submit(std::move(promise0));

  auto out0 = pushmi::none{[](auto e) noexcept {  }};
  auto out1 = pushmi::none{out0, [](auto d, auto e) noexcept {}};
  out1.error(std::exception_ptr{});
  in3.submit(out1);

  auto any0 = pushmi::any_deferred<>(in0);
}

void single_test() {
  auto out0 = pushmi::single{};
  auto out1 = pushmi::single{pushmi::ignoreVF{}};
  auto out2 = pushmi::single{pushmi::ignoreVF{}, pushmi::abortEF{}};
  auto out3 =
      pushmi::single{pushmi::ignoreVF{}, pushmi::abortEF{}, pushmi::ignoreDF{}};
  auto out4 = pushmi::single{[](auto v) { v.get(); }};
  auto out5 = pushmi::single{
      pushmi::on_value{[](auto v) { v.get(); }, [](int v) {}},
      pushmi::on_error{
        [](auto e)noexcept { e.get(); },
        [](std::exception_ptr e) noexcept{}}};
  auto out6 = pushmi::single{
      pushmi::on_error{
        [](auto e) noexcept{ e.get(); },
        [](std::exception_ptr e) noexcept{}}};
  auto out7 = pushmi::single{
      pushmi::on_done{[]() {  }}};

  using Out0 = decltype(out0);

  auto proxy0 = pushmi::single{out0};
  auto proxy1 = pushmi::single{out0, pushmi::passDVF{}};
  auto proxy2 = pushmi::single{out0, pushmi::passDVF{}, pushmi::passDEF{}};
  auto proxy3 = pushmi::single{
      out0, pushmi::passDVF{}, pushmi::passDEF{}, pushmi::passDDF{}};
  auto proxy4 = pushmi::single{out0, [](auto d, auto v) {
    pushmi::set_value(d, v.get());
  }};
  auto proxy5 = pushmi::single{
      out0,
      pushmi::on_value{[](Out0&, auto v) { v.get(); }, [](Out0&, int v) {}},
      pushmi::on_error{[](Out0&, auto e) noexcept { e.get(); },
                       [](Out0&, std::exception_ptr e) noexcept {}}};
  auto proxy6 = pushmi::single{
      out0,
      pushmi::on_error{[](Out0&, auto e) noexcept { e.get(); },
                       [](Out0&, std::exception_ptr e) noexcept {}}};
  auto proxy7 = pushmi::single{
      out0,
      pushmi::on_done{[](Out0&) { }}};

  std::promise<int> p0;
  auto promise0 = pushmi::single{std::move(p0)};
  promise0.value(0);

  std::promise<int> p1;

  auto any0 = pushmi::any_single<int>(std::move(p1));
  auto any1 = pushmi::any_single<int>(std::move(promise0));
  auto any2 = pushmi::any_single<int>(out0);
  auto any3 = pushmi::any_single<int>(proxy0);
}

void single_deferred_test(){
  auto in0 = pushmi::single_deferred{};
  auto in1 = pushmi::single_deferred{pushmi::ignoreSF{}};
  auto in3 = pushmi::single_deferred{[&](auto out){
    in0.submit(pushmi::single{std::move(out),
      pushmi::on_value{[](auto d, int v){ pushmi::set_value(d, v); }}
    });
  }};

  std::promise<int> p0;
  auto promise0 = pushmi::single{std::move(p0)};
  in0 | ep::submit(std::move(promise0));

  auto out0 = pushmi::single{};
  auto out1 = pushmi::single{out0, pushmi::on_value{[](auto d, int v){
    pushmi::set_value(d, v);
  }}};
  in3.submit(out1);

  auto any0 = pushmi::any_single_deferred<int>(in0);
}

void time_single_deferred_test(){
  auto in0 = pushmi::time_single_deferred{};
  auto in1 = pushmi::time_single_deferred{pushmi::ignoreSF{}};
  auto in3 = pushmi::time_single_deferred{[&](auto tp, auto out){
    in0.submit(tp, pushmi::single{std::move(out),
      pushmi::on_value{[](auto d, int v){ pushmi::set_value(d, v); }}
    });
  }};
  auto in4 = pushmi::time_single_deferred{pushmi::ignoreSF{}, pushmi::systemNowF{}};

  std::promise<int> p0;
  auto promise0 = pushmi::single{std::move(p0)};
  in0.submit(in0.now(), std::move(promise0));

  auto out0 = pushmi::single{};
  auto out1 = pushmi::single{out0, pushmi::on_value{[](auto d, int v){
    pushmi::set_value(d, v);
  }}};
  in3.submit(in0.now(), out1);

  auto any0 = pushmi::any_time_single_deferred<int>(in0);

  in3 | op::submit();
  in3 | op::submit_at(in3.now() + 1s);
  in3 | op::submit_after(1s);

#if 0
  auto tr = v::trampoline();
  tr |
    op::transform([]<class TR>(TR tr) {
      return v::get_now(tr);
    }) |
    // op::submit(v::single{});
    op::get<std::chrono::system_clock::time_point>();

  std::vector<std::string> times;
  auto push = [&](int time) {
    return v::on_value([&, time](auto) { times.push_back(std::to_string(time)); });
  };

  auto nt = v::new_thread();

  auto out = v::any_single<v::any_time_executor_ref<std::exception_ptr, std::chrono::system_clock::time_point>>{v::single{}};
  (v::any_time_executor_ref{nt}).submit(v::get_now(nt), v::single{});

  nt |
    op::transform([&]<class NT>(NT nt){
      // auto out = v::any_single<v::any_time_executor_ref<std::exception_ptr, std::chrono::system_clock::time_point>>{v::single{}};
      // nt.submit(v::get_now(nt), std::move(out));
      // nt.submit(v::get_now(nt), v::single{});
      // nt | op::submit_at(v::get_now(nt), std::move(out));
      // nt |
      // op::submit(v::single{});
      // op::submit_at(nt | ep::get_now(), v::on_value{[](auto){}}) |
      // op::submit_at(nt | ep::get_now(), v::on_value{[](auto){}}) |
      // op::submit_after(20ms, v::single{}) ;//|
      // op::submit_after(20ms, v::on_value{[](auto){}}) |
      // op::submit_after(40ms, push(42));
      return v::get_now(nt);
    }) |
   // op::submit(v::single{});
   op::blocking_submit(v::single{});
    // op::get<decltype(v::get_now(nt))>();
    // op::get<std::chrono::system_clock::time_point>();

  auto ref = v::any_time_executor_ref<std::exception_ptr, std::chrono::system_clock::time_point>{nt};
#endif
}

void flow_single_test() {
  auto out0 = pushmi::flow_single{};
  auto out1 = pushmi::flow_single{pushmi::ignoreVF{}};
  auto out2 = pushmi::flow_single{pushmi::ignoreVF{}, pushmi::abortEF{}};
  auto out3 =
      pushmi::flow_single{
        pushmi::ignoreVF{},
        pushmi::abortEF{},
        pushmi::ignoreDF{}};
  auto out4 = pushmi::flow_single{[](auto v) { v.get(); }};
  auto out5 = pushmi::flow_single{
      pushmi::on_value{[](auto v) { v.get(); }, [](int v) {}},
      pushmi::on_error{
        [](auto e)noexcept { e.get(); },
        [](std::exception_ptr e) noexcept{}}};
  auto out6 = pushmi::flow_single{
      pushmi::on_error{
        [](auto e) noexcept{ e.get(); },
        [](std::exception_ptr e) noexcept{}}};
  auto out7 = pushmi::flow_single{
      pushmi::on_done{[]() {  }}};

  auto out8 =
      pushmi::flow_single{
        pushmi::ignoreVF{},
        pushmi::abortEF{},
        pushmi::ignoreDF{},
        pushmi::ignoreStpF{}};
  auto out9 =
      pushmi::flow_single{
        pushmi::ignoreVF{},
        pushmi::abortEF{},
        pushmi::ignoreDF{},
        pushmi::ignoreStpF{},
        pushmi::ignoreStrtF{}};

  using Out0 = decltype(out0);

  auto proxy0 = pushmi::flow_single{out0};
  auto proxy1 = pushmi::flow_single{out0, pushmi::passDVF{}};
  auto proxy2 = pushmi::flow_single{out0, pushmi::passDVF{}, pushmi::passDEF{}};
  auto proxy3 = pushmi::flow_single{
      out0, pushmi::passDVF{}, pushmi::passDEF{}, pushmi::passDDF{}};
  auto proxy4 = pushmi::flow_single{out0, [](auto d, auto v) {
    pushmi::set_value(d, v.get());
  }};
  auto proxy5 = pushmi::flow_single{
      out0,
      pushmi::on_value{[](Out0&, auto v) { v.get(); }, [](Out0&, int v) {}},
      pushmi::on_error{[](Out0&, auto e) noexcept { e.get(); },
                       [](Out0&, std::exception_ptr e) noexcept {}}};
  auto proxy6 = pushmi::flow_single{
      out0,
      pushmi::on_error{[](Out0&, auto e) noexcept { e.get(); },
                       [](Out0&, std::exception_ptr e) noexcept {}}};
  auto proxy7 = pushmi::flow_single{
      out0,
      pushmi::on_done{[](Out0&) { }}};

  auto proxy8 = pushmi::flow_single{out0,
    pushmi::passDVF{},
    pushmi::passDEF{},
    pushmi::passDDF{}};
  auto proxy9 = pushmi::flow_single{out0,
    pushmi::passDVF{},
    pushmi::passDEF{},
    pushmi::passDDF{},
    pushmi::passDStpF{}};

  auto any2 = pushmi::any_flow_single<int>(out0);
  auto any3 = pushmi::any_flow_single<int>(proxy0);
}

void flow_single_deferred_test(){
  auto in0 = pushmi::flow_single_deferred{};
  auto in1 = pushmi::flow_single_deferred{pushmi::ignoreSF{}};
  auto in3 = pushmi::flow_single_deferred{[&](auto out){
    in0.submit(pushmi::flow_single{std::move(out),
      pushmi::on_value{[](auto d, int v){ pushmi::set_value(d, v); }}
    });
  }};

  auto out0 = pushmi::flow_single{};
  auto out1 = pushmi::flow_single{out0, pushmi::on_value{[](auto d, int v){
    pushmi::set_value(d, v);
  }}};
  in3.submit(out1);

  auto any0 = pushmi::any_flow_single_deferred<int>(in0);
}
