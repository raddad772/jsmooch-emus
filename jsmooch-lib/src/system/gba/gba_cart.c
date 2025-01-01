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
#include "gba_bus.h"
#include "gba_cart.h"

void GBA_cart_init(struct GBA_cart* this)
{
    *this = (struct GBA_cart) {}; // Set all fields to 0

    buf_init(&this->ROM);
}

void GBA_cart_delete(struct GBA_cart *this)
{
    buf_delete(&this->ROM);
}

u32 GBA_cart_read(struct GBA_cart *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    addr &= 0x01FFFFFF;
    addr %= this->ROM.size;
    return cR[sz](this->ROM.ptr, addr);
}

u32 GBA_cart_read_wait0(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(&this->cart, addr, sz, access, has_effect);
}

u32 GBA_cart_read_wait1(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(&this->cart, addr, sz, access, has_effect);
}

u32 GBA_cart_read_wait2(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(&this->cart, addr, sz, access, has_effect);
}

#define idstr_SST 0xD4BF
#define idstr_macronix 0x1CC2
#define idstr_panasonic 0x1b32
#define idstr_atmel 0x3D1F
#define idstr_sanyo128k 0x1362
#define idstr_macronix128k 0x09c2

static u32 read_flash(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->cart.RAM.flash.state == GBAFS_get_id) {
        if (addr == 0x0E000000) {
            switch (this->cart.RAM.flash.kind) {
#define FKL(x) case GBAFK_##x: return idstr_##x & 0xFF
                FKL(SST);
                FKL(macronix);
                FKL(panasonic);
                FKL(atmel);
                FKL(macronix128k);
                FKL(sanyo128k);
#undef FKL
                default:
                    assert(1 == 2);
                    return 0xFFFF;
            }
        }
        if (addr == 0x0E000001) {
            switch (this->cart.RAM.flash.kind) {
#define FKL(x) case GBAFK_##x: return idstr_##x >> 8
                FKL(SST);
                FKL(macronix);
                FKL(panasonic);
                FKL(atmel);
                FKL(macronix128k);
                FKL(sanyo128k);
#undef FKL
                default:
                    assert(1 == 2);
                    return 0xFFFF;
            }
        }
    }
    if (addr < 0x0E010000) return ((u8 *)this->cart.RAM.store->data)[(addr & 0xFFFF) | this->cart.RAM.flash.bank_offset];
    printf("\nRead invalid flash addr %08x?", addr);
    return 0xFFFF;
}

u32 GBA_cart_read_sram(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->cart.RAM.is_flash) return read_flash(this, addr, sz, access, has_effect);
    return ((u8 *)this->cart.RAM.store->data)[addr & this->cart.RAM.mask];
}

void GBA_cart_write(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nWARNING write cart addr %08x", addr);
}

static void erase_flash(struct GBA *this)
{
    memset(this->cart.RAM.store->data, 0xFF, this->cart.RAM.store->actual_size);
}

static void write_flash_cmd(struct GBA *this, u32 cmd)
{
    u32 last_cmd = this->cart.RAM.flash.last_cmd;
    printf("\nFLASH CMD %04x", cmd);
    this->cart.RAM.flash.last_cmd = cmd;
    switch(cmd) {
        case 0x5590: // get ID mode
            this->cart.RAM.flash.state = GBAFS_get_id;
            return;
        case 0x55F0: // exit get ID mode
            if (this->cart.RAM.flash.state == GBAFS_get_id) this->cart.RAM.flash.state = GBAFS_idle;
            return;
        case 0x5580: // Erase command start
            return;
        case 0x5510: // Erase entire chip when previous = 5580
            if (last_cmd == 0x5580) {
                erase_flash(this);
                return;
            }
            break;
        case 0x5530: // all devices except atmel, erase 4kb sector 0E00n000h = FF
            if (this->cart.RAM.flash.kind != GBAFK_atmel) {
                this->cart.RAM.flash.state = GBAFS_erase_4k;
                return;
            }
            break;
        case 0x55A0:
            // atmel-only: erase-and-write
            if (this->cart.RAM.flash.kind == GBAFK_atmel) {
                printf("\nIMPLEMENT ATMEL ERASE-AND-WRITE PELASE");
                return;
            }
            // other than atmel: write
            this->cart.RAM.flash.state = GBAFS_write_byte;
            return;
        case 0x55B0:
            this->cart.RAM.flash.state = GBAFS_await_bank;
            return;
    }
    printf("\nUNRECOGNIZED FLASH CMD %04x", cmd);
}

static u32 flash_bank_mask(enum GBA_flash_kinds kind)
{
    switch(kind) {
        case GBAFK_atmel:
        case GBAFK_macronix:
        case GBAFK_SST:
        case GBAFK_panasonic:
            return 0;
        case GBAFK_macronix128k:
        case GBAFK_sanyo128k:
            return 1;
        default:
            assert(1==2);
    }
    return 0;
}

static void write_flash(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nWRITE FLASH addr:%08x val:%02x", addr, val);
    switch(addr) {
        case 0x0E005555: {// reg 0
            if (val == 0xAA) {
                    this->cart.RAM.flash.cmd_loc = 1;
                    printf("\nSET IS FLASH!");
                    this->cart.RAM.is_flash = 1;
                    return; }
            if ((val == 0xF0) && ((this->cart.RAM.flash.kind == GBAFK_macronix) || (this->cart.RAM.flash.kind == GBAFK_macronix128k))) {
                this->cart.RAM.flash.cmd_loc = 0;
                printf("\nFLASH: FORCE CMD END");
                this->cart.RAM.flash.regs.r2a = 0;
                this->cart.RAM.flash.state = GBAFS_idle;
                return;
            }
            write_flash_cmd(this, (this->cart.RAM.flash.regs.r2a << 8) | val);
            return; }
        case 0x0E002AAA: {
            this->cart.RAM.flash.regs.r2a = val;
            return;
        }
    }
    if ((this->cart.RAM.flash.state == GBAFS_await_bank) && (addr == 0x0E000000)) {
        printf("\nFLASH SET BANK %d", val);
        val &= flash_bank_mask(this->cart.RAM.flash.kind);
        this->cart.RAM.flash.bank_offset = val << 16;
        return;
    }
    if ((this->cart.RAM.flash.state == GBAFS_write_byte) && (addr < 0x0E010000)) {
        printf("\nWRITE BYTE FLASH!");
        ((u8 *) this->cart.RAM.store->data)[(addr & 0xFFFF) | this->cart.RAM.flash.bank_offset] = val;
        this->cart.RAM.flash.state = GBAFS_idle;
        this->cart.RAM.store->dirty = 1;
    }
}


void GBA_cart_write_sram(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    assert(sz==1);

    if (!this->cart.RAM.is_not_flash) write_flash(this, addr, sz, access, val);
    if (this->cart.RAM.is_flash) return;

    this->cart.RAM.is_not_flash = 1;

    ((u8 *)this->cart.RAM.store->data)[addr & this->cart.RAM.mask] = val;
    this->cart.RAM.store->dirty = 1;
}


u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    if (SRAM_enable) *SRAM_enable = 1;
    this->RAM.store = &pio->cartridge_port.SRAM;

    this->RAM.store->requested_size = 128 * 1024;
    this->RAM.store->persistent = 1;
    this->RAM.store->dirty = 1;
    this->RAM.store->ready_to_use = 0;
    this->RAM.size = this->RAM.store->requested_size;
    this->RAM.mask = this->RAM.size - 1;
    this->RAM.persists = this->RAM.present = 1;
    this->RAM.flash.kind = GBAFK_sanyo128k;

    return 1;
}