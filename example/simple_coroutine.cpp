#include <chrono>
#include <coroutine>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>

// 前置声明
template<typename T>
struct task;

// 调度器：管理协程的执行
class scheduler {
  public:
    // 将任务提交到调度器
    void post(std::function<void()> func) {
        std::lock_guard<std::mutex> lock(mtx_);
        tasks_.push(std::move(func));
    }

    // 运行调度器，处理所有任务
    void run() {
        while (true) {
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (tasks_.empty()) {
                    break;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    static scheduler &get_default() {
        static scheduler sched;
        return sched;
    }

  private:
    std::mutex mtx_;
    std::queue<std::function<void()>> tasks_;
};

// 等待器基类：用于co_await
template<typename T>
struct awaiter_base {
    // 协程是否需要挂起
    bool await_ready() const noexcept { return false; }

    // 挂起时的操作：将协程加入调度器
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        scheduler::get_default().post([handle]() { handle.resume(); });
    }

    // 恢复时的操作
    T await_resume() noexcept {
        if constexpr (!std::is_void_v<T>) {
            return {};
        }
        return;
    }
};

// 空等待器：用于立即挂起/恢复
struct suspend_always : awaiter_base<void> {};

struct suspend_never : awaiter_base<void> {
    bool await_ready() const noexcept { return true; }
};

// 定时器等待器：等待指定时间
struct timer_awaiter : awaiter_base<void> {
    std::chrono::milliseconds duration_;

    explicit timer_awaiter(std::chrono::milliseconds duration) : duration_(duration) {}

    // 重写挂起逻辑：使用线程延迟后恢复
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        std::thread([handle, duration = duration_]() {
            std::this_thread::sleep_for(duration);
            scheduler::get_default().post([handle]() { handle.resume(); });
        }).detach();
    }
};

// 定时器函数：创建定时器等待器
timer_awaiter delay(std::chrono::milliseconds duration) {
    return timer_awaiter{duration};
}

template<typename T>
struct task_promise;

// 任务类型：协程的返回类型
template<typename T>
struct task {
    using promise_type = task_promise<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    friend promise_type;

    auto get_handle() { return handle_; }

    task(handle_type handle) : handle_(handle) {}

    task(task &&other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

    ~task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // 禁止复制
    task(const task &) = delete;
    task &operator=(const task &) = delete;

    // 实现co_await支持
    auto operator co_await() noexcept {
        struct awaiter : suspend_always {
            task<T> &task_;

            explicit awaiter(task<T> &task) : task_(task) {}

            void await_suspend(std::coroutine_handle<> cont) noexcept {
                task_.handle_.promise().continuation_ = [cont]() {
                    cont.resume();
                };
            }

            T await_resume() {
                // 检查是否有异常
                if (task_.handle_.promise().exception_) {
                    std::rethrow_exception(task_.handle_.promise().exception_);
                }
                return std::move(*task_.handle_.promise().result_);
            }
        };

        return awaiter{*this};
    }

  private:
    handle_type handle_;
};

// task的promise类型：协程内部状态管理
template<typename T>
struct task_promise {
    // 存储协程返回值
    std::optional<T> result_;
    // 异常存储
    std::exception_ptr exception_;
    // 等待此任务完成的回调
    std::function<void()> continuation_;

    // 创建task对象
    task<T> get_return_object() { return task<T>{task<T>::handle_type::from_promise(*this)}; }

    // 初始挂起：立即执行
    suspend_never initial_suspend() noexcept { return {}; }

    // 最终挂起：通知等待者
    auto final_suspend() noexcept {
        struct awaiter : suspend_always {
            task_promise *promise_;

            explicit awaiter(task_promise *promise) : promise_(promise) {}

            void await_suspend(std::coroutine_handle<>) noexcept {
                if (promise_->continuation_) {
                    scheduler::get_default().post(promise_->continuation_);
                }
            }
        };

        return awaiter{this};
    }

    // 处理返回值
    void return_value(T value) noexcept { result_ = std::move(value); }

    // 处理异常
    void unhandled_exception() noexcept { exception_ = std::current_exception(); }
};

// 特化void类型的task
template<>
struct task<void> {
    struct promise_type {
        std::exception_ptr exception_;
        std::function<void()> continuation_;

