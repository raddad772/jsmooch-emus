//
// Created by . on 6/1/24.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "genesis_cart.h"

static void bswap_16_array(void *where, u32 num_bytes)
{
    u16 *ptr = (u16 *)where;

    // byte-swap the ROM
    for (u32 i = 0; i < (num_bytes >> 1); i++) {
        ptr[i] = bswap_16(ptr[i]);
    }
}

static int ascii_hex_to_int(const char c)
{
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'A') && (c <= 'F')) return 10 + (c - 'A');
    assert(1==0);
    return -1;
}

static int compare_str_term2(char *str1, char *str2)
{
    while(*str2 != 0)
        if (*str1++ != *str2++) return 0;
    return 1;
}

// Memcopy that terminates on 0 or zero-terminates the end (at dest[max_len])
static void memcpy_t0(char *dest, char *src, int max_len) {
    int i;
    for (i = 0; i < max_len; i++) {
        dest[i] = src[i];
        if (src[i] == 0) break;
    }
    dest[i] = 0;
}

void genesis_cart_init(struct genesis_cart* this)
{
    *this = (struct genesis_cart) {}; // Set all fields to 0

    buf_init(&this->ROM);
}

void genesis_cart_delete(struct genesis_cart *this)
{
    buf_delete(&this->ROM);
}

static u32 UDS_mask[4] = { 0, 0xFF, 0xFF00, 0xFFFF };
#define UDSMASK UDS_mask[((UDS) << 1) | (LDS)]


