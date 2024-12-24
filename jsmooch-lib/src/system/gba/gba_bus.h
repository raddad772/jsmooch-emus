//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_BUS_H
#define JSMOOCH_EMUS_GBA_BUS_H

#include "gba_clock.h"
#include "gba_ppu.h"
#include "gba_controller.h"
#include "gba_cart.h"

#include "component/cpu/arm7tdmi/arm7tdmi.h"

struct GBA;
typedef u32 (*GBA_rdfunc)(struct GBA *, u32 addr, u32 sz, u32 access, u32 has_effect);
typedef void (*GBA_wrfunc)(struct GBA *, u32 addr, u32 sz, u32 access, u32 val);

struct GBA {
    struct ARM7TDMI cpu;
    struct GBA_clock clock;
    struct GBA_cart cart;
    struct GBA_PPU ppu;
    struct GBA_controller controller;

    struct { // Only bits 27-24 are needed to distinguish valid endpoints
        GBA_rdfunc read[16];
        GBA_wrfunc write[16];
    } mem;

    char WRAM_slow[256 * 1024];
    char WRAM_fast[32 * 1024];

    struct {
        u32 has;
        char data[16384];
    } BIOS;

    struct {
        struct {
            u32 open_bus_data;
        } cpu;
        u32 IE, IF, IME;
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

/*
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
 */
u32 GBA_mainbus_read(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
void GBA_mainbus_write(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
u32 GBA_mainbus_fetchins(void *ptr, u32 addr, u32 sz, u32 access);

void GBA_bus_init(struct GBA *);
void GBA_eval_irqs(struct GBA *);
#endif //JSMOOCH_EMUS_GBA_BUS_H
