#include "myx_coroutine/task.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

namespace myx_coroutine {

#define ENABLE_TEST_NAME 1
#define TEST_NAME_       TaskTest

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
        auto x = task.promise().get_result();
        EXPECT_EQ("hello world", x.s);
    }
    {
        SPDLOG_INFO("get result from r-value task");
        auto x = std::move(task).promise().get_result();
        EXPECT_EQ("hello world", x.s);
    }
    {
        SPDLOG_INFO(
            "get result from moved task (should not use moved task, only for testing purposes)"
        );
        auto x = task.promise().get_result();
        EXPECT_EQ("moved_str", x.s);
    }
}

TEST_F(TEST_NAME, HelloWorldTest) {
    auto h = []() -> Task<std::string> {
        co_return "Hello";
    }();
    auto w = []() -> Task<std::string> {
        co_return "World";
    }();

    EXPECT_THROW(h.promise().get_result(), std::runtime_error);
    EXPECT_THROW(w.promise().get_result(), std::runtime_error);

    h.resume(); // task suspends immediately
    w.resume();

    EXPECT_TRUE(h.is_ready());
    EXPECT_TRUE(w.is_ready());

    auto w_value = std::move(w).promise().get_result();

    EXPECT_TRUE(h.promise().get_result() == "Hello");
    EXPECT_TRUE(w_value == "World");
    SPDLOG_INFO(
        "get result from moved task (should not use moved task, only for testing purposes)"
    );
    EXPECT_TRUE(w.promise().get_result().empty());
}

TEST_F(TEST_NAME, VoidTest) {
    using task_type = Task<>;

    auto t = []() -> task_type {
        std::this_thread::sleep_for(10ms);
        co_return;
    }();

    EXPECT_TRUE(!t.is_ready());
    t.resume();
    EXPECT_TRUE(t.is_ready());
}

TEST_F(TEST_NAME, ExceptionThrownTest) {
    using task_type = Task<std::string>;

    std::string throw_msg = "I'll be reached";

    auto task = [](std::string &throw_msg) -> task_type {
        throw std::runtime_error(throw_msg);
        co_return "I'll never be reached";
    }(throw_msg);

    task.resume();

    EXPECT_TRUE(task.is_ready());

    bool thrown{false};
    try {
        auto value = task.promise().get_result();
    } catch (const std::exception &e) {
        thrown = true;
        EXPECT_TRUE(e.what() == throw_msg);
    }

    EXPECT_TRUE(thrown);
}

TEST_F(TEST_NAME, AwaitInnerTaskTest) {
    auto outer_task = []() -> Task<> {
        auto inner_task = []() -> Task<int> {
            SPDLOG_INFO("inner_task start");
            SPDLOG_INFO("inner_task stop");
            co_return 42;
        };

        SPDLOG_INFO("outer_task start");
        auto v = co_await inner_task();
        EXPECT_TRUE(v == 42);
        SPDLOG_INFO("outer_task stop");
    }();

    outer_task.resume(); // all tasks start suspend, kick it off.

    EXPECT_TRUE(outer_task.is_ready());
}

TEST_F(TEST_NAME, AwaitInnerTaskTest2) {
    auto task1 = []() -> Task<> {
        SPDLOG_INFO("task1 start");
        auto task2 = []() -> Task<int> {
            SPDLOG_INFO("\ttask2 start");
            auto task3 = []() -> Task<int> {
                SPDLOG_INFO("\t\ttask3 start");
                SPDLOG_INFO("\t\ttask3 stop");
                co_return 3;
            };

            auto v2 = co_await task3();
            EXPECT_TRUE(v2 == 3);

            SPDLOG_INFO("\ttask2 stop");
            co_return 2;
        };

        auto v1 = co_await task2();
        EXPECT_TRUE(v1 == 2);

        SPDLOG_INFO("task1 stop");
    }();

    task1.resume(); // all tasks start suspended, kick it off.

    EXPECT_TRUE(task1.is_ready());
}

