//
// Created by . on 12/4/24.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/multisize_memaccess.c"

#include "../nds_bus.h"
#include "nds_cart.h"
#include "../nds_irq.h"
#include "../nds_dma.h"

void NDS_cart_init(struct NDS* this)
{
    buf_init(&this->cart.ROM);
}

void NDS_cart_delete(struct NDS *this)
{
    buf_delete(&this->cart.ROM);
}

static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

void NDS_cart_reset(struct NDS *this)
{
    // TODO: this
}

void NDS_cart_direct_boot(struct NDS *this)
{
    this->cart.data_mode = NCDM_main;
    this->cart.io.romctrl.data_block_size = 1;
}

u32 NDS_cart_load_ROM_from_RAM(struct NDS_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    return 1;
}

u32 NDS_cart_read_spicnt(struct NDS *this)
{
    u32 v = this->cart.io.spi.divider_val;
    v |= this->cart.io.spi.next_chipsel << 6;
    v |= this->cart.io.spi.slot_mode << 13;
    v |= this->cart.io.transfer_ready_irq << 14;
    v |= this->cart.io.nds_slot_enable << 15;
    // 0 = ready, 1 = busy
    v |= (NDS_clock_current7(this) < this->cart.spi_busy_until) << 7;
    return v;
}

static u32 data_ready(struct NDS *this)
{
    return NDS_clock_current7(this) >= this->cart.rom_busy_until;
}

u32 NDS_cart_read_romctrl(struct NDS *this)
{
    u32 v = this->cart.io.romctrl.u;
    return v;
}

static void raise_transfer_irq(struct NDS *this) {
    NDS_update_IFs(this, 19);
    printf("\nRAISE TRANSFER IRQ...");
}

static u32 get_transfer_irq_bits(struct NDS *this) {
    return this->cart.io.transfer_ready_irq && !this->cart.io.romctrl.block_start_status;
}

static void set_block_start_status(struct NDS *this, u32 val, u32 transfer_ready_irq)
{
    u32 old_bits = get_transfer_irq_bits(this);
    this->cart.io.romctrl.block_start_status = val;
    this->cart.io.transfer_ready_irq = transfer_ready_irq;
    u32 new_bits = get_transfer_irq_bits(this);

    if (!old_bits && new_bits)
        raise_transfer_irq(this);
}

static void set_rom_busy_until(struct NDS *this)
{
    static const u32 cpb[2] = { 5, 8 };
    u32 transfer_len = cpb[this->cart.io.romctrl.transfer_clk_rate] * 4;
    this->cart.rom_busy_until = NDS_clock_current7(this) + transfer_len;
}

u32 NDS_cart_read_rom(struct NDS *this, u32 addr, u32 sz)
{
    u32 output = 0xFFFFFFFF;
    if (!data_ready(this)) return output;

    if (this->cart.cmd.sz_out != 0) {
        output = this->cart.cmd.data_out[this->cart.cmd.pos_out % this->cart.cmd.sz_out];
    }
    this->cart.cmd.pos_out++;

    this->cart.io.romctrl.data_word_status = 0;
    this->cart.rom_reading = 1;

    // If we've done all of our data...
    if (this->cart.cmd.pos_out == this->cart.cmd.sz_out) {
        this->cart.cmd.pos_out = 0;
        this->cart.cmd.sz_out = 0;
        set_block_start_status(this, 0, this->cart.io.transfer_ready_irq);
    }
    else {
        set_rom_busy_until(this);
    }

    return output;
}

void NDS_cart_check_transfer(struct NDS *this)
{
    if (this->cart.rom_reading && data_ready(this)) {
        this->cart.rom_reading = 0;
        this->cart.io.romctrl.data_word_status = 1;
        // Schedule DMAs if needed
        NDS_trigger_dma7_if(this, 5);
        NDS_trigger_dma9_if(this, 5);
    }
}

void NDS_cart_spi_write_spicnt(struct NDS *this, u32 val, u32 bnum)
{
    if (bnum == 0) {
        this->cart.io.spi.divider_val = val & 3;
        switch (this->cart.io.spi.divider_val) {
            case 0:
                this->cart.io.spi.divider = 8;
                break;
            case 1:
                this->cart.io.spi.divider = 16;
                break;
            case 2:
                this->cart.io.spi.divider = 32;
                break;
            case 3:
                this->cart.io.spi.divider = 64;
                break;
        }
        this->cart.io.spi.next_chipsel = (val >> 6) & 1;
    }
    else {
        this->cart.io.spi.slot_mode = (val >> 5) & 1;
        this->cart.io.nds_slot_enable = (val >> 7) & 1;
        set_block_start_status(this, this->cart.io.romctrl.block_start_status, (val >> 6) & 1);
    }
}

void NDS_cart_spi_transaction(struct NDS *this, u32 val)
{
    printf("\ncart SPI transaction!?");
}

void NDS_cart_write_romctrl(struct NDS *this, u32 val)
{
    u32 old_bits = this->cart.io.romctrl.u & (1 << 29);
    old_bits |= this->cart.io.romctrl.u & (1 << 31);

    this->cart.io.romctrl.u = (val & 0b01011111011111111111111111111111) | old_bits;
}

static u32 get_block_size(struct NDS *this)
{
    u32 v = this->cart.io.romctrl.data_block_size;
    switch(v) {
        case 0:
            return 0;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            return 0x100 << v;
        case 7:
            return 4;
    }
    return 0;
}

static void start_read(struct NDS *this)
{
    if (this->cart.cmd.sz_out == 0) return;
    /* Read the bytes... */
    u32 start = this->cart.cmd.addr % this->cart.ROM.size;
    u32 end = start + this->cart.cmd.sz_out;
    if (end >= this->cart.ROM.size) {
        end = this->cart.ROM.size - 1;
        printf("\nINVALID END!");
    }
    if (end <= start) {
        printf("\nDOUBLE INVALID START!");
        return;
    }
    memcpy(&this->cart.cmd.data_out, this->cart.ROM.ptr+start, (end - start));
}

static void complete_cmd(struct NDS *this)
{
    u32 cmd_byte = this->cart.cmd.data_in[0];
    switch(cmd_byte) {
        case 0xB7: {// Read! B7aaaaaaaa000000h
            // 4 bytes of address...
            u32 addr = this->cart.cmd.data_in[1] << 24;
            addr |= this->cart.cmd.data_in[2] << 16;
            addr |= this->cart.cmd.data_in[3] << 8;
            addr |= this->cart.cmd.data_in[4];
            this->cart.cmd.pos_out = 0;
            this->cart.cmd.sz_out = get_block_size(this);
            this->cart.cmd.addr = addr * this->cart.cmd.sz_out;
            printf("\nREAD CART ADDR:%08x bytes:%d", this->cart.cmd.addr, this->cart.cmd.sz_out);
            start_read(this);

            set_rom_busy_until(this);
            return; }
        default:
            printf("\nUNKNOWN CART CMD %02x!", cmd_byte);
            return;
    }
}

void NDS_cart_write_cmd(struct NDS *this, u32 addr, u32 val)
{
    this->cart.cmd.data_in[addr] = val;
    if (addr == 7) complete_cmd(this);
}