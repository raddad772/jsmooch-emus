//
// Created by . on 12/4/24.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "helpers/physical_io.h"
#include "helpers/multisize_memaccess.c"

#include "../nds_bus.h"
#include "nds_cart.h"
#include "../nds_irq.h"
#include "../nds_dma.h"
#include "../nds_debugger.h"
#include "nds_eeprom.h"
#include "nds_flash.h"

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
    this->cart.backup.status.busy = 0;
}

void NDS_cart_direct_boot(struct NDS *this)
{
    this->cart.data_mode = NCDM_main;
    this->cart.io.romctrl.data_block_size = 1;
}

u32 NDS_cart_load_ROM_from_RAM(struct NDS_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);

    if (SRAM_enable) *SRAM_enable = 1;
    this->backup.store = &pio->cartridge_port.SRAM;
    this->backup.store->fill_value = 0xFF;
    struct persistent_store *ps = &pio->cartridge_port.SRAM;
    ps->requested_size = 1 * 1024 * 1024;
    ps->persistent = 1;
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
    if (this->cart.io.spi.slot_mode == 0) v |= this->cart.io.romctrl.busy << 7;
    return v;
}

static u32 data_ready(struct NDS *this)
{
    return this->cart.io.romctrl.busy;
}

u32 NDS_cart_read_romctrl(struct NDS *this)
{
    u32 v = this->cart.io.romctrl.u;
    return v;
}

