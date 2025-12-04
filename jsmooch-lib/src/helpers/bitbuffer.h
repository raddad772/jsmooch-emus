//
// Created by . on 8/9/24.
//
// Basic bit-by-bit read/write functions

#pragma once

#include "int.h"
#include "cvec.h"

template<int number_of_bits, bool lsb_first>
struct static_bitbuf {
    std::array<u8, number_of_bits> data{};
    bool lst_first{lsb_first};
    int num_bits{number_of_bits};
    void write_bits(u32 num, u32 val) {
        for (u32 i = num-1; i >=0; i--) {
            data[write.pos++] = (val >> i) & 1;
        }
    }

    int write_final() {
        return 0;
    }

    struct {
        u8 partial{};
        u8 next_bit{};
        u32 pos{};
    } write{};
};

struct bitbuf {
    explicit bitbuf(size_t num_bits_to_preallocate, bool lsb_first_in);
    void clear();
    u32 read_bit(u32 pos);

    std::vector<u8> buf{};
    bool lsb_first{};
    u32 num_bits{};
    void write_bits(i32 num, u32 val);
    int write_final();

    struct {
        u8 partial{};
        u8 next_bit{};
    } write{};

};
