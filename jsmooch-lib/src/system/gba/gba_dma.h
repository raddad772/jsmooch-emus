//
// Created by . on 5/5/25.
//
#pragma once

#include "helpers/int.h"

namespace GBA {

struct core;
struct DMA;

struct DMA_ch {
    explicit DMA_ch(core *parent, u32 num_in) : bus{parent}, num {num_in} {}
    core *bus;
    struct {
        u32 src_addr{}; // 28 bits
        u32 dest_addr{}; // 28 bits
        u32 word_count{}; // 14 bits on ch0-2, 16bit on ch3

        u32 dest_addr_ctrl{};
        u32 src_addr_ctrl{};
        u32 repeat{};
        u32 transfer_size{};
        u32 game_pak_drq{}; // ch3 only
        u32 start_timing{};
        u32 irq_on_end{};
        u32 enable{};
        u32 open_bus{};
    } io{};

    struct {
        u32 started{};
        u32 word_count{};
        u32 src_addr{};
        u32 dest_addr{};
        u32 src_access{}, dest_access{};
    } latch{};
    u32 is_sound{};
    i32 src_add{}, dest_add{};
    u32 num, word_mask{};

    bool go();
    void start();
    void cnt_written(bool old_enable);
    void on_modify_write();
} ;

struct DMA {
    explicit DMA(core *parent);
    void raise_irq_for_dma(u32 num);
    void eval_bit_masks();

    core *bus;
    DMA_ch channel[4];
    struct {
        u32 normal{};
        u32 hblank{};
        u32 vblank{};
    } bit_mask{};
};
}