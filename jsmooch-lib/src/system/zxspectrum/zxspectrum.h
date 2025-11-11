//
// Created by . on 5/2/24.
//

#ifndef JSMOOCH_EMUS_ZXSPECTRUM_H
#define JSMOOCH_EMUS_ZXSPECTRUM_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"

#include "component/cpu/z80/z80.h"
#include "ula.h"
#include "tape_deck.h"

void ZXSpectrum_new(jsm_system* system, enum ZXSpectrum_variants variant);
void ZXSpectrum_delete(jsm_system* system);

struct ZXSpectrum {
    struct ZXSpectrum_ULA ula;
    struct ZXSpectrum_tape_deck tape_deck;
    struct Z80 cpu;

    enum ZXSpectrum_variants variant;

    /*u8 ROM[16*1024];
    u8 RAM[48*1024];*/
    u8 *ROM;
    u32 ROM_size;
    u32 ROM_mask;

    u8 *RAM;
    u32 RAM_size;
    u32 RAM_mask;

    struct {
        u8 *display;
        u8 *ROM;
        u8 *RAM[4];
        u8 disable;
    } bank;


    struct {
        u32 frames_since_restart;
        u32 master_frame;

        i32 ula_x, ula_y;
        u32 screen_bottom;
        u32 screen_right;
        u32 scanlines_before_frame;
        u32 ula_frame_cycle;

        u64 master_cycles;

        struct {
            u32 bit;
            u32 count;
        } flash;

        u32 contended;
    } clock;

    u32 described_inputs;
    u32 cycles_left;
    u32 display_enabled;
    struct cvec* IOs;

};

void ZXSpectrum_notify_IRQ(ZXSpectrum*, u32 level);

#endif //JSMOOCH_EMUS_ZXSPECTRUM_H
