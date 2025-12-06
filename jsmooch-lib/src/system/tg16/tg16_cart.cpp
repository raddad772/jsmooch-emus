#include <cstring>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "tg16_cart.h"
namespace TG16 {
void cart::reset()
{

}

void cart::write(u32 addr, u8 val)
{
    // NEUTOPIA!!!
    printf("\nWARN cart write %06x %02x", addr, val);
    /*if (addr == 0x02b802) {
        dbg_break("CARTWRITE", 0);
    }*/
    //dbg_break("WHATHO", 0);
}

u8 cart::read(u32 addr, u8 old) const {
    u32 bank = (addr >> 17) & 7;
    u32 inner = addr & 0x1FFFF;
    return ptrs[bank][inner];
}

void cart::load_ROM_from_RAM(void *ptr, u64 sz, physical_io_device &pio)
{
    ROM.allocate(sz);
    memcpy(ROM.ptr, ptr, sz);
    u32 num_banks = (sz >> 17) & 7;
    // 384, 512, 768...
    u32 offsets[8];
    switch(num_banks) {
        case 3: {
            // 384kb: 256 + 128x2
            static constexpr u32 o[8] = {0, 1, 0, 1, 2, 2, 2, 2};
            memcpy(offsets, o, sizeof(offsets));
            break; }
        case 4: {
            // 512kb: 256 + 256x3
            static constexpr u32 o[8] = {0, 1, 2, 3, 2, 3, 2, 3};
            memcpy(offsets, o, sizeof(offsets));
            break; }
        case 6: {
            // 768kb: 512 + 256x2
            static constexpr u32 o[8] = {0, 1, 2, 3, 4, 5, 4, 5};
            memcpy(offsets, o, sizeof(offsets));
            break;}
        default: {
            // Elsewise
            static constexpr u32 o[8] = {0, 1, 2, 3, 4, 5, 6, 7};
            memcpy(offsets, o, sizeof(offsets));
            break; }
    }
    for (u32 i = 0; i < 8; i++) {
        ptrs[i] = static_cast<u8 *>(ROM.ptr) + ((offsets[i] * 0x20000) % sz);
    }
    SRAM = &pio.cartridge_port.SRAM;
    if (SRAM) {
        SRAM->persistent = true;
        SRAM->fill_value = 0xFF;
        SRAM->dirty = true;
        SRAM->requested_size = 8192;
    }
}

u8 cart::read_SRAM(u32 addr) const
{
    if (!SRAM) return 0;
    return static_cast<u8 *>(SRAM->data)[addr & 0x1FFF];
}

void cart::write_SRAM(u32 addr, u8 val)
{
    if (SRAM) {
        static_cast<u8 *>(SRAM->data)[addr & 0x1FFF] = val;
        SRAM->dirty = true;
    }
}
}