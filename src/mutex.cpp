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
    std::unique_lock<std::mutex> lock{ mutex_ };
    lock.unlock();
    while (true) {
      lock.lock();
      auto head = head_.exchange(nullptr, std::memory_order_acquire);
      while (!head) {
        if (stop_.load(std::memory_order_acquire)) {
          lock.unlock();
          return;
        }
        cv_.wait(lock, []() { return true; });
        head = head_.exchange(nullptr, std::memory_order_acquire);
      }
      lock.unlock();
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
    cv_.notify_all();
  }

  void post(event* ev) noexcept {
    auto head = head_.load(std::memory_order_acquire);
    do {
      ev->next.store(head, std::memory_order_relaxed);
    } while (!head_.compare_exchange_weak(head, ev, std::memory_order_release, std::memory_order_acquire));
    cv_.notify_one();
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
  std::condition_variable cv_;
  std::mutex mutex_;
};

CREATE_BENCHMARKS(mutex, context, event);

}  // namespace
