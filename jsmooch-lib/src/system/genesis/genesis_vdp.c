//
// Created by . on 6/1/24.
//

#include <assert.h>
#include <stdio.h>

#include "helpers/debug.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"

#define SC_ARRAY_H32      0
#define SC_ARRAY_H40      1
#define SC_ARRAY_DISABLED 2


#define MCLOCKS_PER_LINE 3420
#define SC_HSYNC_GOES_OFF 5
#define SC_HSYNC_GOES_ON 658
#define SC_SLOW_SLOTS 662
#define HSYNC_GOES_OFF (SC_HSYNC_GOES_OFF * 5)
#define HSYNC_GOES_ON (SC_HSYNC_GOES_ON*5)
#define SLOW_MCLOCKS_START (SC_SLOW_SLOTS*5)
#define HBLANK_START 1280

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
    

    // Now do rendering disabled
    s = this->vdp.slot_array[SC_ARRAY_DISABLED];

}
#undef S
#undef SR

void VDP_init(struct genesis* this)
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
        this->vdp.sc_slot = SC_ARRAY_DISABLED; //
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

    set_clock_divisor(this);
}

static void latch_vcounter(struct genesis* this)
{
    this->vdp.io.irq_vcounter = this->vdp.io.line_irq_counter; // TODO: load this from register
}

static void set_sc_array(struct genesis* this)
{
    if ((this->clock.vdp.vblank) || (!this->vdp.io.enable_display)) this->vdp.sc_array = SC_ARRAY_DISABLED;
    else this->vdp.sc_array = this->vdp.io.h40;
}

