#include "myx_coroutine/concurrent_queue.hpp"
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

namespace coroutine {
class ThreadPool {
  public:
    struct Task {
        std::function<void()> fn_ = nullptr;
    };

    ThreadPool(std::size_t thread_count = 0) {
        thread_count_ = (thread_count == 0 ? std::thread::hardware_concurrency() : thread_count);
        thread_pool_.reserve(thread_count_);
        auto do_task = [this](std::size_t id) {
            while (true) {
                Task task;
                if (task_queue_.pop(task)) {
                    if (task.fn_) {
                        task.fn_();
                    }
                }
                if (stop_) {
                    break;
                }
            }
        };
        for (int i = 0; i < thread_count_; ++i) {
            thread_pool_.emplace_back(do_task, i);
        }
    }

    void push_task(std::function<void()> fn) { task_queue_.push(Task{fn}); }

    ~ThreadPool() {
        stop_ = true;
        task_queue_.stop();
        for (auto &thread : thread_pool_) {
            thread.join();
        }
    }

  private:
    std::size_t thread_count_;
    std::vector<std::thread> thread_pool_;
    ConcurrentQueue<Task> task_queue_;
    std::atomic<bool> stop_ = false;
};
} // namespace coroutine
