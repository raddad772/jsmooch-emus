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

static void start_eeprom_read(struct GBA *this) {
    this->cart.RAM.eeprom.mode = GEM_read_request;
    memset(this->cart.RAM.eeprom.cmd.data, 0, sizeof(this->cart.RAM.eeprom.cmd.data));
    this->cart.RAM.eeprom.cmd.pos = 4;
    this->cart.RAM.eeprom.cmd.len = 68;
    // It's going to want MSB as its first read
    // So we need to read MSB->LSBaddr-- in memory, and write LSB->MSB in output bufferpos++

    u8 *read_ptr = (u8 *)this->cart.RAM.store->data + this->cart.RAM.eeprom.cmd.addr;
    u8 *write_ptr = this->cart.RAM.eeprom.cmd.data;

    for (u32 i = 0; i < 64; i++) {
        u32 bn = 63 - i;
        u32 read_byte_num = bn >> 3;
        u32 read_shift = bn & 7;
        u32 bit = (read_ptr[read_byte_num] >> read_shift) & 1;

        u32 write_byte_num = this->cart.RAM.eeprom.cmd.pos >> 3;
        u32 write_shift = this->cart.RAM.eeprom.cmd.pos & 7;
        this->cart.RAM.eeprom.cmd.pos++;
        write_ptr[write_byte_num] |= bit << write_shift;
        }
    this->cart.RAM.eeprom.cmd.pos = 0; // Prepare for DMA to read it all...
}

static void finish_eeprom_write(struct GBA *this) {
    this->cart.RAM.eeprom.mode = GEM_none;
    u8 *read_ptr = this->cart.RAM.eeprom.cmd.data;
    u8 *write_ptr = ((u8 *) this->cart.RAM.store->data) + this->cart.RAM.eeprom.cmd.addr;
    this->cart.RAM.eeprom.cmd.pos -= 2; // truncate extra and align us to correct start position
    // LSB of input is at end
    // So if we write from low...high, we should go from high...lo on the input
    // So we want to go from LSB->MSB in output, since we will get LSB first going backward
    memset(write_ptr, 0, 8);
    for (u32 i = 0; i < 64; i++) {
        u32 read_byte_num = this->cart.RAM.eeprom.cmd.pos >> 3;
        u32 read_shift = this->cart.RAM.eeprom.cmd.pos & 7;
        assert(this->cart.RAM.eeprom.cmd.pos > 0);
        this->cart.RAM.eeprom.cmd.pos--;
        u32 bit = (read_ptr[read_byte_num] >> read_shift) & 1;

        u32 write_shift = i & 7;
        u32 write_byte_num = i >> 3;
        write_ptr[write_byte_num] |= bit << write_shift;
    }
    this->cart.RAM.store->dirty = 1;
}

static void finish_eeprom_cmd(struct GBA *this)
{
    // Interpret the buffer
    u32 cmd_kind = ((this->cart.RAM.eeprom.cmd.data[0] & 2) >> 1) | ((this->cart.RAM.eeprom.cmd.data[0] & 1) << 1);
    // extract address
    u32 cmd_addr = 0;
    for (u32 i = 2; i < (this->cart.RAM.eeprom.addr_bus_size + 2); i++) {
        u32 byte_num = i >> 3;
        u32 shift = i & 7;
        u32 shift_in = (i - 2) & 7;
        cmd_addr |= ((this->cart.RAM.eeprom.cmd.data[byte_num] >> shift) & 1) << shift_in;
    }
    cmd_addr *= 8; // 8 bytes per read/write
    this->cart.RAM.eeprom.cmd.addr = cmd_addr;
    // 11 read request
    // 10 write request
    if (cmd_kind == 3) { // Read request
        start_eeprom_read(this);
        return;
    }
    else if (cmd_kind == 2) {
        finish_eeprom_write(this);
        return;
    }
    else printf("\nUNHANDLED CMD:%d", cmd_kind);
}


