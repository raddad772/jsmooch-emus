//
// Created by . on 6/1/24.
//

/*
 * NOTE: parts of the DMA subsystem are almost directly from Ares.
 * I wanted to get it right and write a good article about it. Finding accurate
 *  info was VERY difficult, so I kept very close to the best source of
 *  info I could find - Ares.
 */

#include <assert.h>
#include <stdio.h>

#include "helpers/debug.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"

#define SC_ARRAY_H32      0
#define SC_ARRAY_H40      1
#define SC_ARRAY_DISABLED 2 // H32 timing
#define SC_ARRAY_H40_VBLANK 3


#define MCLOCKS_PER_LINE 3420
#define SC_HSYNC_GOES_OFF 5
#define SC_HSYNC_GOES_ON 658
#define SC_SLOW_SLOTS 662
#define HSYNC_GOES_OFF (SC_HSYNC_GOES_OFF * 5)
#define HSYNC_GOES_ON (SC_HSYNC_GOES_ON*5)
#define SLOW_MCLOCKS_START (SC_SLOW_SLOTS*5)
#define HBLANK_START 1280

#define printif(x, ...) if (dbg.trace_on && dbg.traces. x) dbg_printf(__VA_ARGS__)

//    UDS<<2 + LDS           neither   just LDS   just UDS    UDS+LDS
static u32 write_maskmem[4] = { 0,     0xFF00,    0x00FF,      0 };

static void set_m68k_dtack(struct genesis* this)
{
    printf("\nYAR");
    //this->m68k.pins.DTACK = !(this->io.m68k.VDP_FIFO_stall | this->io.m68k.VDP_prefetch_stall);
    //this->io.m68k.stuck = !this->m68k.pins.DTACK;
}


static u16 read_VSRAM(struct genesis* this, u16 addr)
{
    if (addr > 39) addr = 0;
    return this->vdp.VSRAM[addr];
}

static void write_VSRAM(struct genesis* this, u16 addr, u16 val)
{
    if (addr > 39) addr = 0;
    this->vdp.VSRAM[addr] = val;
    if (dbg.trace_on && dbg.traces.vram) {
        dbg_printf("\nWRITE VSRAM addr:%d val:04x cyc:%d", addr, val, *this->m68k.trace.cycles);
    }
}

static u16 read_CRAM(struct genesis* this, u16 addr)
{
    u16 data = this->vdp.CRAM[addr & 63];
    u16 o = ((data & 7) << 1);
    o |= ((data >> 3) & 7) << 5;
    o |= ((data >> 6) & 7) << 9;
    return o;
}

static void write_CRAM(struct genesis* this, u16 addr, u16 val)
{
    u16 o = ((val >> 1) & 7);
    o |= ((val >> 5) & 7) << 3;
    o |= ((val >> 9) & 7) << 6;
    if (addr > 63) addr = 0;
    if (dbg.trace_on && dbg.traces.vram) {
        dbg_printf("\nWRITE CRAM addr:%d val:04x cyc:%d", addr, o, *this->m68k.trace.cycles);
    }
    this->vdp.CRAM[addr] = o;
}

