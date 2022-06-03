#ifndef MY_ALLOC_H
#define MY_ALLOC_H

#include <string.h>  // memcpy()

#include <cassert>   // assert()
#include <climits>   // UINT_MAX
#include <cstddef>   // ptrdiff_t, size_t
#include <cstdlib>   // exit()
#include <iostream>  // cerr
#include <new>       // placement new

#define DEBUG

namespace my_alloc {
// typedef void (*) new_handler;
// new_handler set_new_handler(new_handler new_p);

namespace __detail {
/**
 * @brief encapsulate malloc() and free() into static functions
 *
 */
class __malloc_alloc_base {
   public:
    static void* allocate(size_t n) { return malloc(n); }
    static void deallocate(void* p, size_t /* __n */) { free(p); }
    static void* reallocate(void* p, size_t /* old_sz */, size_t new_sz) { return realloc(p, new_sz); }
};
}  // namespace __detail

/**
 * @brief a wrapper class for any typeless allocator
 *
 * @tparam _Tp
 * @tparam _Alloc
 */
template <typename _Tp, class _Alloc = __detail::__malloc_alloc_base>
class simple_alloc {
   private:
    typedef _Alloc allocator_type;
    typedef _Tp value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

   public:
    void* allocate(size_t size) { return _Alloc::allocate(size * sizeof(_Tp)); }
    void deallocate(void* p, size_t size) { _Alloc::deallocate(p, size * sizeof(_Tp)); }
    void* reallocate(void* p, size_t old_sz, size_t new_sz) { return _Alloc::reallocate(p, old_sz * sizeof(_Tp), new_sz * sizeof(_Tp)); }
};

namespace __detail {

template <int __inst>
class __alloc_base {
   private:
    typedef __detail::__malloc_alloc_base _Alloc;
    static const int __MAX_BYTES = 128;
    static const size_t __ALIGN = 8;
    inline static int __round_up(int __bytes) {
        return ((__bytes + __ALIGN - 1) & ~(__ALIGN - 1));
    }
    static char* free_list[__MAX_BYTES / __ALIGN];
    static char*& __next(char* __p) { return *(reinterpret_cast<char**>(__p)); }

    static char* __memory_start;
    static char* __memory_end;
    static size_t __memory_size() { return __memory_end - __memory_start; }
    /**
     * @brief try to return a block of memory of size __n, and append (20 - 1)  * __n to the corresponding free list
     *
     * @param __n
     * @return char*
     */
    static char* __memory_refill(size_t __n);