static void write_eeprom(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    // First, a read must be done!
    if (this->cart.RAM.eeprom.mode == GEM_none) {
        this->cart.RAM.eeprom.mode = GEM_recv;
        memset(this->cart.RAM.eeprom.cmd.data, 0, sizeof(this->cart.RAM.eeprom.cmd.data));
        this->cart.RAM.eeprom.cmd.pos = 0;
        this->cart.RAM.eeprom.cmd.len = this->dma[3].io.word_count;
        if (!this->cart.RAM.eeprom.size_detected) {
            this->cart.RAM.eeprom.size_detected = 1;
            this->cart.RAM.eeprom.addr_bus_size = this->cart.RAM.eeprom.cmd.len - 3;
            this->cart.RAM.eeprom.size = (1 << this->cart.RAM.eeprom.addr_bus_size) * 8;
            this->cart.RAM.eeprom.mask = this->cart.RAM.eeprom.size - 1;
            printf("\nDetect EEPROM size: %d bytes", this->cart.RAM.eeprom.size);
        }
    }
    if (this->cart.RAM.eeprom.mode == GEM_recv) {
        u32 byte_num = this->cart.RAM.eeprom.cmd.pos >> 3;
        u32 shift = this->cart.RAM.eeprom.cmd.pos & 7;
        //this->cart.RAM.eeprom.cmd.data[byte_num] &= 0xFF ^ (1 << shift);
        this->cart.RAM.eeprom.cmd.data[byte_num] |= (val & 1) << shift;
        this->cart.RAM.eeprom.cmd.pos++;
        if (this->cart.RAM.eeprom.cmd.pos >= this->cart.RAM.eeprom.cmd.len) {
            finish_eeprom_cmd(this);
        }
        return;
    }
    printf("\nUNHANDLED EPR!");
}

static u32 read_eeprom(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    //printf("\nREAD EPR addr:%08x", addr);
    if (this->cart.RAM.eeprom.mode == GEM_read_request) {
        u32 byte_num = this->cart.RAM.eeprom.cmd.pos >> 3;
        u32 shift = this->cart.RAM.eeprom.cmd.pos & 7;
        this->cart.RAM.eeprom.cmd.pos++;
        if (this->cart.RAM.eeprom.cmd.pos >= this->cart.RAM.eeprom.cmd.len) {
            this->cart.RAM.eeprom.mode = GEM_none;
        }
        return (this->cart.RAM.eeprom.cmd.data[byte_num] >> shift) & 1;
    }
    return 1;
}

static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

u32 GBA_cart_read(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    addr &= 0x01FFFFFF;
    if ((this->cart.RAM.is_eeprom) && (addr >= 0x0d000000) && (addr < 0x0e000000)) return read_eeprom(this, addr, sz, access, has_effect);

    if (sz == 4){
        addr &= ~3;
        u32 v = GBA_cart_read(this, addr, 2, access, has_effect);
        v |= GBA_cart_read(this, addr+2, 2, access, has_effect) << 16;
        return v;
    }
    if (sz == 2) addr &= ~1;

    if (addr >= this->cart.ROM.size) {
        return (addr >> 1) & masksz[sz];
    }
    return cR[sz](this->cart.ROM.ptr, addr);
}

u32 GBA_cart_read_wait0(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(this, addr, sz, access, has_effect);
}

u32 GBA_cart_read_wait1(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(this, addr, sz, access, has_effect);
}

u32 GBA_cart_read_wait2(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(this, addr, sz, access, has_effect);
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

    /*if (addr >= 0x0E010000) {
        return GBA_open_bus(this, addr, sz);
    }*/
    u32 v = ((u8 *)this->cart.RAM.store->data)[addr & this->cart.RAM.mask];
    if (sz == 2) {
        v *= 0x101;
    }
    if (sz == 4) {
        v *= 0x1010101;
    }
    return v;
}

void GBA_cart_write(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if ((addr >= 0x0d000000) && (addr < 0x0e000000))
        return write_eeprom(this, addr, sz, access, val);
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
    if (this->cart.RAM.is_flash) return write_flash(this, addr, sz, access, val);

    //printf("\nWRITE SRAM! %08x", addr);
    if (sz == 2) {
        if (addr & 1) val >>= 8;
    }
    if (sz == 4) {
        if ((addr & 3) == 1) val >>= 8;
        else if ((addr & 3) == 2) val >>= 16;
        else if ((addr & 3) == 3) val >>= 24;
    }
    val &= 0xFF;
    ((u8 *)this->cart.RAM.store->data)[addr & this->cart.RAM.mask] = val;
    this->cart.RAM.store->dirty = 1;
}

enum save_kinds {
        SK_none,
        SK_EEPROM_Vnnn,
        SK_SRAM_Vnnn,
        SK_FLASH_Vnnn,
        SK_FLASH512_Vnnn,
        SK_FLASH1M_Vnnn
};