        task<void> get_return_object() { return task<void>{handle_type::from_promise(*this)}; }

        suspend_never initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept {
            struct awaiter : suspend_always {
                promise_type *promise_;

                explicit awaiter(promise_type *promise) : promise_(promise) {}

                void await_suspend(std::coroutine_handle<>) noexcept {
                    if (promise_->continuation_) {
                        scheduler::get_default().post(promise_->continuation_);
                    }
                }
            };

            return awaiter{this};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept { exception_ = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    task(handle_type handle) : handle_(handle) {}

    task(task &&other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

    ~task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    task(const task &) = delete;
    task &operator=(const task &) = delete;

    auto operator co_await() noexcept {
        struct awaiter : suspend_always {
            task<void> &task_;

            explicit awaiter(task<void> &task) : task_(task) {}

            void await_suspend(std::coroutine_handle<> cont) noexcept {
                task_.handle_.promise().continuation_ = [cont]() {
                    cont.resume();
                };
            }

            void await_resume() {
                if (task_.handle_.promise().exception_) {
                    std::rethrow_exception(task_.handle_.promise().exception_);
                }
            }
        };

        return awaiter{*this};
    }

    auto get_handle() { return handle_; }

  private:
    handle_type handle_;
};

// 启动协程的函数
#include <concepts>
#include <type_traits>

// 前向声明task模板
template<typename T>
class task;

// 辅助模板：检查类型是否为task的特化版本
template<typename T>
struct is_task : std::false_type {};

template<typename T>
struct is_task<task<T>> : std::true_type {};

// 定义concept：可调用对象调用后返回task<T>
template<typename Func>
concept returns_task = requires(Func &&func) {
    // 约束1：Func必须可调用（无参数，或通过捕获/绑定带参数）
    {
        std::forward<Func>(func)()
    } -> std::convertible_to<
        // 约束2：返回值必须是task的特化版本（任意T）
        typename std::enable_if_t<
            is_task<decltype(std::forward<Func>(func)())>::value,
            decltype(std::forward<Func>(func)())>>;
};

// 正确的 co_spawn：接受可调用对象，调用后返回 task<T>
// 使用concept约束的co_spawn
template<returns_task Func> // 仅接受返回task<T>的可调用对象
auto co_spawn(Func &&func) -> decltype(auto) {
    auto task = std::forward<Func>(func)(); // 调用后得到task<T>
    auto handle = task.get_handle();

    scheduler::get_default().post([handle]() mutable {
        if (!handle.done()) {
            handle.resume();
        }
    });

    return task;
}

// 示例1：简单的异步任务
task<int> async_add(int a, int b) {
    std::cout << "async_add: 开始计算...\n";
    co_await delay(std::chrono::milliseconds(500)); // 模拟耗时操作
    co_return a + b;
}

// 示例2：嵌套协程
task<void> nested_coroutine() {
    std::cout << "nested_coroutine: 开始\n";
    co_await delay(std::chrono::milliseconds(300));
    std::cout << "nested_coroutine: 等待后继续\n";

    // 调用另一个协程
    int result = co_await async_add(2, 3);
    std::cout << "nested_coroutine: 2 + 3 = " << result << "\n";
}

// 示例3：带异常的协程
task<void> coroutine_with_exception() {
    std::cout << "coroutine_with_exception: 开始\n";
    co_await delay(std::chrono::milliseconds(200));

    // 抛出异常
    throw std::runtime_error("这是一个测试异常");
}

int main() {
    std::cout << "主程序: 启动\n";

    // 启动第一个协程
    auto task1 = co_spawn(nested_coroutine);

    // 启动第二个协程
    auto task2 = co_spawn([]() -> task<void> {
        std::cout << "匿名协程: 开始\n";
        co_await delay(std::chrono::milliseconds(100));
        std::cout << "匿名协程: 完成\n";
    });

    // 启动带异常的协程
    auto task3 = co_spawn(coroutine_with_exception);

    std::cout << "主程序: 运行调度器\n";

    scheduler::get_default().run(); // 运行调度器处理所有任务

    std::cout << "主程序: 结束\n";
    return 0;
}
