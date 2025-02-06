#include "jsp.h"

#include <iostream>

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

    jsp::Promise<int, double> p3;
    p3.then([](const int& i, const double& d) {
        return i + d;
    }).then([](const double& d) {
        std::cout << d << std::endl;
    });
    p3.resolve(111, 222.333);
}

jsp::Promise<> test2() {
    co_return;
}

jsp::Promise<int> test3() {
    co_return 2;
}

int main() {
    test1();
    test2();
    test3().then([](const int& i) {
        std::cout << i << std::endl;
    });

    return 0;
}
