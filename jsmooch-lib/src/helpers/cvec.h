#pragma once

#include <vector>

template<typename T>
struct cvec_ptr {
    std::vector<T> *vec;
    u32 index;

    T* get() { return vec->at(index); }
    void make(std::vector<T> &vec, u32 index) {
        this->vec = &vec;
        this->index = index;
    }
};
