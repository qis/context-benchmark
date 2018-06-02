#include <common.h>
#include <winsock2.h>

namespace {

class event : public OVERLAPPED {
public:
  event() noexcept : OVERLAPPED({}) {}

  // clang-format off
#ifdef __INTELLISENSE__
  event(event&& other) {}
  event& operator=(event&& other) { return *this; }
  event(const event& other) {}
  event& operator=(const event& other) { return *this; }
#else
  event(event&& other) = delete;
  event& operator=(event&& other) = delete;
  event(const event& other) = delete;
  event& operator=(const event& other) = delete;
#endif
  // clang-format on

  virtual ~event() = default;

  void resume() noexcept {
    awaiter_.resume();
  }

protected:
  std::experimental::coroutine_handle<> awaiter_;
};

class context : public basic_context {
public:
  context(DWORD hint = 1) noexcept : handle_(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, hint)) {
    assert(handle_);
  }

  ~context() {
    ::CloseHandle(handle_);
  }

  void run() noexcept {
    const auto et = enable_thread();
    std::array<OVERLAPPED_ENTRY, 128> events;
    const auto events_data = events.data();
    const auto events_size = static_cast<ULONG>(events.size());
    while (true) {
      ULONG count = 0;
      if (!::GetQueuedCompletionStatusEx(handle_, events_data, events_size, &count, INFINITE, FALSE)) {
        [[maybe_unused]] const auto code = ::GetLastError();
        assert(code == ERROR_ABANDONED_WAIT_0 || code == ERROR_INVALID_HANDLE);
        break;
      }
      for (ULONG i = 0; i < count; i++) {
        if (const auto ev = static_cast<event*>(events[i].lpOverlapped)) {
          ev->resume();
        }
      }
    }
  }

  void stop() noexcept {
    ::CloseHandle(handle_);
  }

  void post(event* ev) noexcept {
    ::PostQueuedCompletionStatus(handle_, 0, 0, ev);
  }

private:
  struct wsa {
    wsa() noexcept {
      WSADATA wsadata = {};
      WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
  };
  static const wsa wsa;
  HANDLE handle_ = nullptr;
};

CREATE_BENCHMARKS(iocp, context, event);

}  // namespace
