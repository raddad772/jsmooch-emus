//
// Created by . on 2/19/26.
//
#pragma once

#include "helpers/int.h"


namespace PS1 {
struct core;

struct MDEC {
    explicit MDEC(core *parent) : bus(parent) {}
    core *bus;
    void write_data(u32 val);
    u32 read_data();

    void write_ctrl(u32 val);
    u32 read_ctrl();

    struct {
        union {
            struct {
                u32 num_param_words_remaining_minus_1 : 16; // 0-15
                u32 current_block : 3; // 16-18 0..3=Y1..Y4, 4=Cr, 5=Cb, 6-7=?
                u32 _res1 : 4; // 19-22
                u32 bit15_out : 1; // 23
                u32 outut_sign : 1; // 24 0=unsigned, 1 = signed
                u32 output_depth : 2; // 25-26 0=4bit, 1=8bit, 2=24bit, 3=15bit
                u32 data_out_req : 1; // 27 1=set when DMA1 enabled and ready to send data
                u32 data_in_req : 1; // 28 1=set when DMA0 enabled and ready to receive data
                u32 busy : 1; // 29 1=busy, 0=ready to recv cmd
                u32 data_in_fifo_full : 1; // 0=no, 1=full
                u32 data_out_fifo_empty : 1; // 0=no, 1=empty
            };
            u32 u{};
        } stat{};
    } io{};

private:
    void abort();
};

}