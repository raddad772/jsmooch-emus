//
// Created by . on 1/17/25.
//
#include <cstring>
#include <cassert>

#include "../gba_bus.h"
#include "flash.h"

namespace GBA::cart {
#define idstr_SST 0xD4BF
#define idstr_macronix 0x1CC2
#define idstr_panasonic 0x1b32
#define idstr_atmel 0x3D1F
#define idstr_sanyo128k 0x1362
#define idstr_macronix128k 0x09c2

void core::erase_flash()
{
    memset(RAM.store->data, 0xFF, RAM.store->actual_size);
    RAM.store->dirty = true;
}

void core::write_flash_cmd(u32 addr, u32 cmd)
{
    RAM.flash.cmd_loc = 0;
    u32 last_cmd = RAM.flash.last_cmd;
    RAM.flash.last_cmd = cmd;
    switch(cmd) {
        case 0x5590: // get ID mode
            RAM.flash.state = FS_get_id;
            return;
        case 0x55F0: // exit get ID mode
            if (RAM.flash.state == FS_get_id) RAM.flash.state = FS_idle;
            return;
        case 0x5580: // Erase command start
            return;
        case 0x5510: // Erase entire chip when previous = 5580
            if (last_cmd == 0x5580) {
                erase_flash();
                return;
            }
            break;
        case 0x5530: // all devices except atmel, erase 4kb sector 0E00n000h = FF
            if (RAM.flash.kind != FK_atmel) {
                RAM.flash.state = FS_idle;
                if ((addr & 0x0FFF) == 0) {
                    u32 base = addr & 0xF000;
                    memset(static_cast<u8 *>(RAM.store->data) + (base | RAM.flash.bank_offset), 0xFF, 0x1000);
                    RAM.store->dirty = 1;
                }
                return;
            }
            break;
        case 0x55A0:
            RAM.flash.cmd_loc = 3;            // atmel-only: erase-and-write
            if (RAM.flash.kind == FK_atmel) {
                printf("\nIMPLEMENT ATMEL ERASE-AND-WRITE PELASE");
                return;
            }
            // other than atmel: write
            RAM.flash.state = FS_write_byte;
            return;
        case 0x55B0:
            if ((RAM.flash.kind == FK_sanyo128k) || (RAM.flash.kind == FK_macronix128k)) {
                RAM.flash.state = FS_await_bank;
                RAM.flash.cmd_loc = 3;
            }
            else {
                RAM.flash.state = FS_idle;
            }
            return;
    }
    printf("\nUNRECOGNIZED FLASH CMD %04x", cmd);
}

static u32 flash_bank_mask(flash_kinds kind)
{
    switch(kind) {
        case FK_atmel:
        case FK_macronix:
        case FK_SST:
        case FK_panasonic:
            return 0;
        case FK_macronix128k:
        case FK_sanyo128k:
            return 1;
        default:
            assert(1==2);
    }
    return 0;
}


u32 core::read_flash(u32 addr, u32 sz, u32 access, bool has_effect)
{
    addr &= 0x00FFFF;
    addr |= 0x0E000000;
    gba->waitstates.current_transaction += gba->waitstates.sram;
    if (RAM.flash.state == FS_get_id) {
        if (addr == 0x0E000000) {
            switch (RAM.flash.kind) {
#define FKL(x) case FK_##x: return idstr_##x & 0xFF
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
            switch (RAM.flash.kind) {
#define FKL(x) case FK_##x: return idstr_##x >> 8
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
    u32 v = static_cast<u8 *>(RAM.store->data)[addr | RAM.flash.bank_offset];
    if (sz == 4) v *= 0x01010101;
    if (sz == 2) v *= 0x0101;
    return v;
}

void core::finish_flash_cmd(u32 addr, u32 sz, u32 val) {
    RAM.flash.cmd_loc = 0;
    if (RAM.flash.state == FS_await_bank) {
        val &= flash_bank_mask(RAM.flash.kind);
        RAM.flash.bank_offset = val << 16;
        return;
    }
    else if (RAM.flash.state == FS_write_byte) {
        addr &= 0xFFFF;
        if ((sz == 2) && (addr & 1)) val >>= 8;
        if (sz == 4) {
            val >>= (addr & 3) << 3;
        }
        val &= 0xFF;
        static_cast<u8 *>(RAM.store->data)[addr | RAM.flash.bank_offset] = val;
        RAM.flash.state = FS_idle;
        RAM.store->dirty = true;
        return;
    }
    else {
        printf("\nWHIFFED IT GOOD!");
    }
}

void core::write_flash(u32 addr, u32 sz, u32 access, u32 val)
{
    gba->waitstates.current_transaction += gba->waitstates.sram;
    addr &= 0x00FFFF;
    addr |= 0x0E000000;
    switch(RAM.flash.cmd_loc) {
        case 0: {
            if ((addr == 0x0E005555) && (val == 0xAA)) {
                RAM.flash.cmd_loc = 1;
            }
            return;
        }
        case 1: {
            if ((addr == 0x0E002AAA) && (val == 0x55)) {
                RAM.flash.cmd_loc = 2;
            }
            return;
        }
        case 2: {
            write_flash_cmd(addr, 0x5500 | val);
            return;
        }
        case 3:
            finish_flash_cmd(addr, sz, val);
            return;
    }
    printf("\nFLASH CMD NOT HANDLED...");
}
}