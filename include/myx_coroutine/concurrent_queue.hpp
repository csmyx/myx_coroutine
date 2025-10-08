#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace coroutine {
template<typename T>
class ConcurrentQueue {
  public:
    void push(T &&x) {
        std::unique_lock lock(mtx_);
        queue_.push(std::move(x));
        cv_.notify_one();
    }

    bool pop(T &x) {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [this]() { return !queue_.empty() || stop_; });
        if (queue_.empty()) {
            return false;
        }
        x = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool try_pop(T &x) {
        std::unique_lock lock(mtx_);
        if (queue_.empty()) {
            return false;
        }
        x = std::move(queue_.front());
        queue_.pop();
    }

    std::size_t size() {
        std::unique_lock lock(mtx_);
        return queue_.size();
    }

    bool empty() {
        std::unique_lock lock(mtx_);
        return queue_.empty();
    }

    void stop() {
        std::unique_lock lock(mtx_);
        stop_ = true;
        cv_.notify_all();
    }

    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool stop_ = false;
};
} // namespace coroutine
