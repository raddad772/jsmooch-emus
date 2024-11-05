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

//    UDS<<2 + LDS           neither   just LDS   just UDS    UDS+LDS
static u32 write_maskmem[4] = { 0,     0xFF00,    0x00FF,      0 };

static void write_data_port(struct genesis* this, u16 val, u16 mask, u32 is_cpu);

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
        dbg_printf("\nWRITE CRAM addr:%d val:%04x cyc:%d", addr, o, *this->m68k.trace.cycles);
    }
    printf("\nWRITE CRAM addr:%d val:%04x cyc:%lld", addr, o, this->clock.master_cycle_count);
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
    this->vdp.display->scan_x = this->vdp.display->scan_y = 0;
    this->clock.master_frame++;
    //printf("\nNEW GENESIS FRAME! %lld", this->clock.master_frame);
    this->clock.vdp.field ^= 1;
    this->clock.vdp.vcount = 0;
    this->clock.vdp.vblank = 0;
    this->vdp.cur_output = this->vdp.display->output[this->vdp.display->last_written];
    this->vdp.display->last_written ^= 1;

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
    this->vdp.cur_output = ((u16 *)this->vdp.display->output[this->vdp.display->last_written ^ 1]) + (this->clock.vdp.vcount * 1280);

    this->vdp.display->scan_x = 0;
    this->vdp.display->scan_y++;
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
    //if (val != 0) dbg_printf("\nVRAM WRITE addr:%d val:%02x cyc:%lld", addr, val, *this->m68k.trace.cycles);
    u32 LDS = addr & 1;
    u32 UDS = LDS ^ 1;
    u32 mask = write_maskmem[LDS  + (UDS << 1)];
    //printf("\nMASK! %02x LDS:%d UDS:%d", mask, LDS, UDS);
    addr &= 0xFFFF;
    this->vdp.VRAM[addr >> 1] = (this->vdp.VRAM[addr >> 1] & mask) | (val & ~mask);
}
static u32 fifo_full(struct genesis* this)
{
    return this->vdp.fifo.len > 3;
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
            // 16 15 14 13 0 0 0
            vdp->io.plane_a_table_addr = (val & 0b00111000) << 10;
            // 13 is 3
            // << 10
            return;
        case 3: // Window nametable location
            vdp->io.window_table_addr = (val & 0b1111110) << 9; // lowest bit ignored in H40
            return;
        case 4: // Plane B location
            vdp->io.plane_b_table_addr = (val & 7) << 13;
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

static u16 dma_read(struct genesis* this, u32 addr)
{
    if ((addr >= 0x400000) && (addr < 0xFF0000)) {
        printf("\nMODE:%06x ADDR:%06x", (this->vdp.dma.mode & 1) << 23, this->vdp.dma.source << 1);
        fflush(stdout);
    }
    //assert((addr < 0x400000) || (addr >= 0xFF0000));
    if (addr < 0x400000) {
        return genesis_cart_read(&this->cart, addr, 0xFFFF, 1);
    }
    else {
        return this->RAM[(addr >> 1) & 0x7FFF];
    }
}

static inline void inc_dma_source(struct genesis* this)
{
    //printf("\nDMA SOURCE IN:%06x", this->vdp.dma.source);
    this->vdp.dma.source = (((this->vdp.dma.source & 0xFFFF) + 1) & 0xFFFF) | (this->vdp.dma.source & 0xFF0000);
    //printf(" OUT:%06x", this->vdp.dma.source);
}


static void dma_load(struct genesis* this)
{
    // Copy 1 byte
    printf("\nDMA RAM:%06x len:%04x VRAM:%04x (cyc:%lld) mode:%d target:%d", ((this->vdp.dma.mode & 1) << 23) | (this->vdp.dma.source << 1), this->vdp.dma.len, this->vdp.command.address, this->clock.master_cycle_count, this->vdp.dma.mode, this->vdp.command.target);
    FILE *f = fopen("/users/dave/Documents/emu/rom/genesis/dma.log", "a");
    char FSTR[500];
    while(--this->vdp.dma.len) {
        // Read 2 bytes, write 2 bytes....
        u32 addr = ((this->vdp.dma.mode & 1) << 23) | (this->vdp.dma.source << 1);
        u32 data = dma_read(this, addr);
        write_data_port(this, data, 0xFFFF, 0);
        int a = sprintf(FSTR, "VDP DMA: load(%06x, %d:%05x, %04x)\n", addr, this->vdp.command.target, this->vdp.command.address, data);
        fwrite(FSTR, 1, a+1, f);
        inc_dma_source(this);
    }
    fflush(f);
    fclose(f);
}

static void dma_fill(struct genesis* this)
{
    this->vdp.dma.active = 5;
    this->vdp.dma.fill_pending = 0;


    FILE *f = fopen("/users/dave/Documents/emu/rom/genesis/dma.log", "a");
    char FSTR[500];

    while(--this->vdp.dma.len) {
        switch (this->vdp.command.target) {
            case 1:
                write_VRAM_byte(this, this->vdp.command.address ^ 1, this->vdp.command.fill_value >> 8);
                break;
            case 3:
                write_CRAM(this, this->vdp.command.address >> 1, this->vdp.command.fill_value);
                break;
            case 5:
                write_VSRAM(this, this->vdp.command.address >> 1, this->vdp.command.fill_value);
                break;
        }
        int a = sprintf(FSTR, "VDP DMA: fill(%01x:%05x, %04x)\n", this->vdp.command.target, this->vdp.command.address, this->vdp.command.fill_value);
        fwrite(FSTR, 1, a+1, f);

        inc_dma_source(this);
        this->vdp.command.address += this->vdp.command.increment;
    }
    fflush(f);
    fclose(f);
}

static void dma_copy(struct genesis* this)
{
    u8 *vram_ptr = ((u8 *)this->vdp.VRAM);
    while (--this->vdp.dma.len) {
        u16 data = vram_ptr[(this->vdp.dma.source ^ 1) & 0xFFFF];
        vram_ptr[this->vdp.command.address ^ 1] = data;
        inc_dma_source(this);
        this->vdp.command.address += this->vdp.command.increment;
    }
}

static void write_control_port(struct genesis* this, u16 val, u16 mask)
{
    // c000004
    printif(vdp, "\nVDP controlport write: %04x", val);
    // Start DMA here if needed...
    // this->vdp.io.enable_dma
    // this->vdp.command.increment = val;
    // this->vdp->dma.len
    // this->vdp->dma.source (addr)
    // this->vdp->dma.source. is 16-bit addr
    // this->vdp->dma.mode...only 2 bits
    // this->vdp.command.latch

    // DMA modes
    // 0x    memory to VRAM, and the lower mode bit is upper address bit
    // 10    VRAM fill
    // 11    VRAM copy

    //upper bits 10 , RS# 8-12, #0-7 = data to write
    u32 m = ((val >> 14) & 3);
    if ((this->vdp.command.latch == 0) && (m == 2)) { // Register read/write
        u32 reg = (val >> 8) & 31;
        write_vdp_reg(this, reg, val & 0xFF, mask);
        return;
    }

    if (this->vdp.command.latch == 0) { // 1st write! time to do adress setup then!
        this->vdp.command.target = m;
        this->vdp.command.address = val & 0x3FFF;
        this->vdp.command.latch = 1;
        return;
    }

    // INITIATE DMA!
    this->vdp.command.target |= (val >> 2) & 0xC;
    this->vdp.command.address |= (val & 3) << 14;

    if (!(this->vdp.io.enable_dma && (val & 0x80))) return;

    switch(this->vdp.dma.mode) {
        case 0: // mem to VRAM
        case 1:
            this->vdp.dma.active = (this->vdp.dma.len - 1) & 0xFFFF;
            this->vdp.dma.fill_pending = 0;
            dma_load(this);
            break;
        case 2: // VRAM fill
            printf("\nALMOST DMA FILL target:%04x len:%04x", this->vdp.command.address, this->vdp.dma.len);
            this->vdp.dma.fill_pending = 1;
            //command.ready              = data.bit(6) | command.target.bit(0);
            //if ()
            break;
        case 3: // VRAM copy
            printf("\nVRAM COPY");
            this->vdp.dma.active = (this->vdp.dma.len - 1) & 0xFFFF;
            this->vdp.dma.fill_pending = 0;
            dma_copy(this);
            break;
    }
}

static u16 read_control_port(struct genesis* this, u16 old, u32 has_effect)
{
    this->vdp.command.latch = 0;

    u16 v = 0; // NTSC only for now
    v |= this->vdp.dma.active ? 1 : 0;
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
    u16 rval = 0xFFFF;

    switch(this->vdp.command.target) {
        case 0b000000: // VRAM read
            rval = this->vdp.VRAM[(this->vdp.command.address >> 1) & 0x7FFF];
            break;
        case 0b001000: // CRAM read
            assert(mask == 0xFFFF);
            rval = this->vdp.CRAM[(this->vdp.command.address > 127 ? 127 : this->vdp.command.address) >> 1];
            break;
        case 0b000100: // VSRAM read
            rval = this->vdp.VSRAM[(this->vdp.command.address >> 1) % 40];
            break;
        default:
            printf("\nUNKNOWN DATA PORT READ TYPE %d", this->vdp.command.target);
            break;
    }

    this->vdp.command.address += this->vdp.command.increment;

    return rval;
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

static void write_data_port(struct genesis* this, u16 val, u16 mask, u32 is_cpu)
{
    // C00000
    if (dbg.traces.vdp) dbg_printf("\nVDP data port write val:%04x is_cpu:%d target:%d", val, is_cpu, this->vdp.command.target);
    this->vdp.command.latch = 0;

    if (is_cpu && fifo_full(this)) {
        if (fifo_overfull(this)) assert(1==2);
        // Stall CPU here...
        /*this->io.m68k.VDP_FIFO_stall = 1; // We're going to over-fill the FIFO and stall m68k until it's back to normal
        printf("\nDONE1");
        set_m68k_dtack(this);
        dbg_printf("\nPausing the m68k....");*/
        // no stalls...
    }

    if (this->vdp.dma.fill_pending) {
        this->vdp.command.fill_value = val;
        dma_fill(this);
        return;
    }

    //fifo_write(this, this->vdp.command.target, this->vdp.command.address, val);
    // all 16 bits = data
    switch(this->vdp.command.target) {
        // hi and lo bytes are exchanged if A0=1
        case 0b000001: // VRAM write
            assert(mask==0xFFFF);
            if (this->vdp.command.address & 1) val = (((val & 0xFF00) >> 8) | (val << 8)) & 0xFFFF;
            this->vdp.VRAM[(this->vdp.command.address >> 1) & 0x7FFF] = val;
            break;
        case 0b000011: {// CRAM write
            u32 a = this->vdp.command.address;
            if (a > 127) a = 127;
            this->vdp.CRAM[a >> 1] = val;
            break;
        }
        case 0b000101: // VSRAM write
            this->vdp.VSRAM[(this->vdp.command.address >> 1) % 40] = val;
            break;
        default:
            printf("\nunknown VDP write target %d", this->vdp.command.target);
            break;
    }
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
    this->clock.vdp.line_size = 320;
    this->clock.vdp.hblank = this->clock.vdp.vblank = 0;
    this->clock.vdp.vcount = this->clock.vdp.vcount = 0;
    this->vdp.cur_output = (u16 *)this->vdp.display->output[this->vdp.display->last_written ^ 1];
    set_clock_divisor(this);
}

static void do_pixel(struct genesis* this)
{
    // Render a pixel!
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
    u32 hflip[2] = {(pattern_entry[0] >> 11) & 1, (pattern_entry[1] >> 11) & 1};
    u32 vflip[2] = {(pattern_entry[0] >> 12) & 1, (pattern_entry[1] >> 12) & 1};
    u32 palette[2] = {((pattern_entry[0] >> 13) & 3) << 4, ((pattern_entry[1] >> 13) & 3) << 4};
    u32 priority[2] = {(pattern_entry[0] >> 15) & 1, (pattern_entry[1] >> 15) & 1};
    u32 tile_addr[2] = { (tile_number[0] << 5), (tile_number[1] << 5)};

    //u16 *optr = this->vdp.display->output[0] + (1280 * ypos) + xpos;

    u8* tile_ptr[2] = { };
    u32 yp = ypos & 7;
    u32 tile_y[2] = {vflip[0] ? 7 - yp : yp, vflip[1] ? 7 - yp : yp};
    tile_ptr[0] = ((u8*)this->vdp.VRAM) + (tile_addr[0]) + (tile_y[0] << 2);
    tile_ptr[1] = ((u8*)this->vdp.VRAM) + (tile_addr[1]) + (tile_y[1] << 2);
    // 4 bytes per line of 8 pixels
    //static const u32 offset[4] = {0, 1, 2, 3};
    // 4 bytes = 8 pixels
    u32 pw = this->vdp.io.h40 ? 4 : 5;
    u32 bg = this->vdp.CRAM[this->vdp.io.bg_color];
    u32 plane = 1;
    //for (u32 plane=0; plane<2; plane++) { // For 2 planes...
        for (u32 i = 0; i < 4; i++) { // For 8 pixels...
            u32 offset = hflip[plane] ? 3 - i : i;
            u8 px2 = tile_ptr[plane][offset];
            u32 v;
            if (hflip[plane]) v = px2 & 15;
            else v = px2 >> 4;
            /*if (v == 0) v = bg;
            else v = this->vdp.CRAM[palette[plane] + v];*/

            // Draw 4 or 5 repeated pixel
            for (u32 j = 0; j < pw; j++) {
                *(this->vdp.cur_output) = v;
                this->vdp.cur_output++;
                this->vdp.display->scan_x++;
            }

            if (hflip[plane]) v = px2 >> 4;
            else v = px2 & 15;

            /*if (v == 0) v = bg;
            else v = this->vdp.CRAM[palette[plane] + v];*/

            // Draw 4 or 5 repeated pixel
            for (u32 j = 0; j < pw; j++) {
                *(this->vdp.cur_output) = v;
                this->vdp.cur_output++;
                this->vdp.display->scan_x++;
            }
        }

    //}

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
            // Do FIFO and prefetch here, when we implement it!
            if (this->vdp.dma.active) this->vdp.dma.active--;
        }
        else {
            //if (dbg.trace_on && dbg.traces.fifo)
            //    dbg_printf("\nGOT SLOT:%d ARR:%d CYC:%lld SCC:%d ARRAY:%d", this->vdp.slot_array[this->vdp.sc_array][this->vdp.sc_slot], this->vdp.sc_array, *this->m68k.trace.cycles, this->vdp.sc_slot, this->vdp.sc_array);
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
