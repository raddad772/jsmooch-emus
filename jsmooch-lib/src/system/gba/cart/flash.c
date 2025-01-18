//
// Created by . on 1/17/25.
//
#include <string.h>

#include "../gba_bus.h"
#include "flash.h"

#define idstr_SST 0xD4BF
#define idstr_macronix 0x1CC2
#define idstr_panasonic 0x1b32
#define idstr_atmel 0x3D1F
#define idstr_sanyo128k 0x1362
#define idstr_macronix128k 0x09c2

static void erase_flash(struct GBA *this)
{
    memset(this->cart.RAM.store->data, 0xFF, this->cart.RAM.store->actual_size);
    this->cart.RAM.store->dirty = 1;
}

static void write_flash_cmd(struct GBA *this, u32 addr, u32 cmd)
{
    this->cart.RAM.flash.cmd_loc = 0;
    u32 last_cmd = this->cart.RAM.flash.last_cmd;
    this->cart.RAM.flash.last_cmd = cmd;
    switch(cmd) {
        case 0x5590: // get ID mode
            printf("\nGET ID MODE");
            this->cart.RAM.flash.state = GBAFS_get_id;
            return;
        case 0x55F0: // exit get ID mode
            printf("\nEXIT GET ID MODE");
            if (this->cart.RAM.flash.state == GBAFS_get_id) this->cart.RAM.flash.state = GBAFS_idle;
            return;
        case 0x5580: // Erase command start
            printf("\nERASE CMD START!");
            return;
        case 0x5510: // Erase entire chip when previous = 5580
            if (last_cmd == 0x5580) {
                printf("\nERASE DO!");
                erase_flash(this);
                return;
            }
            printf("\nERASE DONT DO!");
            break;
        case 0x5530: // all devices except atmel, erase 4kb sector 0E00n000h = FF
            if (this->cart.RAM.flash.kind != GBAFK_atmel) {
                this->cart.RAM.flash.state = GBAFS_idle;
                if ((addr & 0x0FFF) == 0) {
                    u32 base = addr & 0xF000;
                    printf("\nERASE 4KB SECTOR %04x", addr);
                    memset(((u8 *)this->cart.RAM.store->data) + (base | this->cart.RAM.flash.bank_offset), 0xFF, 0x1000);
                    this->cart.RAM.store->dirty = 1;
                }
                return;
            }
            break;
        case 0x55A0:
            this->cart.RAM.flash.cmd_loc = 3;            // atmel-only: erase-and-write
            if (this->cart.RAM.flash.kind == GBAFK_atmel) {
                printf("\nERASE AND WRITE");
                printf("\nIMPLEMENT ATMEL ERASE-AND-WRITE PELASE");
                return;
            }
            // other than atmel: write
            printf("\nWRITE BYTE");
            this->cart.RAM.flash.state = GBAFS_write_byte;
            return;
        case 0x55B0:
            if ((this->cart.RAM.flash.kind == GBAFK_sanyo128k) || (this->cart.RAM.flash.kind == GBAFK_macronix128k)) {
                printf("\nAWAIT CHANGE BANK");
                this->cart.RAM.flash.state = GBAFS_await_bank;
                this->cart.RAM.flash.cmd_loc = 3;
            }
            else {
                this->cart.RAM.flash.state = GBAFS_idle;
            }
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


u32 GBA_cart_read_flash(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    addr &= 0x00FFFF;
    addr |= 0x0E000000;
    this->waitstates.current_transaction += this->waitstates.sram;
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
    if (sz == 4) addr &= 0xFFFC;
    if (sz == 2) addr &= 0xFFFE;
    if (sz == 1) addr &= 0xFFFF;
    u32 v = ((u8 *)this->cart.RAM.store->data)[addr | this->cart.RAM.flash.bank_offset];
    if (sz == 4) v *= 0x01010101;
    if (sz == 2) v *= 0x0101;
    printf("\nREAD FLASH %04x: %02x", addr, v);
    return v;
    /*printf("\nRead invalid flash addr %08x?", addr);
    return 0xFFFF;*/
}

static void finish_flash_cmd(struct GBA *this, u32 addr, u32 sz, u32 val) {
    this->cart.RAM.flash.cmd_loc = 0;
    if (this->cart.RAM.flash.state == GBAFS_await_bank) {
        printf("\nFLASH SET BANK %d", val);
        val &= flash_bank_mask(this->cart.RAM.flash.kind);
        this->cart.RAM.flash.bank_offset = val << 16;
        return;
    }
    else if (this->cart.RAM.flash.state == GBAFS_write_byte) {
        addr &= 0xFFFF;
        if ((sz == 2) && (addr & 1)) val >>= 8;
        if (sz == 4) {
            val >>= (addr & 3) << 3;
        }
        val &= 0xFF;
        printf("\nWRITE BYTE FLASH %04x: %02x", addr, val & 0xFF);
        ((u8 *) this->cart.RAM.store->data)[addr | this->cart.RAM.flash.bank_offset] = val;
        this->cart.RAM.flash.state = GBAFS_idle;
        this->cart.RAM.store->dirty = 1;
        return;
    }
    else {
        printf("\nWHIFFED IT GOOD!");
    }
}

void GBA_cart_write_flash(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    this->waitstates.current_transaction += this->waitstates.sram;
    addr &= 0x00FFFF;
    addr |= 0x0E000000;
    printf("\nWRITE FLASH addr:%08x val:%02x", addr, val);
    switch(this->cart.RAM.flash.cmd_loc) {
        case 0: {
            if ((addr == 0x0E005555) && (val == 0xAA)) {
                this->cart.RAM.flash.cmd_loc = 1;
            }
            return;
        }
        case 1: {
            if ((addr == 0x0E002AAA) && (val == 0x55)) {
                this->cart.RAM.flash.cmd_loc = 2;
            }
            return;
        }
        case 2: {
            printf("\nWRITE FLASH CMD ADDR:%08x", addr);
            write_flash_cmd(this, addr, 0x5500 | val);
            return;
        }
        case 3:
            finish_flash_cmd(this, addr, sz, val);
            return;
    }
    printf("\nNOT HANDLED...");
}