static void new_scanline(struct genesis* this)
{
    hblank(this, 0);
    this->clock.vdp.hcount = 0;
    this->clock.vdp.vcount++;
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

static u16 fifo_empty(struct genesis* this)
{
    return this->vdp.fifo.len == 0;
}

static u16 fifo_full(struct genesis* this)
{
    return this->vdp.fifo.len >= 4;
}

static void write_vdp_reg(struct genesis* this, u16 rn, u16 val, u16 mask)
{
    if (rn >= 24) {
        printf("\nWARNING illegal VDP register write %d %04x on cycle:%lld", rn, val, this->clock.master_cycle_count);
        return;
    }
    struct genesis_vdp* vdp = &this->vdp;
    u32 va;
    val &= 0xFF;
    switch(rn) {
        case 0: // Mode Register 1
            vdp->io.blank_left_8_pixels = (val >> 5) & 1;
            vdp->io.enable_line_irq = (val >> 4) & 1;
            vdp->io.freeze_hv_on_level2_irq = (val >> 1) & 1;
            vdp->io.enable_display = (val & 1) ^ 1;
            return;
        case 1: // Mode regiser 2
            vdp->io.enable_display2 = (val >> 6) & 1;
            vdp->io.enable_virq = (val >> 5) & 1;
            vdp->io.enable_dma = (val >> 4) & 1;
            vdp->io.cell30 = (val >> 3) & 1;
            if (vdp->io.cell30) printf("\nWARNING PAL DISPLAY 240 SELECTED");
            vdp->io.mode5 = val & 1;
            if (!vdp->io.mode5) printf("\nWARNING MODE5 DISABLED");
            return;
        case 2: // Plane A table location
            vdp->io.plane_a_table_addr = (val & 0x1C) << 10;
            return;
        case 3: // Window nametable location
            vdp->io.window_table_addr = (val & 0x3E) << 10; // lowest bit ignored in H40
            return;
        case 4: // Plane B location
            vdp->io.plane_b_table_addr = (val & 7) << 13;
            return;
        case 5: // Sprite table location
            vdp->io.sprite_table_addr = (val & 0x7F) << 9; // lowest bit ignored in H40
            return;
        case 6: // um
            return;
        case 7:
            vdp->io.bg_color = val & 15;
            vdp->io.bg_palette_line = (val >> 4) & 3;
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
            vdp->io.h32 = (val & 1) == 0;
            vdp->io.h40 = (val & 1) != 0;
            set_sc_array(this);
            if (vdp->io.h32) printf("\n32-cell 256 pixel mode selected");
            if (vdp->io.h40) printf("\n40-cell 320 pixel mode selected");
            set_clock_divisor(this);
            vdp->io.enable_shadow_highlight = (val >> 3) & 1;
            u32 im = (val >> 1) & 3;
            vdp->io.interlace_mode = (im == 2) ? 0 : im;
            return;
        case 13: // horizontal scroll data addr
            vdp->io.hscroll_addr = (val & 0x3F) << 10;
            return;
        case 14: // mostly unused
            return;
        case 15:
            vdp->io.auto_increment = val;
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
            vdp->dma.len = ((vdp->dma.len & 0xFF) | (val << 8) & 0xFF00);
            return;
        case 21:
            vdp->dma.source_addr = (vdp->dma.source_addr & 0xFFFF00) | (val & 0xFF);
            return;
        case 22:
            vdp->dma.source_addr = (vdp->dma.source_addr & 0xFF00FF) | ((val << 8) & 0xFF00);
            return;
        case 23:
            vdp->dma.source_addr = (vdp->dma.source_addr & 0xFFFF) | (((val & 0xFF) << 16) & 0x3F0000);
            vdp->dma.direction = (val >> 6) & 3;
            return;
    }
    printf("\nUNHANDLED WRITE TO VDP REG:%d val:%04x", rn, val);
}

static void write_control_port(struct genesis* this, u16 val, u16 mask)
{
    if (this->vdp.command.latch == 0) {
        // Collect first 16 bits of word
        // Write register if it's correct kind
        if (((val >> 7) & 7) == 0b100) {
            this->vdp.command.latch = 0;
            u32 rn = (val >> 8) & 0x1F;
            write_vdp_reg(this, rn, val & 0xFF, mask);
            return;
        }

        this->vdp.command.latch = 1;
        this->vdp.command.val = val << 16;
        return;
    }
    if (this->vdp.command.latch == 2) {
        printf("\nERROR IT SHOULD NEVER BE 2! cyc:%lld", this->clock.master_cycle_count);
        this->vdp.command.latch = 0;
        return;
    }

    // Write final 16 bits of command and start DMA if it's enabled
    this->vdp.command.latch = 0;
    this->vdp.command.val |= val;
    val = this->vdp.command.val;
    u32 bit76 = (val & 0b11000000) >> 6;

    this->vdp.dma.dest_addr = ((val >> 16) & 0x7FFE) | ((val & 3) << 14);

    this->vdp.command.latch = 0;

    if (bit76 == 0b10) { // Start m68k->VRAM transfer
        u32 cd = ((val >> 31) & 1) | ((val >> 3) & 2);
        assert(cd < 3); // 0 1 and 2 are valid values for us
        this->vdp.dma.direction = cd;
        this->vdp.dma.active = this->vdp.io.enable_dma;
        printf("\nM68k->%d transfer started cyc:%lld", cd, this->clock.master_cycle_count);
    }
    else if (bit76 == 0x0b11) { // Start VRAM->VRAM or VRAM fill
        if ((this->vdp.dma.source_addr & 0xB00000) == 0xB00000) {// VRAM->VRAM
            this->vdp.dma.direction = 3;
            this->vdp.dma.active = this->vdp.io.enable_dma;
            printf("\nVRAM->VRAM transfer started cyc:%lld", this->clock.master_cycle_count);
        }
        else {  // VRAM fill
            printf("\nVRAM fill primed cyc:%lld", this->clock.master_cycle_count);
            this->vdp.dma.direction = 4;
            this->vdp.command.latch = 2;
        }
    }
}

static u16 read_control_port(struct genesis* this, u16 old, u32 has_effect)
{
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


u16 genesis_VDP_mainbus_read(struct genesis* this, u32 addr, u16 old, u16 mask, u32 has_effect)
{
    /* 110. .000 .... .... 000m mmmm */
    if (((addr & 0b110000000000000000000000) ^ 0b001001110000000011100000) != 0b111001110000000011100000) {
        printf("\nERROR LOCK UP VDP!!!!");
        return 0;
    }

    addr = (addr & 0x1F) | 0xC00000;

    switch(addr) {
        case 0xC00004: // VDP control
            return read_control_port(this, old, has_effect);
    }

    printf("\nBAD VDP READ addr:%06x cycle:%lld", addr, this->clock.master_cycle_count);
    gen_test_dbg_break(this);
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
            if (this->vdp.command.latch == 2) {
                this->vdp.dma.fill_value = val >> 8;
                this->vdp.dma.active = this->vdp.io.enable_dma;
                this->vdp.command.latch = 0; // Nothing is latched. 1 = 1 word is latched. 2 = 2 words are latched, only happens for fill mode
                printf("\nDMA FILL STARTED TO:%04x len:%d cycle:%lld", this->vdp.dma.dest_addr, this->vdp.dma.len, this->clock.master_cycle_count);
                return;
            }

            // No DMA fill start, just a plain write to VRAM
            if (fifo_full(this)) this->io.m68k.VDP_FIFO_stall = 1; // We're going to over-fill the FIFO and stall m68k until it's back to normal

            u32 slot = (this->vdp.fifo.head + this->vdp.fifo.len) % 5;
            this->vdp.fifo.len = this->vdp.fifo.len + 1;
            assert(this->vdp.fifo.len < 6);
            struct genesis_vdp_fifo_slot *e = &this->vdp.fifo.slots[slot];

            e->addr = this->vdp.dma.dest_addr;
            e->val = val;
            e->UDS = !!(mask & 0xFF00);
            e->LDS = !!(mask & 0xFF);
            e->target = ft_VRAM;
            break;
        case 0xC00004: // VDP control
            write_control_port(this, val, mask);
            return;
    }
    //printf("\nBAD VDP WRITE addr:%06x val:%04x cycle:%lld", addr, val, this->clock.master_cycle_count);
    gen_test_dbg_break(this);
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

//    UDS<<2 + LDS           neither   just LDS   just UDS    UDS+LDS
static u32 write_maskmem[4] = { 0,     0xFF00,    0x00FF,      0 };


inline void inc_source_addr(struct genesis* this)
{
    // Increment only the lower 16 (17) bits
    this->vdp.dma.source_addr = (this->vdp.dma.source_addr & 0xFF0000) | ((this->vdp.dma.source_addr + (this->vdp.io.auto_increment >> 1)) & 0xFFFF);
}

static void DMA_load_FIFO(struct genesis* this)
{
    if (!this->vdp.dma.active) return;
    if (this->vdp.fifo.len >= 4) return; // We DO want to load up if we already have 4...

    // Don't move head, since we're adding to the tail.
    u32 slot = (this->vdp.fifo.head + this->vdp.fifo.len) % 5;
    this->vdp.fifo.len++;

    struct genesis_vdp_fifo_slot *e = &this->vdp.fifo.slots[slot];

    // 00 = m68k->VRAM. 01 = m68k->CRAM. 10 = m68k->VSRAM. 11 = VRAM->VRAM
    // xfer 1 byte to VRAM, or 2 bytes to VSRAM/CRAM
    u32 src_addr = (this->vdp.dma.source_addr & 0x7FFFFF) << 1;
    e->addr = this->vdp.dma.dest_addr;
    e->UDS = e->LDS = 1;
    switch(this->vdp.dma.direction) {
        case 0: // m68k->VRAM, 1 byte at a time
            e->target = ft_VRAM;
            e->val = genesis_mainbus_read(this, src_addr, 1, 1, 0, 1);
            break;
        case 1: // m68k->CRAM
            e->target = ft_CRAM;
            e->val = genesis_mainbus_read(this, src_addr, 1, 1, 0, 1);
            break;
        case 2: // m68k->VSRAM
            e->target = ft_VSRAM;
            e->val = genesis_mainbus_read(this, src_addr, 1, 1, 0, 1);
            break;
        case 3: // VRAM->VRAM
            e->target = ft_VRAM;
            e->val = this->vdp.VRAM[(src_addr & 0xFFFF) >> 1];
            break;
        case 4: // fill VRAM
            e->target = ft_VRAM;
            e->val = this->vdp.dma.fill_value | (this->vdp.dma.fill_value << 8);
            break;
        default:
            assert(1==0);
    }

    inc_source_addr(this);
    this->vdp.dma.dest_addr = (this->vdp.dma.dest_addr + 1) & 0xFFFF;
    this->vdp.dma.len--;
    if (this->vdp.dma.len <= 0) {
        this->vdp.dma.active = 0;
        printf("\nDMA TRANSFER FINISH AT CYCLE %lld", this->clock.master_cycle_count);
    }
}

// Discharge 1 FIFO entry, if external slot is available. Also refills it if DMA is ongoing
static void discharge_FIFO(struct genesis* this)
{
    // Empty FIFO, don't bother...
    if (this->vdp.fifo.len == 0) {
        assert(this->io.m68k.VDP_FIFO_stall == 0);
        return;
    }
    struct genesis_vdp_fifo_slot *e = &this->vdp.fifo.slots[this->vdp.fifo.head];

    u32 shorten_fifo = 1;

    // Do the write
    u32 vi = write_maskmem[(e->UDS << 1) + e->LDS];
    switch(e->target) {
        case ft_VRAM: { // 1 byte at a time. So first do UDS and bail, then do LDS next time an complete
            u32 v = this->vdp.VRAM[e->addr & 0xFFFE];
            if (e->UDS) { // Write high byte, DO NOT shorten FIFO! 1 byte at a time...
                this->vdp.VRAM[e->addr & 0xFFFE] = (v & 0xFF) | (e->val & 0xFF00);
                e->UDS = 0;
                shorten_fifo = 0;
            }
            else { // Write low byte, DO shorten FIFO!
                this->vdp.VRAM[e->addr & 0xFFFE] = (v & 0xFF00) | (e->val & 0xFF);
                e->LDS = 0;
            }
            break; }
        case ft_CRAM: { // 16 bits at a time
            u32 addr = e->addr & 63;
            // TODO: addr >> 1 ?
            this->vdp.CRAM[addr] = (this->vdp.CRAM[addr] & vi) | (e->val & (~vi)) & ;
            break; }
        case ft_VSRAM: { // 16 bits at a time
            u32 addr = e->addr % 20;
            this->vdp.VSRAM[addr] = (this->vdp.VSRAM[addr] & vi) | (e->val & (~vi)) & ;
            break; }
        default:
            assert(1==0);
            break;
    }

    if (shorten_fifo) {
        this->vdp.fifo.len--;
        this->vdp.fifo.head = (this->vdp.fifo.head + 1) % 5;
        if ((!this->vdp.dma.active) && (this->io.m68k.VDP_FIFO_stall)) { // Unpause m68k if it was previously held up by this
            this->io.m68k.VDP_FIFO_stall = 0;
            this->m68k.pins.DTACK = 1;
        }
        DMA_load_FIFO(this);
    }
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
    if (this->vdp.sc_count == 0) this->vdp.sc_slot++;

    // Do FIFO if this is an external slot
    if (this->vdp.slot_array[this->vdp.sc_array][this->vdp.sc_slot] == slot_external_access) discharge_FIFO(this);

    // 2 cycles per pixel
    this->vdp.cycle ^= 1;
    if (!this->vdp.cycle) return;

    if (this->vdp.sc_slot == SC_HSYNC_GOES_OFF) hblank(this, 0);
    if (this->vdp.sc_slot == SC_HSYNC_GOES_ON) hblank(this, 1);

    // Add a pixel
    this->clock.vdp.hcount++;

    if (this->clock.vdp.line_mclock >= MCLOCKS_PER_LINE)
        new_scanline(this);
}
