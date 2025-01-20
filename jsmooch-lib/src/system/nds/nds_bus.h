//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_BUS_H
#define JSMOOCH_EMUS_NDS_BUS_H

#include "nds_clock.h"
#include "nds_ppu.h"
#include "nds_controller.h"
#include "cart/nds_cart.h"

#include "component/cpu/arm7tdmi/arm7tdmi.h"
#include "component/cpu/arm946es/arm946es.h"

struct NDS;
typedef u32 (*NDS_rdfunc)(struct NDS *, u32 addr, u32 sz, u32 access, u32 has_effect);
typedef void (*NDS_wrfunc)(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);

struct NDS_CPU_FIFO {
    u8 data[64];
    u32 head, tail;
    u32 len;
};

struct NDS {
    struct ARM7TDMI arm7;
    struct ARM946ES arm9;
    struct NDS_clock clock;
    struct NDS_PPU ppu;
    struct NDS_controller controller;
    //struct NDS_APU apu;

    struct {
        u32 current_transaction;
    } waitstates;


    struct {
        struct { // Only bits 27-24 are needed to distinguish valid endpoints, mostly.
            NDS_rdfunc read[16];
            NDS_wrfunc write[16];
        } rw[2]; // ARM7, ARM9 maps...
        u8 RAM[4 * 1024 * 1024]; // 4MB RAM
        u8 WRAM_share[32 * 1024];      // 32KB WRAM mappable
        u8 WRAM_arm7[64 * 1024]; // 64KB of WRAM for ARM7 only
        struct {
            u8 data[16 * 1024];
            u8 code[32 * 1024];
        } tcm;
        u8 oam[2048];
        u8 palette[2048];
        u8 internal_3d[248 * 1024];
        u8 wifi[8 * 1024];
        char bios7[16384];
        char bios9[4096];
        char firmware[256 * 1024];
        struct {
            struct {
                u32 base, mask, disabled, val;
            } RAM7, RAM9;
            struct {
                u32 enabled9;
            } gba_cart;
        } io;

        struct NDS_VRAM {
            u8 data[656 * 1024];
            struct {
                struct {
                    u32 mst, ofs;
                } bank[9];
            } io;
            struct {
                u8 *arm9[1024]; // 16KB banks from 06000000 to 06FFFFFF
                // addr >> 14 & 0x3FF
                u8 *arm7[2];    // 2 128KB banks at 06000000. addr < 06400000, slot[0] or [1] & 128KB
            } map;
        } vram;

    } mem;

    struct {
        struct {
            u32 buttons;
            u32 enable, condition;
        } button_irq;
        u32 IE, IF, IME;
        u32 halted;

        u8 POSTFLG;

        struct {
            u32 arm7, arm9, bios, dma;
        } open_bus;

        struct NDS_CPU_FIFO to_arm7, to_arm9;
    } io;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct NDS_DMA_ch {
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
            u32 open_bus;
        } io;

        struct {
            u32 started;
            u32 word_count;
            u32 word_mask;
            u32 src_addr;
            u32 dest_addr;
            u32 src_access, dest_access;
            u32 sz;
            u32 first_run;
            u32 is_sound;
        } op;
        u32 run_counter;
    } dma[4];

    struct NDS_TIMER {
        struct {
            u32 io;
            u32 mask;
            u32 counter;
        } divider;

        u32 shift;

        u64 enable_at; // cycle # we'll be enabled at
        u64 overflow_at; // cycle # we'll overflow at
        u32 cascade;
        u16 val_at_stop;
        u32 irq_on_overflow;
        u16 reload;
        u64 reload_ticks;
    } timer[4];

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    struct {
        struct NDS_DBG_line {
            u8 window_coverage[256]; // 256 4-bit values, bit 1 = on, bit 0 = off
            struct NDS_DBG_line_bg {
                u32 hscroll, vscroll;
                i32 hpos, vpos;
                i32 x_lerp, y_lerp;
                i32 pa, pb, pc, pd;
                u32 reset_x, reset_y;
                u32 htiles, vtiles;
                u32 display_overflow;
                u32 screen_base_block, character_base_block;
                u32 priority;
                u32 bpp8;
            } bg[4];
            u32 bg_mode;
        } line[160];
        struct NDS_DBG_tilemap_line_bg {
            u8 lines[1024 * 128]; // 4, 128*8 1-bit "was rendered or not" values
        } bg_scrolls[4];
        struct {
            u32 enable;
            char str[257];
        } mgba;
    } dbg_info;

    DBG_START
        DBG_EVENT_VIEW

        DBG_CPU_REG_START(arm7)
            *R[16]
        DBG_CPU_REG_END(arm7)

        DBG_CPU_REG_START(arm9)
                    *R[16]
        DBG_CPU_REG_END(arm9)

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(a_window0)
            MDBG_IMAGE_VIEW(a_window1)
            MDBG_IMAGE_VIEW(a_window2)
            MDBG_IMAGE_VIEW(a_window3)
            MDBG_IMAGE_VIEW(a_bg0)
            MDBG_IMAGE_VIEW(a_bg1)
            MDBG_IMAGE_VIEW(a_bg2)
            MDBG_IMAGE_VIEW(a_bg3)
            MDBG_IMAGE_VIEW(a_bg0map)
            MDBG_IMAGE_VIEW(a_bg1map)
            MDBG_IMAGE_VIEW(a_bg2map)
            MDBG_IMAGE_VIEW(a_bg3map)
            MDBG_IMAGE_VIEW(a_sprites)
            MDBG_IMAGE_VIEW(a_palettes)
            MDBG_IMAGE_VIEW(a_tiles)
            MDBG_IMAGE_VIEW(b_window0)
            MDBG_IMAGE_VIEW(b_window1)
            MDBG_IMAGE_VIEW(b_window2)
            MDBG_IMAGE_VIEW(b_window3)
            MDBG_IMAGE_VIEW(b_bg0)
            MDBG_IMAGE_VIEW(b_bg1)
            MDBG_IMAGE_VIEW(b_bg2)
            MDBG_IMAGE_VIEW(b_bg3)
            MDBG_IMAGE_VIEW(b_bg0map)
            MDBG_IMAGE_VIEW(b_bg1map)
            MDBG_IMAGE_VIEW(b_bg2map)
            MDBG_IMAGE_VIEW(b_bg3map)
            MDBG_IMAGE_VIEW(b_sprites)
            MDBG_IMAGE_VIEW(b_palettes)
            MDBG_IMAGE_VIEW(b_tiles)
        DBG_IMAGE_VIEWS_END
        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(6)
        DBG_WAVEFORM_END1
    DBG_END

};

/*
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
 */
u32 NDS_mainbus_read7(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
void NDS_mainbus_write7(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_mainbus_fetchins7(void *ptr, u32 addr, u32 sz, u32 access);

u32 NDS_mainbus_read9(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
void NDS_mainbus_write9(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_mainbus_fetchins9(void *ptr, u32 addr, u32 sz, u32 access);


void NDS_bus_init(struct NDS *);
void NDS_eval_irqs(struct NDS *);
void NDS_check_dma_at_hblank(struct NDS *);
void NDS_check_dma_at_vblank(struct NDS *);
u32 NDS_open_bus_byte(struct NDS *, u32 addr);
u32 NDS_open_bus(struct NDS *this, u32 addr, u32 sz);
void NDS_dma_start(struct NDS_DMA_ch *ch, u32 i, u32 is_sound);
u64 NDS_clock_current(struct NDS *);

#endif //JSMOOCH_EMUS_NDS_BUS_H
