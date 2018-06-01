#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <benchmark/benchmark.h>
#include <array>
#include <experimental/coroutine>
#include <thread>

struct task {
  struct promise_type {
    task get_return_object() noexcept { return {}; }
    constexpr std::experimental::suspend_never initial_suspend() noexcept { return {}; }
    constexpr std::experimental::suspend_never final_suspend() noexcept { return {}; }
    constexpr void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
};

namespace epoll {

class event {
public:
  event() noexcept = default;
  virtual ~event() = default;

  void resume() noexcept {
    awaiter_.resume();
  }

protected:
  std::experimental::coroutine_handle<> awaiter_;
};

class context {
public:
  context() noexcept : handle_(epoll_create1(0)), events_(eventfd(0, EFD_NONBLOCK)) {
    static epoll_event nev = { EPOLLONESHOT, {} };
    epoll_ctl(handle_, EPOLL_CTL_ADD, events_, &nev);
  }

  ~context() {
    close(events_);
    close(handle_);
  }

  void run() noexcept {
    bool running = true;
    std::array<epoll_event, 128> events;
    while (running) {
      const auto count = epoll_wait(handle_, events.data(), static_cast<int>(events.size()), -1);
      if (count < 0 && errno != EINTR) {
        break;
      }
      for (int i = 0; i < count; i++) {
        if (const auto ev = reinterpret_cast<event*>(events[static_cast<std::size_t>(i)].data.ptr)) {
          ev->resume();
          continue;
        }
        running = false;
      }
    }
  }

  void stop() noexcept {
    static epoll_event nev{ EPOLLOUT | EPOLLONESHOT, {} };
    epoll_ctl(handle_, EPOLL_CTL_MOD, events_, &nev);
  }

  void post(event* ev) noexcept {
    struct epoll_event nev{ EPOLLOUT | EPOLLONESHOT, { ev } };
    epoll_ctl(handle_, EPOLL_CTL_MOD, events_, &nev);
  }

private:
  int handle_ = -1;
  int events_ = -1;
};

class schedule : public event {
public:
  schedule(context& context) noexcept : context_(context) {}

  constexpr bool await_ready() const noexcept { return false; }

  void await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
    awaiter_ = awaiter;
    context_.post(this);
  }

  constexpr void await_resume() const noexcept {}

private:
  context& context_;
};

task coro(context& c0, context& c1, benchmark::State& state) noexcept {
  bool first = true;
  for (auto _ : state) {
    if (first) {
      co_await schedule{ c0 };
    } else {
      co_await schedule{ c1 };
    }
    first = !first;
  }
  c0.stop();
  c1.stop();
  co_return;
}

static void epoll(benchmark::State& state) noexcept {
  context c0;
  context c1;
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

BENCHMARK(epoll)->Threads(1);

}  // namespace epoll