static void raise_transfer_irq(struct NDS *this) {
    NDS_update_IFs_card(this, NDS_IRQ_CART_DATA_READY);
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

u32 NDS_cart_read_spi(struct NDS *this, u32 bnum)
{
    return this->cart.backup.data_out.b8[bnum];
}

u32 NDS_cart_read_rom(struct NDS *this, u32 addr, u32 sz)
{
    u32 output = 0xFFFFFFFF;

    if(!this->cart.io.romctrl.data_ready) {
        return output;
    }

    if (this->cart.cmd.sz_out != 0) {
        output = this->cart.cmd.data_out[this->cart.cmd.pos_out++ % this->cart.cmd.sz_out];
    }
    else
        this->cart.cmd.pos_out++;

    this->cart.io.romctrl.data_ready = 0;

    if (this->cart.cmd.pos_out == this->cart.cmd.sz_out) {
        if (this->cart.cmd.data_in[0] == 0xB7) dbgloglog(NDS_CAT_CART_READ_COMPLETE, DBGLS_INFO, "Finish read of %d words %d %d", this->cart.cmd.sz_out, this->cart.cmd.pos_out, this->cart.cmd.sz_out);
        this->cart.io.romctrl.busy = 0;
        this->cart.cmd.pos_out = 0;
        this->cart.cmd.sz_out = 0;

        if (this->cart.io.transfer_ready_irq) {
            NDS_update_IFs_card(this, NDS_IRQ_CART_DATA_READY);
        }
    } else {
        this->cart.rom_busy_until = NDS_clock_current7(this) + rom_transfer_time(this, this->cart.io.romctrl.transfer_clk_rate, 4);
        if (this->cart.sch_sch) scheduler_delete_if_exist(&this->scheduler, this->cart.sch_id);
        scheduler_add_or_run_abs(&this->scheduler, this->cart.rom_busy_until, NDANB_after_read, this, &NDS_cart_check_transfer, &this->cart.sch_sch);
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

void pprint_mem(void *ptr, u32 num_bytes, u32 addr)
{
    printf("\nROM at addr:%08x", addr);
    u8 *p = (u8 *)ptr;
    for (u32 i = 0; i < num_bytes; i++) {
        printf(" %02x", p[i]);
    }
}

static void handle_cmd(struct NDS *this)
{
    this->cart.cmd.pos_out = 0;
    this->cart.cmd.sz_out = 0;
    this->cart.io.romctrl.data_ready = 0;

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

            dbgloglog(NDS_CAT_CART_READ_START, DBGLS_INFO, "Start read of %d words from cart:%06x", this->cart.cmd.sz_out, address);

            memcpy(this->cart.cmd.data_out, (char *)this->cart.ROM.ptr+address, this->cart.cmd.sz_out * 4);
            //pprint_mem(this->cart.cmd.data_out, 4, address);
            break;
        }
        case 0xB8: { // rom chip ID
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
        if (this->cart.sch_sch) scheduler_delete_if_exist(&this->scheduler, this->cart.sch_id);
        scheduler_add_or_run_abs(&this->scheduler, this->cart.rom_busy_until, NDANB_after_read, this, &NDS_cart_check_transfer, &this->cart.sch_sch);
    }
    else if(this->cart.io.transfer_ready_irq) {
        NDS_update_IFs_card(this, NDS_IRQ_CART_DATA_READY);
    }
}

static void after_read(struct NDS *this)
{
    this->cart.io.romctrl.data_ready = 1;
    NDS_trigger_dma7_if(this, 2);
    NDS_trigger_dma9_if(this, NDS_DMA_DS_CART_SLOT);
}

void NDS_cart_check_transfer(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    switch(key) {
        case NDANB_none:
            return;
        case NDANB_handle_cmd:
            handle_cmd(this);
            return;
        case NDANB_after_read:
            after_read(this);
            return;
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

static void inc_addr(struct NDS *this)
{
    this->cart.backup.cmd_addr = (this->cart.backup.cmd_addr & 0xFFFFFF00) | ((this->cart.backup.cmd_addr + 1) & 0xFF);
}


void NDS_cart_spi_transaction(struct NDS *this, u32 val)
{
    switch(this->cart.backup.detect.kind) {
        case NDSBK_none:
            printf("\nSPI transactions with no backup!?");
            return;
        case NDSBK_flash:
            //printf("\nflash transact: %02x", val);
            NDS_flash_spi_transaction(this, val);
            return;
        case NDSBK_eeprom:
            //printf("\neeprom transact...");
            NDS_eeprom_spi_transaction(this, val);
            return;
        case NDSBK_fram:
            printf("\nFRAM not implement!");
            return;
    }
}

void NDS_cart_write_romctrl(struct NDS *this, u32 val)
{
    u32 mask = 0x7FFFFFFF;
    this->cart.io.romctrl.u = (val & 0x7FFFFFFF) | (this->cart.io.romctrl.u & 0x80000000);
    if ((val & 0x80000000) && !this->cart.io.romctrl.busy) {
        // Trigger handle of command
        this->cart.io.romctrl.busy = 1;

        this->cart.rom_busy_until = NDS_clock_current7(this) + rom_transfer_time(this, this->cart.io.romctrl.transfer_clk_rate, 8);
        if (this->cart.sch_sch) scheduler_delete_if_exist(&this->scheduler, this->cart.sch_id);
        scheduler_add_or_run_abs(&this->scheduler, this->cart.rom_busy_until, NDANB_handle_cmd, this, &NDS_cart_check_transfer, &this->cart.sch_sch);
    }
}

void NDS_cart_write_cmd(struct NDS *this, u32 addr, u32 val)
{
    this->cart.cmd.data_in[addr] = val;
}


#define FS_SUBSYS 0xB
void NDS_cart_detect_kind(struct NDS *this, u32 from, u32 val)
{
    // subsystem tag (bits 0-4) and the data portion (bits 6-31)
    u32 subsystem = val & 0x1F;
    u32 reset = 1;
    if (subsystem == FS_SUBSYS) {
        u32 data = val >> 6;
        if ((this->cart.backup.detect.pos == 0) && (from == 9) && (data == 0)) {
            reset = 0;
        }
        else if ((this->cart.backup.detect.pos < 3) && (from == 9) && (!this->cart.backup.detect.done)) {
            reset = 0;
            this->cart.backup.detect.arg_buf_addr = data;
            u32 second_word = cR32(this->mem.RAM, (this->cart.backup.detect.arg_buf_addr + 4) & 0x3FFFFF);
            // bits 0-1 encode the savedata type (0=none, 1=EEPROM, 2=flash, 3=fram
            // - never seen fram used), and
            // bits 8-15 encode the backup size in a shift amount - i.e. the size in bytes is (1 << n)
            u32 kind = second_word & 3;
            u32 sz = 1 << ((second_word >> 8) & 0xFF);
            printf("\nDETECT CART SAVE KIND:%d SZ:%d bytes", kind, sz);
            this->cart.backup.detect.kind = kind;
            this->cart.backup.detect.sz = sz;
            this->cart.backup.detect.sz_mask = sz - 1;
            this->cart.backup.detect.done = 1;
            this->cart.backup.store->requested_size = sz;
            switch(this->cart.backup.detect.kind) {
                case 0: // none
                break;
                case 1: // eeprom
                    NDS_eeprom_setup(this);
                    break;
                case 2: // flash
                    NDS_flash_setup(this);
                    break;
                case 3: // fram
                    printf("\nFRAM IMPLEMENT!");
                    break;
            }
        }
        else if ((this->cart.backup.detect.pos == 1) && (from == 7) && (data == 1)) {
            reset = 0;
        }
        /*if ((this->cart.backup.detect.pos == 3) && (from == 7) && (data == 1)) {
            reset = 0;
            printf("\nSTEP 4");
        }
        if ((this->cart.backup.detect.pos == 4) && (from == 9)) {
            printf("\nSTEP 5?");
        }*/
    }

    if (reset) {
        this->cart.backup.detect.pos = 0;
    }
    else {
        this->cart.backup.detect.pos++;
    }
}