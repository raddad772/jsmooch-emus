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
    return this->cart.io.transfer_ready_irq && !this->cart.io.romctrl.busy;
}

static void set_block_start_status(struct NDS *this, u32 val, u32 transfer_ready_irq)
{
    u32 old_bits = get_transfer_irq_bits(this);
    this->cart.io.romctrl.busy = val;
    this->cart.io.transfer_ready_irq = transfer_ready_irq;
    u32 new_bits = get_transfer_irq_bits(this);

    if (!old_bits && new_bits)
        raise_transfer_irq(this);
}

static u32 rom_transfer_time(struct NDS *this, u32 clk_spd, u32 sz_in_bytes)
{
    static const u32 cpb[2] = {5, 8};
    return cpb[clk_spd] * sz_in_bytes;
}

u32 NDS_cart_read_rom(struct NDS *this, u32 addr, u32 sz)
{
    u32 output = 0xFFFFFFFF;

    if(!this->cart.io.romctrl.data_ready) {
        return output;
    }

    if (this->cart.cmd.sz_out != 0) {
        output = this->cart.cmd.data_out[this->cart.cmd.pos_out % this->cart.cmd.sz_out];
    }
    this->cart.cmd.pos_out++;

    this->cart.io.romctrl.data_ready = 0;

    if (this->cart.cmd.pos_out == this->cart.cmd.sz_out) {
        this->cart.io.romctrl.busy = 0;
        this->cart.cmd.pos_out = 0;
        this->cart.cmd.sz_out = 0;

        if (this->cart.io.transfer_ready_irq) {
            NDS_update_IFs(this, 19);
        }
    } else {
        this->cart.after_next_busy = NDANB_after_read;
        this->cart.waiting_for_tx_done = 1;
    }

    return output;
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
            return 64 << v;
        case 7:
            return 1;
    }
    return 0;
}

static void handle_cmd(struct NDS *this)
{
    this->cart.cmd.pos_out = 0;
    this->cart.cmd.sz_out = 0;
    this->cart.io.romctrl.data_ready = 0;

    //u32 data_block_size = this->cart.io.romctrl.data_block_size;

    this->cart.cmd.sz_out = get_block_size(this);
    switch(this->cart.cmd.data_in[0]) {
        case 0xB7: { // Read!
            u32 address = this->cart.cmd.data_in[1] << 24;
            address |= this->cart.cmd.data_in[2] << 16;
            address |= this->cart.cmd.data_in[3] <<  8;
            address |= this->cart.cmd.data_in[4] <<  0;

            address %= this->cart.ROM.size;

            if(address <= 0x7FFF) {
                address = 0x8000 + (address & 0x1FF);
            }

#ifndef MIN
#define MIN(a,b) ((a) < (b)) ? (a) : (b)
#endif
            this->cart.cmd.sz_out = MIN(0x80, this->cart.cmd.sz_out);

            memcpy(this->cart.cmd.data_out, this->cart.ROM.ptr+address, this->cart.cmd.sz_out * 4);
            printf("\nCART READ %08x: %d bytes", address, this->cart.cmd.sz_out * 4);
            break;
        }
        case 0xB8: { // rom chip ID
            printf("\nCART GET CHIP ID");
            this->cart.cmd.sz_out = 1;
            this->cart.cmd.data_out[0] = 0x1FC2;
            break;
        }
        default: {
            printf("\nUNHANDLED CMD %02x", this->cart.cmd.data_in[0]);
            break;
        }
    }

    this->cart.io.romctrl.busy = this->cart.cmd.sz_out != 0;

    if (this->cart.io.romctrl.busy) {
        this->cart.rom_busy_until = NDS_clock_current7(this) + rom_transfer_time(this, this->cart.io.romctrl.transfer_clk_rate, 4);
        this->cart.after_next_busy = NDANB_after_read;
        this->cart.waiting_for_tx_done = 1;
    }
    else if(this->cart.io.transfer_ready_irq) {
        NDS_update_IFs(this, 19);
    }
}

static void after_read(struct NDS *this)
{
    this->cart.io.romctrl.data_ready = 1;

    NDS_trigger_dma7_if(this, 5);
    NDS_trigger_dma9_if(this, 5);
}

void NDS_cart_check_transfer(struct NDS *this)
{
    //printf("\nWait for tx done? %d. Clock:%lld busy done:%lld", this->cart.waiting_for_tx_done, this->clock.master_cycle_count7, this->cart.rom_busy_until);
    if (this->cart.waiting_for_tx_done && data_ready(this)) {
        this->cart.waiting_for_tx_done = 0;
        switch(this->cart.after_next_busy) {
            case NDANB_none:
                printf("\nERROR NDANDB: none!");
                return;
            case NDANB_handle_cmd:
                handle_cmd(this);
                return;
            case NDANB_after_read:
                after_read(this);
                return;
        }
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
        this->cart.io.transfer_ready_irq = (val >> 6) & 1;
    }
}

void NDS_cart_spi_transaction(struct NDS *this, u32 val)
{
    printf("\ncart SPI transaction!?");
}

void NDS_cart_write_romctrl(struct NDS *this, u32 val)
{
    u32 mask = 0x7FFFFFFF;
    this->cart.io.romctrl.u = (val & 0x7FFFFFFF) | (this->cart.io.romctrl.u & 0x80000000);
    if ((val & 0x80000000) && !this->cart.io.romctrl.busy) {
        // Trigger handle of command
        this->cart.io.romctrl.busy = 1;

        this->cart.rom_busy_until = NDS_clock_current7(this) + rom_transfer_time(this, this->cart.io.romctrl.transfer_clk_rate, 8);
        this->cart.after_next_busy = NDANB_handle_cmd;
        this->cart.waiting_for_tx_done = 1;
    }
}

void NDS_cart_write_cmd(struct NDS *this, u32 addr, u32 val)
{
    this->cart.cmd.data_in[addr] = val;
}