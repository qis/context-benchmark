#include <sys/event.h>
#include <unistd.h>
#include <benchmark/benchmark.h>
#include <array>
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
  context() noexcept : handle_(kqueue()) {
    struct kevent nev;
    EV_SET(&nev, 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    kevent(handle_, &nev, 1, nullptr, 0, nullptr);
  }

  ~context() {
    close(handle_);
  }

  void run() noexcept {
    bool running = true;
    std::array<struct kevent, 128> events;
    while (running) {
      const auto count = ::kevent(handle_, nullptr, 0, events.data(), static_cast<int>(events.size()), nullptr);
      if (count < 0 && errno != EINTR) {
        break;
      }
      for (int i = 0; i < count; i++) {
        if (const auto ev = reinterpret_cast<event*>(events[static_cast<std::size_t>(i)].udata)) {
          ev->resume();
          continue;
        }
        running = false;
      }
    }
  }

  void stop() noexcept {
    struct kevent nev;
    EV_SET(&nev, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    kevent(handle_, &nev, 1, nullptr, 0, nullptr);
  }

  void post(event* ev) noexcept {
    struct kevent nev;
    EV_SET(&nev, 0, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER, 0, ev);
    kevent(handle_, &nev, 1, nullptr, 0, nullptr);
  }

private:
  int handle_ = -1;
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

static void kqueue(benchmark::State& state) noexcept {
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

BENCHMARK(kqueue)->Threads(1);

}  // namespace
