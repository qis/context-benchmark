#pragma once
#include <benchmark/benchmark.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <experimental/coroutine>
#include <memory>
#include <mutex>
#include <thread>
#include <cassert>

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

class thread_local_storage {
public:
#ifdef WIN32
  using handle_type = DWORD;
#else
  using handle_type = pthread_key_t;
#endif

  thread_local_storage() noexcept {
#ifdef WIN32
    handle_ = ::TlsAlloc();
    assert(handle_);
#else
    [[maybe_unused]] const auto rc = ::pthread_key_create(&handle_, nullptr);
    assert(rc == 0);
#endif
  }

  ~thread_local_storage() {
#ifdef WIN32
    [[maybe_unused]] const auto rc = ::TlsFree(handle_);
    assert(rc);
#else
    [[maybe_unused]] const auto rc = ::pthread_key_delete(handle_);
    assert(rc == 0);
#endif
  }

  void set(void* value) noexcept {
#ifdef WIN32
    [[maybe_unused]] const auto rc = ::TlsSetValue(handle_, value);
    assert(rc);
#else
    [[maybe_unused]] const auto rc = ::pthread_setspecific(handle_, value);
    assert(rc == 0);
#endif
  }

  void* get() noexcept {
#ifdef WIN32
    return ::TlsGetValue(handle_);
#else
    return ::pthread_getspecific(handle_);
#endif
  }

  const void* get() const noexcept {
#ifdef WIN32
    return ::TlsGetValue(handle_);
#else
    return ::pthread_getspecific(handle_);
#endif
  }

private:
  handle_type handle_ = 0;
};

class basic_event {
public:
  basic_event() noexcept = default;

  // clang-format off
#ifdef __INTELLISENSE__
  basic_event(basic_event&& other) {}
  basic_event& operator=(basic_event&& other) { return *this; }
  basic_event(const basic_event& other) {}
  basic_event& operator=(const basic_event& other) { return *this; }
#else
  basic_event(basic_event&& other) = delete;
  basic_event& operator=(basic_event&& other) = delete;
  basic_event(const basic_event& other) = delete;
  basic_event& operator=(const basic_event& other) = delete;
#endif
  // clang-format on

  virtual ~basic_event() = default;

  void resume() noexcept {
    awaiter_.resume();
  }

protected:
  std::experimental::coroutine_handle<> awaiter_;
};

class basic_context {
public:
  virtual ~basic_context() = default;

  static bool concurrent_run_not_supported() noexcept {
    return false;
  }

  bool is_current() const noexcept {
    return index_.get() ? true : false;
  }

protected:
  std::unique_ptr<basic_context, void (*)(basic_context*) noexcept> enable_thread() noexcept {
    index_.set(this);
    const auto deleter = [](basic_context * context) noexcept {
      context->index_.set(nullptr);
    };
    return { this, deleter };
  }

private:
  thread_local_storage index_;
};

template <typename Context, typename Event>
class schedule final : public Event {
public:
  schedule(Context& context, bool post = true) noexcept : context_(context), ready_(!post && context.is_current()) {}

  constexpr bool await_ready() const noexcept {
    return ready_;
  }

  void await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
    Event::awaiter_ = awaiter;
    context_.post(this);
  }

  constexpr void await_resume() const noexcept {}

private:
  Context& context_;
  const bool ready_ = true;
};

struct task {
  struct promise_type {
    task get_return_object() noexcept {
      return {};
    }

    constexpr std::experimental::suspend_never initial_suspend() noexcept {
      return {};
    }

    constexpr std::experimental::suspend_never final_suspend() noexcept {
      return {};
    }

    constexpr void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
};

template <typename Context, typename Event>
task coro(Context& c0, Context& c1, benchmark::State& state) noexcept {
  bool first = true;
  for (auto _ : state) {
    if (first) {
      co_await schedule<Context, Event>{ c0, false };
    } else {
      co_await schedule<Context, Event>{ c1, false };
    }
    first = !first;
  }
  c0.stop();
  c1.stop();
  co_return;
}

template <typename Context, typename Event>
task coro(Context& ctx, bool post, benchmark::State& state) noexcept {
  for (auto _ : state) {
    co_await schedule<Context, Event>{ ctx, post };
  }
  ctx.stop();
  co_return;
}

class startup_helper {
public:
  startup_helper(int threads) noexcept : threads_(threads) {}

  void notify() noexcept {
    threads_.fetch_sub(1, std::memory_order_acq_rel);
    cv_.notify_one();
  }

  void wait() noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return threads_.load(std::memory_order_acquire) <= 0; });
  }

  std::mutex mutex_;
  std::atomic_int threads_ = 0;
  std::condition_variable cv_;
};

template <typename Context, typename Event>
void run(benchmark::State& state) noexcept {
  Context c0{ 1 };
  Context c1{ 1 };
  startup_helper sh{ 2 };
  auto t0 = std::thread([&]() {
    sh.notify();
    c0.run();
  });
  auto t1 = std::thread([&]() {
    sh.notify();
    c1.run();
  });
  sh.wait();
  coro<Context, Event>(c0, c1, state);
  t0.join();
  t1.join();
}

template <typename Context, typename Event>
void run_single(benchmark::State& state) noexcept {
  Context ctx{ 1 };
  coro<Context, Event>(ctx, false, state);
  ctx.run();
}

template <typename Context, typename Event>
void run_single_post(benchmark::State& state) noexcept {
  Context ctx{ 1 };
  coro<Context, Event>(ctx, true, state);
  ctx.run();
}

template <typename Context, typename Event>
void run_random(benchmark::State& state) noexcept {
  if (Context::concurrent_run_not_supported()) {
    state.SkipWithError("concurrent run not supported");
    return;
  }
  Context ctx{ 2 };
  startup_helper sh{ 2 };
  auto t0 = std::thread([&]() {
    sh.notify();
    ctx.run();
  });
  auto t1 = std::thread([&]() {
    sh.notify();
    ctx.run();
  });
  sh.wait();
  coro<Context, Event>(ctx, false, state);
  t0.join();
  t1.join();
}

template <typename Context, typename Event>
static void run_random_post(benchmark::State& state) noexcept {
  if (Context::concurrent_run_not_supported()) {
    state.SkipWithError("concurrent run not supported");
    return;
  }
  Context ctx{ 2 };
  startup_helper sh{ 2 };
  auto t0 = std::thread([&]() {
    sh.notify();
    ctx.run();
  });
  auto t1 = std::thread([&]() {
    sh.notify();
    ctx.run();
  });
  sh.wait();
  coro<Context, Event>(ctx, true, state);
  t0.join();
  t1.join();
}

#define CREATE_BENCHMARKS(name, context, event)                      \
  static void name(benchmark::State& state) noexcept {               \
    run<context, event>(state);                                      \
  }                                                                  \
  BENCHMARK(name)->Threads(1);                                       \
  static void name##_single(benchmark::State& state) noexcept {      \
    run_single<context, event>(state);                               \
  }                                                                  \
  BENCHMARK(name##_single)->Threads(1);                              \
  static void name##_single_post(benchmark::State& state) noexcept { \
    run_single_post<context, event>(state);                          \
  }                                                                  \
  BENCHMARK(name##_single_post)->Threads(1);                         \
  static void name##_random(benchmark::State& state) noexcept {      \
    run_random<context, event>(state);                               \
  }                                                                  \
  BENCHMARK(name##_random)->Threads(1);                              \
  static void name##_random_post(benchmark::State& state) noexcept { \
    run_random_post<context, event>(state);                          \
  }                                                                  \
  BENCHMARK(name##_random_post)->Threads(1);
