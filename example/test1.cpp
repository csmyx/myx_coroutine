#include <iostream>
#include <myx_coroutine/lib.h>

int main() {
    // 测试生成器
    std::cout << "=== Generator Test ===\n";
    for (int i : myx_coroutine::count_to(5)) {
        std::cout << i << " ";
    }
    std::cout << "\n\n";

    // // 测试异步任务（手动驱动协程，实际由调度器自动处理）
    // std::cout << "=== Task Test ===\n";
    // auto work = async_work();
    // while (!work.handle.done()) {
    //     work.handle.resume();
    // }
    // std::cout << "\n";

    // // 测试无返回值协程
    // std::cout << "=== Void Coroutine Test ===\n";
    // auto hello = print_hello();
    // while (!hello.handle.done()) {
    //     hello.handle.resume();
    // }

    return 0;
}