   public:
    typedef _Alloc allocator_type;
    static void* allocate(size_t __n);
    static void deallocate(void* __p, size_t __n);
    static void* reallocate(void* __p, size_t __old_sz, size_t __new_sz);
};

template <int __inst>
char* __alloc_base<__inst>::__memory_start = nullptr;

template <int __inst>
char* __alloc_base<__inst>::__memory_end = nullptr;

template <int __inst>
char* __alloc_base<__inst>::free_list[__MAX_BYTES / __ALIGN] = {0};

template <int __inst>
char* __alloc_base<__inst>::__memory_refill(size_t __n_rounded) {
    int index = __n_rounded / __ALIGN - 1;
    int __nobjs = 20;
    size_t total_bytes = __nobjs * __n_rounded;
    if (__memory_size() < total_bytes) {
        if (__memory_size() >= __n_rounded) {
            __nobjs = __memory_size() / __n_rounded;
            total_bytes = __nobjs * __n_rounded;
        } else {
            size_t bytes_to_get = __n_rounded * __nobjs * 2 + (__memory_size() >> 4);
            bytes_to_get = __round_up(bytes_to_get);
            if (__memory_size() > 0) {
                for (int __index = index - 1; __index >= 0; --__index) {
                    int __size = (__index + 1) * __ALIGN;
                    while (__memory_start < __memory_end) {
                        __next(__memory_start) = free_list[__index];
                        free_list[__index] = __memory_start;
                        __memory_start += __size;
                    }
                }
            }
#ifdef DEBUG
            assert(__memory_start == __memory_end);
#endif
            __memory_start = static_cast<char*>(malloc(bytes_to_get));
            if (__memory_start == nullptr) {
                std::cerr << "out of memory" << std::endl;
                exit(1);
            }
            __memory_end = __memory_start + bytes_to_get;
        }
    }
#ifdef DEBUG
    assert(free_list[index] == nullptr);
#endif  // DEBUG
    for (char* cur = __memory_start; cur < __memory_start + total_bytes; cur += __n_rounded) {
        __next(__memory_start) = free_list[index];
        free_list[index] = __memory_start;
    }
    char* result = __memory_start;
    __memory_start += total_bytes;
    return result;
}

template <int __inst>
void* __alloc_base<__inst>::allocate(size_t __n) {
    if (__n > static_cast<size_t>(__MAX_BYTES)) {
        return _Alloc::allocate(__n);
    } else {
        int index = __round_up(__n) / __ALIGN - 1;
        char* cur = free_list[index];
        if (!cur) cur = __memory_refill((index + 1) * __ALIGN);
        free_list[index] = __next(cur);
        return cur;
    }
}

template <int __inst>
void __alloc_base<__inst>::deallocate(void* __p, size_t __n) {
    if (__n > static_cast<size_t>(__MAX_BYTES)) {
        _Alloc::deallocate(__p, __n);
    } else {
        int index = __round_up(__n) / __ALIGN - 1;
        __next(reinterpret_cast<char*>(__p)) = free_list[index];
        free_list[index] = (char*)__p;
    }
}

template <int __inst>
void* __alloc_base<__inst>::reallocate(void* __p, size_t __old_sz, size_t __new_sz) {
    if (__old_sz > static_cast<size_t>(__MAX_BYTES) || __new_sz > static_cast<size_t>(__MAX_BYTES)) {
        return _Alloc::reallocate(__p, __old_sz, __new_sz);
    } else {
        char* result = allocate(__new_sz);
        size_t cp_size = __new_sz > __old_sz ? __old_sz : __new_sz;
        memcpy(result, __p, cp_size);
        deallocate(__p, __old_sz);
        return result;
    }
}

}  // namespace __detail

template <typename _Tp>
class my_alloc {
   public:
    typedef _Tp value_type;
    typedef _Tp* pointer;
    typedef const _Tp* const_pointer;
    typedef _Tp& reference;
    typedef const _Tp& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    typedef __detail::__alloc_base<0> __alloc_base;

    template <class _Tp1>
    struct rebind {
        typedef my_alloc<_Tp1> other;
    };

    my_alloc() throw() {}
    my_alloc(const my_alloc&) throw() {}
    template <class _Tp1>
    my_alloc(const my_alloc<_Tp1>&) throw() {}
    ~my_alloc() throw() {}

    pointer address(reference __x) const { return &__x; }
    const_pointer address(const_reference __x) const { return &__x; }

    pointer allocate(size_type __n, const void* = 0) {
        if (__n == 0) return 0;
        return (pointer)__alloc_base::allocate(__n * sizeof(_Tp));
    }

    void deallocate(pointer __p, size_type) {
        __alloc_base::deallocate(__p, 0);
    }

    pointer reallocate(pointer __p, size_type __old_sz, size_type __new_sz) {
        return __alloc_base::reallocate(__p, __old_sz * sizeof(_Tp), __new_sz * sizeof(_Tp));
    }

    size_type max_size() const throw() { return size_t(-1) / sizeof(_Tp); }

    void construct(pointer __p, const _Tp& __val) { new (__p) _Tp(__val); }
    void destroy(pointer __p) { __p->~_Tp(); }
};

bool operator==(const my_alloc<int>&, const my_alloc<int>&) { return true; }
bool operator!=(const my_alloc<int>&, const my_alloc<int>&) { return false; }
}  // namespace my_alloc

#endif  // !MY_ALLOC_H