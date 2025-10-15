#include <cassert>
#include <coroutine>
#include <iostream>

// 协程返回类型（简化的 task）
struct task {
    struct promise_type {
        task get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; } // 初始暂停，等待手动启动

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> handle;

    auto get_handle() { return handle; }
};

// 用于切换协程的 awaitable 类型
struct switch_to_awaitable {
    std::coroutine_handle<> target; // 目标协程句柄

    // 必须暂停当前协程，才能切换到目标协程
    bool await_ready() const noexcept { return false; }

    // 核心：返回目标协程句柄，让其先执行
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> current) noexcept {
        std::cout << "切换：当前协程暂停，准备执行目标协程" << std::endl;
        return target; // 返回目标句柄，优先执行目标协程
    }

    // 目标协程执行完毕后，当前协程恢复时调用（此处无逻辑）
    void await_resume() noexcept {}
};

// 辅助函数：创建切换协程的 awaitable
switch_to_awaitable switch_to(std::coroutine_handle<> target) {
    return {target};
}

// 三个示例协程
task task3() {
    std::cout << "task3：开始执行" << std::endl;
    std::cout << "task3：执行完毕，等待恢复后续协程" << std::endl;
    co_return; // 结束，进入 final_suspend 暂停
}

task task2(std::coroutine_handle<> task3_handle) {
    std::cout << "task2：开始执行" << std::endl;
    std::cout << "task2：准备切换到 task3" << std::endl;
    co_await switch_to(task3_handle); // 切换到 task3
    std::cout << "task2：从 task3 切换回来，继续执行" << std::endl;
    std::cout << "task2：执行完毕" << std::endl;
    co_return;
}

task task1(std::coroutine_handle<> task2_handle) {
    std::cout << "task1：开始执行" << std::endl;
    std::cout << "task1：准备切换到 task2" << std::endl;
    co_await switch_to(task2_handle); // 切换到 task2
    std::cout << "task1：从 task2 切换回来，继续执行" << std::endl;
    std::cout << "task1：执行完毕" << std::endl;
    co_return;
}

int main() {
    // 创建三个协程（初始处于暂停状态）
    task t3 = task3();
    task t2 = task2(t3.handle);
    task t1 = task1(t2.handle);

    std::cout << "主线程：启动 task1" << std::endl;
    t1.handle.resume(); // 启动第一个协程

    // 当 task2 执行完毕后，手动恢复 task1
    std::cout << "\n主线程：task2 已完成，恢复 task1" << std::endl;
    t1.handle.resume();

    // 当 task3 执行完毕后，手动恢复 task2（实际场景可能由事件驱动） std::cout
    //     << "\n主线程：task3 已完成，恢复 task2" << std::endl;
    t2.handle.resume();

    // 清理协程句柄
    t1.handle.destroy();
    t2.handle.destroy();
    t3.handle.destroy();
    return 0;
}
