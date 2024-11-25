//
// Created by . on 6/1/24.
//

/*
 * NOTE: parts of the DMA subsystem are almost directly from Ares.
 * I wanted to get it right and write a good article about it. Finding accurate
 *  info was VERY difficult, so I kept very close to the best source of
 *  info I could find - Ares.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"


#define PLANE_A 0
#define PLANE_B 1
#define PLANE_WINDOW 2

#define SC_ARRAY_H32      0
#define SC_ARRAY_H40      1
#define SC_ARRAY_DISABLED 2 // H32 timing
#define SC_ARRAY_H40_VBLANK 3


#define MCLOCKS_PER_LINE 3420
#define DEBUG_PIXELS_PER_LINE 440
#define DEBUG_MCLOCK_DIVISOR (MCLOCKS_PER_LINE / DEBUG_PIXELS_PER_LINE)
//#define MCLOCKS_PER_LINE 3400
#define SC_HSYNC_GOES_OFF 5
#define SC_HSYNC_GOES_ON 658

#define SC_SLOW_SLOTS 662
#define HSYNC_GOES_OFF (SC_HSYNC_GOES_OFF * 5)
#define HSYNC_GOES_ON (SC_HSYNC_GOES_ON*5)
#define SLOW_MCLOCKS_START (SC_SLOW_SLOTS*5)
#define HBLANK_START 1280

//    UDS<<2 + LDS           neither   just LDS   just UDS    UDS+LDS
static u32 write_maskmem[4] = { 0,     0xFF00,    0x00FF,      0 };
static void dma_run_if_ready(struct genesis* this);
static void write_data_port(struct genesis* this, u16 val, u32 is_cpu);
static void render_sprites(struct genesis *this);
static void get_line_hscroll(struct genesis* this, u32 line_num, u32 *planes);
static void get_vscrolls(struct genesis* this, int column, u32 *planes);

static inline u16 VRAM_read(struct genesis* this, u32 addr, int report)
{
    u16 a = this->vdp.VRAM[addr & 0x7FFF];
    if (dbg.traces.vdp4 && report) DFT("\nVRAM READ %04x: %04x", addr, a);
    return a;
}


static void set_m68k_dtack(struct genesis* this)
{
    printf("\nYAR");
    //this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
    //this->io.m68k.stuck = !this->m68k.pins.DTACK;
}

static void VRAM_write(struct genesis* this, u16 addr, u16 val)
{
    assert(this->vdp.io.vram_mode == 0);
    if (dbg.traces.vdp) printf("\nVRAM write %04x: %04x, cyc:%lld", addr & 0x7FFF, val, this->clock.master_cycle_count);
    if (dbg.traces.vdp4) DFT("\nVRAM write %04x: %04x", addr & 0x7FFF, val);
    this->vdp.VRAM[addr & 0x7FFF] = val;
}

#define WSWAP     if (addr & 1) v = (v & 0xFF00) | (val & 0xFF);\
                  else v = (v & 0xFF) | (val & 0xFF00);

static u16 VRAM_read_byte(struct genesis* this, u16 addr)
{
    u16 val = VRAM_read(this, addr, 1);
    u16 v = 0;
    WSWAP;
    return v;
}

static void VRAM_write_byte(struct genesis* this, u16 addr, u16 val)
{
    u16 v = VRAM_read(this, addr >> 1, 1);
    if (dbg.traces.vdp4) DFT("\nVRAM writebyte_r %04x: %04x", addr, v);
    WSWAP;
    if (dbg.traces.vdp4) DFT("\nVRAM writebyte %04x: %04x", addr, v);
    VRAM_write(this, addr >> 1, v);
}

static u16 VSRAM_read(struct genesis* this, u16 addr)
{
    return (addr > 39) ? 0 : this->vdp.VSRAM[addr];
}

static void VSRAM_write(struct genesis* this, u16 addr, u16 val)
{
    if (addr < 40) this->vdp.VSRAM[addr] = val & 0x3FF;
}

static void VSRAM_write_byte(struct genesis* this, u16 addr, u16 val)
{
    u16 v = VSRAM_read(this, addr >> 1);
    WSWAP;
    VSRAM_write(this, addr >> 1, v);
}

static u16 CRAM_read(struct genesis* this, u16 addr)
{
    return this->vdp.CRAM[addr & 0x3F];
}

static void CRAM_write(struct genesis* this, u16 addr, u16 val)
{
    if (dbg.traces.vdp4) DFT("\nCRAM WRITE %04x: %04x", addr & 0x3F, val & 0x1FF);
    this->vdp.CRAM[addr & 0x3F] = val & 0x1FF;
}

static void CRAM_write_byte(struct genesis* this, u16 addr, u16 val)
{
    addr = (addr & 0x7F);
    u16 v = CRAM_read(this, addr >> 1);
    WSWAP;
    CRAM_write(this, addr >> 1, v);
}

static u16 read_counter(struct genesis* this)
{
    if (this->vdp.io.counter_latch) return this->vdp.io.counter_latch_value;
    u16 vcnt = this->clock.vdp.vcount;
    /*if (this->vdp.io.interlace_mode & 1) {
        if (this->vdp.io.interlace_mode & 2) vcnt <<= 1;
        vcnt = (vcnt & 0xFFFE) | ((vcnt >> 8) ^ 1);
    }*/
    if(vcnt >= 0xEB) vcnt += 0xFA;
    return vcnt << 8 | this->clock.vdp.hcount;
}

void init_slot_tables(struct genesis* this)
{
    // scanline h32
#define SR(start,end,value) for (u32 i = (start); i < (end); i++) { s[i] = value; }
    enum slot_kinds *s = this->vdp.slot_array[SC_ARRAY_H32];
    s[0] = slot_hscroll_data;
    SR(1,5,slot_sprite_pattern);
    s[5] = slot_layer_a_mapping;
    s[6] = slot_sprite_pattern;
    SR(7,9,slot_layer_a_pattern);
    s[9] = slot_layer_b_mapping;
    s[10] = slot_sprite_pattern;
    SR(12,14,slot_layer_b_pattern);

    u32 sn = 13;
    for (u32 j = 0; j < 5; j++) { // repeat 5 times
         for (u32 x = 0; x < 4; x++) { // repeat 4 times with minor change
#define S(x) s[sn] = x; sn++
             S(slot_layer_a_mapping);
             if (x == 3) { S(slot_external_access); }
             else { S(slot_refresh_cycle); }
             S(slot_layer_a_pattern);
             S(slot_layer_a_pattern);
             S(slot_layer_b_mapping);
             S(slot_sprite_mapping);
             S(slot_layer_b_pattern);
             S(slot_layer_b_pattern);
        }
    }
    SR(141, 143, slot_external_access);
    SR(143, 156, slot_sprite_pattern);
    s[156] = slot_external_access;
    SR(157, 170, slot_sprite_pattern);
    s[170] = slot_external_access;

    // Now do h40
    s = this->vdp.slot_array[SC_ARRAY_H40];
    s[0] = slot_hscroll_data;
    SR(1,5,slot_sprite_pattern);
    s[5] = slot_layer_a_mapping;
    s[6] = slot_sprite_pattern;
    s[7] = s[8] = slot_layer_a_pattern;
    s[9] = slot_layer_b_mapping;
    s[10] = slot_sprite_pattern;
    s[11] = s[12] = slot_layer_b_pattern;

    sn = 13;
    for (u32 j = 0; j < 5; j++) {
        for (u32 x = 0; x < 4; x++) {
            S(slot_layer_a_mapping);
            if (x == 3) { S(slot_refresh_cycle); }
            else { S(slot_external_access); }
            S(slot_layer_a_pattern);
            S(slot_layer_a_pattern);
            S(slot_layer_b_mapping);
            S(slot_sprite_mapping);
            S(slot_layer_b_pattern);
            S(slot_layer_b_pattern);
        }
    }

    SR(173,175, slot_external_access);
    SR(175,198, slot_sprite_pattern);
    s[198] = slot_external_access;
    SR(199, 209, slot_sprite_pattern);

    // Now do rendering disabled
    s = this->vdp.slot_array[SC_ARRAY_DISABLED];
    //TODO: fill this
    // FOR NOW just put in 1 refersh slot every 16
    for (u32 i = 0; i < 171; i++) {
        if ((i & 7) == 0) { S(slot_external_access); }
        else { S(slot_refresh_cycle); }
    }

    // Now do vblank H40
    s = this->vdp.slot_array[SC_ARRAY_H40];
    for (u32 i = 0; i < 211; i++) {
        if ((i & 7) == 0) { S(slot_refresh_cycle); }
        else { S(slot_external_access); }
    }
}
#undef S
#undef SR

