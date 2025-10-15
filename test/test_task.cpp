#include "myx_coroutine/task.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

namespace myx_coroutine {
struct TaskTest : public testing::Test {
  protected:
    void SetUp() override {
        spdlog::set_pattern(
            "[%Y-%m-%d %H:%M:%S] " // time
            "[%^%-5l%$] "          // level
            "[thread %t] "         // thread
            "[%s:%#]: "            // file:line
            "%v"                   // content
        );
        SPDLOG_INFO("testing started");
    }

    void TearDown() override { SPDLOG_INFO("testing finished"); }
};

using namespace std::chrono_literals;

TEST_F(TaskTest, simpleTest) {
    auto f = []() -> Task<std::string> {
        co_return "hello world";
    };
    auto task = f();
    task.resume();
    {
        auto x = task.promise().result();
        EXPECT_EQ("hello world", x);
    }
    {
        auto x = std::move(task).promise().result();
        EXPECT_EQ("hello world", x);
    }
    {
        auto x = task.promise().result();
        EXPECT_TRUE(x.empty());
    }
}
} // namespace myx_coroutine
