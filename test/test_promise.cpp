#include "myx_coroutine/promise.hpp"
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace myx_coroutine {
struct PromiseTest : public testing::Test {
  protected:
    void SetUp() override {
        spdlog::set_pattern(
            "[%Y-%m-%d %H:%M:%S] " // time
            "[%^%-5l%$] "          // level
            "[thread %t] "         // thread
            "[%s:%#]: "            // file:line
            "%v"                   // content
        );
        SPDLOG_INFO("Hello, spdlog!");
        SPDLOG_ERROR("An error occurred");
    }

    void TearDown() override {}
};

using namespace std::chrono_literals;

TEST_F(PromiseTest, simpleTest) {
    Promise<int> p;
    auto f = p.get_future();
    SPDLOG_INFO("1");
    int x = 100;
    std::thread t(
        [&](Promise<int> promise, int x) {
            std::this_thread::sleep_for(1s);
            promise.set_value(x);
            SPDLOG_INFO("set value");
        },
        std::move(p), x
    );
    t.join();
    SPDLOG_INFO("2");
    auto v = f.get();
    SPDLOG_INFO("3");

    EXPECT_EQ(v, x);
}
} // namespace myx_coroutine