void genesis_VDP_handle_iack(void* ptr)
{
    struct genesis *this = (struct genesis*)ptr;
    if (this->vdp.irq.vblank.enable && this->vdp.irq.vblank.pending) this->vdp.irq.vblank.pending = 0;
    else if (this->vdp.irq.hblank.enable && this->vdp.irq.hblank.pending) this->vdp.irq.hblank.pending = 0;
    genesis_bus_update_irqs(this);
}

void genesis_VDP_init(struct genesis* this)
{
    memset(&this->vdp, 0, sizeof(this->vdp));
    this->vdp.term_out_ptr = &this->vdp.term_out[0];
    init_slot_tables(this);
    M68k_register_iack_handler(&this->m68k, (void*)this, &genesis_VDP_handle_iack);
    this->vdp.last_r = 50000;
}

static void set_clock_divisor(struct genesis* this)
{
    u32 clk = 5;

    if (this->vdp.io.h40 && (this->clock.vdp.line_mclock >= HSYNC_GOES_OFF) && (
            this->clock.vdp.line_mclock < SLOW_MCLOCKS_START))
        clk = 4;

    this->clock.vdp.clock_divisor = (i32)clk;
}

static inline void latch_hcounter(struct genesis* this)
{
    this->vdp.irq.hblank.counter = this->vdp.irq.hblank.reload; // TODO: load this from register
}

static void do_vcounter(struct genesis* this)
{
    if ((this->clock.vdp.vcount >= 0) && (this->clock.vdp.vcount <= 224)) {
        // Do down counter if we're not in line 0 or 225
        if (this->vdp.irq.hblank.counter == 0) {
            this->vdp.irq.hblank.pending = 1;
            latch_hcounter(this);
            genesis_bus_update_irqs(this);
        } else this->vdp.irq.hblank.counter--;
    }
}

static void hblank(struct genesis* this, u32 new_value)
{
    this->clock.vdp.hblank_active = new_value;
    //if (new_value)

}

static void vblank(struct genesis* this, u32 new_value)
{
    u32 old_value = this->clock.vdp.vblank_active;
    this->clock.vdp.vblank_active = new_value;  // Our value that indicates vblank yes or no
    this->vdp.io.vblank = new_value;     // Our IO vblank value, which can be reset

    if (new_value) {
        this->vdp.sc_slot = SC_ARRAY_DISABLED + this->vdp.io.h40; //

        this->vdp.irq.vblank.pending = 1;
        genesis_bus_update_irqs(this);
    }

    set_clock_divisor(this);
}


static void new_frame(struct genesis* this)
{
    if (this->dbg.events.view.vec) event_view_begin_frame(this->dbg.events.view);
    this->vdp.cur_output = ((u16 *)this->vdp.display->output[this->vdp.display->last_written ^ 1]);
    this->clock.master_frame++;
    this->vdp.hscroll_debug = (this->vdp.hscroll_debug + 1) & 511;
    //printf("\nNEW GENESIS FRAME! %lld", this->clock.master_frame);
    this->clock.vdp.field ^= 1;
    this->clock.vdp.vcount = 0;
    this->clock.vdp.vblank_active = 0;
    this->vdp.cur_output = this->vdp.display->output[this->vdp.display->last_written];
    memset(this->vdp.cur_output, 0, 1280*240*2);
    this->vdp.display->last_written ^= 1;
    this->vdp.display->scan_y = 0;
    this->vdp.display->scan_x = 0;

    set_clock_divisor(this);
}

static void set_sc_array(struct genesis* this)
{
    if ((this->clock.vdp.vblank_active) || (!this->vdp.io.enable_display)) {
        this->vdp.sc_array = SC_ARRAY_DISABLED + this->vdp.io.h40;
    }
    else this->vdp.sc_array = this->vdp.io.h40;
}

static void print_scroll_info(struct genesis* this)
{
    printf("\nSCROLL AT LINE %d. HMODE:%d VMODE:%d", this->clock.vdp.vcount, this->vdp.io.hscroll_mode, this->vdp.io.vscroll_mode);
    printf("\nHSCROLL A:%d VSCROLL A:%d", this->vdp.fetcher.hscroll[0], this->vdp.fetcher.vscroll_latch[0]);
    printf("\nFG WIDTH:%d  HEIGHT:%d", this->vdp.io.foreground_width, this->vdp.io.foreground_height);
}

static void new_scanline(struct genesis* this)
{
    this->clock.vdp.vcount++;
    if (this->clock.vdp.vcount == 262) {
        new_frame(this);
    }
    this->vdp.sprite_collision = this->vdp.sprite_overflow = 0;
    this->clock.vdp.hcount = 0;
    this->vdp.display->scan_y++;
    this->vdp.display->scan_x = 0;
    this->vdp.cur_pixel = this->clock.vdp.vcount * 1280;
    assert(this->vdp.cur_pixel < (1280*300));
    assert(this->clock.vdp.vcount < 280);
    if (this->clock.vdp.vcount < 224) {
        this->vdp.line.screen_x = 0;

        // Set up the current row's fetcher...
        this->vdp.fetcher.column = 0;
        this->vdp.fetcher.hscroll[2] = 0; // Reset window
        memset(&this->vdp.ringbuf, 0, sizeof(this->vdp.ringbuf));

        u32 planes[2];
        get_line_hscroll(this, this->clock.vdp.vcount, planes);

        u32 plane_wrap = (8 * this->vdp.io.foreground_width) - 1;

        for (u32 p = 0; p < 2; p++) {
            // Calculate fine X...
            this->vdp.fetcher.fine_x[p] = planes[p] & 15;

            // Unflip the hscroll value...
            this->vdp.fetcher.hscroll[p] = (0 - ((i32)planes[p])) & plane_wrap;
        }

        // DEBUG STUFF NOW
        struct genesis_vdp_debug_row *r = &this->vdp.debug_info[this->clock.vdp.vcount];
        r->h40 = this->vdp.io.h40;
        r->hscroll_mode = this->vdp.io.hscroll_mode;
        r->vscroll_mode = this->vdp.io.vscroll_mode;
        r->video_mode = this->vdp.io.mode5 ? 5 : 4;
        r->hscroll[0] = this->vdp.fetcher.hscroll[0];
        r->hscroll[1] = this->vdp.fetcher.hscroll[1];

        if (this->clock.vdp.vcount == 0) {
            struct genesis_vdp_debug_row *b = &this->vdp.debug_info[223];
            i32 col = -1;
            for (u32 i = 0; i < 21; i++) {
                u32 vscrolls[2];
                get_vscrolls(this, col++, vscrolls);
                r->vscroll[0][i] = vscrolls[0];
                r->vscroll[1][i] = vscrolls[1];
            }
        }

        // Render out sprites...
        render_sprites(this);

        // So, fetches happen in 16-pixel (2-word) groups.
        // There's an extra 16-pixel fetch off the left side of the screen, for fine hscroll &15
    }

    this->vdp.line.screen_x = 0;
    this->vdp.line.screen_y = this->clock.vdp.vcount;

    this->clock.vdp.line_mclock -= MCLOCKS_PER_LINE;
    this->vdp.sc_count = 0;
    this->vdp.sc_slot = 0;
    this->vdp.fetcher.vscroll_latch[0] = this->vdp.VSRAM[0];
    this->vdp.fetcher.vscroll_latch[1] = this->vdp.VSRAM[1];

    switch(this->clock.vdp.vcount) {
        case 0:
            vblank(this, 0);
            break;
        case 0xE0: // vblank start
            vblank(this, 1);
            this->vdp.io.z80_irq_clocks = 2573; // Thanks @MaskOfDestiny
            genesis_z80_interrupt(this, 1); // Z80 asserts vblank interrupt for 2573 mclocks
            break;
    }
    //if (this->clock.vdp.vcount == 0) latch_hcounter(this);
    if (this->clock.vdp.vcount >= 225) latch_hcounter(this); // in vblank, continually reload. TODO: make dependent on vblank
    else do_vcounter(this);
    set_clock_divisor(this);
    set_sc_array(this);
}

