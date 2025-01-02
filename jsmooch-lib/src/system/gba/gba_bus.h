//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_BUS_H
#define JSMOOCH_EMUS_GBA_BUS_H

#include "gba_clock.h"
#include "gba_ppu.h"
#include "gba_apu.h"
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
    struct GBA_APU apu;

    struct { // Only bits 27-24 are needed to distinguish valid endpoints
        GBA_rdfunc read[16];
        GBA_wrfunc write[16];
    } mem;

    struct {
        u32 current_transaction;
    } waitstates;

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
        struct {
            u32 a,b,up,down,left,right,start,select,l,r;
            u32 enable, condition;
        } button_irq;
        u32 IE, IF, IME;
        u32 halted;
        u32 open_bus;

        // Unsupported-yet stub stuff
        u32 W8;
        struct {
            u32 general_purpose_data;
            u32 control;
            u32 send;
            u32 multi[4];
        } SIO;
    } io;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    i32 cycles_to_execute;
    i32 cycles_executed;

    struct GBA_DMA_ch {
        struct {
            u32 src_addr; // 28 bits
            u32 dest_addr; // 28 bits
            u32 word_count; // 14 bits on ch0-2, 16bit on ch3

            u32 dest_addr_ctrl;
            u32 src_addr_ctrl;
            u32 repeat;
            u32 transfer_size;
            u32 game_pak_drq; // ch3 only
            u32 start_timing;
            u32 irq_on_end;
            u32 enable;
        } io;

        struct {
            u32 started;
            u32 word_count;
            u32 word_mask;
            u32 src_addr;
            u32 dest_addr;
            u32 sz;
            u32 first_run;
        } op;
    } dma[4];

    struct GBA_TIMER {
        struct {
            u32 io;
            u32 mask;
            u32 counter;
        } divider;

        u32 enable;
        u32 cascade;
        u32 irq_on_overflow;

        struct {
            u16 val;
            u16 reload;
        } counter;
    } timer[4];

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    struct {
        struct GBA_DBG_line {
            u8 window_coverage[240]; // 240 4-bit values, bit 1 = on, bit 0 = off
            struct GBA_DBG_line_bg {
                u32 hscroll, vscroll;
                i32 hpos, vpos;
                i32 pa, pb, pc, pd;
                u32 htiles, vtiles;
                u32 display_overflow;
                u32 screen_base_block, character_base_block;
                u32 priority;
                u32 bpp8;
            } bg[4];
            u32 bg_mode;
        } line[160];
    } dbg_info;

    DBG_START
        DBG_CPU_REG_START1
            *R[16]
        DBG_CPU_REG_END1

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(window0)
            MDBG_IMAGE_VIEW(window1)
            MDBG_IMAGE_VIEW(window2)
            MDBG_IMAGE_VIEW(window3)
            MDBG_IMAGE_VIEW(bg0)
            MDBG_IMAGE_VIEW(bg1)
            MDBG_IMAGE_VIEW(bg2)
            MDBG_IMAGE_VIEW(bg3)
            MDBG_IMAGE_VIEW(sprites)
            MDBG_IMAGE_VIEW(palettes)
            MDBG_IMAGE_VIEW(tiles)
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
void GBA_check_dma_at_hblank(struct GBA *);
void GBA_check_dma_at_vblank(struct GBA *);
#endif //JSMOOCH_EMUS_GBA_BUS_H
