#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

std::atomic<int> x = 0;
std::atomic<int> y = 0;

// thread 1
void f() {
    x.store(1, std::__1::memory_order::release);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    y.store(1, std::__1::memory_order::release);
}

// thread 2
void g() {
    while (x.load(std::__1::memory_order::acquire) != 1)
        ;
    // assert(y.load(std::__1::memory_order::acquire) == 1);
    if (!(y.load(std::__1::memory_order::acquire) == 1)) {
        std::cout << "fuck you" << std::endl;
    }
}

int main() {
    auto y = std::thread{g};
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto x = std::thread{f};
    y.join();
    x.join();
    return 0;
}
