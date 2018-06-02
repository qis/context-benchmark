#include <windows.h>
#include <benchmark/benchmark.h>
#include <atomic>
#include <experimental/coroutine>
#include <thread>

// TODO: This is probably wrong. Not sure if it's worth looking into.

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

private:
  friend class context;
  std::atomic<event*> next = nullptr;
};

class context {
public:
  context() noexcept : pool_(CreateThreadpool(nullptr)) {
    SetThreadpoolThreadMaximum(pool_, 1);
    SetThreadpoolThreadMinimum(pool_, 1);
    InitializeThreadpoolEnvironment(&env_);
    work_ = CreateThreadpoolWork(cb, this, &env_);
  }

  void run() noexcept {
    while (!stop_.load(std::memory_order_acquire)) {
      WaitForThreadpoolWorkCallbacks(work_, FALSE);
    }
  }

  void stop() noexcept {
    stop_ = true;
    SubmitThreadpoolWork(work_);
  }

  void post(event* ev) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      ev->next = head;
    } while (!head_.compare_exchange_weak(head, ev, std::memory_order_release, std::memory_order_acquire));
    SubmitThreadpoolWork(work_);
  }

  void post(event* beg, event* end) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      end->next = head;
    } while (!head_.compare_exchange_weak(head, beg, std::memory_order_release, std::memory_order_acquire));
    SubmitThreadpoolWork(work_);
  }

private:
  static VOID CALLBACK cb(PTP_CALLBACK_INSTANCE, PVOID parameter, PTP_WORK) {
    auto& self = *reinterpret_cast<context*>(parameter);
    // Handle empty list.
    const auto head = self.head_.exchange(nullptr, std::memory_order_acquire);
    if (!head) {
      return;
    }

    // Handle single entry.
    auto tail = head->next.load(std::memory_order_relaxed);
    if (!tail) {
      head->resume();
      return;
    }

    // Handle two entries.
    auto next = tail->next.load(std::memory_order_relaxed);
    if (!next) {
      self.post(head);
      tail->resume();
      return;
    }

    // Handle n entries.
    for (auto last = next->next.load(std::memory_order_relaxed); last; last = last->next.load(std::memory_order_relaxed)) {
      tail = next;
      next = last;
    }
    self.post(head, tail);
    next->resume();
  }

  std::atomic_bool stop_ = false;
  std::atomic<event*> head_ = nullptr;
  TP_CALLBACK_ENVIRON env_ = {};
  PTP_POOL pool_ = nullptr;
  PTP_WORK work_ = nullptr;
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

#if 1
static void threadpool(benchmark::State& state) noexcept {
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
BENCHMARK(threadpool)->Threads(1);
#endif

}  // namespace
