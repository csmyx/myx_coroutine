#include "myx_coroutine/promise.hpp"
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>

namespace myx_coroutine {

#define ENABLE_TEST_NAME 0
#define TEST_NAME_       PromiseTest

#if ENABLE_TEST_NAME
#define TEST_NAME TEST_NAME_
#else
#define TEST_NAME DISABLED_##TEST_NAME_
#endif

struct TEST_NAME : public testing::Test {
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

TEST_F(TEST_NAME, simpleTest) {
    Promise<int> p;
    auto f = p.get_future();
    int x = 100;
    std::thread t(
        [&](Promise<int> promise, int x) {
            std::this_thread::sleep_for(500ms);
            promise.set_value(x);
        },
        std::move(p), x
    );
    auto v = f.get();
    t.join();

    EXPECT_EQ(v, x);
}

TEST_F(TEST_NAME, setException) {
    Promise<int> p;
    auto f = p.get_future();
    const std::string err_msg = "test exception";

    std::thread t([&p, &err_msg]() {
        std::this_thread::sleep_for(500ms);
        try {
            throw std::runtime_error(err_msg);
        } catch (...) {
            p.set_exception(std::current_exception());
        }
        SPDLOG_INFO("set exception");
    });

    t.join();
    SPDLOG_INFO("waiting for exception");

    // verify catch expection.
    EXPECT_THROW(
        {
            try {
                f.get();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), err_msg.c_str());
                throw;
            }
        },
        std::runtime_error
    );
}

TEST_F(TEST_NAME, promiseDestroyedWithoutValue) {
    Promise<int> p;
    auto f = p.get_future();

    std::thread t([p = std::move(p)]() mutable {
        std::this_thread::sleep_for(500ms);
        // destroy promise without setting value(broken promise).
    });
    t.join();

    EXPECT_THROW(f.get(), std::runtime_error);
}

TEST_F(TEST_NAME, moveSemantics) {
    Promise<int> p1;
    auto f1 = p1.get_future();
    Promise<int> p2(std::move(p1));

    // test move construct
    EXPECT_THROW(p1.set_value(100), std::runtime_error);

    std::thread t1([p = std::move(p2)]() mutable {
        std::this_thread::sleep_for(500ms);
        p.set_value(200);
    });
    t1.join();
    EXPECT_EQ(f1.get(), 200);

    Promise<int> p3;
    auto f3 = p3.get_future();
    Promise<int> p4;
    p4 = std::move(p3);

    // test move assignment
    EXPECT_THROW(p3.set_value(300), std::runtime_error);

    std::thread t2([p = std::move(p4)]() mutable {
        std::this_thread::sleep_for(500ms);
        p.set_value(400);
    });
    t2.join();
    EXPECT_EQ(f3.get(), 400);
}

// Test promise of void type
TEST_F(TEST_NAME, voidType) {
    Promise<void> p;
    auto f = p.get_future();
    bool flag = false;

    std::thread t([&p, &flag]() {
        std::this_thread::sleep_for(500ms);
        flag = true;
        p.set_value(); // No argument for void type
        SPDLOG_INFO("void promise set");
    });

    f.get(); // Wait for completion, no return value
    t.join();
    EXPECT_TRUE(flag); // Verify that the sub-thread logic has been executed
}

// Test calling get_future multiple times (should throw an exception, consistent with std)
TEST_F(TEST_NAME, multipleGetFuture) {
    Promise<int> p;
    (void)p.get_future(); // First call is valid
    // Subsequent calls should throw an exception
    EXPECT_THROW(p.get_future(), std::runtime_error);
    EXPECT_THROW(p.get_future(), std::runtime_error);
}

// Test the blocking waiting behavior of future
TEST_F(TEST_NAME, futureBlockUntilReady) {
    Promise<std::string> p;
    auto f = p.get_future();
    std::string result;
    std::atomic<bool> thread_started = false;

    std::thread t([&p, &thread_started]() {
        thread_started = true;
        std::this_thread::sleep_for(1s); // Simulate time-consuming operation
        p.set_value("hello future");
    });

    // Wait for the sub-thread to start
    while (!thread_started) {
        std::this_thread::yield();
    }

    // Time to verify that get() will block until the value is ready
    auto start = std::chrono::steady_clock::now();
    result = f.get();
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    t.join();
    EXPECT_EQ(result, "hello future");
    EXPECT_GE(duration, 900); // Ensure to wait for at least close to 1 second (allowing some error)
    SPDLOG_INFO("duration: {}", duration);
}

// Test multiple futures waiting for the same promise (through shared state)
TEST_F(TEST_NAME, multipleFuturesShareState) {
    Promise<long> p;
    auto f1 = p.get_future();
    // do not allow multiple calls to get_future
    EXPECT_THROW(auto f2 = p.get_future(), std::runtime_error);
}
} // namespace myx_coroutine
