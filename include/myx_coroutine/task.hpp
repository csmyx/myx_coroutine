#pragma once

#include <coroutine>

namespace myx_coroutine {
template<typename T>
class Task;

template<typename T>
class TaskPromise {
    Task<T> get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
    }
};

template<typename T = void>
class Task {
  public:
    using promise_type = TaskPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

  private:
    handle_type handle_;

  public:
    Task(const Task &) = delete;
    Task(Task &&) = delete;
    Task &operator=(const Task &) = delete;
    Task &operator=(Task &&) = delete;

    explicit Task<T>(handle_type handle) noexcept : handle_(handle) {}
};
} // namespace myx_coroutine
