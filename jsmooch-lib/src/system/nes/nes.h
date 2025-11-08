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
    NES_clock clock{};
    NES_APU apu{};
    r2A03 cpu;
    NES_PPU ppu;

    NES();
    ~NES();

    u32 described_inputs=0;
    u32 cycles_left=0;
    u32 display_enabled=0;
    struct cvec* IOs;

    struct NES_bus bus{};
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

        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(5)
        DBG_WAVEFORM_END1

    DBG_END

    struct NESDBGDATA {
        struct DBGNESROW {
            struct {
                u32 bg_hide_left_8, bg_enable, emph_bits, bg_pattern_table;
                u32 x_scroll, y_scroll;
            } io;
        } rows[240];

    } dbg_data;
};

#endif //JSMOOCH_EMUS_NES_H
