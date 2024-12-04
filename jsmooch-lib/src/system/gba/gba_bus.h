//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_BUS_H
#define JSMOOCH_EMUS_GBA_BUS_H

#include "gba_clock.h"
#include "gba_ppu.h"

#include "component/cpu/arm7tdmi//arm7tdmi.h"

struct GBA {
    struct ARM7TDMI cpu;
    struct GBA_clock clock;
    //struct GBA_cart cart;
    struct GBA_PPU ppu;
    struct GBA_controller controller;

    struct {
        struct {
            u32 open_bus_data;
        } cpu;
    } io;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    DBG_START
        DBG_CPU_REG_START1
            *R[16]
        DBG_CPU_REG_END1

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(thing)
        DBG_IMAGE_VIEWS_END
    DBG_END

};

u32 GBA_mainbus_read(struct GBA*, u32 addr, u32 sz,u32 open_bus, u32 has_effect);

#endif //JSMOOCH_EMUS_GBA_BUS_H
