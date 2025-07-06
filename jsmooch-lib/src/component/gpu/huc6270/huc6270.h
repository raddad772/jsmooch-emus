//
// Created by . on 6/18/25.
// HUC6270 GPU for TurboGrafx-16 and later consoles too

#ifndef JSMOOCH_EMUS_HUC6270_H
#define JSMOOCH_EMUS_HUC6270_H

#include "helpers/scheduler.h"
#include "helpers/int.h"


struct HUC6270 {
    u16 VRAM[0x8000];

    struct {
        union UN16 RXR, BXR, BYR;
        union UN16 MAWR, MARR, VWR, VRR;
        union UN16 SOUR, DESR, LENR, DVSSR;

        union UN16 RCR;

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
            union {
                u8 lo;
                u8 hi;
            };
            u16 u;
        } CR;
        u8 ADDR;

        union {
            struct {
                u8 CR : 1; // Collision detect sprite #0 collide with 1-63
                u8 OR : 1; // >17 sprite/line, not enough time for sprites, CGX error
                u8 RR : 1; // scanning line detect
                u8 DS : 1; // VRAM->SATB complete
                u8 DV : 1; // VRAM->VRAM complete
                u8 VD : 1; // vblank started
                u8 BSY : 1; // read or write is happening currently in response to CPU
            };
            u8 u;

        } STATUS;
        u8 HSW, HDS, HDW, HDE, VSW, VDS;
        union UN16 VDW;
        u8 VCR;
        struct {
            u32 x_tiles, y_tiles;
            u32 x_tiles_mask, y_tiles_mask;
        } bg;
    } io;

    struct {
        u32 IR;
        u32 line;
        void (*update_func)(void *, u32);
        void *update_func_ptr;
    } irq;

    struct {
        u32 sprites_on, bg_on;
    } latch;

    struct {
        u32 x_tiles, y_tiles;
        u32 x_tiles_mask, y_tiles_mask;
    } bg;

    struct {
        u32 vblank_in_y, vblank_out_y;
        u32 px_width, px_height;

    } screen;

    struct {
        u32 y;

        i32 yscroll, next_yscroll;
        u16 vram_inc;

        u32 px_out;
    } regs;
};


void HUC6270_init(struct HUC6270 *);
void HUC6270_delete(struct HUC6270 *);
void HUC6270_reset(struct HUC6270 *);
void HUC6270_write(struct HUC6270 *, u32 addr, u32 val);
u32 HUC6270_read(struct HUC6270 *, u32 addr, u32 old);
void HU6270_cycle(struct HUC6270 *);
void HUC6270_hsync(struct HUC6270 *, u32 val);
void HUC6270_vsync(struct HUC6270 *, u32 val);

#endif //JSMOOCH_EMUS_HUC6270_H
