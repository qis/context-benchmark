#include <windows.h>
#include <winsock2.h>
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

class event : public OVERLAPPED {
public:
  event() noexcept : OVERLAPPED({}) {}
  virtual ~event() = default;

  void resume() noexcept {
    awaiter_.resume();
  }

protected:
  std::experimental::coroutine_handle<> awaiter_;
};

class context {
public:
  struct wsa {
    wsa() noexcept {
      WSADATA wsadata = {};
      WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
  };

  context() noexcept : handle_(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1)) {}

  ~context() {
    CloseHandle(handle_);
  }

  void run() noexcept {
    bool running = true;
    std::array<OVERLAPPED_ENTRY, 128> events;
    while (running) {
      ULONG count = 0;
      if (!GetQueuedCompletionStatusEx(handle_, events.data(), static_cast<ULONG>(events.size()), &count, INFINITE, FALSE)) {
        break;
      }
      for (ULONG i = 0; i < count; i++) {
        if (const auto ev = static_cast<event*>(events[i].lpOverlapped)) {
          ev->resume();
          continue;
        }
        running = false;
      }
    }
  }

  void stop() noexcept {
    PostQueuedCompletionStatus(handle_, 0, 0, nullptr);
  }

  void post(event* ev) noexcept {
    PostQueuedCompletionStatus(handle_, 0, 0, ev);
  }

private:
  static const wsa wsa;
  HANDLE handle_ = nullptr;
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

static void iocp(benchmark::State& state) noexcept {
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

BENCHMARK(iocp)->Threads(1);

}  // namespace