TEST_F(TEST_NAME, AwaitMutilTest1) {
    auto task = []() -> Task<void> {
        co_await std::suspend_always{};
        co_await std::suspend_never{};
        co_await std::suspend_always{};
        co_await std::suspend_always{};
        co_return;
    }();

    task.resume(); // initial suspend
    EXPECT_FALSE(task.is_ready());

    task.resume(); // first internal suspend
    EXPECT_FALSE(task.is_ready());

    task.resume(); // second internal suspend
    EXPECT_FALSE(task.is_ready());

    task.resume(); // third internal suspend
    EXPECT_TRUE(task.is_ready());
}

TEST_F(TEST_NAME, AwaitMutilTest2) {
    auto task = []() -> Task<int> {
        co_await std::suspend_always{};
        co_await std::suspend_always{};
        co_await std::suspend_always{};
        co_return 11;
    }();

    task.resume(); // initial suspend
    EXPECT_FALSE(task.is_ready());

    task.resume(); // first internal suspend
    EXPECT_FALSE(task.is_ready());

    task.resume(); // second internal suspend
    EXPECT_FALSE(task.is_ready());

    task.resume(); // third internal suspend
    EXPECT_TRUE(task.is_ready());
    EXPECT_TRUE(task.promise().get_result() == 11);
}

// Task resume from promise to coroutine handles of different types.
TEST_F(TEST_NAME, ResumeTest) {
    auto task1 = []() -> Task<int> {
        SPDLOG_INFO("Task ran");
        co_return 42;
    }();

    auto task2 = []() -> Task<void> {
        SPDLOG_INFO("Task 2 ran");
        co_return;
    }();

    // task.resume();  normal method of resuming

    std::vector<std::coroutine_handle<>> handles;

    handles.emplace_back(
        std::coroutine_handle<Task<int>::promise_type>::from_promise(task1.promise())
    );
    handles.emplace_back(
        std::coroutine_handle<Task<void>::promise_type>::from_promise(task2.promise())
    );

    auto &coro_handle1 = handles[0];
    coro_handle1.resume();
    auto &coro_handle2 = handles[1];
    coro_handle2.resume();

    EXPECT_TRUE(task1.is_ready());
    EXPECT_TRUE(coro_handle1.done());
    EXPECT_TRUE(task1.promise().get_result() == 42);

    EXPECT_TRUE(task2.is_ready());
    EXPECT_TRUE(coro_handle2.done());
}

// Task throws of void type.
TEST_F(TEST_NAME, ThrowTest) {
    auto task = []() -> Task<void> {
        throw std::runtime_error{"I always throw."};
        co_return;
    }();

    EXPECT_NO_THROW(task.resume());
    EXPECT_TRUE(task.is_ready());
    EXPECT_THROW(task.promise().get_result(), std::runtime_error);
}

// Task throws of non-void l-value.
TEST_F(TEST_NAME, ThrowTest2) {
    auto task = []() -> Task<int> {
        throw std::runtime_error{"I always throw."};
        co_return 42;
    }();

    EXPECT_NO_THROW(task.resume());
    EXPECT_TRUE(task.is_ready());
    EXPECT_THROW(task.promise().get_result(), std::runtime_error);
}

// Task throws non-void r-value.
TEST_F(TEST_NAME, ThrowTest3) {
    struct type {
        int m_value;
    };

    auto task = []() -> Task<type> {
        type return_value{42};

        throw std::runtime_error{"I always throw."};
        co_return std::move(return_value);
    }();

    task.resume();
    EXPECT_TRUE(task.is_ready());
    EXPECT_THROW(task.promise().get_result(), std::runtime_error);
}

// // Const task returning a reference
// TEST_F(TEST_NAME, ReturnRefTest) {
//     struct type {
//         int m_value;
//     };

//     type return_value{42};

//     auto task = [](type &return_value) -> Task<const type &> {
//         co_return std::ref(return_value);
//     }(return_value);

