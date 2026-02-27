//
// Created by . on 2/19/26.
//
#pragma once

#include "helpers/int.h"


namespace PS1 {
struct core;
static constexpr u32 MDEC_NUMHWORDS_OUT = 0x40000;
static constexpr u32 MDEC_NUMHWORDS_IN = 0x10000;

struct GFIFOIN {
    u32 head{}, tail{}, len{};
    u32 words[MDEC_NUMHWORDS_IN];

    void push(u32 val);
    u32 pop();
    void reset();
};

struct GFIFOUT {
    i32 head{}, tail{}, len{};
    u32 words[MDEC_NUMHWORDS_OUT];

    void push(u32 val);
    u32 pop();
    void reset();
};
enum MDECMODE {
    MM_Idle,
    MM_DecodeMacroblock,
    MM_SetQuantTable,
    MM_SetScaleTable
};

struct MDEC {
    explicit MDEC(core *parent) : bus(parent) {}
    core *bus;
    void write_data(u32 val);
    u32 read_data();
    void do_decode();
    void write_ctrl(u32 val);
    u32 read_ctrl();
    u32 mainbus_read(u32 addr, u8 sz);
    void mainbus_write(u32 addr, u8 sz, u32 val);
    bool decode_block(i16 *block, u8 *table);

    GFIFOIN fifo_in{};
    GFIFOUT fifo_out{};
    u32 num_param_words{};
    u32 output[256];
    struct {
        union {
            struct {
                u32 num_param_words_remaining_minus_1 : 16; // 0-15
                u32 current_block : 3; // 16-18 0..3=Y1..Y4, 4=Cr, 5=Cb, 6-7=?
                u32 _res1 : 4; // 19-22
                u32 output_mask_bit : 1; // 23
                u32 output_sign : 1; // 24 0=unsigned, 1 = signed
                u32 output_depth : 2; // 25-26 0=4bit, 1=8bit, 2=24bit, 3=15bit
                u32 data_out_req : 1; // 27 1=set when DMA1 enabled and ready to send data
                u32 data_in_req : 1; // 28 1=set when DMA0 enabled and ready to receive data
                u32 busy : 1; // 29 1=busy, 0=ready to recv cmd
                u32 data_in_fifo_full : 1; // 0=no, 1=full
                u32 data_out_fifo_empty : 1; // 0=no, 1=empty
            };
            u32 u{};
        } stat{};
        MDECMODE mode{};
        u32 num_remaining_param_words{};
        i32 offset{};
    } io{};

    struct {
        u8  luma[64], chroma[64];
        i16 scale[64];
        i16 cr[64], cb[64];
        s16 y0[64], y1[64], y2[64], y3[64];
    } BLOCK;

    void decodeIDCT0(i16 *source, i16 *target);
    void decodeIDCT1(i16 *source, i16 *target);
    void convert_y(u32 *out, i16 *luma);
    void convert_yuv(u32 *out, i16 *luma, u32 bx, u32 by);
    bool can_dreq_out() const;
    bool can_dreq_in() const;

private:
    void abort();
    void write_fifo_out(u32 w);
    u32 read_fifo_in();
};

}