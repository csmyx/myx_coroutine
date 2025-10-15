#include "myx_coroutine/thread_pool.hpp"
#include <spdlog/spdlog.h>

template<typename T>
constexpr auto get_value(const T &val) {
    if constexpr (std::is_same_v<T, std::atomic<int>>) {
        return val.load();
    } else {
        return val;
    }
}

int main() {
    spdlog::set_pattern(
        "[%Y-%m-%d %H:%M:%S] " // time
        "[%^%-5l%$] "          // level
        "[thread %t] "         // thread
        "[%s:%#]: "            // file:line
        "%v"                   // content
    );
    SPDLOG_INFO("Hello, spdlog!");
    SPDLOG_ERROR("An error occurred");
    std::atomic<int> cnt = 0;
    // int cnt = 0;
    {
        coroutine::ThreadPool thread_pool;
        auto f = [&cnt]() {
            for (int i = 0; i < 1000; ++i) {
                cnt++;
            }
        };
        for (int i = 0; i < 10; ++i) {
            thread_pool.push_task(f);
        }
    }
    SPDLOG_INFO("cnt: {}", get_value(cnt));
}
