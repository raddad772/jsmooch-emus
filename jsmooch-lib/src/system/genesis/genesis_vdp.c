//
// Created by . on 6/1/24.
//

#include <assert.h>
#include <stdio.h>

#include "helpers/debug.h"
#include "genesis_bus.h"
#include "genesis_vdp.h"

static inline u32 h32(struct genesis* this)
{
    // TODO: fix this up
    return this->vdp.io.h32;
}

static inline u32 h40(struct genesis* this)
{
    // TODO: fix this up
    return this->vdp.io.h40;
}

static inline u32 fast_h40(struct genesis* this)
{
    // TODO: fix this up
    return h40(this) & this->vdp.io.fast_h40;
}

static void vblank(struct genesis* this, u32 new_value)
{
    u32 old_value = this->clock.vdp.vblank;
    this->clock.vdp.vblank = new_value;  // Our value that indicates vblank yes or no
    this->vdp.io.vblank = new_value;     // Our IO vblank value, which can be reset

    if ((!old_value) && new_value) {
        genesis_m68k_vblank(this, 1);
    }

    if (new_value) {
        if (!fast_h40(this))
            this->clock.vdp.clock_divisor = 5; // unless....
    }
    else {
        if (!h32(this)) // H32 is /5 always
            this->clock.vdp.clock_divisor = 4; // unless....
    }
}


static void new_scanline(struct genesis* this)
{
    this->clock.vdp.hcount = 0;
    this->clock.vdp.vcount++;

    switch(this->clock.vdp.vcount) {
        case 0:
            vblank(this, 0);
            break;
        case 0xE0: // vblank start
            vblank(this, 1);
            this->vdp.io.z80_irq_clocks = 2573; // Thanks @MaskOfDestiny
            genesis_z80_interrupt(this, 1); // Z80 asserts vblank interrupt for 1 scanline
            break;
    }
}


static u16 fifo_empty(struct genesis* this)
{
    return 1;
}

static u16 fifo_full(struct genesis* this)
{
    return 0;
}

static void write_vdp_reg(struct genesis* this, u16 rn, u16 data, u16 mask)
{
    if (rn >= 24) {
        printf("\nWARNING illegal VDP register write %d %04x on cycle:%lld", rn, data, this->clock.master_cycle_count);
        return;
    }
    data &= 0xFF;
    switch(rn) {

    }
    printf("\nUNHANDLED WRITE TO VDP REG:%d data:%04x", rn, data);
}

static void write_control_port(struct genesis* this, u16 data, u16 mask)
{
    if (((data >> 7) & 7) == 0b100) {
        u32 rn = (data >> 8) & 0x1F;
        write_vdp_reg(this, rn, data & 0xFF, mask);
        return;
    }

}

static u16 read_control_port(struct genesis* this, u16 old, u32 has_effect)
{
    u16 v = 0; // NTSC only for now
    v |= 0 << 1; // no DMA yet
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
        genesis_m68k_vblank(this, 0);
    }

    // printf("\nCONTROL PORT RETURN: %04x", v);
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

    addr = (addr & 0x1F) | 0xC00000;
    switch(addr) {
        case 0xC00000: // VDP data port
            this->vdp.command.latch = 0;
            this->vdp.command.ready = 1;
            //fifo_write(this, this->vdp.command.target, this->vdp.command.address, val);
            u16 v = this->vdp.VRAM[this->vdp.command.address >> 1] & (~mask);
            v |= (val & mask);
            this->vdp.VRAM[this->vdp.command.address >> 1] = v;
            this->vdp.command.address += this->vdp.command.increment;

            // the read command is processed after all writes have been processed and FIFO is empty.
        case 0xC00004: // VDP control
            write_control_port(this, val, mask);
            return;
    }
    //printf("\nBAD VDP WRITE addr:%06x val:%04x cycle:%lld", addr, val, this->clock.master_cycle_count);
    gen_test_dbg_break(this);
}

void genesis_VDP_cycle(struct genesis* this)
{
    // yo!
    if (this->vdp.io.z80_irq_clocks) {
        this->vdp.io.z80_irq_clocks -= this->clock.vdp.clock_divisor;
        if (this->vdp.io.z80_irq_clocks <= 0) {
            this->vdp.io.z80_irq_clocks = 0;
            genesis_z80_interrupt(this, 0);
        }
    }

    // 2 cycles per pixel
    this->vdp.cycle ^= 1;
    if (!this->vdp.cycle) return;

    this->clock.vdp.hcount++;
}