//     task.resume();
//     EXPECT_TRUE(task.is_ready());
//     auto &result = task.promise().get_result();
//     EXPECT_TRUE(result.m_value == 42);
//     EXPECT_TRUE(std::addressof(return_value) == std::addressof(result));
//     static_assert(std::is_same_v<decltype(task.promise().get_result()), const type &>);
// }

// TEST_CASE("mutable task returning a reference", "[task]")
// {
//     struct type
//     {
//         int m_value;
//     };

//     type return_value{42};

//     auto task = [](type& return_value) -> Task<type&> { co_return
//     std::ref(return_value);
//     }(return_value);

//     task.resume();
//     EXPECT_TRUE(task.is_ready());
//     auto& result = task.promise().get_result();
//     EXPECT_TRUE(result.m_value == 42);
//     EXPECT_TRUE(std::addressof(return_value) == std::addressof(result));
//     static_assert(std::is_same_v<decltype(task.promise().get_result()), type&>);
// }

// TEST_CASE("task doesn't require default constructor", "[task]")
// {
//     // https://github.com/jbaldwin/libcoro/issues/163
//     // Reported issue that the return type required a default constructor.
//     // This test explicitly creates an object that does not have a default
//     // constructor to verify that the default constructor isn't required.

//     struct A
//     {
//         A(int value) : m_value(value) {}

//         int m_value{};
//     };

//     auto make_task = []() -> Task<A> { co_return A(42); };

//     EXPECT_TRUE(coro::sync_wait(make_task()).m_value == 42);
// }

// TEST_CASE("task supports instantiation with rvalue reference", "[task]")
// {
//     // https://github.com/jbaldwin/libcoro/issues/180
//     // Reported issue that the return type cannot be rvalue reference.
//     // This test explicitly creates an coroutine that returns a task
//     // instantiated with rvalue reference to verify that rvalue
//     // reference is supported.

//     int  i         = 42;
//     auto make_task = [](int& i) -> Task<int&&> { co_return std::move(i); };
//     int  ret       = coro::sync_wait(make_task(i));
//     EXPECT_TRUE(ret == 42);
// }

// struct move_construct_only
// {
//     static int move_count;
//     move_construct_only(int& i) : i(i) {}
//     move_construct_only(move_construct_only&& x) noexcept : i(x.i) { ++move_count; }
//     move_construct_only(const move_construct_only&)            = delete;
//     move_construct_only& operator=(move_construct_only&&)      = delete;
//     move_construct_only& operator=(const move_construct_only&) = delete;
//     ~move_construct_only()                                     = default;
//     int& i;
// };

// int move_construct_only::move_count = 0;

// struct copy_construct_only
// {
//     static int copy_count;
//     copy_construct_only(int i) : i(i) {}
//     copy_construct_only(copy_construct_only&&) = delete;
//     copy_construct_only(const copy_construct_only& x) noexcept : i(x.i) { ++copy_count; }
//     copy_construct_only& operator=(copy_construct_only&&)      = delete;
//     copy_construct_only& operator=(const copy_construct_only&) = delete;
//     ~copy_construct_only()                                     = default;
//     int i;
// };

// int copy_construct_only::copy_count = 0;

// struct move_copy_construct_only
// {
//     static int move_count;
//     static int copy_count;
//     move_copy_construct_only(int i) : i(i) {}
//     move_copy_construct_only(move_copy_construct_only&& x) noexcept : i(x.i) { ++move_count;
//     } move_copy_construct_only(const move_copy_construct_only& x) noexcept : i(x.i) {
//     ++copy_count; } move_copy_construct_only& operator=(move_copy_construct_only&&)      =
//     delete; move_copy_construct_only& operator=(const move_copy_construct_only&) = delete;
//     ~move_copy_construct_only()                                          = default;
//     int i;
// };

// int move_copy_construct_only::move_count = 0;
// int move_copy_construct_only::copy_count = 0;

// TEST_CASE("task supports instantiation with non assignable type", "[task]")
// {
//     // https://github.com/jbaldwin/libcoro/issues/193
//     // Reported issue that the return type cannot be non assignable type.
//     // This test explicitly creates an coroutine that returns a task
//     // instantiated with non assignable types to verify that non assignable
//     // types are supported.