static u32 fifo_empty(struct genesis* this)
{
    return this->vdp.fifo.len == 0;
}

static u32 fifo_overfull(struct genesis* this)
{
    return this->vdp.fifo.len > 4;
}

static u32 fifo_full(struct genesis* this)
{
    return this->vdp.fifo.len > 3;
}

static void recalc_window(struct genesis* this)
{/*
    vdp->io.window_h_pos = (val & 0x1F) << 14;
    vdp->io.window_R = (val >> 7) & 1;
    vdp->io.window_v_pos = (val & 0x1F) << 3;
    vdp->io.window_draw_top_to_bottom = (val >> 7) & 1;*/
    // We will
    //this->vdp.window.left_col =
    if (this->vdp.io.window_RIGHT) {
        this->vdp.window.left_col = this->vdp.io.window_h_pos;
        this->vdp.window.right_col = this->vdp.io.h40 ? 40 : 32;
    }
    else {
        this->vdp.window.left_col = 0;
        this->vdp.window.right_col = this->vdp.io.window_h_pos;
    }
    if (this->vdp.io.window_DOWN) {
        this->vdp.window.top_row = this->vdp.io.window_v_pos;
        this->vdp.window.bottom_row = 30;
    }
    else {
        this->vdp.window.top_row = 0;
        this->vdp.window.bottom_row = this->vdp.io.window_v_pos;
    }
    this->vdp.window.nt_base = this->vdp.io.window_table_addr;
    if (this->vdp.io.h40)
        this->vdp.window.nt_base &= 0b111110; // Ignore lowest bit in h40
    this->vdp.window.nt_base <<= 10;
}

static void write_vdp_reg(struct genesis* this, u16 rn, u16 val)
{
    /*if ((rn >= 24)  {
        if (dbg.traces.vdp) dbg_printf("\nWARNING illegal VDP register write %d %04x on cycle:%lld", rn, val, this->clock.master_cycle_count);
        return;
    }*/
    struct genesis_vdp* vdp = &this->vdp;
    u32 va;
    val &= 0xFF;
    switch(rn) {
        case 0: // Mode Register 1
            vdp->io.blank_left_8_pixels = (val >> 5) & 1;
            vdp->irq.hblank.enable = (val >> 4) & 1;
            if ((!vdp->io.counter_latch) && (val & 2))
                vdp->io.counter_latch_value = read_counter(this);
            vdp->io.counter_latch = (val >> 1) & 1;
            vdp->io.enable_overlay = (val & 1) ^ 1;
            genesis_bus_update_irqs(this);
            // TODO: handle more bits
            return;
        case 1: // Mode regiser 2
            vdp->io.vram_mode = (val >> 7) & 1;
            vdp->io.enable_display = (val >> 6) & 1;
            vdp->irq.vblank.enable = (val >> 5) & 1;
            vdp->dma.enable = (val >> 4) & 1;
            vdp->io.cell30 = (val >> 3) & 1;
            // TODO: handle more bits
            if (vdp->io.cell30) printf("\nWARNING PAL DISPLAY 240 SELECTED");
            vdp->io.mode5 = (val >> 2) & 1;
            if (!vdp->io.mode5) printf("\nWARNING MODE5 DISABLED");

            genesis_bus_update_irqs(this);
            dma_run_if_ready(this);
            return;
        case 2: // Plane A table location
            vdp->io.plane_a_table_addr = (val & 0b01111000) << 9;
            return;
        case 3: // Window nametable location
            vdp->io.window_table_addr = (val & 0b111110) >> 1; // lowest bit ignored in H40 << 9
            return;
        case 4: // Plane B location
            vdp->io.plane_b_table_addr = (val & 15) << 12;
            return;
        case 5: // Sprite table location
            vdp->io.sprite_table_addr = (val & 0xFF) << 8;
            return;
        case 6: // um
            //vdp->io.sprite_generator_addr = (vdp->io.sprite_generator_addr & 0x7FFF) | (((val >> 5) & 1) << 15);
            return;
        case 7:
            vdp->io.bg_color = val & 63;
            return;
        case 8: // master system hscroll
            printf("\nwrite SMS hscroll %d", val & 0xFF);
            return;
        case 9: // master system vscroll
            printf("\nwrite SMS vscroll %d", val & 0xFF);
            return;
        case 10: // horizontal interrupt counter
            vdp->irq.hblank.reload = val;
            return;
        case 11: // Mode register 3
            vdp->io.enable_th_interrupts = (val >> 3) & 1;
            vdp->io.vscroll_mode = (val >> 2) & 1;
            vdp->io.hscroll_mode = val & 3;
            // TODO: do all of these
            return;
        case 12: // Mode register 4
            // TODO: handle more of these
            vdp->io.h32 = (val & 1) == 0;
            vdp->io.h40 = (val & 1) != 0;
            set_sc_array(this);
            if (vdp->io.h32) printf("\n32-cell 256 pixel mode selected");
            if (vdp->io.h40) printf("\n40-cell 320 pixel mode selected");
            set_clock_divisor(this);
            recalc_window(this);
            vdp->io.enable_shadow_highlight = (val >> 3) & 1;
            vdp->io.interlace_mode = (val >> 1) & 3;
            return;
        case 13: // horizontal scroll data addr
            vdp->io.hscroll_addr = (val & 0x7F) << 9;
            printf("\nNEW HSCROLL ADDR %04x", vdp->io.hscroll_addr);
            return;
        case 14: // mostly unused
            return;
        case 15:
            vdp->command.increment = val & 0xFF;
            return;
        case 16:
            va = (val &3);
            switch(va) {
                case 0:
                    vdp->io.foreground_width = 32;
                    break;
                case 1:
                    vdp->io.foreground_width = 64;
                    break;
                case 2:
                    printf("\nWARNING BAD FOREGROUND WIDTH");
                    break;
                case 3:
                    vdp->io.foreground_width = 128;
                    break;
            }
            va = (val >> 4) & 3;
            switch(va) {
                case 0:
                    vdp->io.foreground_height = 32;
                    break;
                case 1:
                    vdp->io.foreground_height = 64;
                    break;
                case 2:
                    printf("\nWARNING BAD FOREGROUND HEIGHT");
                    break;
                case 3:
                    vdp->io.foreground_width = 128;
                    break;
            }
            vdp->io.foreground_width_mask = vdp->io.foreground_width - 1;
            vdp->io.foreground_height_mask = vdp->io.foreground_height - 1;

            // 64x128, 128x64, and 128x128 are invalid
            u32 total = vdp->io.foreground_width * vdp->io.foreground_height;
            if (total > 4096) {
                printf("\nWARNING INVALID FOREGROUND HEIGHT AND WIDTH COMBO");
            }
            return;
        case 17: // window plane horizontal position
            vdp->io.window_h_pos = val & 0x1F;
            vdp->io.window_RIGHT = (val >> 7) & 1;
            recalc_window(this);
            return;
        case 18: // window plane vertical postions
            vdp->io.window_v_pos = (val & 0x1F);
            vdp->io.window_DOWN = (val >> 7) & 1;
            recalc_window(this);
            return;
        case 19:
            vdp->dma.len = (vdp->dma.len & 0xFF00) | (val & 0xFF);
            if (dbg.traces.vdp2) DFT("\nWR DMALEN(13) %04x %02x", vdp->dma.len, val);
            return;
        case 20:
            vdp->dma.len = (vdp->dma.len & 0xFF) | ((val << 8) & 0xFF00);
            if (dbg.traces.vdp2) DFT("\nWR DMALEN(14) %04x %02x", vdp->dma.len, val);
            return;
        case 21:
            // source be 22 bits
            vdp->dma.source_address = (vdp->dma.source_address & 0xFFFF00) | (val & 0xFF);
            if (dbg.traces.vdp2) DFT("\nWR DMA ADDR(15) %06x", vdp->dma.source_address);
            return;
        case 22:
            vdp->dma.source_address = (vdp->dma.source_address & 0xFF00FF) | ((val << 8) & 0xFF00);
            if (dbg.traces.vdp2) DFT("\nWR DMA ADDR(16) %06x", vdp->dma.source_address);
            return;
        case 23:
            vdp->dma.source_address = (vdp->dma.source_address & 0xFFFF) | (((val & 0x3F) << 16) & 0x7F0000);
            vdp->dma.mode = (val >> 6) & 3;
            vdp->dma.fill_pending = (val >> 7) & 1;
            if (dbg.traces.vdp) printf("\nMODE! %d FILL PENDING! %d", this->vdp.dma.mode, this->vdp.dma.fill_pending);
            if (dbg.traces.vdp2) DFT("\nWR DMA ADDR(17) %06x MODE:%d PEND:%d WAIT:%d", vdp->dma.source_address, vdp->dma.mode, vdp->command.pending, vdp->dma.fill_pending);
            dma_run_if_ready(this);
            return;
        case 29: // KGEN debug register, enable debugger
            dbg_break("KGEN debugger", this->clock.master_cycle_count);
            break;

        case 30: // character for terminal out
            if (val == 0) {
                *this->vdp.term_out_ptr = 0;
                this->vdp.term_out_ptr = &this->vdp.term_out[0];
                printf("[GEN CONSOLE] %s", this->vdp.term_out);
                this->vdp.term_out[0] = 0;
            }
            else {
                u64 sz = (this->vdp.term_out_ptr - (&this->vdp.term_out[0]));
                if (sz > 16382) {
                    printf("\nGEN TERM OVERFLOW! FLUSHING!");
                    *this->vdp.term_out_ptr = 0;
                    this->vdp.term_out_ptr = &this->vdp.term_out[0];
                    printf("[GEN CONSOLE] %s", this->vdp.term_out);
                    this->vdp.term_out[0] = 0;
                }
                *this->vdp.term_out_ptr = val;
                this->vdp.term_out_ptr++;
            }
            break;
        default:
            printf("\nUNHANDLED VDP REG WRITE %02x: %02x", rn, val);
            break;
    }
}

