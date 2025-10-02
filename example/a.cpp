#include <format>
#include <iostream>
#include <myx_coroutine/core.h>

int main() {
    int a = 1, b = 41;
    std::print("a+b={}\n", add(a, b));
}