static u16 read_counter(struct genesis* this)
{
    if (this->vdp.io.counter_latch) return this->vdp.io.counter_latch_value;
    u16 vcnt = this->clock.vdp.vcount;
    if (this->vdp.io.interlace_mode & 1) {
        if (this->vdp.io.interlace_mode & 2) vcnt <<= 1;
        vcnt = (vcnt & 0xFFFE) | ((vcnt >> 8) ^ 1);
    }
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

void genesis_VDP_init(struct genesis* this)
{
    init_slot_tables(this);
}

static void set_clock_divisor(struct genesis* this)
{
    u32 clk = 5;

    if (this->vdp.io.h40 && (this->clock.vdp.line_mclock >= HSYNC_GOES_OFF) && (
            this->clock.vdp.line_mclock < SLOW_MCLOCKS_START))
        clk = 4;

    this->clock.vdp.clock_divisor = (i32)clk;
}

static void hblank(struct genesis* this, u32 new_value)
{
    this->clock.vdp.hblank = new_value;
}

static void vblank(struct genesis* this, u32 new_value)
{
    u32 old_value = this->clock.vdp.vblank;
    this->clock.vdp.vblank = new_value;  // Our value that indicates vblank yes or no
    this->vdp.io.vblank = new_value;     // Our IO vblank value, which can be reset

    if ((!old_value) && new_value) {
        genesis_m68k_vblank_irq(this, 1);
    }

    if (new_value) {
        this->vdp.sc_slot = SC_ARRAY_DISABLED + this->vdp.io.h40; //
    }

    set_clock_divisor(this);
}


static void new_frame(struct genesis* this)
{
    this->clock.master_frame++;
    //printf("\nNEW GENESIS FRAME! %lld", this->clock.master_frame);
    this->clock.vdp.field ^= 1;
    this->clock.vdp.vcount = 0;
    this->clock.vdp.vblank = 0;
    this->vdp.cur_output = this->vdp.display->device.display.output[this->vdp.display->device.display.last_written];
    this->vdp.display->device.display.last_written ^= 1;

    set_clock_divisor(this);
}

static void latch_vcounter(struct genesis* this)
{
    this->vdp.io.irq_vcounter = this->vdp.io.line_irq_counter; // TODO: load this from register
}

static void set_sc_array(struct genesis* this)
{
    if ((this->clock.vdp.vblank) || (!this->vdp.io.enable_display)) {
        this->vdp.sc_array = SC_ARRAY_DISABLED + this->vdp.io.h40;
    }
    else this->vdp.sc_array = this->vdp.io.h40;
}

static void new_scanline(struct genesis* this)
{
    this->clock.vdp.hcount = 0;
    this->clock.vdp.vcount++;

    this->vdp.line.hscroll = 0;
    this->vdp.line.screen_x = 0;
    this->vdp.line.screen_y = this->clock.vdp.vcount;

    this->clock.vdp.line_mclock -= MCLOCKS_PER_LINE;
    this->vdp.sc_count = 0;
    this->vdp.sc_slot = 0;

    switch(this->clock.vdp.vcount) {
        case 0:
            vblank(this, 0);
            break;
        case 0xE0: // vblank start
            vblank(this, 1);
            this->vdp.io.z80_irq_clocks = 2573; // Thanks @MaskOfDestiny
            genesis_z80_interrupt(this, 1); // Z80 asserts vblank interrupt for 2573 mclocks
            break;
        case 262: // frame end
            new_frame(this);
            break;
    }
    if (this->clock.vdp.vcount == 0) latch_vcounter(this);
    if (this->clock.vdp.vcount >= 225) latch_vcounter(this);
    else if (this->clock.vdp.vcount != 0) {
        // Do down counter if we're not in line 0 or 225
        if ((this->vdp.io.irq_vcounter <= 0) && (this->vdp.io.line_irq_counter)) {
            genesis_m68k_line_count_irq(this, 1);
            latch_vcounter(this);
        }
        else this->vdp.io.irq_vcounter--;
    }

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

static u16 read_VRAM_byte(struct genesis* this, u16 addr)
{
    if (addr & 1) return this->vdp.VRAM[addr >> 1] & 0xFF;
    return this->vdp.VRAM[addr >> 1] >> 8;
}

static void FIFO_advance(struct genesis* this)
{
    this->vdp.fifo.head = (this->vdp.fifo.head + 1) % 5;
    this->vdp.fifo.len--;
    if (dbg.trace_on && dbg.traces.fifo) dbg_printf("\nFIFO_advance. new head: %d, new len: %d", this->vdp.fifo.head, this->vdp.fifo.len);
    assert(this->vdp.fifo.len<6);
    if (this->io.m68k.VDP_FIFO_stall) {
        this->io.m68k.VDP_FIFO_stall = 0;
        printf("\nDONE2");
        set_m68k_dtack(this);
    }
}

static void write_VRAM_byte(struct genesis* this, u16 addr, u16 val)
{
    if (dbg.trace_on && dbg.traces.vram)
        dbg_printf("\nVRAM WRITE addr:%d val:%02x", addr, val);
    if (val != 0) dbg_printf("\nVRAM WRITE addr:%d val:%02x cyc:%lld", addr, val, *this->m68k.trace.cycles);
    u32 LDS = addr & 1;
    u32 UDS = LDS ^ 1;
    u32 mask = write_maskmem[LDS  + (UDS << 1)];
    addr &= 0xFFFF;
    this->vdp.VRAM[addr >> 1] = (this->vdp.VRAM[addr >> 1] & mask) | (val & ~mask);
}

static u32 prefetch_run(struct genesis* this)
{
    struct genesis_vdp_prefetch *pf = &this->vdp.fifo.prefetch;
    if (pf->UDS && pf->LDS) return 0; // prefetch is full

    u32 target = this->vdp.command.target;
    if (target == 0 && this->vdp.io.vram_mode == 0) {
        pf->LDS = pf->UDS = 1;
        pf->val = (read_VRAM_byte(this, this->vdp.command.address & 0xFFFE) << 8) | (read_VRAM_byte(this, (this->vdp.command.address & 0xFFFE) | 1));
        this->vdp.command.ready = 1;
        return 1;
    }

    if (target == 0 && this->vdp.io.vram_mode == 1) {
        pf->UDS = pf->LDS = 1;
        pf->val = read_VRAM_byte(this, this->vdp.command.address | 1);
        pf->val |= (pf->val << 8);
        this->vdp.command.ready = 1;
        return 1;
    }

    if (target == 4) {
        pf->UDS = pf->LDS = 1;
        pf->val = read_VSRAM(this,this->vdp.command.address >> 1);
        pf->val = (pf->val & 0x07FF) | (this->vdp.fifo.slots[this->vdp.fifo.head].val & 0xF800);
        this->vdp.command.ready = 1;
        return 1;
    }

    if (target == 8) {
        pf->UDS = pf->LDS = 1;
        pf->val = read_CRAM(this, this->vdp.command.address >> 1);
        pf->val = ((pf->val & 0x0EEE) | (this->vdp.fifo.slots[this->vdp.fifo.head].val & ~0x0EEE)) & 0xFFFF;
        this->vdp.command.ready = 1;
        return 1;
    }

    if (target == 12) {
        pf->UDS = pf->LDS = 1;
        pf->val = read_VRAM_byte(this, this->vdp.command.address ^ 1);
        pf->val |= (pf->val << 8);
        this->vdp.command.ready = 1;
        return 1;
    }

    pf->UDS = pf->LDS = 1;
    this->vdp.command.ready = 1;
    printif(fifo, "\nREAD PREFETCH TARGET %d", target);
    return 1;
}


static u32 fifo_full(struct genesis* this)
{
    return this->vdp.fifo.len > 3;
}

// Discharge 1 FIFO entry, if external slot is available.
static u32 run_FIFO(struct genesis* this) {
    // Empty FIFO, don't bother...
    if (this->vdp.fifo.len == 0) {
        if (dbg.traces.fifo) dbg_printf("\nrun_FIFO: FIFO EMPTY");
        assert(this->io.m68k.VDP_FIFO_stall == 0);
        return 0;
    }
    struct genesis_vdp_fifo_slot *e = &this->vdp.fifo.slots[this->vdp.fifo.head];

    struct genesis_vdp_fifo_slot *slot0 = &this->vdp.fifo.slots[this->vdp.fifo.head];
    if (dbg.traces.fifo) dbg_printf("\nrun_FIFO: using slot %d", this->vdp.fifo.head);
    if (slot0->target == 1 && this->vdp.io.vram_mode == 0) {
        if (slot0->LDS) {
            slot0->LDS = 0;
            write_VRAM_byte(this, slot0->addr ^ 1, slot0->val & 0xFF);
            if (dbg.traces.fifo) dbg_printf("\nrun_FIFO: WROTE BYTE addr:%04x byte:%02x", slot0->addr ^ 1, slot0->val & 0xFF);
            return 1;
        }
        if (slot0->UDS) {
            slot0->UDS = 0;
            write_VRAM_byte(this, slot0->addr, slot0->val >> 8);
            if (this->vdp.command.pending && this->vdp.dma.mode == 2) {
                this->vdp.dma.data = slot0->val;
                this->vdp.dma.wait = 0;
            }
            FIFO_advance(this);
            if (dbg.traces.fifo) dbg_printf("\nFIFO WROTE BYTE AND SLIMMED %d", this->vdp.fifo.len);
            return 1;
        }
        assert(1==2);
    }

    if (slot0->target == 1 && this->vdp.io.vram_mode == 1)
        write_VRAM_byte(this, slot0->addr | 1, slot0->val & 0xFF);
    else if (slot0->target == 3)
        write_CRAM(this, slot0->addr >> 1, slot0->val);
    else if (slot0->target == 5)
        write_VSRAM(this, slot0->addr >> 1, slot0->val);
    else
        if (dbg.traces.fifo) dbg_printf("\nWarning, FIFO write target? %d addr:%04x val:%04x", slot0->target, slot0->addr, slot0->val);

    if ((this->vdp.command.pending) && (this->vdp.dma.mode == 2)) {
        struct genesis_vdp_fifo_slot *slot1 = &(this->vdp.fifo.slots[(this->vdp.fifo.head + 1) % 5]);
        this->vdp.dma.data = slot1->val;
        this->vdp.dma.wait = 0;
    }

    slot0->UDS = slot0->LDS = 0;
    FIFO_advance(this);
    return 1;
}


static void write_vdp_reg(struct genesis* this, u16 rn, u16 val, u16 mask)
{
    if (rn >= 24) {
        if (dbg.traces.vdp) dbg_printf("\nWARNING illegal VDP register write %d %04x on cycle:%lld", rn, val, this->clock.master_cycle_count);
        return;
    }
    struct genesis_vdp* vdp = &this->vdp;
    u32 va;
    val &= 0xFF;
    switch(rn) {
        case 0: // Mode Register 1
            vdp->io.blank_left_8_pixels = (val >> 5) & 1;
            vdp->io.enable_line_irq = (val >> 4) & 1;
            if ((!vdp->io.counter_latch) && (val & 2))
                vdp->io.counter_latch_value = read_counter(this);
            vdp->io.counter_latch = (val >> 1) & 1;
            vdp->io.enable_overlay = (val & 1) ^ 1;
            // TODO: handle more bits
            return;
        case 1: // Mode regiser 2
            vdp->io.vram_mode = (val >> 7) & 1;
            vdp->io.enable_display = (val >> 6) & 1;
            vdp->io.enable_virq = (val >> 5) & 1;
            vdp->io.enable_dma = (val >> 4) & 1;
            vdp->io.cell30 = (val >> 3) & 1;
            // TODO: handle more bits
            if (vdp->io.cell30) printf("\nWARNING PAL DISPLAY 240 SELECTED");
            vdp->io.mode5 = (val >> 2) & 1;
            if (!vdp->io.mode5) printf("\nWARNING MODE5 DISABLED");
            return;
        case 2: // Plane A table location
            vdp->io.plane_a_table_addr = (val & 0b11111000) << 9;
            return;
        case 3: // Window nametable location
            vdp->io.window_table_addr = (val & 0b1111110) << 9; // lowest bit ignored in H40
            return;
        case 4: // Plane B location
            vdp->io.plane_b_table_addr = (val & 7) << 12;
            return;
        case 5: // Sprite table location
            vdp->io.sprite_table_addr = (val & 0xFF) << 8; // lowest bit ignored in H40
            return;
        case 6: // um
            vdp->io.sprite_generator_addr = (vdp->io.sprite_generator_addr & 0x7FFF) | (((val >> 5) & 1) << 15);
            return;
        case 7:
            vdp->io.bg_color = val & 63;
            return;
        case 8: // master system hscroll
            return;
        case 9: // master system vscroll
            return;
        case 10: // horizontal interrupt counter
            vdp->io.line_irq_counter = val;
            return;
        case 11: // Mode register 3
            vdp->io.enable_th_interrupts = (val >> 3) & 1;
            vdp->io.vscroll_mode = (val >> 2) & 1;
            vdp->io.hscroll_mode = val & 3;
            return;
        case 12: // Mode register 4
            // TODO: handle more of these
            vdp->io.h32 = (val & 1) == 0;
            vdp->io.h40 = (val & 1) != 0;
            set_sc_array(this);
            if (vdp->io.h32) printf("\n32-cell 256 pixel mode selected");
            if (vdp->io.h40) printf("\n40-cell 320 pixel mode selected");
            set_clock_divisor(this);
            vdp->io.enable_shadow_highlight = (val >> 3) & 1;
            vdp->io.interlace_mode = (val >> 1) & 3;
            return;
        case 13: // horizontal scroll data addr
            vdp->io.hscroll_addr = (val & 0x7F) << 9;
            return;
        case 14: // mostly unused
            return;
        case 15:
            vdp->command.increment = val;
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
            // 64x128, 128x64, and 128x128 are invalid TODO: complain about it
            return;
        case 17: // window plane horizontal position
            vdp->io.window_h_pos = (val & 0x1F);
            vdp->io.window_draw_L_to_R = (val >> 7) & 1;
            return;
        case 18: // window plane vertical postions
            vdp->io.window_v_pos = (val & 0x1F);
            vdp->io.window_draw_top_to_bottom = (val >> 7) & 1;
            return;
        case 19:
            vdp->dma.len = (vdp->dma.len & 0xFF00) | (val & 0xFF);
            return;
        case 20:
            vdp->dma.len = (vdp->dma.len & 0xFF) | ((val << 8) & 0xFF00);
            return;
        case 21:
            vdp->dma.source = (vdp->dma.source & 0xFFFF00) | (val & 0xFF);
            return;
        case 22:
            vdp->dma.source = (vdp->dma.source & 0xFF00FF) | ((val << 8) & 0xFF00);
            return;
        case 23:
            vdp->dma.source = (vdp->dma.source & 0xFFFF) | (((val & 0xFF) << 16) & 0x3F0000);
            vdp->dma.mode = (val >> 6) & 3;
            return;
    }
}

static void dma_test_enable(struct genesis* this)
{
    if (this->vdp.command.pending && !this->vdp.dma.wait && this->vdp.dma.mode <= 1) {
        this->vdp.dma.active = 1;
        dbg_printf("\nDMA GO! dest:%04x len:%d cyc:%lld", this->vdp.command.address, this->vdp.dma.len, *this->m68k.trace.cycles);
    }
    else {
        dbg_printf("\nDMA STOP!");
        this->vdp.dma.active = 0;
    }
}

static void write_control_port(struct genesis* this, u16 val, u16 mask)
{
    printif(vdp, "\nVDP controlport write: %04x", val);
    if (this->vdp.command.latch) {
        this->vdp.command.latch = 0;
        // bis 14-16 = data 0-2
        this->vdp.command.address = (this->vdp.command.address & 0x3FFF) | ((val & 7) << 14);
        this->vdp.command.target = (this->vdp.command.target & 0b0011) | ((val >> 2) & 0b1100);
        printif(vdp, "\nSET TARGET %d FROM VAL %04x", this->vdp.command.target, val);
        this->vdp.command.ready = ((val >> 6) & 1) | (this->vdp.command.target & 1);
        this->vdp.command.pending |= ((val >> 7) & 1) & this->vdp.io.enable_dma;

        if ((this->vdp.command.target & 1) == 0) {
            this->vdp.fifo.prefetch.UDS = this->vdp.fifo.prefetch.LDS = 0;
        }
        if (this->vdp.command.pending && this->vdp.dma.mode == 1) this->vdp.dma.delay = 4;
        this->vdp.dma.wait = this->vdp.dma.mode == 2;
        dma_test_enable(this);
        return;
    }

    this->vdp.command.target = (this->vdp.command.target & 0b1100) | ((val >> 14) & 3);
    printif(dma, "\nSET TARGET2 %d from %04x", this->vdp.command.target, val);
    this->vdp.command.ready = 1;

    if (((val >> 14) & 3) != 2) {
        // bits 0-13
        // 14, 15 = 4, 8 = C, 3
        this->vdp.command.address = (this->vdp.command.address & 0x1C000) | (val & 0x3FFFF);
        this->vdp.command.latch = 1;
        return;
    }

    u32 reg = (val >> 8) & 31;
    write_vdp_reg(this, reg, val, mask);
}

static u16 read_control_port(struct genesis* this, u16 old, u32 has_effect)
{
    this->vdp.command.latch = 0;

    u16 v = 0; // NTSC only for now
    v |= this->vdp.dma.active; // no DMA yet
    v |= this->clock.vdp.hblank << 2;
    v |= this->clock.vdp.vblank << 3;
    v |= (this->clock.vdp.field && this->vdp.io.interlace_field) << 4;
    //<< 5; SC: 1 = any two sprites have non-transparent pixels overlapping. Used for pixel-accurate collision detection.
    //<< 6; SO: 1 = sprite limit has been hit on current scanline. i.e. 17+ in 256 pixel wide mode or 21+ in 320 pixel wide mode.
    //<< 7; VI: 1 = vertical interrupt occurred.
    v |= this->vdp.io.vblank << 7;

    v |= fifo_full(this) << 8;
    v |= fifo_empty(this) << 9;

    // Open bus bits 10-15
    v |= (old & 0xFC00);

    if (has_effect) {
        this->vdp.io.vblank = 0;
        genesis_m68k_vblank_irq(this, 0);
    }

    return v;
}

static u16 read_data_port(struct genesis* this, u16 old, u16 mask)
{
    this->vdp.command.latch = 0;
    this->vdp.command.ready = 0;
    // TODO: this
    this->vdp.command.address += this->vdp.command.increment;

    return 0;
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
        case 0xC0000C:
            return read_counter(this) & mask;
    }

    printf("\nBAD VDP READ addr:%06x cycle:%lld", addr, this->clock.master_cycle_count);
    gen_test_dbg_break(this, "vdp_mainbus_read");
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

static void fifo_write(struct genesis* this, u32 target, u16 dest_addr, u16 val)
{
    u32 slot = (this->vdp.fifo.head + this->vdp.fifo.len) % 5;
    this->vdp.fifo.len++;
    assert(this->vdp.fifo.len < 6);
    struct genesis_vdp_fifo_slot *e = &this->vdp.fifo.slots[slot];

    printif(fifo, "\nFIFO WRITE addr:%04x val:%04x target:%d slot:%d len:%d", dest_addr, val, target, slot, this->vdp.fifo.len);

    e->addr = dest_addr;
    e->val = val;
    e->UDS = 1;
    e->LDS = 1;
    e->target = target;
}

static void write_data_port(struct genesis* this, u16 val, u16 mask, u32 is_cpu)
{
    if (dbg.traces.vdp) dbg_printf("\nVDP data port write val:%04x is_cpu:%d target:%d", val, is_cpu, this->vdp.command.target);
    this->vdp.command.latch = 0;
    this->vdp.command.ready = 1;

    if (is_cpu && fifo_full(this)) {
        if (fifo_overfull(this)) assert(1==2);
        /*this->io.m68k.VDP_FIFO_stall = 1; // We're going to over-fill the FIFO and stall m68k until it's back to normal
        printf("\nDONE1");
        set_m68k_dtack(this);
        dbg_printf("\nPausing the m68k....");*/
        // no stalls...
        run_FIFO(this);
        run_FIFO(this);
        run_FIFO(this);
    }

    fifo_write(this, this->vdp.command.target, this->vdp.command.address, val);
    //run_FIFO(this);
    this->vdp.command.address += this->vdp.command.increment;
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
            write_data_port(this, val, mask, 1);
            return;
        case 0xC00004: // VDP control
        case 0xC00006:
            write_control_port(this, val, mask);
            return;
    }
    printf("\nBAD VDP WRITE addr:%06x val:%04x cycle:%lld", addr, val, this->clock.master_cycle_count);
    gen_test_dbg_break(this, "vdp_mainbus_write");
}

void genesis_VDP_reset(struct genesis* this)
{
    this->vdp.io.h32 = 0;
    this->vdp.io.h40 = 1; // H40 mode to start
    this->clock.vdp.line_size = 320;
    this->clock.vdp.hblank = this->clock.vdp.vblank = 0;
    this->clock.vdp.vcount = this->clock.vdp.vcount = 0;
    set_clock_divisor(this);
}

static void do_pixel(struct genesis* this)
{
    // Render a pixel!
}

static void DMA_load_FIFO_load(struct genesis* this)
{
    if (this->vdp.dma.delay > 0) { this->vdp.dma.delay--; printif(dma, "\nLOAD DELAY %d", this->vdp.dma.delay); return; }
    if (fifo_full(this)) { if (dbg.traces.fifo) dbg_printf("\nLOAD FIFO FULL"); return; }

    u32 addr = ((this->vdp.dma.mode & 1) << 23) | (this->vdp.dma.source << 1);
    u16 data = genesis_mainbus_read(this, addr, 1, 1, this->io.m68k.open_bus_data, 1);
    write_data_port(this, data, 0xFFFF, 0);
    if (dbg.traces.fifo) dbg_printf("\nLOADED val:%04x addr:%06x!", data, addr);

    this->vdp.dma.source = (this->vdp.dma.source & 0xFF0000) | ((this->vdp.dma.source + 1) & 0xFFFF);
    if (--this->vdp.dma.len == 0) {
        this->vdp.command.pending = 0;
        dma_test_enable(this);
    }
}

static void DMA_load_FIFO_fill(struct genesis* this)
{
    switch(this->vdp.command.target) {
        case 1:
            write_VRAM_byte(this, this->vdp.command.address ^ 1, this->vdp.dma.data >> 8);
            break;
        case 3:
            this->vdp.CRAM[this->vdp.command.address] = this->vdp.dma.data;
            break;
        case 5:
            this->vdp.VSRAM[this->vdp.command.address] = this->vdp.dma.data;
            break;
    }
    this->vdp.dma.source = (this->vdp.dma.source & 0xFF0000) | ((this->vdp.dma.source + 1) & 0xFFFF);
    if (--this->vdp.dma.len == 0) {
        this->vdp.command.pending = 0;
        dma_test_enable(this);
    }
}

static void DMA_load_FIFO_copy(struct genesis* this)
{
    if (!this->vdp.dma.read) {
        this->vdp.dma.read = 1;
        this->vdp.dma.data = read_VRAM_byte(this, this->vdp.dma.source ^ 1);
        return;
    }
    this->vdp.dma.read = 0;
    write_VRAM_byte(this, this->vdp.command.address ^ 1, this->vdp.dma.data);

    this->vdp.dma.source = (this->vdp.dma.source & 0xFF0000) | ((this->vdp.dma.source + 1) & 0xFFFF);
    if (--this->vdp.dma.len == 0) {
        this->vdp.command.pending = 0;
        dma_test_enable(this);
    }
}

static u32 DMA_load_FIFO(struct genesis* this)
{
    if (this->vdp.command.pending && !this->vdp.dma.wait) {
        if (this->vdp.dma.mode <= 1 && !fifo_full(this)) {
            printif(dma, "\nDMA LOAD FIFO!");
            DMA_load_FIFO_load(this);
            return 1;
        }
        else if (this->vdp.dma.mode == 2 && fifo_empty(this)) {
            if (dbg.traces.dma) dbg_printf("\nDMA FILL!");
            DMA_load_FIFO_fill(this);
            return 1;
        }
        else if (this->vdp.dma.mode == 3) {
            if (dbg.traces.dma) dbg_printf("\nDMA LOAD COPY!");
            DMA_load_FIFO_copy(this);
            return 1;
        }
        printif(dma, "\nDMA FALL THRU? %d", this->vdp.dma.mode);
    }
    return 0;
}


static void render_8_pixels(struct genesis* this)
{

}

static u16 read_VRAM(struct genesis* this, u16 addr) {
    return this->vdp.VRAM[addr >> 1];
}

static void render_8_more(struct genesis* this)
{
    // OK, get our x pos by doing ((sc - 13) * 2) - xscroll
    u32 xpos = this->vdp.line.screen_x;
    u32 ypos = this->vdp.line.screen_y;
    ypos = (ypos % 224);
    this->vdp.line.screen_x += 8;

    // Get pattern table
    // 2 bytes per entry. same number as plane size y * (2 * io.foreground_width) + (x * 2)

    u32 pa = ((ypos >> 3) * 2 * this->vdp.io.foreground_width) + ((xpos >> 3) * 2);
    u32 pattern_addr[2] = { pa + this->vdp.io.plane_a_table_addr, pa + this->vdp.io.plane_b_table_addr };
    u32 pattern_entry[2] = { read_VRAM(this, pattern_addr[0]), read_VRAM(this, pattern_addr[1]) };

    u32 tile_number[2] = {pattern_entry[0] & 0x3FF, pattern_entry[1] & 0x3FF};

    u32 tile_addr[2] = { (tile_number[0] << 4), (tile_number[1] << 4)};

    u32 myx = (2 * 320 * ypos) + xpos;

    u16 *optr = this->vdp.display->device.display.output[0] + (320 * ypos) + xpos;

    u8* tile_ptr[2] = { };

    tile_ptr[0] = ((u8*)this->vdp.VRAM) + (tile_addr[0]) + ((ypos & 7) << 2);
    for (u32 i = 0; i < 4; i++) {
        u8 px2 = *tile_ptr[0];
        tile_ptr[0]++;
        if (px2 != 0) printf("\nPX2 x:%d y:%d", xpos+i, ypos);

        /**optr = px2 >> 4;
        optr++;
        *optr = px2 & 15;
        optr++;*/
        u32 v = 0;
        u32 r = (this->clock.master_frame & 3);
        if (xpos == 20) v = 15;
        if ( r == 0) {
            if (ypos == 0) v = 15;
        }
        else if (r == 1) {
            if (ypos == 50) v = 15;
        }
        else {
            if (ypos == 200) v = 15;
        }
        *optr = v;
        optr++;
        *optr = v;
        optr++;
    }

    // + 4 * (ypos & 7)

    // Get attribute

    // Get actual pattern
}

static void render_left_column(struct genesis* this)
{
    // Fetch left column. Discard xscroll pixels, render the rest.
    this->vdp.line.screen_x += 8;
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

    // Set up next clock divisor
    set_clock_divisor(this);

    // 4 of our cycles per SC transfer
    this->vdp.sc_count = (this->vdp.sc_count + 1) & 3;
    if (this->vdp.sc_count == 0) {
        this->vdp.sc_slot++;
        // Do FIFO if this is an external slot
        if (this->vdp.slot_array[this->vdp.sc_array][this->vdp.sc_slot] == slot_external_access) {
            if (dbg.trace_on && dbg.traces.fifo)
                dbg_printf("\nGOT EXT", *this->m68k.trace.cycles);
            if (!run_FIFO(this)) prefetch_run(this);
            DMA_load_FIFO(this);
        }
        else {
            if (dbg.trace_on && dbg.traces.fifo)
                dbg_printf("\nGOT SLOT:%d ARR:%d CYC:%lld SCC:%d ARRAY:%d", this->vdp.slot_array[this->vdp.sc_array][this->vdp.sc_slot], this->vdp.sc_array, *this->m68k.trace.cycles, this->vdp.sc_slot, this->vdp.sc_array);
        }
    }


    if (this->vdp.sc_slot == SC_HSYNC_GOES_OFF) hblank(this, 0);
    if (this->vdp.sc_slot == SC_HSYNC_GOES_ON) hblank(this, 1);

    // Add a pixel

    /*
     * move this to take effect on HV read, not for all our tracking, for now
    if(this->vdp.io.h40) {
        // hcount 0-b5, e4-ff
        if(this->clock.vdp.hcount == 0xb6) this->clock.vdp.hcount = 0xe4;
    } else {
        // hcount 0-93, e9-ff
        if(this->clock.vdp.hcount == 0x94) this->clock.vdp.hcount = 0xe9;
    }

     *
     */

    // 2 cycles per pixel
    this->vdp.cycle ^= 1;

    // Do last 8 pixels
    if(this->vdp.io.h32) {
        // 1 sc count = 4 serial clocks. 2 serial clocks per pixel. so
        // 1 sc count = 2 pixels
        // we want to do 8 pixels every 4 SC count
        i32 r = ((i32)this->vdp.sc_slot - 13) >> 1;
        if (r == 0) render_left_column(this);
        else if (((r & 3) == 0) && (this->vdp.sc_slot < 142)) render_8_more(this);
    } else {
        i32 r = ((i32)this->vdp.sc_slot - 13) >> 1;
        if (r == 0) render_left_column(this);
        else if (((r & 3) == 0) && (this->vdp.sc_slot < 175)) render_8_more(this);
    }


    this->clock.vdp.hcount++;
    if (this->clock.vdp.line_mclock >= MCLOCKS_PER_LINE)
        new_scanline(this);
}