static void dma_source_inc(struct genesis* this)
{
    this->vdp.dma.source_address = (this->vdp.dma.source_address & 0xFFFF0000) | ((this->vdp.dma.source_address + 1) & 0xFFFF);
}

static void dma_load(struct genesis* this)
{
    this->vdp.dma.active = 1;

    u32 addr = ((this->vdp.dma.mode & 1) << 23) | (this->vdp.dma.source_address << 1);
    u16 data = genesis_mainbus_read(this, addr & 0xFFFFFF, 1, 1, 0, 1);
    if (dbg.traces.vdp2) DFT("\nVDP DMA LOAD ADDR:%04x DATA:%04x LEN:%04x", (u32)addr, (u32)data, (u32)this->vdp.dma.len);
    write_data_port(this, data, 0);

    dma_source_inc(this);
    if (--this->vdp.dma.len == 0) {
        this->vdp.command.pending = 0;
        this->vdp.dma.active = 0;
    }
    this->vdp.sc_skip = 1; // every-other
}

static void dma_fill(struct genesis* this)
{
    this->vdp.dma.active = 1;
    switch(this->vdp.command.target) {
        case 1:
            if (dbg.traces.vdp2) DFT("\nVDP DMA FILL VRAM ADDR:%04x DATA:%02x", this->vdp.command.address, this->vdp.dma.fill_value);
            VRAM_write_byte(this, this->vdp.command.address, this->vdp.dma.fill_value);
            break;
        case 3:
            if (dbg.traces.vdp2) DFT("\nVDP DMA FILL CRAM ADDR:%04x DATA:%02x", this->vdp.command.address, this->vdp.dma.fill_value);
            CRAM_write_byte(this, this->vdp.command.address, this->vdp.dma.fill_value);
            break;
        case 5:
            VSRAM_write_byte(this, this->vdp.command.address, this->vdp.dma.fill_value);
            break;
        default:
            if (dbg.traces.vdp) printf("\nDMA FILL UNKNOWN!? target:%d value:%02x addr:%04x cyc:%lld", this->vdp.command.target, this->vdp.dma.fill_value, this->vdp.command.target, this->clock.master_cycle_count);
            break;
    }
    //tracer.dma->notify({"fill(", hex(target, 1L), ":", hex(address, 5L), ", ", hex(data, 4L), ")"});

    dma_source_inc(this);
    this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
    if(--this->vdp.dma.len == 0) {
        this->vdp.command.pending = 0;
        this->vdp.dma.active = 0;
    }
}

static void dma_copy(struct genesis* this)
{
    this->vdp.dma.active = 1;

    u32 data = VRAM_read_byte(this, this->vdp.dma.source_address);
    if (dbg.traces.vdp4) DFT("\nVDP DMA COPY VRAM SRC:%04x DEST:%04x DATA:%04x", (u32)this->vdp.dma.source_address, (u32)this->vdp.command.address, (u32)data);

    VRAM_write_byte(this, this->vdp.command.address, data);
    //tracer.dma->notify({"copy(", hex(source, 6L), ", ", hex(target, 1L), ":", hex(address, 5L), ", ", hex(data, 4L), ")"});

    dma_source_inc(this);
    this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
    if(--this->vdp.dma.len == 0) {
        this->vdp.command.pending = 0;
        this->vdp.dma.active = 0;
    }

}

static void dma_run_if_ready(struct genesis* this)
{
    if (!this->vdp.dma.locked) {
        this->vdp.dma.locked = 1;
        if (this->vdp.dma.enable) {
            //if (this->vdp.command.pending && !this->vdp.dma.fill_pending) {
            while(this->vdp.command.pending && !this->vdp.dma.fill_pending) {
                switch(this->vdp.dma.mode) {
                    case 0: case 1: dma_load(this); break;
                    case 2: dma_fill(this); break;
                    case 3: dma_copy(this); break;
                }
            }
        }
        this->vdp.dma.locked = 0;
    }
}

static void write_control_port(struct genesis* this, u16 val, u16 mask)
{
    if (dbg.traces.vdp) printf("\nWRITE VDP CTRL PORT %04x cyc:%lld", val, this->clock.master_cycle_count);
    //if (dbg.traces.vdp3) DFT("\nWR VDP CTRL: %04x", val);
    //printf("\nIT HAPPENED!");
    //dbg_break("yes", this->clock.master_cycle_count);
    /*if (val == 0x8F02) {
        dbg.var++;
        if (dbg.var > 1) {
            dbg_break("Broke on weird control port write", this->clock.master_cycle_count);
        }
    }*/
    if (this->vdp.command.latch == 1) { // Finish latching a DMA transfer command
        this->vdp.command.latch = 0;
        this->vdp.command.address = (this->vdp.command.address & 0b11111111111111) | ((val & 7) << 14);
        this->vdp.command.target = (this->vdp.command.target & 3) | ((val >> 4) & 3) << 2;
        //this->vdp.command.ready = ((val >> 6) & 1) | (this->vdp.command.target & 1);
        this->vdp.command.pending = ((val >> 7) & 1) & this->vdp.dma.enable;

        this->vdp.dma.fill_pending = this->vdp.dma.mode == 2; // Wait for fill

        if (dbg.traces.vdp2) DFT("\nSECOND WRITE. VAL:%04x ADDR:%06x DMA_ENABLE:%d TARGET:%d PENDING:%d WAIT:%d", val, this->vdp.command.address, this->vdp.dma.enable, this->vdp.command.target, this->vdp.command.pending, this->vdp.dma.fill_pending);
        if (dbg.traces.vdp) printf("\nLATCHED. MODE! %d FILL PENDING! %d", this->vdp.dma.mode, this->vdp.dma.fill_pending);
        dma_run_if_ready(this);
        return;
    }

    // command.address.bit(0,13) = data.bit(0,13);
    this->vdp.command.address = (this->vdp.command.address & 0b111100000000000000) | (val & 0b11111111111111);
    //   command.target.bit(0,1)   = data.bit(14,15);
    this->vdp.command.target = (this->vdp.command.target & 0b1100) | ((val >> 14) & 3);
    if (dbg.traces.vdp) printf("   SETTING TARGET TO %d", this->vdp.command.target);

    if (dbg.traces.vdp2) DFT("\nFIRST WRITE. VAL:%04x ADDR:%06x TARGET:%d PENDING:%d", val, this->vdp.command.address, this->vdp.command.target, this->vdp.command.pending);
    //  VAL:c000 ADDR:00c000
    /*if ((val == 0xc000) && (this->vdp.command.address == 0xc000) && (this->vdp.command.target == 3)) {
        printf("\nDID IT HERE AT CYCLE:%lld", this->clock.master_cycle_count);
    }*/

    if (((val >> 14) & 3) != 2) { // It'll equal 2 if we're writing a register
        this->vdp.command.latch = 1;
        return;
    }

    write_vdp_reg(this, (val >> 8) & 0x1F, val & 0xFF);
}

