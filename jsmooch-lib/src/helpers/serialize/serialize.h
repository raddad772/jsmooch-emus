//
// Created by . on 11/10/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/enums.h"
#include "helpers/img.h"
#include "helpers/jsm_string.h"

struct serialized_opt_data {
    char buf[512]{};
    u32 len{};
};

struct ser_iter {
    u8 *buf{};
    u64 offset{};
};

struct serialized_state_section {
    u32 offset{};

    char friendly_name[50]{}; // PPU, CPU, etc.
    u32 kind{}; // each system will have its own section kinds
    u32 version{};
    serialized_opt_data opt{};
    u64 sz{};
};

struct serialized_state {
    jsm::systems kind{};
    u32 version{};
    serialized_opt_data opt{};

    jsm_string fname{200}, fpath{200};

    std::vector<serialized_state_section>sections{};
    std::vector<u8> buf{};

    u32 has_screenshot{};
    jsimg screenshot{};

    ser_iter iter{};

    serialized_state_section *cur_section;

    void new_section(const char *friendly_name, int kind, int version);
    void write_to_file(FILE *f) const;
    int read_from_file(FILE *f, size_t file_size);
};


void Sadd(serialized_state *, const void *ptr, u64 howmuch);
void Sload(serialized_state *, void *ptr, u64 howmuch);

#define u64S(x) Sadd(state, &(x), 8)
#define i64S(x) u64S(x)
#define u32S(x) Sadd(state, &(x), 4)
#define i32S(x) u32S(x)
#define u16S(x) Sadd(state, &(x), 2)
#define i16S(x) u16S(x)
#define u8S(x) Sadd(state, &(x), 1)
#define i8S(x) u8S(x)
