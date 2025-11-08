#pragma once

#include <vector>

template<typename T>
struct cvec_ptr {
    std::vector<T> *vec=nullptr;
    u32 index=0;

    cvec_ptr() = default;
    cvec_ptr(std::vector<T> &v, u32 idx) : vec(&v), index(idx) {};

    // Copy constructor (defaulted)
    cvec_ptr(const cvec_ptr&) = default;

    // Move constructor
    cvec_ptr(cvec_ptr&& other) noexcept
        : vec(other.vec), index(other.index)
    {
        // Leave source in a valid state
        other.vec = nullptr;
        other.index = 0;
    }


    // Copy assignment (defaulted)
    cvec_ptr& operator=(const cvec_ptr&) = default;

    // Move assignment
    cvec_ptr& operator=(cvec_ptr&& other) noexcept {
        if (this != &other) {
            vec = other.vec;
            index = other.index;
            other.vec = nullptr;
            other.index = 0;
        }
        return *this;
    }


    T& get() { return vec->at(index); }
    void make(std::vector<T> &v, u32 idx) {
        vec = &v;
        index = idx;
    }
};