static u16 read_control_port(struct genesis* this, u16 old, u32 has_effect)
{
    this->vdp.command.latch = 0;
    // Ares 360c  1000001100
    // Me   3688  1010001000

    u16 v = 0; // NTSC only for now
    v |= this->vdp.dma.active; // no DMA yet
    v |= this->clock.vdp.hblank_active << 2;
    v |= (this->clock.vdp.vblank_active || (1 ^ this->vdp.io.enable_display)) << 3;
    v |= (this->clock.vdp.field && this->vdp.io.interlace_field) << 4;
    v |= this->vdp.sprite_collision << 5;
    v |= this->vdp.sprite_overflow << 6;
    //<< 5; SC: 1 = any two sprites have non-transparent pixels overlapping. Used for pixel-accurate collision detection.
    //<< 6; SO: 1 = sprite limit has been hit on current scanline. i.e. 17+ in 256 pixel wide mode or 21+ in 320 pixel wide mode.
    //<< 7; VI: 1 = vertical interrupt occurred.
    v |= this->vdp.io.vblank << 7;

    v |= fifo_full(this) << 8;
    v |= fifo_empty(this) << 9;

    // Open bus bits 10-15
    //v |= (old & 0xFC00);
    v |= 0b0011010000000000;

    if (has_effect && !this->vdp.io.mode5) {
        //assert(1==2);
        //this->vdp.io.vblank = 0;
        //genesis_m68k_vblank_irq(this, 0);
        printf("\nWHAT!?!");
    }
    //if (dbg.traces.vdp3) DFT("\nRD VDP CP DATA:%04x", v);
    return v;
}

static u16 read_data_port(struct genesis* this, u16 old, u16 mask)
{
    this->vdp.command.latch = 0;
    this->vdp.command.ready = 0;
    u16 addr, data;
    switch(this->vdp.command.target) {
        case 0: // VRAM read
            addr = (this->vdp.command.address & 0x1FFFE) >> 1;
            data = VRAM_read(this, addr, 1);
            this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
            return data;
        case 4: // VSRAM read
            addr = (this->vdp.command.address >> 1) & 63;
            data = VSRAM_read(this, addr);
            this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
            return data;
        case 8: // CRAM read
            addr = (this->vdp.command.address >> 1) & 0x3F;
            data = CRAM_read(this, addr);
            this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
            return ((data & 7) << 1) | (((data >> 3) & 7) << 5) | (((data >> 6) & 7) << 9);
    }

    if (dbg.traces.vdp) printf("\nUnknown read port target %d", this->vdp.command.target);
    return 0;
}

static void write_data_port(struct genesis* this, u16 val, u32 is_cpu)
{
    if (dbg.traces.vdp && is_cpu) dbg_printf("\nVDP data port write val:%04x is_cpu:%d target:%d", val, is_cpu, this->vdp.command.target);

    this->vdp.command.latch = 0;
    if (dbg.traces.vdp) printf("\nWRITE VDP DATA PORT %04x cyc:%lld", val, this->clock.master_cycle_count);

    if(this->vdp.dma.fill_pending) {
        this->vdp.dma.fill_pending = false;
        this->vdp.dma.fill_value = val >> 8;
        if (dbg.traces.vdp3) DFT("\nWR VDP DATA, FILL START");
    }

    u32 addr;

    if (this->vdp.command.target == 1) { // VRAM write
        //addr = (this->vdp.command.address << 1) & 0x1FFFE;
        addr = (this->vdp.command.address & 0x1FFFE) >> 1;
        if (this->vdp.command.address & 1) val = (val >> 8) | (val << 8);
        if (dbg.traces.vdp3) DFT("\nWR VDP DP VRAM DEST:%04x DATA:%04x", addr, val);
        VRAM_write(this, addr, val);
        this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
        dma_run_if_ready(this);
        return;
    }

    if (this->vdp.command.target == 5) { // VSRAM write
        addr = (this->vdp.command.address >> 1) & 0x3F;
        //data format: ---- --yy yyyy yyyy
        addr = (addr % 40);
        if (dbg.traces.vdp3) DFT("\nWR VDP DP VSRAM DEST:%04x DATA:%04x", addr, val);
        this->vdp.VSRAM[addr] = val & 0x3FF;
        this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
        return;
    }

    if (this->vdp.command.target == 3) { // CRAM write
        addr = (this->vdp.command.address >> 1) & 0x3F;
        if (dbg.traces.vdp3) DFT("\nWR VDP DP CRAM DEST:%04x DATA:%04x", addr, val);
        u32 v = (((val >> 1) & 7) << 0) | (((val >> 5) & 7) << 3) | (((val >> 9) & 7) << 6);
        CRAM_write(this, addr, v);
        this->vdp.command.address = (this->vdp.command.address + this->vdp.command.increment) & 0x1FFFF;
        return;
    }
    printf("\nUnknown data port write! Target:%d is_cpu:%d addr:%04x", this->vdp.command.target, is_cpu, this->vdp.command.address);
}

u16 genesis_VDP_mainbus_read(struct genesis* this, u32 addr, u16 old, u16 mask, u32 has_effect)
{
    /* 110. .000 .... .... 000m mmmm */
    if (((addr & 0b110000000000000000000000) ^ 0b001001110000000011100000) != 0b111001110000000011100000) {
        printf("\nERROR LOCK UP VDP!!!!");
        return 0;
    }

    addr = (addr & 0x1F) | 0xC00000;

    switch(addr) {
        case 0xC00000: // VDP data port
        case 0xC00002: // VDP data port
            return read_data_port(this, old, mask) & mask;
        case 0xC00004: // VDP control
        case 0xC00006: // VDP control
            return read_control_port(this, old, has_effect) & mask;
        case 0xC00008:
        case 0xC0000A:
        case 0xC0000C:
        case 0xC0000E:
            return read_counter(this) & mask;
    }

    //printf("\nBAD VDP READ addr:%06x cycle:%lld", addr, this->clock.master_cycle_count);
    //gen_test_dbg_break(this, "vdp_mainbus_read");
    return old;
}

u8 genesis_VDP_z80_read(struct genesis* this, u32 addr, u8 old, u32 has_effect)
{
    if ((addr >= 0x7F00) && (addr < 0x7F20)) {
        return genesis_VDP_mainbus_read(this, (addr & 0x1E) | 0xC00000, old, 0xFF, has_effect) & 0xFF;
    }
    return 0xFF;
}

void genesis_VDP_z80_write(struct genesis* this, u32 addr, u8 val)
{
    if ((addr >= 0x7F00) && (addr < 0x7F20)) {
        genesis_VDP_mainbus_write(this, (addr & 0x1E) | 0xC00000, val, 0xFF);
    }
}

