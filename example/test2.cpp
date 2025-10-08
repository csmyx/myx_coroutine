#include <format>
#include <iostream>
#include <ostream>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

template<typename... T>
std::ostream &print(std::ostream &out, T... args) {
    ((out << args << ", "), ...);
    return out << std::endl;
}

template<typename T>
std::ostream &print(std::ostream &out, std::span<T> span) {
    for (const auto &arg : span) {
        out << arg << ", ";
    }
    return out << std::endl;
}

// template<typename... T>
// std::ostream &print_v(std::ostream &out, T... args) {
//     return ((out << args << ", "), ...);
// }

int main() {
    std::vector<int> v1{1, 2, 3};
    std::vector<std::string> v2{"man", "what can i say", "男人", "什么罐头", "我说"};
    print(std::cout, 1, "hello", 3);
    print<int>(std::cout, v1);
    print<std::string>(std::cout, v2);
    print(std::cout, std::span<std::string>(v2.begin() + 2, v2.end()));
}
