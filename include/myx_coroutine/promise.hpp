#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <type_traits>
#include <variant>

namespace myx_coroutine {

/// State shared by Promise and Future.
template<typename T>
class FutureState {
    std::optional<T> value_;
    std::exception_ptr exception_;
    std::atomic<int8_t> ref_cnt_;
    std::mutex mtx_;
    std::condition_variable cv_;

  public:
    ~FutureState() {}

    void increment_ref() { ref_cnt_.fetch_add(1, std::memory_order_relaxed); }

    void decrement_ref() {
        int cnt = ref_cnt_.fetch_sub(1, std::memory_order_acq_rel);
        SPDLOG_INFO("cnt: {}", cnt);
        assert(cnt > 0);
        if (cnt == 1) {
            delete this;
        }
    }

    void wait() {
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

    void get(std::variant<std::monostate, T, std::exception_ptr> &result) {
        std::unique_lock lock(mtx_);
        assert(is_ready());
        if (has_exception()) [[unlikely]] {
            result.template emplace<std::exception_ptr>(exception_);
            return;
        }
        result.template emplace<T>(value_.value());
    }

    void set_value(T &&value) {
        std::unique_lock lock(mtx_);
        assert(!is_ready());
        if constexpr (std::is_lvalue_reference_v<T>) {
            value_ = value;
        } else {
            value_ = std::move(value);
        }
        cv_.notify_one();
    }

    void set_exception(std::exception_ptr exception) {
        std::unique_lock lock(mtx_);
        assert(!is_ready());
        exception_ = exception;
    }

    void get_exception() { return; }

    static FutureState *Init() { return new FutureState(); }

  private:
    // Set to private to Prevent constructing FutureState from stack.
    FutureState() : ref_cnt_(1) {}

    bool has_value() { return value_.has_value(); }

    bool has_exception() { return exception_ != nullptr; }

    bool is_ready() { return has_value() || has_exception(); }
};

template<typename T>
class Future {
    FutureState<T> *state_;

  public:
    Future(FutureState<T> *state) : state_{state} {
        if (state_) {
            state_->increment_ref();
        }
    }

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

    T get() {
        wait();
        std::variant<std::monostate, T, std::exception_ptr> result;
        state_->get(result);
        if (std::holds_alternative<std::exception_ptr>(result)) [[unlikely]] {
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        }
        if (!std::holds_alternative<T>(result)) [[unlikely]] {
            throw std::logic_error("get bad value");
        }
        return std::get<T>(result);
    }
};

template<typename T>
class Promise {
    FutureState<T> *state_;
    bool has_future = false;

  public:
    Promise() : state_{FutureState<T>::Init()} {}

    Promise(Promise &&rhs) {
        state_ = rhs.state_;
        rhs.state_ = nullptr;
        has_future = rhs.has_future;
    }

    Promise(const Promise &rhs) = delete;
    Promise &operator=(const Promise &rhs) = delete;
    Promise &operator=(Promise &&rhs) = delete;

    ~Promise() {
        if (state_) {
            state_->decrement_ref();
        }
    }

    Future<T> get_future() {
        assert(state_);
        assert(!has_future);
        has_future = true;
        return Future<T>(state_);
    }

    template<typename Arg>
    void set_value(Arg &&value) {
        state_->set_value(T(std::forward<Arg>(value)));
    }
};
} // namespace myx_coroutine
