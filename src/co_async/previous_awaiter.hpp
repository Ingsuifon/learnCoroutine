#pragma once

#include <coroutine>

namespace co_async {

struct PreviousAwaiter {
  std::coroutine_handle<> mPrevious;

  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> coroutine) const noexcept {
    if (mPrevious) {
      debug(), "ID未知的协程结束，ID未知的协程恢复执行";
      return mPrevious;
    }
    return std::noop_coroutine();
  }

  void await_resume() const noexcept { debug(), "ID未知的协程恢复执行"; }
};

}  // namespace co_async