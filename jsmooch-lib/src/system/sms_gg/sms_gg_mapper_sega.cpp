//
// Created by Dave on 2/9/2024.
//

#include <cassert>

#include "sms_gg_mapper_sega.h"
#include "sms_gg_bus.h"

namespace SMSGG {

void mapper_sega::map(u16 sms_start_addr, u16 sms_end_addr, enum mem_kinds kind, u32 offset)
{
    u32 start_page = sms_start_addr >> 8;
    u32 end_page = sms_end_addr >> 8;
    for (u32 page = start_page; page <= end_page; page++) {
        assert(page < 256);
        mem_region *region = &regions[page];
        region->kind = kind;
        region->empty = 0;

        switch(kind) {
            case MK_empty:
                region->empty = 1;
                region->buf = nullptr;
                region->read_only = 1;
                region->offset = 0;
                break;
            case MK_cart_rom:
                region->empty = 0;
                region->buf = &ROM;
                region->read_only = 1;
                region->offset = offset;
                break;
            case MK_bios:
                region->empty = 0;
                region->buf = &BIOS;
                region->read_only = 1;
                region->offset = offset;
                break;
            case MK_cart_ram:
                region->empty = 0;
                region->buf = &cart_RAM;
                region->read_only = 0;
                region->offset = offset;
                break;
            case MK_sys_ram:
                region->empty = 0;
                region->buf = &RAM;
                region->read_only = 0;
                region->offset = offset;
                break;
            default:
                assert(1==0);
        }
        offset += 0x100;
    }
}

void mapper_sega::refresh_nomapper()
{
    map(0, 0x3FFF, MK_cart_rom, 0);
    map(0x4000, 0x7FFF, MK_cart_rom, 0x4000);
    map(0x8000, 0xBFFF, MK_cart_rom, 0x8000);
}

void mapper_sega::refresh_sega_mapper()
{
    // slot0 0-3FF is fixed on ROM
    map(0x0000, 0x03FF,  MK_cart_rom, 0);

    // slot0 400-3FFF is bankable
    map(0x0400, 0x3FFF, MK_cart_rom, 0x400 + (io.frame_control_register[0] << 14));

    // slot1
    map(0x4000, 0x7FFF, MK_cart_rom, io.frame_control_register[1] << 14);

    // slot2, ROM or RAM
    if (io.cart_ram.mapped) {
        map(0x8000, 0xBFFF, MK_cart_ram, io.cart_ram.bank << 14);
    }
    else {
        map(0x8000, 0xBFFF, MK_cart_rom, (io.frame_control_register[2] + io.bank_shift) << 14);
    }


    // C000-FFFF stays mapped to RAM
}

void mapper_sega::refresh_mapping()
{
    if (!sega_mapper_enabled) // Just generic SMS mapping
        refresh_nomapper();
    else
        refresh_sega_mapper();
}

mapper_sega::mapper_sega(jsm::systems in_variant) : variant(in_variant)
{
    is_sms = (variant == jsm::systems::SMS1) || (variant == jsm::systems::SMS2);
    //u32 is_gg = (variant == jsm::systems::SG1000);
    switch(variant) {
        case jsm::systems::SMS1:
        case jsm::systems::SMS2:
        case jsm::systems::GG:
            RAM.allocate(8 * 1024);
            cart_RAM.allocate(32 * 1024);
            RAM.clear();
            cart_RAM.clear();
            sega_mapper_enabled = 1;
            break;
        case jsm::systems::SG1000:
            RAM.allocate(2 * 1024);
            RAM.clear();
            sega_mapper_enabled = 0;
            break;
        default:
            assert(1==2);
    }
    map(0xC000, 0xFFFF, MK_sys_ram, 0);

    io.cart_enabled = !io.bios_enabled;
    io.frame_control_register[0] = 0;
    io.frame_control_register[1] = 1;
    io.frame_control_register[2] = 2;
}

void mapper_sega::reset()
{
    //io.bios_enabled = has_bios;
    io.bios_enabled = false;
    io.cart_enabled = true;
    refresh_mapping();
}

void mapper_sega::set_BIOS(u32 to)
{
    // BIOS currently not supported
    return;
    if ((has_bios) && (to != io.bios_enabled)) {
        io.bios_enabled = to;
        refresh_mapping();
    }
}

void mapper_sega::write_registers(u16 addr, u8 val)
{
    if (sega_mapper_enabled) {
        switch (addr) {
            case 0xFFFC:
                switch(val & 3) {
                    case 0:
                        io.bank_shift = 0;
                        break;
                    case 1:
                        io.bank_shift = 0x18;
                        break;
                    case 2:
                        io.bank_shift = 0x10;
                        break;
                    case 3:
                        io.bank_shift = 0x08;
                        break;
                default:
                }
                if (io.bank_shift > 0) printf("\nWARNING BANKSHIFT %02x", io.bank_shift);
                io.cart_ram.bank = (val >> 2) & 1;
                io.cart_ram.mapped = (val >> 3) & 1;
                if ((val >> 4) & 1) {
                    printf("\nWARNING ATTEMPT TO MAP CART RAM TO SYSTEM RAM");
                }
                break;
            case 0xFFFD:
            case 0xFFFE:
            case 0xFFFF: {
                u32 w = addr - 0xFFFD;
                bool mapping_changed = (val != io.frame_control_register[w]);
                io.frame_control_register[w] = val;
                if (mapping_changed) refresh_mapping();
                break;
            }
            default:
        }
    }

    //return 1;
}


u8 core::main_bus_read(u16 addr, u32 has_effect)
{
    mem_region &mregion = mapper.regions[addr >> 8];
    if (mregion.empty) return 0xFF;
    if (!mregion.buf->ptr) {
        assert(1==2);
        return 0xFF;
    }
    u32 baddr = ((addr & 0xFF) + mregion.offset) & mregion.buf->mask;
    return mregion.buf->ptr[baddr];
}

void core::main_bus_write(u16 addr, u8 val)
{
    mapper.write_registers(addr, val);

    mem_region &mregion = mapper.regions[addr >> 8];
    if (mregion.read_only) return;
    if (mregion.empty) return;
    if (!mregion.buf->ptr) {
        assert(1==2);
    }
    u32 baddr = ((addr & 0xFF) + mregion.offset) & mregion.buf->mask;
    assert(baddr < mregion.buf->sz);
    mregion.buf->ptr[baddr] = val;
}

void mapper_sega::load_BIOS_from_RAM(buf &inBIOS)
{
    BIOS.allocate(inBIOS.size);
    memcpy(BIOS.ptr, inBIOS.ptr, inBIOS.size);
    has_bios = 1;
}

void mapper_sega::load_ROM_from_RAM(buf& inbuf)
{
    u64 sz = inbuf.size;
    u64 offset = 0;
    if ((inbuf.size % 16384) != 0) {
        printf("\nWARNING HEADER DETECTED SIZE: %ld", inbuf.size % 16384);
        offset = 512;
        sz -= 512;
    }
    assert(sz % 16384 == 0);

    ROM.allocate(sz);
    memcpy(ROM.ptr, static_cast<char *>(inbuf.ptr)+offset, sz);
}

}