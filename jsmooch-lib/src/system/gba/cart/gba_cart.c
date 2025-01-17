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

#include "../gba_bus.h"
#include "gba_cart.h"
#include "eeprom.h"

void GBA_cart_init(struct GBA_cart* this)
{
    *this = (struct GBA_cart) {}; // Set all fields to 0

    this->prefetch.enable = 1;
    buf_init(&this->ROM);
}

void GBA_cart_delete(struct GBA_cart *this)
{
    buf_delete(&this->ROM);
}

static const u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

static u32 prefetch_stop(struct GBA *this)
{
    return 0;
    // So we need to cover a few cases here...
    // "If ROM data/SRAM/FLASH is accessed in a cycle, where the prefetch unit
    //  is active and finishing a half-word access, then a one-cycle penalty applies."
    if (this->cart.prefetch.enable) {
        u32 page = this->cpu.regs.R[15] >> 24;
        if ((page >= 8) && (page < 0xE) && (this->cart.prefetch.cycles_banked > 0)) {
            // OK so we have cycles-banked that can be up to 8* what it should compare to
            i32 cb = this->cart.prefetch.cycles_banked % this->cart.prefetch.duty_cycle;
            if (cb == (this->cart.prefetch.duty_cycle - 1)) { // ABOUT to finish
                return 1;
            }
        }
    }
    return 0;
}

u32 GBA_cart_read(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect, u32 ws) {
    if ((this->cart.RAM.is_eeprom) && (addr >= 0x0d000000) && (addr < 0x0e000000))
        return GBA_cart_read_eeprom(this, addr, sz, access, has_effect);
    if (sz == 4) addr &= ~3;
    if (sz == 2) addr &= ~1;

    u32 page = addr >> 24;
    addr &= 0x01FFFFFF;

    if (addr >= this->cart.ROM.size) { // OOB read
        this->waitstates.current_transaction++;
        return (addr >> 1) & masksz[sz];
    }

    /*if (this->cart.prefetch.was_disabled) {
        access &= ~ARM7P_sequential; // Clear sequential flag
        this->cart.prefetch.was_disabled = 0;
    }*/
    u32 sequential = (access & ARM7P_sequential);
    if ((addr & 0x1FFFF) == 0) sequential = 0; // 128KB blocks are non-sequential
    // determine cycles of this access
    if (dbg.trace_on) {
        struct trace_view *tv = this->cpu.dbg.tvptr;
        if (tv) {
            trace_view_startline(tv, 3);
            trace_view_printf(tv, 0, "ifetch");
            trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
            trace_view_printf(tv, 2, "%08x", addr);
            trace_view_printf(tv, 4, "READ GAMEPAK. seq:%d code:%d page:%d", sequential, !!(access & ARM7P_code), page);
            trace_view_endline(tv);
        }
    }
    u32 outcycles = 0;
    if (!this->cart.prefetch.enable) {
        // Just do a normal read
        outcycles = prefetch_stop(this);
        if (sz == 4) outcycles += this->waitstates.timing32[sequential][page];
        else outcycles += this->waitstates.timing16[sequential][page];
        this->waitstates.current_transaction += outcycles;
        return cR[sz](this->cart.ROM.ptr, addr);
    }

    // If we got here, prefetch is enabled.
    i64 tt = (i64)GBA_clock_current(this);
    // This is fine as non-sequential. Sequential is always paid for upfront
    i64 this_cycles = (sz == 4) ? this->waitstates.timing32[1][page] : this->waitstates.timing16[1][page];
    // If we are at the next prefetch addr, and it's code...
    if (this->cart.prefetch.last_access != 0xFFFFFFFFFFFFFFFF)
        this->cart.prefetch.cycles_banked += (tt - (i64)this->cart.prefetch.last_access);
    if (this->cart.prefetch.cycles_banked > (this_cycles * 8)) { // We can only get ahead 8 times
        this->cart.prefetch.cycles_banked = this_cycles * 8;
    }

    if (addr == this->cart.prefetch.next_addr && (access & ARM7P_code)) {
        // Add cycles since last visit


        // Subtract # of cycles of this access
        this->cart.prefetch.cycles_banked -= this_cycles;
        // if we don't have enough...
        if (this->cart.prefetch.cycles_banked < 0) {
            if (dbg.trace_on) {
                struct trace_view *tv = this->cpu.dbg.tvptr;
                if (tv) {
                    trace_view_startline(tv, 3);
                    trace_view_printf(tv, 0, "ifetch");
                    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
                    trace_view_printf(tv, 2, "%08x", addr);
                    trace_view_printf(tv, 4, "partial complete. cycles left: %d", (0 - this->cart.prefetch.cycles_banked));
                    trace_view_endline(tv);
                }
            }
            // Add what we have left to the wait
            outcycles += (0 - this->cart.prefetch.cycles_banked);
            // Reset cycles banked to 0
            this->cart.prefetch.cycles_banked = 0;
        } else { // if we DO have enough...
            outcycles++; // transaction only takes 1 cycle!
            //if (this->cart.prefetch.cycles_banked > (this->cart.prefetch.duty_cycle * 8)) { // We can only get ahead 8 times
            //    this->cart.prefetch.cycles_banked = this->cart.prefetch.duty_cycle * 8;
            //}
        }
    }
    else { // Check for another case: fetch is tried of the currently-in-progress fetch
        // First calculate what the current in-progress fetch is
        u32 current_fetch_addr = this->cart.prefetch.next_addr;
        u32 duty_cycle = this->waitstates.timing16[1][page];
        if (sz == 4) duty_cycle *= 2;

        u32 fetch_cycles = this->cart.prefetch.cycles_banked / duty_cycle;
        current_fetch_addr += (fetch_cycles * sz);
        if (addr == current_fetch_addr && (access & ARM7P_code) && (fetch_cycles > 0)) {
            //printf("\nI HIT THE CASE...");
            u32 cycles_left_to_fetch = this->cart.prefetch.cycles_banked % duty_cycle;
            outcycles += cycles_left_to_fetch;
            this->cart.prefetch.cycles_banked = 0;
        }
        else { // Abort the prefetcher
            outcycles += prefetch_stop(this); // Penalty if we're 1 from end!
            this->cart.prefetch.cycles_banked = 0; // Restart prefetches
            outcycles += (sz == 4) ? this->waitstates.timing32[sequential][page] : this->waitstates.timing16[sequential][page];; // Full cost of the read
        }
    }
    this->cart.prefetch.duty_cycle = this->waitstates.timing16[1][page];
    this->cart.prefetch.next_addr = addr + sz;
    this->cart.prefetch.last_access = tt + outcycles;
    this->waitstates.current_transaction += outcycles;

    return cR[sz](this->cart.ROM.ptr, addr);
}

