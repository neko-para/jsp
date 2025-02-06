#include "jsp.h"

#include <iostream>
#include <tuple>

void test1() {
    jsp::Promise<> p1;
    p1.then([]() {
        std::cout << 123 << std::endl;
    });
    p1.resolve();

    jsp::Promise<int> p2;
    p2.resolve(246);
    p2.then([](const int& i) {
        std::cout << i << std::endl;
    });
}

jsp::Promise<int> test2() {
    co_return 1;
}

jsp::Promise<int> testp() {
    co_return test2();
}

jsp::Promise<int, std::string> test3() {
    co_return std::make_tuple(1, "123");
}

jsp::Promise<int> test4() {
    std::cout << co_await test2() << std::endl;
    auto [i, d] = co_await test3();
    std::cout << i << ' ' << d << std::endl;
    co_return 2;
}

int main() {
    test1();
    test4().then([](const int& i) {
        std::cout << i << std::endl;
    });

    return 0;
}
