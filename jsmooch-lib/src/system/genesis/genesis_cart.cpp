//
// Created by . on 6/1/24.
//

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "genesis_cart.h"

namespace genesis {
static void bswap_16_array(void *where, u32 num_bytes)
{
    auto *ptr = static_cast<u16 *>(where);

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

static int compare_str_term2(const char *str1, const char *str2)
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

cart::cart()
{
    for (u32 i = 0; i < 8; i++) {
        bank_offset[i] = i << 19;
    }
}

static constexpr u32 UDS_mask[4] = { 0, 0xFF, 0xFF00, 0xFFFF };
#define UDSMASK UDS_mask[((UDS) << 1) | (LDS)]

void cart::write_RAM8(u32 addr, u32 val)
{
    if (!SRAM) {
        printf("\nAttempt to write bad SRAM!");
        return;
    }
    if (!SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    static_cast<u8 *>(SRAM->data)[(addr >> 1) & RAM_mask] = static_cast<u8>(val);
    //printf("\nWRITE SRAM %04x %02x", (u32)(address >> 1), (u32)data);
    //printf("\nWRITE SRAM ")

    SRAM->dirty = true;
}

void cart::write_RAM16(u32 addr, u32 val)
{
    if (!SRAM) {
        printf("\nAttempt to write bad SRAM!");
        return;
    }
    if (!SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    u8 *ptr = static_cast<u8 *>(SRAM->data) + (addr & RAM_mask);
    ptr[0] = val >> 8;
    ptr[1] = val & 0xFF;
    SRAM->dirty = true;
}

u8 cart::read_RAM8(u32 addr)
{
    if (!SRAM) {
        printf("\nAttempt to read bad SRAM!");
        return 0;
    }
    if (!SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    addr >>= 1;
    u32 v = static_cast<u8 *>(SRAM->data)[addr & RAM_mask];
    return v;
}

u16 cart::read_RAM16(u32 addr)
{
    if (!SRAM) {
        printf("\nAttempt to read bad SRAM!");
        return 0;
    }
    if (!SRAM->ready_to_use) {
        printf("\nSRAM not ready to use!");
        abort();
    }
    u8 *ptr = &static_cast<u8 *>(SRAM->data)[addr & RAM_mask];
    return (ptr[0] << 8) | ptr[1];

}

void cart::write(u32 addr, u32 mask, u32 val, bool SRAM_enable) {
    u32 ramaddr = addr - 0x200000;
    val &= mask;

    /* it looks like carts don't actually enable SRAM to enable writes. it's always writable */
    if (RAM_present && (addr >= header.extra_ram_addr_start) && (addr < header.extra_ram_addr_end)) {
        switch (header.extra_ram_kind) {
            case sce_none:
                break;
            case sce_8bit_even:
            case sce_8bit_odd:
                if (header.extra_ram_kind == sce_8bit_odd)
                    val &= 0xFF;
                else
                    val >>= 8;
                write_RAM8(ramaddr, val);
                break;
            case sce_16bit: {
                write_RAM16(ramaddr, val);
                break;
            }
            default:
                assert(1 == 2);
                break;
        }
    }
}

// Carts are read in 16 bits at a time
u16 cart::read(u32 addr, u32 mask, bool has_effect, bool SRAM_enable)
{
    const u32 saddr = addr & 0x7FFFF;
    const u32 offs = bank_offset[(addr >> 19) & 7];
    const u32 rom_addr = (saddr + offs) % ROM.size;
    const u8* ptr = &static_cast<u8 *>(ROM.ptr)[rom_addr];
    u32 v;

    if (addr < 0x200000) {
        v = ((ptr[0] << 8) | ptr[1]);
    }
    else {
        if (RAM_present && (SRAM_enable || RAM_always_on)) {
            u32 ramaddr = addr - 0x200000;
            switch(header.extra_ram_kind) {
                case sce_8bit_even:
                case sce_8bit_odd:
                    v = read_RAM8(ramaddr);
                    v |= (v << 8);
                    break;
                case sce_16bit:
                    ramaddr &= RAM_mask;
                    v = read_RAM16(ramaddr);
                    break;
                default:
                    assert(1==2);
                    break;
            }
        }
        else {
            v = (ptr[0] << 8) | ptr[1];
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

char *cart::eval_cart_RAM(char *tptr)
{
    u32 silly_goose = 0;
    // If Sonic & Knuckles 3, use Sonic 3's header for RAM
    if ((strncmp("GM MK-1563 -00", header.serial_number, 14) == 0) && (ROM.size == 4194304)) {
        silly_goose = 1;
        tptr += 0x200000;
    }
    if ((tptr[0] == 'R') && (tptr[1] == 'A')) {
        u8 r = static_cast<u8>(tptr[2]);
        RAM_persists = 0;
        if (r & 0x40) {
            RAM_persists = 1;
            r -= 0x40;
        }
        switch(r) {
            case 0xA0:
                header.extra_ram_kind = sce_16bit;
                break;
            case 0xB0:
                header.extra_ram_kind = sce_8bit_even;
                break;
            case 0xB8:
                header.extra_ram_kind = sce_8bit_odd;
                break;
            default:
                printf("\nUnknown extra RAM type %02x", static_cast<u32>(r));
                break;
        }
        r = tptr[3];
        switch(r) {
            case 0x20:
                header.extra_ram_is_eeprom = 0;
                break;
            case 0x40:
                header.extra_ram_is_eeprom = 1;
                break;
            default:
                printf("\nUnknown extra RAM extra %02x", static_cast<u32>(r));
                break;
        }
        tptr += 4;
        header.extra_ram_addr_start = bswap_32(*reinterpret_cast<u32 *>(tptr));
        tptr += 4;
        header.extra_ram_addr_end = bswap_32(*reinterpret_cast<u32 *>(tptr));
        tptr += 4;
        u32 ram_size = (header.extra_ram_addr_end - header.extra_ram_addr_start) + 1;
        ram_size = get_closest_pow2(ram_size);
        printf("\nPRE RAM SIZE: %d", ram_size);
        switch(header.extra_ram_kind) {
            case sce_8bit_odd:
            case sce_8bit_even:
                ram_size >>= 1;
                break;
            default:
                break;
        }
        printf("\nPOST RAM SIZE: %d", ram_size);
        printf("\nRAM addr start:%06x end:%06x", header.extra_ram_addr_start, header.extra_ram_addr_end);
        printf("\nFound %d bytes of extra RAM!", ram_size);
        assert(ram_size<(1 * 1024 * 1024));

        RAM_present = 1;
        RAM_mask = ram_size-1;
        RAM_size = ram_size;
    } else {
        RAM_present = 0;
        RAM_persists = 0;
        header.extra_ram_kind = sce_none;
        RAM_present = 0;
        RAM_size = 0;
        tptr += 12;
    }
    if (silly_goose) tptr -= 0x200000;
    return tptr;
}

bool cart::load_ROM_from_RAM(char* fil, size_t fil_sz, physical_io_device *pio, bool *SRAM_enable)
{
    ROM.allocate(fil_sz);
    memcpy(ROM.ptr, fil, fil_sz);
    *SRAM_enable = false;

    auto *tptr = static_cast<char *>(ROM.ptr);

    tptr += 0x100;

    kind = sc_invalid;
#define CK(x, y) if (compare_str_term2(tptr, x)) kind = y
        CK("SEGA MEGA DRIVE", sc_genesis);
        CK("SEGA GENESIS", sc_genesis);
        CK("SEGA 32X", sc_32x);
        CK("SEGA EVERDRIVE", sc_everdrive);
        CK("SEGA SSF", sc_ssf);
        CK("SEGA MEGAWIFI", sc_megawifi);
        CK("SEGA PICO", sc_pico);
        CK("SEGA TERA68K", sc_tera68k);
        CK("SEGA TERA286", sc_tera286);
#undef CK
    if ((kind != sc_genesis) && (kind != sc_ssf)) {
        printf("\nError loading Sega Genesis cart");
        return false;
    }

    tptr += 16;

    memcpy(&header.copyright, tptr, 16);
    header.copyright[16] = 0;
    tptr += 16;

    memcpy_t0(header.title_domestic, tptr, 48);
    tptr += 48;

    memcpy_t0(header.title_overseas, tptr, 48);
    tptr += 48;

    memcpy_t0(header.serial_number, tptr, 14);
    tptr += 14;

    header.rom_checksum = bswap_16(*(u16 *)tptr);
    tptr += 2;

#define CK(w, k) case w: header.devices_supported[i] = scd_##k; break
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
                header.devices_supported[i] = scd_none;
                break;
        }
    }
#undef CK
    tptr += 16;

    header.rom_addr_start = bswap_32(*(u32 *)tptr);
    tptr += 4;
    header.rom_addr_end = bswap_32(*(u32 *)tptr);
    tptr += 4;

    header.ram_addr_start = bswap_32(*(u32 *)tptr);
    tptr += 4;
    header.ram_addr_end = bswap_32(*(u32 *)tptr);
    tptr += 4;
    //assert(header.ram_addr_start == 0xFF0000);
    //assert(header.ram_addr_end == 0xFFFFFF);
    tptr = eval_cart_RAM(tptr);
    pio->cartridge_port.SRAM.requested_size = RAM_size;
    pio->cartridge_port.SRAM.ready_to_use = 0;
    pio->cartridge_port.SRAM.dirty = RAM_size > 0;
    SRAM = &pio->cartridge_port.SRAM;
    if ((SRAM->requested_size > 0) && (ROM.size <= 0x200000)) *SRAM_enable = true;
    SRAM->persistent = 1;
    tptr += 52; // skip modem, padding
    u32 num_regions = 0;
    for (u32 i = 0; i < 3; i++) {
        switch(tptr[i]) {
            case 'J':
                header.region_japan = 1;
                num_regions++;
                break;
            case 'U':
                num_regions++;
                header.region_usa = 1;
                break;
            case 'E':
                num_regions++;
                header.region_europe = 1;
                break;
            case ' ':
                break;
            default:
                if (num_regions == 0) {
                    int r = ascii_hex_to_int(tptr[i]);
                    if (r != -1) {
                        if (r & 1) {
                            header.region_japan = 1;
                            num_regions++;
                        };
                        if (r & 2) { printf("\nInvalid region bit!"); };
                        if (r & 4) {
                            header.region_usa = 1;
                            num_regions++;
                        };
                        if (r & 8) {
                            header.region_europe = 1;
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
    printf("\nREGION JP:%d US:%d", header.region_japan, header.region_usa);
    printf("\nCOPYRIGHT: %s", header.copyright);
    printf("\nTITLE: %s", header.title_domestic);
    printf("\nROM END: %08x", header.rom_addr_end);

    //bswap_16_array(&ROM.ptr, ROM.size);
    return true;
}
}