u32 GBA_cart_read_wait0(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(this, addr, sz, access, has_effect, 0);
}

u32 GBA_cart_read_wait1(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(this, addr, sz, access, has_effect, 1);
}

u32 GBA_cart_read_wait2(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return GBA_cart_read(this, addr, sz, access, has_effect, 2);
}

#define idstr_SST 0xD4BF
#define idstr_macronix 0x1CC2
#define idstr_panasonic 0x1b32
#define idstr_atmel 0x3D1F
#define idstr_sanyo128k 0x1362
#define idstr_macronix128k 0x09c2

static u32 read_flash(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    this->waitstates.current_transaction++;
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
    this->waitstates.current_transaction += this->waitstates.sram;
    return v;
}

static void write_RTC(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    // Ignore byte writes...weirdly?
    if (sz == 1) return;
}

void GBA_cart_write(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (this->cart.RTC.present && (addr >= 0x080000C4) && (addr < 0x080000CA)) {
        return write_RTC(this, addr, sz, access, val);
    }
    if (this->cart.RAM.is_eeprom && (addr >= 0x0d000000) && (addr < 0x0e000000))
        return GBA_cart_write_eeprom(this, addr, sz, access, val);
    this->waitstates.current_transaction++;
    this->waitstates.current_transaction += prefetch_stop(this);
    this->cart.prefetch.cycles_banked = 0;
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

static void detect_RTC(struct GBA_cart *this, struct buf *ROM)
{
    // offset 00000C4 at least 6 bytes filled with 0
    u8 *ptr = ((u8 *)ROM->ptr) + 0xC4;
    u32 detect = 1;
    for (u32 i = 0; i < 6; i++) {
        if (ptr[i] != 0) detect = 0;
    }
    this->RTC.present = detect;
    if (detect) printf("\nRTC DETECTED!");
}

u32 GBA_cart_load_ROM_from_RAM(struct GBA_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable) {
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    if (SRAM_enable) *SRAM_enable = 1;
    this->RAM.store = &pio->cartridge_port.SRAM;
    this->RAM.store->fill_value = 0xFF;
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
            GBA_cart_init_eeprom(this);
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

    detect_RTC(this, &this->ROM);

    return 1;
}