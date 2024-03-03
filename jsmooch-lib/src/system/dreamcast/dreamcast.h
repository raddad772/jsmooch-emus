//
// Created by Dave on 2/11/2024.
//

#ifndef JSMOOCH_EMUS_DREAMCAST_H
#define JSMOOCH_EMUS_DREAMCAST_H

#include "helpers/buf.h"
#include "helpers/sys_interface.h"
#include "helpers/scheduler.h"

#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "component/cpu/sh4/sh4_interpreter.h"
#include "dc_mem.h"

#define DC_CYCLES_PER_SEC 200000000

#define DC_INT_VBLANK_IN 0x08
#define DC_INT_VBLANK_OUT 0x10

enum DC_MEM_SIZE {
    DC8 = 1,
    DC16 = 2,
    DC32 = 4,
    DC64 = 8
};

void DC_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void DC_delete(struct jsm_system* system);

struct DC {
    struct SH4 sh4;

    u8 RAM[16 * 1024 * 1024];
    u8 VRAM[8 * 1024 * 1024];
    u8 OC[8 * 1024]; // Operand Cache[

    struct buf BIOS;
    struct buf flash;
    struct buf ROM;

    struct scheduler_t scheduler;

    struct {
        u32 TCOR0;
        u32 TCNT0;
        u32 PCTRA;
        u32 PDTRA;
        u32 TOCR;
        u32 TSTR;
        u32 SDMR;
        u32 SB_ISTNRM;
        u32 SB_LMMODE0;
        u32 SB_LMMODE1;
        u32 SB_IML2NRM;
        u32 SB_IML4NRM;
        u32 SB_IML6NRM;

        u32 BSCR;
        u32 BSCR2;
        u32 RTCOR;
        u32 RFCR;
        u32 RTCSR;
        u32 WCR1;
        u32 WCR2;
        u32 MCR; // memory control register
        u32 MMUCR;
    } io;

    struct {
        u32 ARMRST;
    } aica;

    struct {
        u32 frame_cycle;
        u32 cycles_per_frame;
        u32 cycles_per_line;
        u32 in_vblank;

        struct {
            u32 vblank_in_start;
            u32 vblank_in_end;
            u32 vblank_out_start;
            u32 vblank_out_end;

            u32 vblank_in_yet;
            u32 vblank_out_yet;

        } interrupt;
    } clock;

#include "holly_regs.h"

    struct {
        u32 (*read_map[0x100])(struct DC*, u32, enum DC_MEM_SIZE);
        void (*write_map[0x100])(struct DC*, u32, u32, enum DC_MEM_SIZE);
    } mem;

};


void DC_mem_init(struct DC* this);
void DC_raise_interrupt(struct DC* this, u32 imask);
void DC_recalc_interrupts(struct DC* this);

#endif //JSMOOCH_EMUS_DREAMCAST_H
