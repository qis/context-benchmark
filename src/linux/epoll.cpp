#include <common.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace {

class context : public basic_context {
public:
  context(int = 1) noexcept : handle_(::epoll_create1(0)), events_(::eventfd(0, EFD_NONBLOCK)) {
    assert(handle_ != -1);
    assert(events_ != -1);
    epoll_event nev = { EPOLLONESHOT, {} };
    [[maybe_unused]] const auto rv = ::epoll_ctl(handle_, EPOLL_CTL_ADD, events_, &nev);
    assert(rv == 0);
  }

  ~context() {
    ::close(events_);
    ::close(handle_);
  }

  static constexpr bool concurrent_run_not_supported() noexcept {
    return true;  // deadlocks on epoll_wait
  }

  void run() {
    const auto et = enable_thread();
    std::array<epoll_event, 128> events;
    const auto events_data = events.data();
    const auto events_size = static_cast<int>(events.size());
    while (true) {
      const auto count = ::epoll_wait(handle_, events_data, events_size, -1);
      if (count < 0 && errno != EINTR) {
        break;
      }
      for (auto i = 0; i < count; i++) {
        if (const auto ev = reinterpret_cast<basic_event*>(events[static_cast<std::size_t>(i)].data.ptr)) {
          ev->resume();
        } else {
          return;
        }
      }
    }
  }

  void stop() noexcept {
    epoll_event nev{ EPOLLOUT | EPOLLONESHOT, {} };
    [[maybe_unused]] const auto rv = ::epoll_ctl(handle_, EPOLL_CTL_MOD, events_, &nev);
    assert(rv == 0);
  }

  void post(basic_event* ev) noexcept {
    // clang-format off
    epoll_event nev { EPOLLOUT | EPOLLONESHOT, { ev } };
    // clang-format on
    [[maybe_unused]] const auto rv = ::epoll_ctl(handle_, EPOLL_CTL_MOD, events_, &nev);
    assert(rv == 0);
  }

private:
  int handle_ = -1;
  int events_ = -1;
};

CREATE_BENCHMARKS(epoll, context, basic_event);

}  // namespace
