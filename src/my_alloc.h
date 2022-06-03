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
    // static const size_t __ALIGN = 8;

    /*****************
     * memory pool  *
     ***************/

    static char* __memory_start;
    static char* __memory_end;
    inline static size_t __heap_size() { return __memory_end - __memory_start; }

    /**
     * @brief refill memory from the memory pool, if memory pool is not enough, malloc to increase the memory pool size
     *
     * @param __sz the size of memory to be allocated
     * @return char* the pointer to the allocated memory
     */
    static char* __memory_refill(size_t& __sz);

    /************************
     * free list structure  *
     ***********************/

    static const int __HEADER_SIZE = sizeof(size_t);
    static const int __MIN_EXTRA_SIZE = __HEADER_SIZE + sizeof(char*);
    static char* __free_list_head;
    static void __init_free_list();
    static size_t __size(void* p) { return *(size_t*)((char*)p - __HEADER_SIZE); }
    static void __set_size(void* p, size_t sz) { *(size_t*)((char*)p - __HEADER_SIZE) = sz; }
    static char* __next_free(void* p) { return *(char**)p; }
    static void __set_next_free(void* prev, void* next) { *(char**)prev = (char*)next; }
    static void __insert_head(void* p) { __set_next_free(p, __next_free(__free_list_head)), __set_next_free(__free_list_head, p); }

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
char* __alloc_base<__inst>::__free_list_head = nullptr;

template <int __inst>
void __alloc_base<__inst>::__init_free_list() {
    __free_list_head = (char*)malloc(__MIN_EXTRA_SIZE) + __HEADER_SIZE;
    __set_next_free(__free_list_head, nullptr);
    __set_size(__free_list_head, 0);
}

template <int __inst>
char* __alloc_base<__inst>::__memory_refill(size_t& sz) {
    int __nobjs = 20;
    size_t __total_bytes = (sz + __HEADER_SIZE) * __nobjs;
    char* result = __memory_allocate(sz + __HEADER_SIZE, __total_bytes) + __HEADER_SIZE;
    if (__total_bytes > sz) {
        __set_size(result + sz, __total_bytes - sz - __HEADER_SIZE * 2);
#ifdef DEBUG
        std::cout << "[__alloc_base.__memory_refill]: added to free list: size = " << __total_bytes - sz - __HEADER_SIZE * 2 << " at " << (void*)(result + sz) << std::endl;

#endif
        __insert_head(result + sz);
    }
    return result;
}

template <int __inst>
void* __alloc_base<__inst>::allocate(size_t __n) {
    if (!__free_list_head) {
        __init_free_list();
    }
    void* __ret = 0;
    if (__n > (size_t)__MAX_BYTES) {
        __ret = (char*)allocator_type::allocate(__n + __HEADER_SIZE) + __HEADER_SIZE;
        __set_size(__ret, __n);
    } else {
#ifdef DEBUG
        std::cout << "[__alloc_base.allocate]: allocate " << __n << " bytes" << std::endl;
#endif
        char* cur = __free_list_head;
        char* prev_free = 0;
        while (cur) {
            size_t sz = __size(cur);
#ifdef DEBUG
            std::cout << "[__alloc_base.allocate]: cur = " << (void*)cur << " size = " << sz << std::endl;
#endif  // DEBUG
            if (sz >= __n) {
#ifdef DEBUG
                std::cout << "[__alloc_base.allocate]: allocated from free list at " << (void*)cur << " of size = " << sz << std::endl;
#endif  // DEBUG
                __ret = cur;
                if (sz <= __n + __MIN_EXTRA_SIZE) {
                    __set_next_free(prev_free, __next_free(cur));
                } else {
                    char* nxt = (char*)cur + __n + __HEADER_SIZE;
                    __set_size(nxt, sz - __HEADER_SIZE - __n);
                    __set_next_free(nxt, __next_free(cur));

                    __set_next_free(prev_free, nxt);
                    __set_size(cur, __n);
                }
                break;
            }
            prev_free = cur;
            cur = __next_free(cur);
        }
        if (__ret == 0) {
#ifdef DEBUG
            std::cout << "[__alloc_base.allocate]: no more in free list, allocate more memory" << std::endl;
#endif  // DEBUG
            __ret = __memory_refill(__n) /* + __HEADER_SIZE */;
            __set_size(__ret, __n);
        }
    }
#ifdef DEBUG
    std::cout << "[__alloc_base.allocate]: allocated " << __n << " bytes at " << __ret << std::endl;
#endif  // DEBUG
    return __ret;
}

template <int __inst>
void __alloc_base<__inst>::deallocate(void* __p, size_t) {
    size_t __n = __size(__p);
#ifdef DEBUG
    std::cout << "[__alloc_base.deallocate]: deallocate " << __n << " bytes at " << __p << std::endl;
#endif  // DEBUG
    if (__n > (size_t)__MAX_BYTES) {
        allocator_type::deallocate((char*)__p - __HEADER_SIZE, __n + __HEADER_SIZE);
    } else {
#ifdef DEBUG
        std::cout << "[__alloc_base.deallocate]: added to free list: size = " << __n << " at " << (void*)__p << std::endl;
#endif  // DEBUG
        __insert_head(__p);
        // }
    }
}

template <int __inst>
void* __alloc_base<__inst>::reallocate(void* __p, size_t __old_sz, size_t __new_sz) {
    if (__old_sz > (size_t)__MAX_BYTES && __new_sz > (size_t)__MAX_BYTES) {
        return allocator_type::reallocate(__p, __old_sz, __new_sz);
    } else {
        void* __ret = allocate(__new_sz);
        size_t cp_sz = (__old_sz < __new_sz) ? __old_sz : __new_sz;
        memcpy(__ret, __p, cp_sz);
        deallocate(__p, __old_sz);
        return __ret;
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

    typedef __detail::__alloc_base<> __alloc_base;

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
#ifdef DEBUG
        std::cout << "[my_alloc.allocate]: allocating " << __n << std::endl;
#endif  // DEBUG
        return (pointer)__alloc_base::allocate(__n * sizeof(_Tp));
    }

    void deallocate(pointer __p, size_type) {
#ifdef DEBUG
        std::cout << "[my_alloc.deallocate]: deallocating " << __p << std::endl;
#endif  // DEBUG
        __alloc_base::deallocate(__p, 0);
    }

    pointer reallocate(pointer __p, size_type __old_sz, size_type __new_sz) {
#ifdef DEBUG
        std::cout << "[my_alloc.reallocate]: reallocating " << __p << " from " << __old_sz << " to " << __new_sz << std::endl;
#endif  // DEBUG
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