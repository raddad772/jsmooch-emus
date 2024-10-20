//
// Created by Dave on 2/4/2024.
//

#ifndef JSMOOCH_EMUS_NES_H
#define JSMOOCH_EMUS_NES_H

#include "component/audio/nes_apu/nes_apu.h"

#include "nes_clock.h"
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
    struct r2A03 cpu;
    struct NES_PPU ppu;
    struct NES_APU apu;

    u32 described_inputs;
    u32 cycles_left;
    u32 display_enabled;
    struct cvec* IOs;

    struct NES_bus bus;
    struct NES_cart cart;

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    DBG_START
        DBG_CPU_REG_START1 *A, *X, *Y, *P, *S, *PC DBG_CPU_REG_END1
        DBG_EVENT_VIEW

        DBG_IMAGE_VIEWS_START
        MDBG_IMAGE_VIEW(nametables)
        DBG_IMAGE_VIEWS_END

        DBG_WAVEFORM_START
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(5)
        DBG_WAVEFORM_END

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
