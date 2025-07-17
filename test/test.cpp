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

void testCb(std::function<void(const int&)> func) {
    func(123321);
}

void testCbMove(std::function<void(int&&)> func) {
    func(321123);
}
jsp::Promise<int> testCallCb() {
    jsp::Promise<int> result;
    jsp::Promise<int> result2;
    testCb(result);
    testCbMove(result2);
    int val = co_await result + co_await result2;
    std::cout << val << std::endl;
    co_return val;
}

// jsp::Promise<int> genInts() {
//     co_yield 111;
//     co_yield 222;
//     co_yield 333;

//     co_return 999;
// }

int main() {
    test1();
    test4().then([](const int& i) {
        std::cout << i << std::endl;
    });
    testCallCb();

    return 0;
}
