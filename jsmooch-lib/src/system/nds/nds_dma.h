//
// Created by . on 1/20/25.
//

#pragma once

#include "helpers/int.h"

/*
0  Start Immediately
  1  Start at V-Blank
  2  Start at H-Blank (paused during V-Blank)
  3  Synchronize to start of display
  4  Main memory display
  5  DS Cartridge Slot
  6  GBA Cartridge Slot
  7  Geometry Command FIFO */
namespace NDS {
enum DMA_start_timings {
    DMA_IMMEDIATE=0,
    DMA_VBLANK=1,
    DMA_HBLANK=2,
    DMA_START_OF_DISPLAY=3,
    DMA_MAIN_MEMORY_DISPLAY=4,
    DMA_DS_CART_SLOT=5,
    DMA_GBA_CART_SLOT=6,
    DMA_GE_FIFO=7
};

struct DMA_ch {
    u32 active{};
    u32 num{};
    struct {
        u32 src_addr{}; // 28 bits
        u32 dest_addr{}; // 28 bits
        u32 word_count{}; // 14 bits on ch0-2{}, 16bit on ch3

        u32 dest_addr_ctrl{};
        u32 src_addr_ctrl{};
        u32 repeat{};
        u32 transfer_size{};

        u32 start_timing{};
        u32 irq_on_end{};
        u32 enable{};
        u32 open_bus{};
    } io{};

    struct {
        u32 started{};
        u32 word_count{};
        u32 word_mask{};
        u32 src_addr{};
        u32 dest_addr{};
        u32 src_access{}, dest_access{};
        u8 sz{};
        u32 first_run{};
        u32 is_sound{};
        i32 chunks{};
        // for mode GE_FIFO
    } op{};
    u32 run_counter{};
};
}