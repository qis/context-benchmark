#include <common.h>
#include <sys/event.h>
#include <unistd.h>

namespace {

class context : public basic_context {
public:
  context(int = 1) noexcept : handle_(::kqueue()) {
    assert(handle_ != -1);
  }

  ~context() {
    ::close(handle_);
  }

  void run() noexcept {
    const auto et = enable_thread();
    std::array<struct kevent, 128> events;
    const auto events_data = events.data();
    const auto events_size = static_cast<int>(events.size());
    while (true) {
      const auto count = ::kevent(handle_, nullptr, 0, events_data, events_size, nullptr);
      if (count < 0 && errno != EINTR) {
        return;
      }
      for (auto i = 0; i < count; i++) {
        if (const auto ev = reinterpret_cast<basic_event*>(events[static_cast<std::size_t>(i)].udata)) {
          ev->resume();
        } else {
          return;
        }
      }
    }
  }

  void stop() noexcept {
    struct kevent nev;
    EV_SET(&nev, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    [[maybe_unused]] const auto rv = ::kevent(handle_, &nev, 1, nullptr, 0, nullptr);
    assert(rv != -1);
  }

  void post(basic_event* ev) noexcept {
    struct kevent nev;
    EV_SET(&nev, 0, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER, 0, ev);
    [[maybe_unused]] const auto rv = ::kevent(handle_, &nev, 1, nullptr, 0, nullptr);
    assert(rv != -1);
  }

private:
  int handle_ = -1;
};

CREATE_BENCHMARKS(kqueue, context, basic_event);

}  // namespace
