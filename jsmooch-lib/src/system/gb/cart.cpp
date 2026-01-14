#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "helpers/int.h"
#include "cart.h"
#include "system/gb/gb_clock.h"
#include "system/gb/mappers/mapper.h"
#include "gb_bus.h"

namespace GB {
static u32 NUM_ROMBANKS(u32 inp) {
    switch (inp) {
        case 0: return 0;
        case 1: return 4;
        case 2: return 8;
        case 3: return 16;
        case 4: return 32;
        case 5: return 64;
        case 6: return 128;
        case 7: return 256;
        case 8: return 512;
        case 0x52: return 72;
        case 0x53: return 80;
        case 0x54: return 96;
    }
    assert(1 != 0);
    return 1;
}

cart::cart(variants variant_in, core* parent) : variant(variant_in), bus(parent) {

}

cart::~cart() {
    if (ROM != nullptr) {
        free(ROM);
        ROM = nullptr;
    }
    if (mapper != nullptr) {
        delete_mapper(mapper);
        mapper = nullptr;
    }
}

#ifndef MIN
#define MIN(x,y) (((x) <= (y)) ? (x) : (y));
#endif

void cart::read_ROM(u8 *inp, u64 size)
{
    u32 k = MIN((u32)header.ROM_size, (u32)size);
    header.ROM_size = k;
    assert(ROM!=nullptr);
    for (u32 i = 0; i < k; i++) {
        ROM[i] = inp[i];
    }
}

void cart::setup_mapper()
{
    mapper = new_mapper(clock, bus, header.mapper);
    if (bus->mapper != nullptr) {
        delete_mapper(bus->mapper);
    }
    bus->mapper = mapper;
    mapper->set_cart(mapper, this);
}

void cart::load_ROM_from_RAM(void* ibuf, u64 size, physical_io_device *pio)
{
    SRAM = &pio->cartridge_port.SRAM;
    SRAM->ready_to_use = false;
    SRAM->requested_size = 0;

    auto *inp = static_cast<u8 *>(ibuf);
    if ((inp[0x104] != 0xCE) || (inp[0x105] != 0xED)) {
        assert(1 != 0);
    }

    clock->cgb_enable = (inp[0x143] == 0x80) || (inp[0x143] == 0xC0);
    if (variant != GBC) clock->cgb_enable = 0;
    fflush(stdout);

    header.ROM_banks = NUM_ROMBANKS(inp[0x0148]);
    header.ROM_size = (header.ROM_banks ? (header.ROM_banks * 16384) : 32768);
    if (ROM != nullptr) {
        free(ROM);
        ROM = nullptr;
    }
    
    ROM = static_cast<u8 *>(malloc(header.ROM_size));

    switch (inp[0x149]) {
    case 0:
        header.RAM_size = 0;
        header.RAM_banks = 0;
        header.RAM_mask = 0;
        break;
    case 1:
        header.RAM_size = 2048;
        header.RAM_mask = 0x7FF;
        header.RAM_banks = 0;
        break;
    case 2:
        header.RAM_mask = 0x1FFF;
        header.RAM_size = 8192;
        header.RAM_banks = 0;
        break;
    case 3:
        header.RAM_mask = 0x1FFF;
        header.RAM_size = 32768;
        header.RAM_banks = 4;
        break;
    case 4:
        header.RAM_mask = 0x1FFF;
        header.RAM_size = 131072;
        header.RAM_banks = 16;
        break;
    case 5:
        header.RAM_mask = 0x1FFF;
        header.RAM_size = 65536;
        header.RAM_banks = 8;
        break;
    default:
        // @ts-ignore
        printf("UNKNOWN RAM SIZE %d", static_cast<u32>(inp[0x149]));
        header.RAM_size = 0;
        header.RAM_banks = 0;
        header.RAM_mask = 0;
        break;
    }

    header.battery_present = 0;
    header.timer_present = 0;
    header.rumble_present = 0;
    header.sensor_present = 0;
    // @ts-ignore
    u32 mn = inp[0x0147];
    header.mapper = NONE;
    //printf("MAPPER % d", mn);
    switch (mn) {
        case 0:
            header.mapper = NONE;
            break;
        case 1: // MBC1
        case 2: // MBC1+RAM
        case 3: // MBC1+RAM+BATTERY
            header.mapper = MBC1;
            break;
        case 6: // MBC2+BATTERY
        case 5: // MBC2
            header.mapper = MBC2;
            break;
        case 0x0D:
        case 0x0C:
        case 0x0B: // MMM01
            header.mapper = MMM01;
            break;
        case 0x0F: // MMBC3+TIMER+BATTERY
        case 0x10: // MMBC3+TIMER+RAM+BATTERY
        case 0x11: // MMBC3
        case 0x12: // MMBC3+RAM
        case 0x13: // MMBC3+RAM+BATTERY
            header.mapper = MBC3;
            break;
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
            header.mapper = MBC5;
            break;
        case 0x20:
            header.mapper = MBC6;
            break;
        case 0x22:
            header.mapper = MBC7;
            break;
        case 0xFC:
            header.mapper = POCKET_CAMERA;
            break;
        case 0xFD:
            header.mapper = BANDAI_TAMA5;
            break;
        case 0xFE:
            header.mapper = HUC3;
            break;
        case 0xFF:
            header.mapper = HUC1;
            break;
        default:
            printf("Unrecognized mapper %d", mn);
            return;
    }

    switch (mn) {
    case 0x02: // MBC1+RAM
        header.ram_present = true;
        break;
    case 0x03: // MBC1+RAM+BATTERY
        header.battery_present = true;
        header.ram_present = true;
        break;
    case 0x06: // MBC2+BATTERY
        header.battery_present = true;
        break;
    case 0x08: // ROM+RAM
        header.ram_present = true;
        break;
        // @ts-ignore
    case 0x09: // ROM+RAM+BATTERY
        header.battery_present = true;
        header.ram_present = true;
        break;
    case 0x0C: // MMM01+RAM
        header.ram_present = true;
        break;
    case 0x0D: // MMM01+RAM+BATTERY
        header.battery_present = true;
        header.ram_present = true;
        break;
    case 0x0F: // MMBC3+TIMER+BATTERY
        header.timer_present = true;
        header.battery_present = true;
        break;
    case 0x10: // MMBC3+TIMER+RAM+BATTERY
        header.timer_present = true;
        header.battery_present = true;
        header.ram_present = true;
        break;
    case 0x12: // MMBC3+RAM
        header.ram_present = true;
        break;
    case 0x13: // MMBC3+RAM+BATTERY
        header.ram_present = true;
        header.battery_present = true;
        break;
    case 0x1A: // +RAM
        header.ram_present = true;
        break;
    case 0x1B: // +RAM+BATTERY
        header.ram_present = true;
        header.battery_present = true;
        break;
    case 0x1C: // +RUMBLE
        header.rumble_present = true;
        break;
    case 0x1D: // +RUMBLE+RAM
        header.rumble_present = true;
        header.ram_present = true;
        break;
    case 0x1E: // +RUMBLE+RAM+BATTERY
        header.rumble_present = true;
        header.ram_present = true;
        header.battery_present = true;
        break;
    case 0x22: // +SENSOR+RUMBLE+RAM+BATTERY
        header.rumble_present = true;
        header.ram_present = true;
        header.battery_present = true;
        header.sensor_present = true;
        break;
    case 0xFF: // +RAM+BATTERY
        header.ram_present = true;
        header.battery_present = true;
        break;
    }
    SRAM->persistent = header.battery_present;
    SRAM->dirty = header.RAM_size > 0;
    SRAM->requested_size = header.RAM_size;
    SRAM->ready_to_use = false;

    read_ROM(inp, size);
    setup_mapper();
}
}