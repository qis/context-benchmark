#ifdef _MSC_VER
#  pragma warning(push, 0)
#endif
#include <boost/asio/io_context.hpp>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include <benchmark/benchmark.h>
#include <experimental/coroutine>
#include <thread>

namespace {

struct task {
  struct promise_type {
    task get_return_object() noexcept { return {}; }
    constexpr std::experimental::suspend_never initial_suspend() noexcept { return {}; }
    constexpr std::experimental::suspend_never final_suspend() noexcept { return {}; }
    constexpr void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
};

class schedule {
public:
  schedule(boost::asio::io_context& context) noexcept : context_(context) {}

  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
    awaiter_ = awaiter;
    context_.post([this]() {
      awaiter_.resume();
    });
  }

  constexpr void await_resume() const noexcept {}

private:
  boost::asio::io_context& context_;
  std::experimental::coroutine_handle<> awaiter_;
};

task coro(boost::asio::io_context& c0, boost::asio::io_context& c1, benchmark::State& state) noexcept {
  boost::asio::io_context::work w0{ c0 };
  boost::asio::io_context::work w1{ c1 };
  bool first = true;
  for (auto _ : state) {
    if (first) {
      co_await schedule{ c0 };
    } else {
      co_await schedule{ c1 };
    }
    first = !first;
  }
  co_return;
}

#if 1
static void asio(benchmark::State& state) noexcept {
  boost::asio::io_context c0{ 1 };
  boost::asio::io_context c1{ 1 };
  coro(c0, c1, state);
  auto t0 = std::thread([&]() {
    c0.run();
  });
  auto t1 = std::thread([&]() {
    c1.run();
  });
  t0.join();
  t1.join();
}
BENCHMARK(asio)->Threads(1);
#endif

}  // namespace
