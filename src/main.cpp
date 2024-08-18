#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>

#include "debug.hpp"
#include "task.hpp"

namespace co_async {

auto checkError(auto res) {
  if (res == -1) [[unlikely]] {
    throw std::system_error(errno, std::system_category());
  }
  return res;
}

struct EpollFilePromise : Promise<void> {
  auto get_return_object() {
    return std::coroutine_handle<EpollFilePromise>::from_promise(*this);
  }

  EpollFilePromise &operator=(EpollFilePromise &&) = delete;

  int mFileNo;
  uint32_t mEvents;
};

struct EpollLoop {
  void addListener(EpollFilePromise &promise) {
    struct epoll_event event;
    event.events = promise.mEvents;
    event.data.ptr = &promise;
    checkError(epoll_ctl(mEpoll, EPOLL_CTL_ADD, promise.mFileNo, &event));
  }

  void tryRun() {
    struct epoll_event ebuf[10];
    int res = checkError(epoll_wait(mEpoll, ebuf, 10, -1));
    for (int i = 0; i < res; i++) {
      auto &event = ebuf[i];
      auto &promise = *(EpollFilePromise *)event.data.ptr;
      checkError(epoll_ctl(mEpoll, EPOLL_CTL_DEL, promise.mFileNo, NULL));
      std::coroutine_handle<EpollFilePromise>::from_promise(promise).resume();
    }
  }

  EpollLoop &operator=(EpollLoop &&) = delete;
  ~EpollLoop() { close(mEpoll); }

  int mEpoll = checkError(epoll_create1(0));
};

struct EpollFileAwaiter {
  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<EpollFilePromise> coroutine) const {
    debug(), "监听文件描述符", mFileNo;
    auto &promise = coroutine.promise();
    debug(), "ID为", promise.id, "的协程被挂起";
    id = promise.id;
    promise.mFileNo = mFileNo;
    promise.mEvents = mEvents;
    loop.addListener(promise);
  }

  void await_resume() const noexcept {
    debug(), "ID为", id, "的协程恢复执行";
  }

  EpollLoop &loop;
  int mFileNo;
  uint32_t mEvents;
  int mutable id{};
};

inline Task<void, EpollFilePromise> wait_file(EpollLoop &loop, int fileNo,
                                              uint32_t events) {
  co_await EpollFileAwaiter(loop, fileNo, events, -1);
}

}  // namespace co_async

co_async::EpollLoop loop;

co_async::Task<std::string> reader() {
  co_await wait_file(loop, 0, EPOLLIN);
  debug(), "有数据可读";
  std::string s;
  while (true) {
    char c;
    ssize_t len = read(0, &c, 1);
    if (len == -1) {
      if (errno != EWOULDBLOCK) [[unlikely]] {
        throw std::system_error(errno, std::system_category());
      }
      break;
    }
    s.push_back(c);
  }
  co_return s;
}

co_async::Task<void> async_main() {
  while (true) {
    auto s = co_await reader();
    debug(), "读到了", s;
    if (s == "quit\n") break;
  }
}

int main() {
  int attr = 1;
  ioctl(0, FIONBIO, &attr);

  auto t = async_main();
  debug(), "开始执行";
  t.mCoroutine.resume();
  debug(), "Come back";
  while (!t.mCoroutine.done()) {
    loop.tryRun();
  }

  return 0;
}