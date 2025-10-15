#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

std::atomic<int> atomic_counter{0};
std::mutex mutex_counter;
int plain_counter = 0;

// 原子变量版本
void atomic_increment(int cnt) {
    while (cnt--) {
        atomic_counter.fetch_add(1, std::memory_order_relaxed);
    }
}

// mutex版本
void mutex_increment(int cnt) {
    while (cnt--) {
        std::lock_guard lock(mutex_counter);
        ++plain_counter;
    }
}

void test1(auto &&func, auto &x) {
    size_t cnt = 4;
    std::vector<std::thread> v;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < cnt; ++i) {
        v.emplace_back(func);
    }
    for (auto &x : v) {
        x.join();
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    int ns = duration.count();
    int t = ns / x;
    std::cout << "time:" << t << "ns per op " << "x: " << x << std::endl;
}

int main() {
    int cnt = 1'000'000;
    test1([cnt] { atomic_increment(cnt); }, atomic_counter);
    test1([cnt] { mutex_increment(cnt); }, plain_counter);
}