//     int i                           = 42;
//     move_construct_only::move_count = 0;
//     auto move_task                  = [&i]() -> Task<move_construct_only> { co_return
//     move_construct_only(i); }; auto move_ret                   =
//     coro::sync_wait(move_task()); EXPECT_TRUE(std::addressof(move_ret.i) == std::addressof(i));
//     EXPECT_TRUE(move_construct_only::move_count == 2);

//     move_construct_only::move_count = 0;
//     auto move_task2                 = [&i]() -> Task<move_construct_only> { co_return
//     i; }; auto move_ret2                  = coro::sync_wait(move_task2());
//     EXPECT_TRUE(std::addressof(move_ret2.i) == std::addressof(i));
//     EXPECT_TRUE(move_construct_only::move_count == 1);

//     copy_construct_only::copy_count = 0;
//     auto copy_task                  = [&i]() -> Task<copy_construct_only> { co_return
//     copy_construct_only(i); }; auto copy_ret                   =
//     coro::sync_wait(copy_task()); EXPECT_TRUE(copy_ret.i == 42);
//     EXPECT_TRUE(copy_construct_only::copy_count == 2);

//     copy_construct_only::copy_count = 0;
//     auto copy_task2                 = [&i]() -> Task<copy_construct_only> { co_return
//     i; }; auto copy_ret2                  = coro::sync_wait(copy_task2());
//     EXPECT_TRUE(copy_ret2.i == 42);
//     EXPECT_TRUE(copy_construct_only::copy_count == 1);

//     move_copy_construct_only::move_count = 0;
//     move_copy_construct_only::copy_count = 0;
//     auto move_copy_task = [&i]() -> Task<move_copy_construct_only> { co_return
//     move_copy_construct_only(i); }; auto task           = move_copy_task(); auto
//     move_copy_ret1 = coro::sync_wait(task); auto move_copy_ret2 =
//     coro::sync_wait(std::move(task)); EXPECT_TRUE(move_copy_ret1.i == 42);
//     EXPECT_TRUE(move_copy_ret2.i == 42);
//     EXPECT_TRUE(move_copy_construct_only::move_count == 2);
//     EXPECT_TRUE(move_copy_construct_only::copy_count == 1);

//     auto make_tuple_task = [](int i) -> Task<std::tuple<int, int>> {
//         co_return {i, i * 2};
//     };
//     auto tuple_ret = coro::sync_wait(make_tuple_task(i));
//     EXPECT_TRUE(std::get<0>(tuple_ret) == 42);
//     EXPECT_TRUE(std::get<1>(tuple_ret) == 84);

//     auto  make_ref_task = [&i]() -> Task<int&> { co_return std::ref(i); };
//     auto& ref_ret       = coro::sync_wait(make_ref_task());
//     EXPECT_TRUE(std::addressof(ref_ret) == std::addressof(i));
// }

// TEST_CASE("task promise sizeof", "[task]")
// {
//     EXPECT_TRUE(sizeof(coro::detail::promise<void>) >= sizeof(std::coroutine_handle<>) +
//     sizeof(std::exception_ptr)); EXPECT_TRUE(
//         sizeof(coro::detail::promise<int32_t>) ==
//         sizeof(std::coroutine_handle<>) + sizeof(std::variant<int32_t, std::exception_ptr>));
//     EXPECT_TRUE(
//         sizeof(coro::detail::promise<int64_t>) >=
//         sizeof(std::coroutine_handle<>) + sizeof(std::variant<int64_t, std::exception_ptr>));
//     EXPECT_TRUE(
//         sizeof(coro::detail::promise<std::vector<int64_t>>) >=
//         sizeof(std::coroutine_handle<>) + sizeof(std::variant<std::vector<int64_t>,
//         std::exception_ptr>));
// }

// TEST_CASE("~task", "[task]")
// {
//     SPDLOG_INFO("[~task]\n");
// }

} // namespace myx_coroutine
