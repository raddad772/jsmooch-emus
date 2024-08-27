//
// Created by Dave on 2/4/2024.
//

#ifndef JSMOOCH_EMUS_NES_H
#define JSMOOCH_EMUS_NES_H

#include "nes_clock.h"
#include "nes_bus.h"
#include "mappers/mapper.h"
#include "nes_cart.h"
#include "nes_cpu.h"
#include "nes_ppu.h"

void NES_new(struct jsm_system* system);
void NES_delete(struct jsm_system* system);

#define NES_INPUTS_CHASSIS 0
#define NES_INPUTS_CARTRIDGE 1
#define NES_INPUTS_PLAYER1 0
#define NES_INPUTS_PLAYER2 1

#define NES_OUTPUTS_DISPLAY 0

enum NES_TIMINGS {
    NES_NTSC,
    NES_PAL,
    NES_DENDY
};

struct NES {
    struct NES_clock clock;
    //struct NES_bus bus;
    struct r2A03 cpu;
    struct NES_PPU ppu;

    u32 described_inputs;
    u32 cycles_left;
    u32 display_enabled;
    struct cvec* IOs;

    struct NES_mapper bus;
    struct NES_cart cart;

    DBG_START
        DBG_CPU_REG_START *A, *X, *Y, *P, *S, *PC DBG_CPU_REG_END
        DBG_EVENT_VIEW
        DBG_IMAGE_VIEW(nametables)
    DBG_END

    struct {
        struct DBGNESROW {
            struct {
                u32 bg_hide_left_8, bg_enable, emph_bits, bg_pattern_table;
                u32 x_scroll, y_scroll;
            } io;
        } rows[240];

    } dbg_data;
};

#endif //JSMOOCH_EMUS_NES_H
