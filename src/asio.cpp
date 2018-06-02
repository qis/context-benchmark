#include <common.h>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <boost/asio/io_context.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace {

class context : public basic_context {
public:
  context(int hint = 1) noexcept : context_(hint), work_(std::make_unique<boost::asio::io_context::work>(context_)) {}

  void run() noexcept {
    context_.run();
  }

  void stop() noexcept {
    work_.reset();
  }

  void post(basic_event* ev) noexcept {
    context_.post([ev]() {
      ev->resume();
    });
  }

private:
  boost::asio::io_context context_;
  std::unique_ptr<boost::asio::io_context::work> work_;
};

CREATE_BENCHMARKS(asio, context, basic_event);

}  // namespace
