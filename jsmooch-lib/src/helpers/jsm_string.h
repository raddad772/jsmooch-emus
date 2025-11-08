#pragma once
//
// Created by Dave on 2/14/2024.
//

#include <cstdarg>

#include "helpers/int.h"

struct jsm_string {
    char *ptr{}; // Pointer
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


// thanks https://stackoverflow.com/questions/744766/how-to-compare-ends-of-strings-in-c
u32 ends_with(const char *str, const char *suffix);
