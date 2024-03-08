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

    struct { // io
#include "generated/io_decls.h"

        u32 PDTRA;
        u32 TOCR;
        u32 TSTR;
        u32 SB_ISTNRM;
        u32 BSCR;
        u32 RFCR;
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

    struct {
#include "generated/holly_decls.h"

        float FPU_CULL_VAL;
        float FPU_PERP_VAL;
        float ISP_BACKGND_D;
        u32 FOG_TABLE[128];
        u32 last_used_buffer;
        u32 cur_output_num;
        u32 *cur_output;
        u32 *out_buffer[2];

        u64 master_frame;
    } holly;

    struct {
#include "generated/gdrom_decls.h"
    } gdrom;

    struct {
#include "generated/maple_decls.h"
    } maple;

    struct {
        u64 (*read[0x40])(struct DC*, u32, enum DC_MEM_SIZE, u32*);
        void (*write[0x40])(struct DC*, u32, u64, enum DC_MEM_SIZE, u32*);
    } mem;

};


void DC_mem_init(struct DC* this);
void DC_raise_interrupt(struct DC* this, u32 imask);
void DC_recalc_interrupts(struct DC* this);

#endif //JSMOOCH_EMUS_DREAMCAST_H