void genesis_VDP_mainbus_write(struct genesis* this, u32 addr, u16 val, u16 mask)
{
    if (((addr & 0b110000000000000000000000) ^ 0b001001110000000011100000) != 0b111001110000000011100000) {
        printf("\nERROR LOCK UP VDP!!!!");
        return;
    }

    // Duplicate!
    if (mask == 0xFF) {
        val = (val & 0xFF) | (val << 8);
    }
    else if (mask == 0xFF00) {
        val = (val & 0xFF00) | (val >> 8);
    }

    addr = (addr & 0x1F) | 0xC00000;
    switch(addr) {
        case 0xC00000: // VDP data port
        case 0xC00002:
            write_data_port(this, val, 1);
            return;
        case 0xC00004: // VDP control
        case 0xC00006:
            write_control_port(this, val, mask);
            return;
        case 0xC00008: // H/V READ ONLY
        case 0xC0000A:
        case 0xC0000C:
        case 0xC0000E:
            return;
        case 0xC00010: // SN76489 write
        case 0xC00012:
        case 0xC00014:
        case 0xC00016:
            if ((mask & 0xFF) == 0) return; // Byte writes to wrong address have no effect
            SN76489_write_data(&this->psg, val & 0xFF);
            return;
        case 0xC00018:
        case 0xC0001A:
            printf("\nDEBUG REG ADDR WRITE!");
            return;
        case 0xC0001C: // debug reg
        case 0xC0001E:
            printf("\nDEBUG REG WRITE!");
            return;
    }
    printf("\nBAD VDP WRITE addr:%06x val:%04x cycle:%lld", addr, val, this->clock.master_cycle_count);
    gen_test_dbg_break(this, "vdp_mainbus_write");
}

void genesis_VDP_reset(struct genesis* this)
{
    this->vdp.io.h32 = 0;
    this->vdp.io.h40 = 1; // H40 mode to start
    this->clock.vdp.hblank_active = this->clock.vdp.vblank_active = 0;
    this->clock.vdp.vcount = this->clock.vdp.vcount = 0;
    this->vdp.cur_output = (u16 *)this->vdp.display->output[this->vdp.display->last_written ^ 1];
    this->vdp.display->scan_x = this->vdp.display->scan_y = 0;
    set_clock_divisor(this);
}

static void get_line_hscroll(struct genesis* this, u32 line_num, u32 *planes)
{
    u32 addr = this->vdp.io.hscroll_addr;
    switch(this->vdp.io.hscroll_mode) {
        case 0: // full-screen
            break;
        case 1: // invalid!
            printf("\nWARN INVALID HSCROLL MODE 1");
            return;
        case 2: // 8-pixel rows
            line_num >>= 3; // We only care about every 8 lines
            addr += line_num << 4; // Every 16 words
            break;
        case 3: // per-line scrolls
            addr += line_num << 1;
            break;
    }
    // - 1 & 1023 ^ 1023 should convert from negative to positive here. 0 = FFFF = 0 for instance. 1 = 0 = FFFF.
    planes[PLANE_A] = VRAM_read(this, addr, 0);
    planes[PLANE_B] = VRAM_read(this, addr + 1, 0);
}

static void get_vscrolls(struct genesis* this, int column, u32 *planes)
{
    if (this->vdp.io.vscroll_mode == 0) {
        //planes[0] = this->vdp.VSRAM[0];
        //planes[1] = this->vdp.VSRAM[1];
        planes[0] = this->vdp.fetcher.vscroll_latch[0];
        planes[1] = this->vdp.fetcher.vscroll_latch[1];
        return;
    }
    if (column == -1) { // TODO: correctly simulate this
        if (this->vdp.io.h40) {
            u32 v = this->vdp.VSRAM[19] & this->vdp.VSRAM[39];
            planes[0] = v;
            planes[1] = v;
            return;
        }
        else {
            planes[0] = 0;
            planes[1] = 0;
            return;
        }
    }
    u32 col = (u32)column << 1;
    assert(col<20);
    planes[0] = this->vdp.VSRAM[col];
    planes[1] = this->vdp.VSRAM[col+1];
}

struct slice {
    u8 pattern[16]; // From left-to-right, decoded and h/vflipped AND with palette!
    // bits 0..5 are CRAM color #, w palette (forced to 0 for transparent always)
    // bit 6 is priority
};

static int fetch_order[4] = { 1, 0, 3, 2 };

static void render_to_ringbuffer(struct genesis* this, u32 plane, u8 *pattern, u32 pattern_pos, u32 pattern_max)
{
    u32 epos = pattern_max + 1;
    u32 *tail = &this->vdp.ringbuf.tail[plane];
    i32 *num = &this->vdp.ringbuf.num[plane];
    while(pattern_pos < epos) {
        struct genesis_vdp_pixel_buf *b = &this->vdp.ringbuf.buf[*tail];
        u32 c = pattern[pattern_pos] & 63;
        u32 priority = (pattern[pattern_pos] >> 7) & 1;
        if (c != 0) {
            b->has[plane] = 1;
            b->color[plane] = c;
            //b->priority[plane] = priority;
        } else {
            b->has[plane] = 0;
            b->color[plane] = 0;
        }
        b->priority[plane] = priority;
        pattern_pos++;
        *tail = (*tail + 1) & 31;
        (*num)++;
    }
}


static inline u32 window_transitions_off(struct genesis* this)
{
    u32 row = this->clock.vdp.vcount >> 3;
    if ((row >= this->vdp.window.top_row) && (row < this->vdp.window.bottom_row)) return 0;
    if (this->vdp.io.window_RIGHT) return 0; // Window only transitions OFF if left-to-middle
    if (this->vdp.window.right_col == 0) return 0; // Window doesn't transition OFF if hpos == 0
    int col = this->vdp.fetcher.column;

    return this->vdp.window.right_col == (u32)(col);
}


static inline u32 in_window(struct genesis* this)
{
    u32 col = this->vdp.fetcher.column;
    u32 row = this->clock.vdp.vcount >> 3;
    return (((col >= this->vdp.window.left_col) && (col < this->vdp.window.right_col)) ||
            ((row >= this->vdp.window.top_row) && (row < this->vdp.window.bottom_row)));
}

static void fetch_slice(struct genesis* this, u32 nt_base_addr, u32 col, u32 row, u8 *pattern, u32 is_planea)
{
    // Fetch a 16-pixel slice of a nametable to a provided struct
    u32 tile_row = (row >> 3) & this->vdp.io.foreground_height_mask;
    u32 tile_col = (col >> 3) & this->vdp.io.foreground_width_mask;
    u32 fine_row = row & 7;
    u32 foreground_width = this->vdp.io.foreground_width;
    if (is_planea && in_window(this)) {
        // Replace with a window fetch!
        nt_base_addr = this->vdp.window.nt_base;
        tile_row = this->clock.vdp.vcount >> 3;
        tile_col = this->vdp.fetcher.column << 1;
        fine_row = this->clock.vdp.vcount & 7;
        foreground_width = this->vdp.io.h40 ? 64 : 32;
    }
    u32 foreground_width_mask = foreground_width - 1;
    //printf("\nline:%d screen_x:%d row:%d col:%d  /  hmask:%d vmask:%d  /  tile_row:%d tile_col:%d FGW:%d", this->clock.vdp.vcount, this->vdp.line.screen_x, row, col, this->vdp.io.foreground_height_mask, this->vdp.io.foreground_width_mask, tile_row, tile_col, this->vdp.io.foreground_width);
    u32 tile_data = VRAM_read(this, nt_base_addr + (tile_row * foreground_width) + tile_col, 0);
    u32 vflip = (tile_data >> 12) & 1;
    u32 tile_addr = (tile_data & 0x7FF) << 5;
    tile_addr += (vflip ? (7 - fine_row) : fine_row) << 2;

    u32 attr = tile_data;
    u8 *ptr = ((u8 *)this->vdp.VRAM) + tile_addr;
    u32 pattern_idex = 0;
    u32 hflip = (tile_data >> 11) & 1;
    u32 palette = ((tile_data >> 13) & 3) << 4;
    u32 priority = ((tile_data >> 15) & 1) << 7;
    for (u32 i = 0; i < 4; i++) {
        u32 offset = hflip ? 3 - i : i;
        u8 px2 = ptr[fetch_order[offset]];
        u8 px[2];
        px[hflip] = px2 >> 4;
        px[hflip ^ 1] = px2 & 15;
        for (u32 j = 0; j < 2; j++) {
            pattern[pattern_idex++] = priority | (px[j] ? (palette | px[j]) : 0);
        }
    }

    tile_col = (tile_col + 1) & foreground_width_mask;
    tile_data = VRAM_read(this, nt_base_addr + (tile_row * foreground_width) + tile_col, 0);

    tile_addr = (tile_data & 0x7FF) << 5;
    vflip = (tile_data >> 12) & 1;
    tile_addr += (vflip ? (7 - fine_row) : fine_row) << 2;

    ptr = ((u8 *)this->vdp.VRAM) + tile_addr;
    hflip = (tile_data >> 11) & 1;
    palette = ((tile_data >> 13) & 3) << 4;
    priority = ((tile_data >> 15) & 1) << 7;
    for (u32 i = 0; i < 4; i++) {
        u32 offset = hflip ? 3 - i : i;
        u8 px2 = ptr[fetch_order[offset]];
        u8 px[2];
        px[hflip] = px2 >> 4;
        px[hflip ^ 1] = px2 & 15;
        for (u32 j = 0; j < 2; j++) {
            pattern[pattern_idex++] = priority | (px[j] ? (palette | px[j]) : 0);
        }
    }
}

