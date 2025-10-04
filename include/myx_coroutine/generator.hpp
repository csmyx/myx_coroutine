#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <myx_coroutine/util.h>
#include <type_traits>

namespace myx_coroutine {

template<class T>
class Generator;

template<class T>
struct GeneratorPromise {
    using value_type = std::remove_reference_t<T>;

    Generator<T> get_return_object() {
        return Generator<T>{std::coroutine_handle<GeneratorPromise<T>>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() noexcept { return {}; }

    std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept { exception_ = std::current_exception(); }

    void return_void() noexcept {}

    std::suspend_always yield_value(value_type value) noexcept {
        value_ = std::move(value);
        return {};
    }

    std::exception_ptr exception_;
    value_type value_;
};

template<class T = void>
class Generator {
  public:
    using promise_type = GeneratorPromise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    Generator(Generator &&) = default;
    Generator(const Generator &) = delete;
    Generator &operator=(const Generator &) = delete;
    Generator &operator=(Generator &&) = delete;

    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    explicit Generator<T>(handle_type handle) noexcept : handle_(handle) {}

    struct iterator {
        handle_type handle_;

        bool is_end() const { return !handle_ || handle_.done(); }

        T operator*() {
            MESSAGE_ASSERT(!is_end(), "operator* can only applies to un-finished iterator");
            return handle_.promise().value_;
        }

        iterator &operator++() {
            MESSAGE_ASSERT(!is_end(), "operator++ can only applies to un-finished iterator");
            handle_.resume();
            return *this;
        }

        bool operator==(iterator rhs) {
            MESSAGE_ASSERT(rhs.is_end(), "operator== can only applies to end iterator");
            return is_end();
        }
    };

    iterator begin() { return iterator{handle_}; }

    iterator end() { return iterator{nullptr}; }

  private:
    handle_type handle_ = nullptr;
};
} // namespace myx_coroutine
