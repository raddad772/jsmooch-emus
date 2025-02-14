//
// Created by . on 2/13/25.
//

#ifndef JSMOOCH_EMUS_GALAKSIJA_BUS_H
#define JSMOOCH_EMUS_GALAKSIJA_BUS_H

#include "component/cpu/z80/z80.h"

/*
 * 56 top overscan
 * 208 visible
 * 56 bottom overscan
 * = 320 lines
 *
 * IRQ fires at beginning of visible scanline 0 in hblank area
 * X timing is 32- 128 - 32 = 192. so 192x320 4:3
 * y=56, x=0 is interrupt
 * 2 pixels per 1 CPU clock
 *
 *
 * character ROM addressing works as so,
 * A10-A7 are from IO latch,
 * A6-A0 come from CPU
 *
 * bitmap is 1=off, 0=on
 * MSB on the left so shift out hi bits
 * pixel data goes into shift register
 */

struct galaksija {
    struct Z80 z80;

    u8 ROMA[4096];
    u8 ROMB[4096];
    u32 ROMB_present;
    u8 RAM[6 * 1024];

    u8 ROM_char[2048];

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    struct {
        u8 latch;
        u32 open_bus;
    } io;

    struct {
        u64 master_cycle_count;
        u64 master_frame;
        u32 z80_divider;
    } clock;

    struct {
        u8 *cur_output;
        struct cvec_ptr display_ptr;
        struct JSM_DISPLAY *display;
        u32 x, y;
        u64 scanline_start;

        u8 shift_register;
        u32 shift_count;
    } crt;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
    } jsm;

    DBG_START
        DBG_EVENT_VIEW

        DBG_CPU_REG_START1 *A, *B, *C, *D, *E, *HL, *F, *AF_, *BC_, *DE_, *HL_, *PC, *SP, *IX, *IY, *EI, *HALT, *CE DBG_CPU_REG_END1

        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(2)
        DBG_WAVEFORM_END1
    DBG_END
};


u32 galaksija_mainbus_read(struct galaksija *, u32 addr, u32 open_bus, u32 has_effect);
void galaksija_mainbus_write(struct galaksija *, u32 addr, u32 val);
void galaksija_bus_init(struct galaksija *);
void galaksija_cycle(struct galaksija *);
#endif //JSMOOCH_EMUS_GALAKSIJA_BUS_H
