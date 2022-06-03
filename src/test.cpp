#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include "my_alloc.h"

template <class _Alloc>
void test(const _Alloc &) {
    std::vector<int, _Alloc> v;
    std::cout << "sizeof(v) = " << sizeof(v) << std::endl;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }
    std::cout << "push_back 100000 ints" << std::endl;
    for (int i = 0; i < 100; ++i) {
        int index = rand() % v.size();
        v.insert(v.begin() + index, i);
    }
    std::cout << "insert 100000 ints" << std::endl;
}

void test_my_alloc() {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    test(my_alloc::my_alloc<int>());
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "[test_my_alloc]: my_alloc Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;
    begin = std::chrono::steady_clock::now();
    test(std::allocator<int>());
    end = std::chrono::steady_clock::now();
    std::cout << "[test_my_alloc]: std::alloc Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;
}

int main(int argc, char const *argv[]) {
    test_my_alloc();
    return 0;
}
