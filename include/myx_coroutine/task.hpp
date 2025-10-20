#pragma once

#include <asio/awaitable.hpp>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <utility>
#include <variant>

namespace myx_coroutine {
template<typename T = void>
class Task;

namespace detail {
    template<typename T>
    struct TaskPromise;

    template<typename T = void>
    struct final_awaiter {
        constexpr bool await_ready() noexcept { return false; }

        constexpr void await_resume() const noexcept {}

        std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise<T>> handle) {
            return handle.promise().get_continuation();
        }
    };

    template<typename T>
    struct TaskPromiseBase {
        std::suspend_always initial_suspend() { return {}; }

        final_awaiter<T> final_suspend() noexcept { return {}; }

        void set_continuation(std::coroutine_handle<> continuation) noexcept {
            continuation_ = continuation;
        }

        std::coroutine_handle<> get_continuation() const noexcept { return continuation_; }

        std::coroutine_handle<> continuation_{std::noop_coroutine()};
    };

    template<typename T = void>
    class TaskPromise final : public TaskPromiseBase<T> {
      public:
        Task<T> get_return_object();

        void return_value(T &&value) { set_value(std::forward<T>(value)); }

        void unhandled_exception() { set_exception(std::current_exception()); }

      public:
        template<typename Self>
        T get_result(this Self &&self) {
            if (std::holds_alternative<std::exception_ptr>(self.result_)) [[unlikely]] {
                std::rethrow_exception(std::get<std::exception_ptr>(self.result_));
            } else if (std::holds_alternative<std::monostate>(self.result_)) [[unlikely]] {
                throw std::runtime_error("result has not been set");
            } else {
                return std::get<T>(std::forward<Self>(self).result_);
            }
        }

      private:
        void set_exception(std::exception_ptr exception) {
            ensure_result_is_not_set();
            result_.template emplace<std::exception_ptr>(exception);
        }

        void set_value(T &&value) {
            ensure_result_is_not_set();
            result_.template emplace<T>(std::forward<T>(value));
        }

        void ensure_result_is_not_set() {
            if (!std::holds_alternative<std::monostate>(result_)) {
                try {
                    throw std::logic_error("result has already been set");
                } catch (...) {
                    result_.template emplace<std::exception_ptr>(std::current_exception());
                }
            }
        }

        using ResultT = std::variant<std::monostate, T, std::exception_ptr>;
        ResultT result_;
    };

    template<>
    class TaskPromise<void> final : public TaskPromiseBase<void> {
      public:
        Task<> get_return_object();

        void return_void() {}

        void unhandled_exception() { set_exception(std::current_exception()); }

      public:
        template<typename Self>
        void get_result(this Self &&self) {
            if (self.result_ != nullptr) {
                std::rethrow_exception(self.result_);
            }
        }

      private:
        void set_exception(std::exception_ptr exception) {
            ensure_result_is_not_set();
            result_ = exception;
        }

        void ensure_result_is_not_set() {
            if (result_ != nullptr) {
                try {
                    throw std::logic_error("result has already been set");
                } catch (...) {
                    result_ = std::current_exception();
                }
            }
        }

        using ResultT = std::exception_ptr;
        ResultT result_;
    };
} // namespace detail

template<typename T>
class Task {
  public:
    using promise_type = detail::TaskPromise<T>;
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

    bool is_ready() { return handle_ == nullptr || handle_.done(); }

    auto operator co_await() const & {
        struct awaitable {
            handle_type handle_;

            bool await_ready() { return false; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) {
                handle_.promise().set_continuation(continuation);
                return handle_;
            }

            T await_resume() { return handle_.promise().get_result(); }
        };

        return awaitable{handle_};
    }

  private:
    handle_type handle_;
};

namespace detail {
    template<typename T>
    inline Task<T> TaskPromise<T>::get_return_object() {
        return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
    }

    inline Task<> TaskPromise<void>::get_return_object() {
        return Task<>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
    }
} // namespace detail

} // namespace myx_coroutine
