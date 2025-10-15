#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

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

        void return_value(T &&value) { set_value(std::forward<T>(value)); }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { auto exception = std::current_exception(); }

      public:
        template<typename Self>
        T result(this Self &&self) {
            if (std::holds_alternative<std::exception_ptr>(self.result_)) [[unlikely]] {
                std::rethrow_exception(std::get<std::exception_ptr>(self.result_));
            } else if (std::holds_alternative<std::monostate>(self.result_)) [[unlikely]] {
                throw std::runtime_error("result has not been set");
            } else {
                return std::get<T>(std::forward<Self>(self).result_);
            }
        }

        using ResultT = std::variant<std::monostate, T, std::exception_ptr>;
        ResultT result_;

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
                    set_exception(std::current_exception());
                }
                return;
            }
        }
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
