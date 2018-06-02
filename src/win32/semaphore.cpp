#include <common.h>

namespace {

class event : public basic_event {
public:
  std::atomic<event*> next = nullptr;
};

class context : public basic_context {
public:
  context(LONG hint = 1) noexcept : handle_(::CreateSemaphore(nullptr, 0, hint, nullptr)) {
    assert(handle_);
  }

  ~context() {
    ::CloseHandle(handle_);
  }

  void run() noexcept {
    const auto et = enable_thread();
    while (true) {
      auto head = head_.exchange(nullptr, std::memory_order_acquire);
      while (!head) {
        if (stop_.load(std::memory_order_acquire)) {
          return;
        }
        if (::WaitForSingleObject(handle_, INFINITE) != WAIT_OBJECT_0) {
          return;
        }
        head = head_.exchange(nullptr, std::memory_order_acquire);
      }
      auto tail = head->next.load(std::memory_order_relaxed);
      if (!tail) {
        head->resume();
        continue;
      }
      auto next = tail->next.load(std::memory_order_relaxed);
      if (!next) {
        repost(head, head);
        tail->resume();
        continue;
      }
      for (auto last = next->next.load(std::memory_order_relaxed); last;) {
        tail = next;
        next = last;
        last = last->next.load(std::memory_order_relaxed);
      }
      repost(head, tail);
      next->resume();
    }
  }

  void stop() noexcept {
    stop_.store(true, std::memory_order_release);
    ::ReleaseSemaphore(handle_, 1, nullptr);
  }

  void post(event* ev) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      ev->next.store(head, std::memory_order_relaxed);
    } while (!head_.compare_exchange_weak(head, ev, std::memory_order_release, std::memory_order_acquire));
    ::ReleaseSemaphore(handle_, 1, nullptr);
  }

private:
  void repost(event* beg, event* end) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      end->next.store(head, std::memory_order_relaxed);
    } while (!head_.compare_exchange_weak(head, beg, std::memory_order_release, std::memory_order_acquire));
  }

  std::atomic_bool stop_ = false;
  std::atomic<event*> head_ = nullptr;
  HANDLE handle_ = nullptr;
};

CREATE_BENCHMARKS(semaphore, context, event);

}  // namespace