static void write_cart_RAM8(struct genesis_cart* this, u32 addr, u32 val)
{
    if (!this->SRAM) {
        printf("\nAttempt to write bad SRAM!");
        return;
    }
    if (!this->SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    ((u8 *) this->SRAM->data)[(addr >> 1) & this->RAM_mask] = (u8) val;
    //printf("\nWRITE SRAM %04x %02x", (u32)(address >> 1), (u32)data);
    //printf("\nWRITE SRAM ")

    this->SRAM->dirty = 1;
}

static void write_cart_RAM16(struct genesis_cart* this, u32 addr, u32 val)
{
    if (!this->SRAM) {
        printf("\nAttempt to write bad SRAM!");
        return;
    }
    if (!this->SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    u8 *ptr = ((u8 *) this->SRAM->data) + (addr & this->RAM_mask);
    ptr[0] = val >> 8;
    ptr[1] = val & 0xFF;
    this->SRAM->dirty = 1;
}

static u8 read_cart_RAM8(struct genesis_cart* this, u32 addr)
{
    if (!this->SRAM) {
        printf("\nAttempt to read bad SRAM!");
        return 0;
    }
    if (!this->SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    addr >>= 1;
    u32 v = ((u8 *)this->SRAM->data)[addr & this->RAM_mask];
    return v;
}

static u16 read_cart_RAM16(struct genesis_cart* this, u32 addr)
{
    if (!this->SRAM) {
        printf("\nAttempt to read bad SRAM!");
        return 0;
    }
    if (!this->SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    u8 *ptr = &((u8 *)this->SRAM->data)[addr & this->RAM_mask];
    return ((ptr[0] << 8) | ptr[1]);;

}

void genesis_cart_write(struct genesis_cart *this, u32 addr, u32 mask, u32 val, u32 SRAM_enable) {
    u32 ramaddr = addr - 0x200000;
    val &= mask;

    /* it looks like carts don't actually enable SRAM to enable writes. it's always writable */
    if (this->RAM_present && (addr >= this->header.extra_ram_addr_start) && (addr < this->header.extra_ram_addr_end)) {
        switch (this->header.extra_ram_kind) {
            case sega_cart_exram_none:
                break;
            case sega_cart_exram_8bit_even:
            case sega_cart_exram_8bit_odd:
                if (this->header.extra_ram_kind == sega_cart_exram_8bit_odd)
                    val &= 0xFF;
                else
                    val >>= 8;
                write_cart_RAM8(this, ramaddr, val);
                break;
            case sega_cart_exram_16bit: {
                write_cart_RAM16(this, ramaddr, val);
                break;
            }
            default:
                assert(1 == 2);
                break;
        }
    }
}

// Carts are read in 16 bits at a time
u16 genesis_cart_read(struct genesis_cart *this, u32 addr, u32 mask, u32 has_effect, u32 SRAM_enable)
{
    u8* ptr = &((u8 *)this->ROM.ptr)[addr % this->ROM.size];
    u16 v = 0;

    if (addr < 0x200000) {
        v = ((ptr[0] << 8) | ptr[1]);
    }
    else {
        if (this->RAM_present && (SRAM_enable || this->RAM_always_on)) {
            u32 ramaddr = addr - 0x200000;
            switch(this->header.extra_ram_kind) {
                case sega_cart_exram_8bit_even:
                case sega_cart_exram_8bit_odd:
                    v = read_cart_RAM8(this, ramaddr);
                    v |= (v << 8);
                    break;
                case sega_cart_exram_16bit:
                    ramaddr &= this->RAM_mask;
                    v = read_cart_RAM16(this, ramaddr);
                    break;
                default:
                    assert(1==2);
                    break;
            }
        }
        else {
            v = ((ptr[0] << 8) | ptr[1]);
        }
    }
    return v & mask;
}

static u32 get_closest_pow2(u32 b)
{
    //u32 b = MAX(w, h);
    u32 out = 128;
    while(out < b) {
        out <<= 1;
        assert(out < 0x40000000);
    }
    return out;
}

static char *eval_cart_RAM(struct genesis_cart* this, char *tptr)
{
    u32 silly_goose = 0;
    // If Sonic & Knuckles 3, use Sonic 3's header for RAM
    if ((strncmp("GM MK-1563 -00", this->header.serial_number, 14) == 0) && (this->ROM.size == 4194304)) {
        silly_goose = 1;
        tptr += 0x200000;
    }
    if ((tptr[0] == 'R') && (tptr[1] == 'A')) {
        u8 r = (u8)tptr[2];
        this->RAM_persists = 0;
        if (r & 0x40) {
            this->RAM_persists = 1;
            r -= 0x40;
        }
        switch(r) {
            case 0xA0:
                this->header.extra_ram_kind = sega_cart_exram_16bit;
                break;
            case 0xB0:
                this->header.extra_ram_kind = sega_cart_exram_8bit_even;
                break;
            case 0xB8:
                this->header.extra_ram_kind = sega_cart_exram_8bit_odd;
                break;
            default:
                printf("\nUnknown extra RAM type %02x", (u32)r);
                break;
        }
        r = tptr[3];
        switch(r) {
            case 0x20:
                this->header.extra_ram_is_eeprom = 0;
                break;
            case 0x40:
                this->header.extra_ram_is_eeprom = 1;
                break;
            default:
                printf("\nUnknown extra RAM extra %02x", (u32)r);
                break;
        }
        tptr += 4;
        this->header.extra_ram_addr_start = bswap_32(*(u32 *)tptr);
        tptr += 4;
        this->header.extra_ram_addr_end = bswap_32(*(u32 *)tptr);
        tptr += 4;
        u32 ram_size = (this->header.extra_ram_addr_end - this->header.extra_ram_addr_start) + 1;
        ram_size = get_closest_pow2(ram_size);
        printf("\nPRE RAM SIZE: %d", ram_size);
        switch(this->header.extra_ram_kind) {
            case sega_cart_exram_8bit_odd:
            case sega_cart_exram_8bit_even:
                ram_size >>= 1;
                break;
            default:
                break;
        }
        printf("\nPOST RAM SIZE: %d", ram_size);
        printf("\nRAM addr start:%06x end:%06x", this->header.extra_ram_addr_start, this->header.extra_ram_addr_end);
        printf("\nFound %d bytes of extra RAM!", ram_size);
        assert(ram_size<(1 * 1024 * 1024));

        this->RAM_present = 1;
        this->RAM_mask = ram_size-1;
        this->RAM_size = ram_size;
    } else {
        this->RAM_present = 0;
        this->RAM_persists = 0;
        this->header.extra_ram_kind = sega_cart_exram_none;
        this->RAM_present = 0;
        this->RAM_size = 0;
        tptr += 12;
    }
    if (silly_goose) tptr -= 0x200000;
    return tptr;
}

u32 genesis_cart_load_ROM_from_RAM(struct genesis_cart* this, char* fil, u64 fil_sz, struct physical_io_device *pio, u32 *SRAM_enable)
{
    buf_allocate(&this->ROM, fil_sz);
    memcpy(this->ROM.ptr, fil, fil_sz);
    *SRAM_enable = 0;

    char *tptr = (char *)this->ROM.ptr;

    tptr += 0x100;

    this->kind = sega_cart_invalid;
#define CK(x, y) if (compare_str_term2(tptr, x)) this->kind = y
        CK("SEGA MEGA DRIVE", sega_cart_genesis);
        CK("SEGA GENESIS", sega_cart_genesis);
        CK("SEGA 32X", sega_cart_32x);
        CK("SEGA EVERDRIVE", sega_cart_everdrive);
        CK("SEGA SSF", sega_cart_ssf);
        CK("SEGA MEGAWIFI", sega_cart_megawifi);
        CK("SEGA PICO", sega_cart_pico);
        CK("SEGA TERA68K", sega_cart_tera68k);
        CK("SEGA TERA286", sega_cart_tera286);
#undef CK
    assert(this->kind == sega_cart_genesis);
    if (this->kind == sega_cart_invalid) {
        printf("\nError loading Sega Genesis cart");
        return 0;
    }

    tptr += 16;

    memcpy(&this->header.copyright, tptr, 16);
    this->header.copyright[16] = 0;
    tptr += 16;

    memcpy_t0(this->header.title_domestic, tptr, 48);
    tptr += 48;

    memcpy_t0(this->header.title_overseas, tptr, 48);
    tptr += 48;

    memcpy_t0(this->header.serial_number, tptr, 14);
    tptr += 14;

    this->header.rom_checksum = bswap_16(*(u16 *)tptr);
    tptr += 2;

#define CK(w, k) case w: this->header.devices_supported[i] = sega_cart_device_##k; break
    char *cptr = tptr;
    for (u32 i = 0; i < 16; i++) {
        switch(cptr[i]) {
            CK('J', cnt_3button);
            CK('6', cnt_6button);
            CK('0', cnt_master_system);
            CK('A', analog_joystick);
            CK('4', multitap);
            CK('G', lightgun);
            CK('L', activator);
            CK('M', mouse);
            CK('B', trackball);
            CK('T', tablet);
            CK('V', paddle);
            CK('K', keyboard_or_keypad);
            CK('R', rs_232);
            CK('P', printer);
            CK('C', segacd);
            CK('F', floppy);
            CK('D', download);
            default:
                printf("\nUnknown device %c", cptr[i]);
                break;
            case ' ':
                this->header.devices_supported[i] = sega_cart_device_none;
                break;
        }
    }
#undef CK
    tptr += 16;

    this->header.rom_addr_start = bswap_32(*(u32 *)tptr);
    tptr += 4;
    this->header.rom_addr_end = bswap_32(*(u32 *)tptr);
    tptr += 4;

    this->header.ram_addr_start = bswap_32(*(u32 *)tptr);
    tptr += 4;
    this->header.ram_addr_end = bswap_32(*(u32 *)tptr);
    tptr += 4;
    //assert(this->header.ram_addr_start == 0xFF0000);
    //assert(this->header.ram_addr_end == 0xFFFFFF);
    tptr = eval_cart_RAM(this, tptr);
    pio->cartridge_port.SRAM.requested_size = this->RAM_size;
    pio->cartridge_port.SRAM.ready_to_use = 0;
    pio->cartridge_port.SRAM.dirty = this->RAM_size > 0;
    this->SRAM = &pio->cartridge_port.SRAM;
    if ((this->SRAM->requested_size > 0) && (this->ROM.size <= 0x200000)) *SRAM_enable = 1;
    tptr += 52; // skip modem, padding
    u32 num_regions = 0;
    for (u32 i = 0; i < 3; i++) {
        switch(tptr[i]) {
            case 'J':
                this->header.region_japan = 1;
                num_regions++;
                break;
            case 'U':
                num_regions++;
                this->header.region_usa = 1;
                break;
            case 'E':
                num_regions++;
                this->header.region_europe = 1;
                break;
            case ' ':
                break;
            default:
                if (num_regions == 0) {
                    int r = ascii_hex_to_int(tptr[i]);
                    if (r != -1) {
                        if (r & 1) {
                            this->header.region_japan = 1;
                            num_regions++;
                        };
                        if (r & 2) { printf("\nInvalid region bit!"); };
                        if (r & 4) {
                            this->header.region_usa = 1;
                            num_regions++;
                        };
                        if (r & 8) {
                            this->header.region_europe = 1;
                            num_regions++;
                        };
                    }
                    if (num_regions == 0) {
                        printf("\nInvalid new-style region character!");
                    }
                }
                else {
                    printf("\nUnknown region %c", tptr[i]);
                }
                break;
        }
    }
    printf("\nREGION JP:%d US:%d", this->header.region_japan, this->header.region_usa);
    printf("\nCOPYRIGHT: %s", this->header.copyright);
    printf("\nTITLE: %s", this->header.title_domestic);
    printf("\nROM END: %08x", this->header.rom_addr_end);

    //bswap_16_array(&this->ROM.ptr, this->ROM.size);
    return 1;
}