struct spr_out {
    u32 hpos, vpos;
    u32 hsize, vsize;
    u32 priority, palette;
    u32 vflip, hflip;
    u32 gfx; // tile # in VRAM
};

static u32 fetch_sprites(struct genesis* this, struct spr_out *sprites)
{
    u32 compare_y = this->clock.vdp.vcount + 128;
    u16 *sprite_table_ptr = this->vdp.VRAM + this->vdp.io.sprite_table_addr;
    u32 num = 0;
    u16 next_sprite = 0;
    u32 tiles_on_line = 0;
    u32 sprites_total_max = this->vdp.io.h40 ? 80 : 64;
    u32 sprites_max_on_line = this->vdp.io.h40 ? 20 : 16;
    for (u32 i = 0; i < sprites_total_max; i++) {
        u16 *sp = sprite_table_ptr + (4 * next_sprite);
        next_sprite = sp[1] & 127;

        sprites->vpos = sp[0] & 0x1FF;
        sprites->vsize = ((sp[1] >> 8) & 3) + 1;

        u32 sprite_max = sprites->vpos + (sprites->vsize * 8);
        if ((compare_y >= sprites->vpos) && (compare_y < sprite_max)) {
            sprites->hsize = ((sp[1] >> 10) & 3) + 1;
            sprites->hflip = (sp[2] >> 11) & 1;
            sprites->vflip = (sp[2] >> 12) & 1;
            sprites->palette = ((sp[2] >> 13) & 3) << 4;
            sprites->hpos = sp[3] & 0x1FF;
            // TODO: exit when x = 0?
            sprites->gfx = sp[2] & 0x7FF;
            sprites->priority = (sp[2] >> 15) & 1;

            tiles_on_line += sprites->hsize;

            sprites++;
            num++;
        }
        if ((tiles_on_line >= 40) || (num >= sprites_max_on_line)) break;
    }

    return num;
}

static inline u32 sprite_tile_index(u32 tile_x, u32 tile_y, u32 hsize, u32 vsize, u32 hflip, u32 vflip)
{
    u32 ty = vflip ? (vsize-1) - tile_y : tile_y;
    u32 tx = hflip ? (hsize-1) - tile_x : tile_x;
    return (vsize * tx) + ty;
}


static void render_sprites(struct genesis *this)
{
    // Render sprites for a whole line!
    memset(&this->vdp.sprite_line_buf[0], 0, sizeof(struct genesis_vdp_sprite_pixel) * 320);

    // So. 80 hardware sprites, 8 bytes each
    struct spr_out sprites[20];

    u32 num_sprites = fetch_sprites(this, &sprites[0]);

    i32 xmax = this->vdp.io.h40 ? 320 : 256;

    u32 cur_y = this->clock.vdp.vcount + 128;

    for (u32 spr=0; spr < num_sprites; spr++) {
        // Render sprite from left to right
        struct spr_out *sp = &sprites[spr];
        u32 x_right = 128 + (this->vdp.io.h40 ? 320 : 256);
        u32 tile_stride = sp->vsize;

        u32 y_min = sp->vpos;
        u32 sp_line = cur_y - y_min;
        u32 fine_row = sp_line & 7;
        u32 sp_ytile = sp_line >> 3;
        u32 tile_y_addr_offset = (sp->vflip ? (7 - fine_row) : fine_row) << 2;

        //if (this->clock.vdp.vcount == 80) printf("\nSPRITE NUM:%d HSIZE: %d VSIZE:%d", spr, sp->hsize, sp->vsize);
        for (u32 tx = 0; tx < sp->hsize; tx++) { // Render across tiles...
            u32 tile_num = sp->gfx + sprite_tile_index(tx, sp_ytile, sp->hsize, sp->vsize, sp->hflip, sp->vflip);
            u32 tile_addr = (tile_num << 5) + tile_y_addr_offset;
            i32 x = (((i32)sp->hpos) - 128) + (((i32)tx) * 8);
            u8 *ptr = ((u8 *)this->vdp.VRAM) + tile_addr;
            for (u32 byte = 0; byte < 4; byte++) { // 4 bytes per 8 pixels
                u32 offset = sp->hflip ? 3 - byte : byte;
                u8 px2 = ptr[fetch_order[offset]];
                u32 px[2];
                px[sp->hflip] = px2 >> 4;
                px[sp->hflip ^ 1] = px2 & 15;
                for (u32 ps = 0; ps < 2; ps++) {
                    if ((x >= 0) && (x < xmax)) {
                        u32 v = px[ps];
                        if (v != 0) { // If there's not already a sprite pixel here, and ours HAS something other than transparent...
                            struct genesis_vdp_sprite_pixel *p = &this->vdp.sprite_line_buf[x];
                            if (p->has_px) {
                                this->vdp.sprite_collision = 1;
                            }
                            else {
                                p->has_px = 1;
                                p->priority = sp->priority;
                                p->color = sp->palette + v;
                            }
                        }
                    }
                    x++;
                }
            }
        }
    }
}

static void render_16(struct genesis* this)
{
    // Point of this function is to render 16 pixels outta the ringbuffer, to the pixelbuffer. Also combine with sprites and priority.
    // At this point, there will be 16-31 pixels in the ring buffer.
    u32 wide = this->vdp.io.h40 ? 4 : 5;
    u32 xmax = this->vdp.io.h40 ? 320 : 256;
    for (u32 num = 0; num < 16; num++) {
        struct genesis_vdp_pixel_buf *ring = this->vdp.ringbuf.buf + this->vdp.ringbuf.head;
        this->vdp.ringbuf.head = (this->vdp.ringbuf.head + 1) & 31;
        assert(this->vdp.ringbuf.num[PLANE_A] > 0);
        assert(this->vdp.ringbuf.num[PLANE_A] <= 32);
        assert(this->vdp.ringbuf.num[PLANE_B] > 0);
        assert(this->vdp.ringbuf.num[PLANE_B] <= 32);
        this->vdp.ringbuf.num[PLANE_A]--;
        this->vdp.ringbuf.num[PLANE_B]--;
        if (this->vdp.line.screen_x >= xmax) continue;

        // We've got out ringbuffer. Now get our sprite pixel...
        struct genesis_vdp_sprite_pixel *sprite = this->vdp.sprite_line_buf + this->vdp.line.screen_x;

        if (!this->opts.vdp.enable_A) {
            ring->has[PLANE_A] = ring->color[PLANE_A] = ring->priority[PLANE_A] = 0;
        }
        if (!this->opts.vdp.enable_B) {
            ring->has[PLANE_B] = ring->color[PLANE_B] = ring->priority[PLANE_B] = 0;
        }
        if (!this->opts.vdp.enable_sprites) {
            sprite->has_px = sprite->color = sprite->priority = 0;
        }


#define PX_NONE 0
#define PX_A 1
#define PX_B 2
#define PX_SP 3
#define PX_BG 4
        // Using logic from Ares here.
        // I originally had my own. Then I went to do shadow&highlight, and use Ares logic.
        // I couldn't get it to work. Then I tried making my own and realized I was just reproducing Ares but
        //  understanding it, so gave in.

#define solid_sprite (sprite->color & 15)
#define above_sprite (sprite->has_px && solid_sprite && sprite->priority)
#define solid(plane) (ring->has[plane] && (ring->color[plane] & 15))
#define above(plane) (solid(plane) && ring->priority[plane])

        u32 bg = PX_NONE;
        u32 fg = PX_NONE;
        if (above(PLANE_A) || solid(PLANE_A) && !above(PLANE_B)) {
            bg = PX_A;
        }
        else {
            if (solid(PLANE_B)) {
                bg = PX_B;
            }
            else
            {
                bg = PX_BG;
            }
        }
        if (above_sprite || (solid_sprite && !above(PLANE_B) && !above(PLANE_A)))
            fg = PX_SP;
        else
            fg = bg;

        u32 pixel = fg; // Output pixel will now be sprite or backgorund color or A or B
        u32 mode = 1;


        if(this->vdp.io.enable_shadow_highlight) {
            mode = ring->priority[PLANE_A] || ring->priority[PLANE_B];
            if(fg == PX_SP) switch(sprite->color) {
                    case 0x0E:
                    case 0x1E:
                    case 0x2E: mode  = 1; break;
                    case 0x3E: mode += 1; pixel = bg; break;
                    case 0x3F: mode  = 0; pixel = bg; break;
                    default:  if (sprite->has_px) mode |= sprite->priority; break;
                }
        }

        u32 final_color = 0;
        switch(pixel) {
            case PX_A:
                final_color = ring->color[PLANE_A];
                break;
            case PX_B:
                final_color = ring->color[PLANE_B];
                break;
            case PX_SP:
                final_color = sprite->color;
                break;
            case PX_BG:
                final_color = this->vdp.io.bg_color;
                break;
            default:
                assert(1==2);
                break;
        }

        assert(final_color < 64);
        final_color = this->vdp.CRAM[final_color] | (mode << 10);
        for (u32 i = 0; i < wide; i++) {
            assert(this->vdp.cur_pixel < (1280*240));
            this->vdp.cur_output[this->vdp.cur_pixel] = final_color;

            this->vdp.cur_pixel++;
            assert(this->vdp.cur_pixel < (1280*240));
        }
        this->vdp.line.screen_x++;
    }
}

