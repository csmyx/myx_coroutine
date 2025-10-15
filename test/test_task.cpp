#include "myx_coroutine/task.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <variant>

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
using myx_coroutine::Task;

TEST_F(TaskTest, simpleTest) {
    struct T {
        std::string s;

        T(const T &t) : s(t.s) { SPDLOG_INFO("call COPY construtor"); }

        T(T &&t) : s(std::move(t.s)) {
            t.s = "moved_str";
            SPDLOG_INFO("call MOVE constructor");
        }

        T(const std::string &s) : s(s) {}
    };

    auto f = []() -> Task<T> {
        co_return T("hello world");
    };
    auto task = f();
    task.resume();
    {
        SPDLOG_INFO("get result from l-value task");
        auto x = task.promise().result();
        EXPECT_EQ("hello world", x.s);
    }
    {
        SPDLOG_INFO("get result from r-value task");
        auto x = std::move(task).promise().result();
        EXPECT_EQ("hello world", x.s);
    }
    {
        SPDLOG_INFO(
            "get result from moved task (should not use moved task, only for testing purposes)"
        );
        auto x = task.promise().result();
        EXPECT_EQ("moved_str", x.s);
    }
}

TEST_F(TaskTest, HelloWorldTest) {
    auto h = []() -> Task<std::string> {
        co_return "Hello";
    }();
    auto w = []() -> Task<std::string> {
        co_return "World";
    }();

    EXPECT_THROW(h.promise().result(), std::runtime_error);

    // REQUIRE_THROWS_AS(w.promise().result(), std::runtime_error);

    // h.resume(); // task suspends immediately
    // w.resume();

    // REQUIRE(h.is_ready());
    // REQUIRE(w.is_ready());

    // auto w_value = std::move(w).promise().result();

    // REQUIRE(h.promise().result() == "Hello");
    // REQUIRE(w_value == "World");
    // REQUIRE(w.promise().result().empty());
}
