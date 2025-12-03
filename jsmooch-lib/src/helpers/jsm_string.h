#pragma once
//
// Created by Dave on 2/14/2024.
//

#include <cstdarg>

#include "helpers/int.h"

struct jsm_string {
    char *ptr{nullptr}; // Pointer
    char *cur{}; // Current location in string
    u32 allocated_len{}; // Maximum length

    explicit jsm_string(u32 sz);
    ~jsm_string();
    void seek(i32 pos);
    int sprintf(const char* format, ...);
    int vsprintf(const char* format, va_list va);
    void empty();
    void quickempty();
};
#include <cstddef>
#include <utility>
template <std::size_t N, std::size_t... Is>
constexpr auto make_jsm_string_array_impl(int num, std::index_sequence<Is...>) {
    // construct array of N jsm_string(num) calls — all explicitly constructed
    return std::array<jsm_string, N>{ ( (void)Is, jsm_string(num) )... };
}

template <std::size_t N>
constexpr auto make_jsm_string_array(int num) {
    return make_jsm_string_array_impl<N>(num, std::make_index_sequence<N>{});
}

// thanks https://stackoverflow.com/questions/744766/how-to-compare-ends-of-strings-in-c
u32 ends_with(const char *str, const char *suffix);