static void render_16_more(struct genesis* this)
{
    // Fetch a slice
    u32 vscrolls[2];
    get_vscrolls(this, this->vdp.fetcher.column, vscrolls);
    u32 plane_wrap = (this->vdp.io.foreground_width * 8) - 1;
    //printf("\nRENDER 16 line:%d col:%d screen_x:%d sc_slot:%d", this->clock.vdp.vcount, this->vdp.fetcher.column, this->vdp.line.screen_x, this->vdp.sc_slot);

    // Dump to ringbuffer
    u8 pattern[16];
    for (u32 plane = 0; plane < 2; plane++) {
        fetch_slice(this, plane == 0 ? this->vdp.io.plane_a_table_addr : this->vdp.io.plane_b_table_addr, this->vdp.fetcher.hscroll[plane], vscrolls[plane]+this->clock.vdp.vcount, pattern, plane == 0);
        this->vdp.fetcher.hscroll[plane] = (this->vdp.fetcher.hscroll[plane] + 16) & plane_wrap;

        // If we just went from window to regular and (xscroll % 15) != 0, glitch time!
        if ((plane == 0) && window_transitions_off(this) && (this->vdp.fetcher.fine_x[0] != 0)) {
            render_to_ringbuffer(this, plane, pattern, 15 - this->vdp.fetcher.fine_x[0], 15);
        }

        render_to_ringbuffer(this, plane, pattern, 0, 15);
    }
    this->vdp.fetcher.hscroll[2] += 16;
    this->vdp.fetcher.column++;

    // Render 16 pixels from ringbuffer
    render_16(this);
}


static void render_left_column(struct genesis* this)
{
    // Fetch left column. Discard xscroll pixels, render the rest.
    //printf("\nRENDER LEFT COLUMN line:%d", this->clock.vdp.vcount);
    u32 plane_wrap = (8 * this->vdp.io.foreground_width) - 1;

    u32 vscrolls[2];
    get_vscrolls(this, -1, vscrolls);

    // Fill pixel buffers
    u8 pattern[16];
    // Now draw 0-15 pixels of each one out to the ringbuffer...
    for (u32 pl = 0; pl < 2; pl++) {
        u32 plane = pl;
        u32 hs = this->vdp.fetcher.hscroll[plane];
        fetch_slice(this, plane == 0 ? this->vdp.io.plane_a_table_addr : this->vdp.io.plane_b_table_addr, hs & 0x3F0, vscrolls[plane]+this->clock.vdp.vcount, pattern, 0);
        u32 fine_x = this->vdp.fetcher.fine_x[plane];
        if ((((plane == 0) && (!in_window(this))) || (plane == 1)) && (fine_x > 0)) {
            render_to_ringbuffer(this, plane, pattern, 15 - fine_x, 15);
        }
        this->vdp.fetcher.hscroll[plane] = (this->vdp.fetcher.hscroll[plane] + fine_x) & plane_wrap;
    }

    // Now that the initial data and pixel load is done, do one more regular one!
    render_16_more(this);
}

void genesis_VDP_cycle(struct genesis* this)
{
    // Take care of Z80 interrupt line
    if (this->vdp.io.z80_irq_clocks) {
        this->vdp.io.z80_irq_clocks -= this->clock.vdp.clock_divisor;
        if (this->vdp.io.z80_irq_clocks <= 0) {
            this->vdp.io.z80_irq_clocks = 0;
            genesis_z80_interrupt(this, 0);
        }
    }

    // Add our clock divisor
    this->clock.vdp.line_mclock += this->clock.vdp.clock_divisor;
    u32 sc_clock = this->clock.vdp.line_mclock >> 2;

    // Set up next clock divisor
    set_clock_divisor(this);

    // 4 of our cycles per SC transfer
    u32 run_dma = 0, run_pixels = 0;
    this->vdp.sc_count = (this->vdp.sc_count + 1) & 3;
    if ((this->vdp.sc_count & 1) == 0) {
        this->vdp.display->scan_x++;
    }
    if (this->vdp.sc_count == 0) {
        this->vdp.sc_slot++;
        // Do FIFO if this is an external slot
        if (this->vdp.slot_array[this->vdp.sc_array][this->vdp.sc_slot] == slot_external_access) {
            if (this->vdp.sc_skip) {
                this->vdp.sc_skip--;
            }
            else run_dma = 1;
        }
    }
    run_dma |= (this->vdp.io.enable_display ^ 1);
    if (run_dma) dma_run_if_ready(this);

    if (sc_clock == SC_HSYNC_GOES_OFF) {
        //printf("\nHBLANK OFF %d x:%d", this->clock.vdp.vcount, this->vdp.line.screen_x);
        hblank(this, 0);
    }
    if (sc_clock == SC_HSYNC_GOES_ON) {
        hblank(this, 1);
    }

    if (this->vdp.io.enable_display && (this->clock.vdp.vcount < 224)) {
        if (this->vdp.io.h32) {
            // 1 sc count = 4 serial clocks. 2 serial clocks per pixel. so
            // 1 sc count = 2 pixels
            // we want to do 8 pixels every 4 SC count
            i32 r = ((i32) this->vdp.sc_slot - 13) >> 1;
            if ((r >= 0) && (this->vdp.last_r != r)) {
                this->vdp.last_r = r;
                if (r == 0) render_left_column(this);
                else if (((r & 3) == 0) && (this->vdp.sc_slot < 142)) render_16_more(this);
            }
        } else {
            i32 r = ((i32) this->vdp.sc_slot - 13) >> 1;
            if ((r >= 0) && (this->vdp.last_r != r)) {
                this->vdp.last_r = r;
                if (r == 0) render_left_column(this);
                else if (((r & 3) == 0) && (this->vdp.sc_slot < 173)) render_16_more(this);
            }
        }
    }

    this->clock.vdp.hcount++;
    if (this->clock.vdp.hcount >= 0xE9) this->clock.vdp.hcount = 0xE9;
    if (this->clock.vdp.line_mclock >= MCLOCKS_PER_LINE)
        new_scanline(this);
}
