#include "myx_coroutine/task.hpp"
#include <print>

using myx_coroutine::Task;

Task<std::string> f() {
    co_return "world";
}

int main() {
    auto task = f();
    task.resume();
    {
        auto x = task.promise().result();
        std::println("hello {}", x);
    }
    {
        auto x = std::move(task).promise().result();
        std::println("hello {}", x);
    }
    {
        auto x = task.promise().result();
        std::println("hello {}", x);
    }
}
