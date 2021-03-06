#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include "my_alloc.h"

template <class _Alloc>
void test(const _Alloc &) {
    std::vector<int, _Alloc> v;
    for (int i = 0; i < 100000; ++i) {
        v.push_back(i);
    }
    std::cout << "push_back 100000 ints" << std::endl;
    for (int i = 0; i < 100000; ++i) {
        int index = rand() % v.size();
        v.insert(v.begin() + index, i);
    }
    std::cout << "insert 100000 ints" << std::endl;
    for (int i = 0; i < 100000; ++i) {
        int index = rand() % v.size();
        v.erase(v.begin() + index);
    }
    std::cout << "erase 100000 ints" << std::endl;
    v.assign(100000, 0);
    std::cout << "assign 100000 ints" << std::endl;
    v.resize(10000000);
    std::cout << "resize 10000000 ints" << std::endl;
    for (int i = 0; i < 10000000; ++i) {
        v.emplace(v.begin() + i, i);
    }
    std::cout << "emplace 10000000 ints" << std::endl;
    v.shrink_to_fit();
    std::cout << "shrink_to_fit" << std::endl;
    for (int i = 0; i < 10000000; ++i) {
        v.pop_back();
    }
    std::cout << "pop_back 10000000 ints" << std::endl;
    v.clear();
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
