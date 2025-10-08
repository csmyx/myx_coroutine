#include "myx_coroutine/promise.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

void accumulate(
    std::vector<int>::iterator first,
    std::vector<int>::iterator last,
    std::promise<int> accumulate_promise
) {
    int sum = std::accumulate(first, last, 0);
    accumulate_promise.set_value(sum); // Notify future
}

void do_work(std::promise<void> barrier) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    barrier.set_value();
}

void test_std_promise() {
    // Demonstrate using promise<int> to transmit a result between threads.
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6};
    std::promise<int> accumulate_promise;
    std::future<int> accumulate_future = accumulate_promise.get_future();
    std::thread work_thread(
        accumulate, numbers.begin(), numbers.end(), std::move(accumulate_promise)
    );

    // future::get() will wait until the future has a valid result and retrieves it.
    // Calling wait() before get() is not needed
    // accumulate_future.wait(); // wait for result
    std::cout << "result=" << accumulate_future.get() << '\n';
    work_thread.join(); // wait for thread completion

    // Demonstrate using promise<void> to signal state between threads.
    std::promise<void> barrier;
    std::future<void> barrier_future = barrier.get_future();
    std::thread new_work_thread(do_work, std::move(barrier));
    barrier_future.wait();
    new_work_thread.join();
}

// void test_myx_promise() {
//     // Demonstrate using promise<int> to transmit a result between threads.
//     std::vector<int> numbers = {1, 2, 3, 4, 5, 6};
//     myx_coroutine::Promise<int> accumulate_promise;
//     myx_coroutine::Future<int> accumulate_future = accumulate_promise.get_future();
//     std::thread work_thread(
//         accumulate, numbers.begin(), numbers.end(), std::move(accumulate_promise)
//     );

//     // future::get() will wait until the future has a valid result and retrieves it.
//     // Calling wait() before get() is not needed
//     // accumulate_future.wait(); // wait for result
//     std::cout << "result=" << accumulate_future.get() << '\n';
//     work_thread.join(); // wait for thread completion

//     // Demonstrate using promise<void> to signal state between threads.
//     myx_coroutine::Promise<void> barrier;
//     myx_coroutine::Future<void> barrier_future = barrier.get_future();
//     std::thread new_work_thread(do_work, std::move(barrier));
//     barrier_future.wait();
//     new_work_thread.join();
// }

int main() {
    std::printf("=== test std promise ===\n");
    test_std_promise();
    std::printf("=== test myx promise ===\n");
    // test_myx_promise();
}
