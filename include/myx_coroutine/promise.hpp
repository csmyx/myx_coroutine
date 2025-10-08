#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <gmock/gmock.h>
#include <memory>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace myx_coroutine {

// Unit plays the role of a simplest type in case we couldn't
// use void directly.
//
// User shouldn't use this directly.
struct Unit {
    constexpr bool operator==(const Unit &) const { return true; }

    constexpr bool operator!=(const Unit &) const { return false; }
};

/// State shared by Promise and Future.
template<typename T>
    requires(!std::is_void_v<T>)
class FutureState {
  public:
    using ValueT = std::variant<std::monostate, T, std::exception_ptr>;

  private:
    ValueT value_;
    std::atomic<int8_t> ref_cnt_;
    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;

  public:
    ~FutureState() {}

    void increment_ref() noexcept { ref_cnt_.fetch_add(1, std::memory_order_relaxed); }

    void decrement_ref() {
        int cnt = ref_cnt_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        assert(cnt >= 0);
        if (cnt == 0) {
            delete this;
        }
    }

    void detach_promise() {
        int cnt = ref_cnt_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        // The call to has_value here in not inside a lock region, which is totally fine.
        // Because all potential concurent operations from the thread of `Future` are read only.
        if (!has_value() && cnt > 0) {
            // If value has not been set yet, and there is also a future, then throw error.
            try {
                throw std::runtime_error("Promise is broken");
            } catch (...) {
                set_exception(std::current_exception());
            }
        }
    }

    void wait() const {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [this]() { return is_ready(); });
    }

    template<typename Rep, typename Period>
    void wait_for(const std::chrono::duration<Rep, Period> &duration) const {
        std::unique_lock lock(mtx_);
        cv_.wait_for(lock, duration, [this]() { return is_ready(); });
    }

    template<class Clock, class Duration>
    void wait_until(const std::chrono::time_point<Clock, Duration> &timeout) const {
        std::unique_lock lock(mtx_);
        cv_.wait_until(lock, timeout, [this]() { return is_ready(); });
    }

    void get(ValueT &result) const {
        std::unique_lock lock(mtx_);
        assert(is_ready());
        result = value_;
    }

    void set_value(T &&value) {
        std::unique_lock lock(mtx_);
        assert(!is_ready());
        if constexpr (std::is_lvalue_reference_v<T>) {
            value_.template emplace<T>(value);
        } else {
            value_.template emplace<T>(std::move(value));
        }
        cv_.notify_one();
    }

    void set_value()
        requires(std::is_same_v<T, Unit>)
    {
        std::unique_lock lock(mtx_);
        assert(!is_ready());
        value_.template emplace<T>(Unit());
        cv_.notify_one();
    }

    void set_exception(std::exception_ptr exception) {
        std::unique_lock lock(mtx_);
        assert(!is_ready());
        value_.template emplace<std::exception_ptr>(exception);
        cv_.notify_one();
    }

    static FutureState *Init() { return new FutureState(); }

  private:
    // Set to private to Prevent constructing FutureState from stack.
    FutureState() : ref_cnt_(1) {}

    bool has_value() const noexcept { return std::holds_alternative<T>(value_); }

    bool has_exception() const noexcept {
        return std::holds_alternative<std::exception_ptr>(value_);
    }

    bool is_ready() const noexcept { return !std::holds_alternative<std::monostate>(value_); }
};

template<typename T>
class Promise;

template<typename T>
class Future {
    using value_type = std::conditional_t<std::is_void_v<T>, Unit, T>;
    FutureState<value_type> *state_;

    Future(FutureState<value_type> *state) : state_{state} {
        if (state_) {
            state_->increment_ref();
        }
    }

  public:
    friend Promise<T>;

    ~Future() {
        if (state_) {
            state_->decrement_ref();
        }
    }

    void wait() const { state_->wait(); }

    template<typename Rep, typename Period>
    void wait_for(const std::chrono::duration<Rep, Period> &duration) const {
        state_->wait_for(duration);
    }

    template<class Clock, class Duration>
    void wait_until(const std::chrono::time_point<Clock, Duration> &timeout) const {
        state_->wait_until(timeout);
    }

    T get() const {
        wait();
        typename FutureState<value_type>::ValueT result;
        state_->get(result);
        if (std::holds_alternative<std::exception_ptr>(result)) [[unlikely]] {
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        }
        if (!std::holds_alternative<value_type>(result)) [[unlikely]] {
            throw std::logic_error("get bad value");
        }
        if constexpr (!std::is_void_v<T>) {
            return std::get<value_type>(result);
        }
    }
};

template<typename T>
class Promise {
    using value_type = std::conditional_t<std::is_void_v<T>, Unit, T>;
    FutureState<value_type> *state_;
    bool has_future_ = false;

  public:
    Promise() : state_{FutureState<value_type>::Init()} {}

    Promise(Promise &&rhs) noexcept : state_(rhs.state_), has_future_(rhs.has_future_) {
        rhs.state_ = nullptr;
        rhs.has_future_ = false;
    }

    Promise &operator=(Promise &&rhs) noexcept {
        if (state_) {
            state_->detach_promise();
        }
        state_ = rhs.state_;
        has_future_ = rhs.has_future_;
        rhs.state_ = nullptr;
        rhs.has_future_ = false;
        return *this;
    }

    Promise(const Promise &rhs) = delete;
    Promise &operator=(const Promise &rhs) = delete;

    ~Promise() {
        if (state_) {
            state_->detach_promise();
        }
    }

    Future<T> get_future() {
        check_state();
        check_future();
        has_future_ = true;
        return Future<T>(state_);
    }

    template<typename Arg>
    void set_value(Arg &&value)
        requires(!std::is_void_v<T>)
    {
        check_state();
        state_->set_value(T(std::forward<Arg>(value)));
    }

    void set_value()
        requires(std::is_void_v<T>)
    {
        check_state();
        state_->set_value();
    }

    void set_exception(std::exception_ptr exception) {
        check_state();
        state_->set_exception(exception);
    }

  private:
    void check_state() const {
        if (!state_) {
            throw std::runtime_error("state is not valid");
        }
    }

    void check_future() const {
        if (has_future_) {
            throw std::runtime_error("future has already been set");
        }
    }
};
} // namespace myx_coroutine
