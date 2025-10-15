#include <condition_variable>
#include <mutex>

namespace myx_coroutine {

struct SyncWaitEvent {
    std::condition_variable cv_;
    std::mutex mtx_;
    bool is_ready_{false};

    void wait() {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [this]() { return is_ready_; });
    }

    void notify() {
        std::unique_lock lock(mtx_);
        is_ready_ = true;
        cv_.notify_one();
    }

    void reset() {
        std::unique_lock lock(mtx_);
        is_ready_ = false;
    }
};

} // namespace myx_coroutine
