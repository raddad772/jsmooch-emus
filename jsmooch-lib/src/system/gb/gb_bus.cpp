#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "gb_bus.h"
#include "gb_clock.h"
#include "gb_cpu.h"
#include "gb_debugger.h"
#include "mappers/mapper.h"

namespace GB {

core::core(variants variant_in) :
    cpu(this, &clock, variant_in),
    ppu(variant_in, &clock, this),
    cart(variant_in, this),
    variant(variant_in)
    {
    has.load_BIOS = true;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = true;
    has.set_audiobuf = true;
    switch(variant) {
        case DMG:
            snprintf(label, sizeof(label), "Nintendo GameBoy");
            break;
        case GBC:
            snprintf(label, sizeof(label), "Nintendo GameBoy Color");
            break;
        case SGB:
            snprintf(label, sizeof(label), "Nintendo Super GameBoy");
            break;
    }

}

void core::generic_mapper_reset()
{
    generic_mapper.VRAM_bank_offset = 0;
    generic_mapper.WRAM_bank_offset = 0x1000;
}

void core::reset()
{
    generic_mapper_reset();
}

u32 core::PPU_read(u32 addr)
{
    if ((addr < 0x8000) || (addr >= 0xC000)) {
        printf("WAIT WHAT BAD READ? %d", addr);
        return 0xFF;
    }
    return generic_mapper.VRAM[(addr - 0x8000) & 0x3FFF];
}

void core::CPU_write(u32 addr, u32 val) {
    if ((addr >= 0xE000) && (addr < 0xFE00)) addr -= 0x2000;  // Mirror WRAM

    if ((addr >= 0x8000) && (addr < 0xA000)) { // VRAM
        DBG_EVENT(DBG_GB_EVENT_VRAM_WRITE);
        generic_mapper.VRAM[(addr & 0x1FFF) + generic_mapper.VRAM_bank_offset] = (u8)val;
        return;
    }

    if ((addr >= 0xC000) && (addr < 0xD000)) { // WRAM lo bank
        generic_mapper.WRAM[addr & 0xFFF] = (u8)val;
        return;
    }
    if ((addr >= 0xD000) && (addr < 0xE000)) { // WRAM hi bank
        generic_mapper.WRAM[(addr & 0xFFF) + generic_mapper.WRAM_bank_offset] = val;
        return;
    }
    if ((addr >= 0xFE00) && (addr < 0xFF00)) { // OAM
        ppu.write_OAM(addr, val);
        return;
    }
    if ((addr >= 0xFF00) && (addr < 0xFF80)) {// registers
        write_IO(addr, val);
        return;
    }
    if ((addr >= 0xFF80) && (addr < 0xFFFF)) { // HRAM always accessible
        generic_mapper.HRAM[addr - 0xFF80] = (u8)val;
        return;
    }
    if (addr == 0xFFFF) {  // 0xFFFF register
        write_IO(addr, val);
        return;
    }
    mapper->CPU_write(mapper, addr, val);
}

u32 core::CPU_read(u32 addr, u32 val, bool has_effect) {
    if ((addr >= 0xE000) && (addr < 0xFE00)) addr -= 0x2000; // Mirror WRAM
    if (clock.bootROM_enabled) {
        if (addr < 0x100) {
            return static_cast<u8 *>(BIOS.ptr)[addr];
        }
        if (generic_mapper.BIOS_big && (addr >= 0x200) && (addr < 0x900))
            return static_cast<u8 *>(BIOS.ptr)[addr - 0x100];
    }

    if ((addr >= 0x8000) && (addr < 0xA000)) { // VRAM, banked
        //if (clock.CPU_can_VRAM)
        return generic_mapper.VRAM[(addr & 0x1FFF) + generic_mapper.VRAM_bank_offset];
        return 0xFF;
    }

    if ((addr >= 0xC000) && (addr < 0xD000)) // WRAM lo bank
        return generic_mapper.WRAM[addr & 0xFFF];
    if ((addr >= 0xD000) && (addr < 0xE000)) // WRAM hi bank
        return generic_mapper.WRAM[(addr & 0xFFF) + generic_mapper.WRAM_bank_offset];
    if ((addr >= 0xFE00) && (addr < 0xFF00)) // OAM
        return ppu.read_OAM(addr);
    if ((addr >= 0xFF00) && (addr < 0xFF80)) // registers
        return read_IO(addr, val);//, has_effect);
    if ((addr >= 0xFF80) && (addr < 0xFFFF)) // HRAM always accessible
        return generic_mapper.HRAM[addr - 0xFF80];
    if (addr == 0xFFFF)
        return read_IO(0xFFFF, val);//, has_effect); // 0xFFFF register

    return mapper->CPU_read(mapper, addr, val, has_effect);
}

void core::set_BIOS(u8 *BIOSbuf, u32 sz)
{
    BIOS.allocate(sz);
    memcpy(BIOS.ptr, BIOSbuf, sz);
    generic_mapper.BIOS_big = sz > 256;
}
}