//
// Created by . on 6/18/25.
// HUC6270 GPU for TurboGrafx-16 and later consoles too

#pragma once

#include "helpers/scheduler.h"
#include "helpers/int.h"
#include "helpers/debugger/debuggerdefs.h"

namespace HUC6270 {

enum states {
    S_sync_window,
    S_wait_for_display,
    S_display,
    S_wait_for_sync_window
};

struct sprite {
    sprite();
    u64 pattern_shifter{};
    u64 num_left{};
    u32 triggered{};
    u32 original_num{};
    i32 x{};
    u32 palette{}, priority{};
};


struct chip {
    explicit chip(scheduler_t *scheduler_in) : scheduler(scheduler_in) {}
    void cycle();
    void reset();
    void update_irqs();
    static void vram_vram_end(void *ptr, u64 key, u64 clock, u32 jitter);
    static void update_RCR(void *ptr, u64 key, u64 clock, u32 jitter);
    static void vram_satb_end(void *ptr, u64 key, u64 clock, u32 jitter);
    void write(u32 addr, u8 val);
    u8 read(u32 addr, u8 old);

    u8 read_status();

    scheduler_t *scheduler{};
    u16 VRAM[0x8000]{};
    u16 SAT[0x100]{};

    struct {
        u32 vblank_in_y{}, vblank_out_y{};
        u32 px_width{}, px_height{};

        struct {
            states state{};
            i32 counter{};
        } h, v{};
    } timing{};


    struct {
        UN16 RXR{}, BXR{}, BYR{};
        UN16 MAWR{}, MARR{}, VWR{}, VRR{};
        UN16 SOUR{}, DESR{}, LENR{}, DVSSR{};

        UN16 RCR{};

        union {
            struct {
                u16 IE : 4;
                u16 EX : 2;
                u16 SB : 1;
                u16 BB : 1;
                u16 TE : 2;
                u16 DR : 1;
                u16 IW : 2;
            };
            struct {
                u8 lo;
                u8 hi;
            };
            u16 u{};
        } CR{};
        u8 ADDR{};

        union {
            struct {
                u8 CR : 1; // Collision detect sprite #0 collide with 1-63
                u8 OR : 1; // >16 sprite/line, not enough time for sprites, CGX error
                u8 RR : 1; // scanning line detect
                u8 DS : 1; // VRAM->SATB complete
                u8 DV : 1; // VRAM->VRAM complete
                u8 VD : 1; // vblank started
                u8 BSY : 1; // read or write is happening currently in response to CPU
            };
            u8 u{};

        } STATUS{};
        u8 HSW{}, HDS{}, HDW{}, HDE{}, VSW{}, VDS{};
        UN16 VDW{};
        u8 VCR{};
        struct {
            u32 x_tiles{32}, y_tiles{32};
            u32 x_tiles_mask{}, y_tiles_mask{};
        } bg{};

        union {
            struct {
                u8 DSC : 1; // vram-satb interrupt enable
                u8 DVC : 1; // vram-vram interrupt enable
                u8 SI_D : 1; // Source addr inc/dec 0=inc
                u8 DI_D : 1; // Dest addr inc/dec 0=inc
                u8 DSR : 1; // VRAM-SATB auto-repeat
            };
            u8 u{};

        } DCR{};
    } io{};

    struct {
        u32 line{};
        void (*update_func)(void *, u32){};
        void *update_func_ptr{};
    } irq{};

    struct {
        u32 sprites_on{}, bg_on{};
    } latch{};

    struct {
        u32 x_tiles{}, y_tiles{};
        u32 x_tiles_mask{}, y_tiles_mask{};
        u32 y_compare{};

        u32 x_tile{}, y_tile{};
    } bg{};

    struct {
        u32 y_compare{};
        u32 num_tiles_on_line{};
        sprite tiles[16];
    } sprites{};

    struct {
        u32 num{};
        u64 pattern_shifter{};
        u32 palette{};
    } pixel_shifter{};

    struct {
        i32 yscroll{}, next_yscroll{};
        u16 vram_inc{};

        u32 BAT_size{1024};

        /*struct {
            u32 vals[32]{}; // max 32 long!
            u32 num{};
            u32 head{}, tail{};
        } px_out_fifo{};*/

        u32 px_out{};
        u32 y_counter{};
        u32 blank_line{};
        u32 ignore_hsync{}, ignore_vsync{};
        u32 first_render{};
        u32 draw_clock{};

        u32 vram_satb_pending{};
        bool in_vblank{};

        u32 IE{};
        u32 x_counter{};
        u32 HDW{};
        u32 divisor{};
    } regs{};

    DBG_START
        DBG_EVENT_VIEW_START
        WRITE_VRAM{}, WRITE_RCR{}, HIT_RCR{}, WRITE_XSCROLL{}, WRITE_YSCROLL
        DBG_EVENT_VIEW_END
        events_view *evptr{};
    DBG_END

private:
    void force_new_frame();
    void force_new_line();
    void hsync(bool val);
    void vsync(bool val);
    void new_v_state(states st);
    void new_h_state(states st);
    void setup_new_line();
    void eval_sprites();
    void setup_new_frame();
    void write_VRAM(u32 addr, u16 val);
    void vram_satb();
    void trigger_vram_satb();
    void trigger_vram_vram();
    void hblank(bool val);
    void vblank(bool val);
    void write_addr(u8 val);
    void update_ie();
    void write_lsb(u8 val);
    void write_msb(u8 val);
    [[nodiscard]] u16 read_VRAM(u32 addr) const { return (addr & 0x8000) ? 0 : this->VRAM[addr]; }
};


}

