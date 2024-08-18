#pragma once

#include <coroutine>
#include <exception>
#include <utility>

#include "debug.hpp"
#include "previous_awaiter.hpp"
#include "uninitialized.hpp"

namespace co_async {

int getID() {
  static int id = 0;
  return id++;
}

template <class T>
struct Promise {
  auto initial_suspend() noexcept {
    debug(), "ID为", id, "的协程初始挂起";
    return std::suspend_always();
  }

  auto final_suspend() noexcept {
    debug(), "ID为", id, "的协程最终挂起";
    return PreviousAwaiter(mPrevious);
  }

  void unhandled_exception() noexcept { mException = std::current_exception(); }

  void return_value(T &&ret) { mResult.putValue(std::move(ret)); }

  void return_value(T const &ret) { mResult.putValue(ret); }

  T result() {
    if (mException) [[unlikely]] {
      std::rethrow_exception(mException);
    }
    return mResult.moveValue();
  }

  auto get_return_object() {
    return std::coroutine_handle<Promise>::from_promise(*this);
  }

  std::coroutine_handle<> mPrevious;
  std::exception_ptr mException{};
  Uninitialized<T> mResult;  // destructed??
  int id{};

  Promise() : id(getID()) {}
  Promise &operator=(Promise &&) = delete;
};

template <>
struct Promise<void> {
  auto initial_suspend() noexcept {
    debug(), "ID为", id, "的协程初始挂起";
    return std::suspend_always();
  }

  auto final_suspend() noexcept {
    debug(), "ID为", id, "的协程最终挂起";
    return PreviousAwaiter(mPrevious);
  }

  void unhandled_exception() noexcept { mException = std::current_exception(); }

  void return_void() noexcept {}

  void result() {
    if (mException) [[unlikely]] {
      std::rethrow_exception(mException);
    }
  }

  auto get_return_object() {
    return std::coroutine_handle<Promise>::from_promise(*this);
  }

  std::coroutine_handle<> mPrevious;
  std::exception_ptr mException{};
  int id{};

  Promise() : id(getID()) {}
  Promise &operator=(Promise &&) = delete;
};

template <class T = void, class P = Promise<T>>
struct [[nodiscard]] Task {
  using promise_type = P;

  Task(std::coroutine_handle<promise_type> coroutine = nullptr) noexcept
      : mCoroutine(coroutine) {
    debug(), "ID为", mCoroutine.promise().id, "的协程被创建";
  }

  Task(Task &&that) noexcept : mCoroutine(that.mCoroutine) {
    that.mCoroutine = nullptr;
  }

  Task &operator=(Task &&that) noexcept {
    std::swap(mCoroutine, that.mCoroutine);
  }

  ~Task() {
    if (mCoroutine) mCoroutine.destroy();
  }

  struct Awaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<> coroutine) const noexcept {
      promise_type &promise = mCoroutine.promise();
      debug(), "ID未知的协程挂起， ID为", promise.id, "的协程开始执行";
      promise.mPrevious = coroutine;
      return mCoroutine;
    }

    T await_resume() const {
      debug(), "获取ID为", mCoroutine.promise().id, "的协程的结果作为co_wait表达式的值";
      return mCoroutine.promise().result();
    }

    std::coroutine_handle<promise_type> mCoroutine;
  };

  auto operator co_await() const noexcept { return Awaiter(mCoroutine); }

  operator std::coroutine_handle<promise_type>() const noexcept {
    return mCoroutine;
  }

  std::coroutine_handle<promise_type> mCoroutine;
};

template <class Loop, class T, class P>
T run_task(Loop &loop, Task<T, P> const &t) {
  auto a = t.operator co_await();
  a.await_suspend(std::noop_coroutine()).resume();
  loop.run();
  return a.await_resume();
};

}  // namespace co_async