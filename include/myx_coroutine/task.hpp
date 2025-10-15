#pragma once

#include <coroutine>
#include <exception>
#include <utility>

namespace myx_coroutine {

template<typename T = void>
class Task {
  public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    explicit Task<T>(handle_type handle) noexcept : handle_(handle) {}
    Task(const Task &) = delete;

    // Task(Task &&task) : handle_{std::exchange(task.handle_, nullptr)} {}
    Task(Task &&task) = delete;

    Task &operator=(const Task &) = delete;
    Task &operator=(Task &&) = delete;

    ~Task() {
        if (handle_ != nullptr) {
            handle_.destroy();
        }
    }

    struct promise_type {
      public:
        Task<T> get_return_object() {
            return Task<T>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_value(T &&value) { value_ = std::forward<T>(value); }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { throw std::current_exception(); }

      public:
        T result() && { return std::move(value_); }

        T result() & { return value_; }

        T result() const & { return value_; }

        T value_;
    };

    promise_type &promise() & { return handle_.promise(); }

    const promise_type &promise() const & { return handle_.promise(); }

    promise_type &&promise() && { return std::move(handle_.promise()); }

    [[maybe_unused]]
    bool resume() {
        if (!handle_.done()) {
            handle_.resume();
            return true;
        }
        return false;
    }

  private:
    handle_type handle_;
};

} // namespace myx_coroutine
