#include <common.h>

namespace {

class event : public basic_event {
public:
  std::atomic<event*> next = nullptr;
};

class context : public basic_context {
public:
  context(int = 1) noexcept {}

  void run() noexcept {
    const auto et = enable_thread();
    int compare = 0;
    while (true) {
      auto head = head_.exchange(nullptr, std::memory_order_acquire);
      while (!head) {
        if (stop_.load(std::memory_order_acquire)) {
          return;
        }
        if (trigger_.load(std::memory_order_acquire) == compare) {
          if (!::WaitOnAddress(&trigger_, &compare, sizeof(trigger_), INFINITE)) {
            if (::GetLastError() != ERROR_TIMEOUT) {
              return;
            }
          }
        }
        trigger_.store(compare, std::memory_order_release);
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
    trigger_.store(1, std::memory_order_release);
    ::WakeByAddressSingle(&trigger_);
  }

  void post(event* ev) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      ev->next.store(head, std::memory_order_relaxed);
    } while (!head_.compare_exchange_weak(head, ev, std::memory_order_release, std::memory_order_acquire));
    trigger_.store(1, std::memory_order_release);
    ::WakeByAddressSingle(&trigger_);
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
  std::atomic<int> trigger_ = 0;
};

CREATE_BENCHMARKS(wake, context, event);

}  // namespace