static u32 firstmatch[3] =  {
        0x52504545, // RPEE or EEPR backward
        0x4D415253, // MARS of SRAM backward
        0x53414C46 // SALF or FLAS
};

static int cmpstr(struct buf *f, u32 addr, const char *cmp)
{
    char *ptr1 = ((char *)f->ptr) + addr;
    const char *ptr2 = cmp;
    while((*ptr2)!=0) {
        if (*ptr2 != *ptr1) {
            return 0;
        }
        ptr2++;
        ptr1++;
    }
    return 1;
}


static enum save_kinds search_strings(struct buf *f)
{
    /* First, find a first blush. */
    i64 found_addr = -1;
    u32 *buf = (u32 *)f->ptr;
    for (u32 i = 0; i < f->size; i += 4) {
        u32 m = *buf;
        buf++;
        u32 found = (m == 0x52504545) || (m == 0x4D415253) || (m == 0x53414C46);
        if (found) {
            // Do full string match
            if (cmpstr(f, i, "EEPROM_V")) return SK_EEPROM_Vnnn;
            else if (cmpstr(f, i, "SRAM_V")) return SK_SRAM_Vnnn;
            else if (cmpstr(f, i, "FLASH_V")) return SK_FLASH_Vnnn;
            else if (cmpstr(f, i, "FLASH512_V")) return SK_FLASH512_Vnnn;
            else if (cmpstr(f, i, "FLASH1M_V")) return SK_FLASH1M_Vnnn;
        }
    }

    return SK_none;
}


u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    if (SRAM_enable) *SRAM_enable = 1;
    this->RAM.store = &pio->cartridge_port.SRAM;
    struct persistent_store *ps = &pio->cartridge_port.SRAM;
    this->RAM.is_sram = 0;
    this->RAM.is_flash = 0;
    this->RAM.is_eeprom = 0;

    enum save_kinds save_kind = search_strings(&this->ROM);
    switch(save_kind) {
        case SK_none:
            printf("\nNO SAVE STRING FOUND!");
            this->RAM.store->requested_size = 128 * 1024;
            this->RAM.store->persistent = 0;
            break;
        case SK_SRAM_Vnnn:
            printf("\nSRAM found!");
            this->RAM.store->requested_size = 32 * 1024;
            this->RAM.store->persistent = 1;
            this->RAM.is_sram = 1;
            break;
        case SK_FLASH_Vnnn:
            this->RAM.flash.kind = GBAFK_panasonic;
            printf("\nFLASH (old) 64k found!");
            this->RAM.is_flash = 1;
            this->RAM.store->requested_size = 64 * 1024;
            this->RAM.store->persistent = 1;
            break;
        case SK_FLASH512_Vnnn:
            this->RAM.flash.kind = GBAFK_panasonic;
            printf("\nFLASH (new) 64k found!");
            this->RAM.is_flash = 1;
            this->RAM.store->requested_size = 64 * 1024;
            this->RAM.store->persistent = 1;
            break;
        case SK_FLASH1M_Vnnn:
            this->RAM.flash.kind = GBAFK_sanyo128k;
            printf("\nFLASH 128K found! Using Sanyo!");
            this->RAM.is_flash = 1;
            this->RAM.store->requested_size = 128 * 1024;
            this->RAM.store->persistent = 1;
            break;
        case SK_EEPROM_Vnnn:
            // 512 bytes (SMA) to 8K (Boktai)
            printf("\nEEPROM detect!");
            this->RAM.is_eeprom = 1;
            this->RAM.store->persistent = 1;
            this->RAM.store->requested_size = 8 * 1024;
            break;
    }
    this->RAM.store->dirty = 1;
    this->RAM.store->ready_to_use = 0;

/*
  EEPROM_Vnnn    EEPROM 512 bytes or 8 Kbytes (4Kbit or 64Kbit)
  SRAM_Vnnn      SRAM 32 Kbytes (256Kbit)
  FLASH_Vnnn     FLASH 64 Kbytes (512Kbit) (ID used in older files)
  FLASH512_Vnnn  FLASH 64 Kbytes (512Kbit) (ID used in newer files)
  FLASH1M_Vnnn   FLASH 128 Kbytes (1Mbit) */



    this->RAM.size = this->RAM.store->requested_size;
    this->RAM.mask = this->RAM.size - 1;
    this->RAM.persists = this->RAM.present = 1;

    return 1;
}