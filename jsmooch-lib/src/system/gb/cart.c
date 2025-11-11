#include <assert.h>
#include <stdio.h>
#include "stdlib.h"
#include "helpers/int.h"
#include "cart.h"
#include "system/gb/gb_clock.h"
#include "system/gb/mappers/mapper.h"
#include "gb_bus.h"

u32 NUM_ROMBANKS(u32 inp) {
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

void GB_cart_init(GB_cart* this, enum GB_variants variant, GB_clock* clock, GB_bus* bus) {
    this->variant = variant;
    this->clock = clock;
    this->bus = bus;

    this->ROM = NULL;

    this->mapper = NULL;

    this->header.ROM_banks = 0;
    this->header.ROM_size = 0;
    this->header.RAM_size = 0;
    this->header.RAM_banks = 0;
    this->header.RAM_mask = 0;
    this->header.ram_present = 0;
    this->header.battery_present = 0;
    this->header.timer_present = 0;
    this->header.rumble_present = 0;
    this->header.sensor_present = 0;
    this->header.sgb_functions = 0;
    this->header.gb_compatible = 0;
    this->header.cgb_compatible = 0;
    this->header.mapper = NONE;
}

void GB_cart_delete(GB_cart* this) {
    if (this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }
    if (this->mapper != NULL) {
        delete_GB_mapper(this->mapper);
        this->mapper = NULL;
    }
}

#ifndef MIN
#define MIN(x,y) (((x) <= (y)) ? (x) : (y));
#endif

void GBC_read_ROM(GB_cart* this, u8 *inp, u64 size)
{
    u32 k = MIN((u32)this->header.ROM_size, (u32)size);
    this->header.ROM_size = k;
    for (u32 i = 0; i < k; i++) {
        // @ts-ignore
        this->ROM[i] = inp[i];
    }
}

void GBC_setup_mapper(GB_cart* this)
{
    this->mapper = new_GB_mapper(this->clock, this->bus, this->header.mapper);
    if (this->bus->mapper != NULL) {
        delete_GB_mapper(this->bus->mapper);
    }
    this->bus->mapper = this->mapper;
    this->mapper->set_cart(this->mapper, this);
}

void GB_cart_load_ROM_from_RAM(GB_cart*this, void* ibuf, u64 size, physical_io_device *pio)
{
    this->SRAM = &pio->cartridge_port.SRAM;
    this->SRAM->ready_to_use = 0;
    this->SRAM->requested_size = 0;

    u8* inp = (u8*)ibuf;
    if ((inp[0x104] != 0xCE) || (inp[0x105] != 0xED)) {
        assert(1 != 0);
    }

    this->clock->cgb_enable = (inp[0x143] == 0x80) || (inp[0x143] == 0xC0);
    if (this->variant != GBC) this->clock->cgb_enable = 0;
    fflush(stdout);

    this->header.ROM_banks = NUM_ROMBANKS(inp[0x0148]);
    this->header.ROM_size = (this->header.ROM_banks ? (this->header.ROM_banks * 16384) : 32768);
    if (this->ROM != NULL) {
        free(this->ROM);
        this->ROM = NULL;
    }
    
    this->ROM = (u8 *)malloc(this->header.ROM_size);

    switch (inp[0x149]) {
    case 0:
        this->header.RAM_size = 0;
        this->header.RAM_banks = 0;
        this->header.RAM_mask = 0;
        break;
    case 1:
        this->header.RAM_size = 2048;
        this->header.RAM_mask = 0x7FF;
        this->header.RAM_banks = 0;
        break;
    case 2:
        this->header.RAM_mask = 0x1FFF;
        this->header.RAM_size = 8192;
        this->header.RAM_banks = 0;
        break;
    case 3:
        this->header.RAM_mask = 0x1FFF;
        this->header.RAM_size = 32768;
        this->header.RAM_banks = 4;
        break;
    case 4:
        this->header.RAM_mask = 0x1FFF;
        this->header.RAM_size = 131072;
        this->header.RAM_banks = 16;
        break;
    case 5:
        this->header.RAM_mask = 0x1FFF;
        this->header.RAM_size = 65536;
        this->header.RAM_banks = 8;
        break;
    default:
        // @ts-ignore
        printf("UNKNOWN RAM SIZE %d", (u32)inp[0x149]);
        this->header.RAM_size = 0;
        this->header.RAM_banks = 0;
        this->header.RAM_mask = 0;
        break;
    }

    this->header.battery_present = 0;
    this->header.timer_present = 0;
    this->header.rumble_present = 0;
    this->header.sensor_present = 0;
    // @ts-ignore
    u32 mn = inp[0x0147];
    this->header.mapper = NONE;
    //printf("MAPPER % d", mn);
    switch (mn) {
        case 0:
            this->header.mapper = NONE;
            break;
        case 1: // MBC1
        case 2: // MBC1+RAM
        case 3: // MBC1+RAM+BATTERY
            this->header.mapper = MBC1;
            break;
        case 6: // MBC2+BATTERY
        case 5: // MBC2
            this->header.mapper = MBC2;
            break;
        case 0x0D:
        case 0x0C:
        case 0x0B: // MMM01
            this->header.mapper = MMM01;
            break;
        case 0x0F: // MMBC3+TIMER+BATTERY
        case 0x10: // MMBC3+TIMER+RAM+BATTERY
        case 0x11: // MMBC3
        case 0x12: // MMBC3+RAM
        case 0x13: // MMBC3+RAM+BATTERY
            this->header.mapper = MBC3;
            break;
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
            this->header.mapper = MBC5;
            break;
        case 0x20:
            this->header.mapper = MBC6;
            break;
        case 0x22:
            this->header.mapper = MBC7;
            break;
        case 0xFC:
            this->header.mapper = POCKET_CAMERA;
            break;
        case 0xFD:
            this->header.mapper = BANDAI_TAMA5;
            break;
        case 0xFE:
            this->header.mapper = HUC3;
            break;
        case 0xFF:
            this->header.mapper = HUC1;
            break;
        default:
            printf("Unrecognized mapper %d", mn);
            return;
    }

    switch (mn) {
    case 0x02: // MBC1+RAM
        this->header.ram_present = 1;
        break;
    case 0x03: // MBC1+RAM+BATTERY
        this->header.battery_present = 1;
        this->header.ram_present = 1;
        break;
    case 0x06: // MBC2+BATTERY
        this->header.battery_present = 1;
        break;
    case 0x08: // ROM+RAM
        this->header.ram_present = 1;
        break;
        // @ts-ignore
    case 0x09: // ROM+RAM+BATTERY
        this->header.battery_present = 1;
        this->header.ram_present = 1;
        break;
    case 0x0C: // MMM01+RAM
        this->header.ram_present = 1;
        break;
    case 0x0D: // MMM01+RAM+BATTERY
        this->header.battery_present = 1;
        this->header.ram_present = 1;
        break;
    case 0x0F: // MMBC3+TIMER+BATTERY
        this->header.timer_present = 1;
        this->header.battery_present = 1;
        break;
    case 0x10: // MMBC3+TIMER+RAM+BATTERY
        this->header.timer_present = 1;
        this->header.battery_present = 1;
        this->header.ram_present = 1;
        break;
    case 0x12: // MMBC3+RAM
        this->header.ram_present = 1;
        break;
    case 0x13: // MMBC3+RAM+BATTERY
        this->header.ram_present = 1;
        this->header.battery_present = 1;
        break;
    case 0x1A: // +RAM
        this->header.ram_present = 1;
        break;
    case 0x1B: // +RAM+BATTERY
        this->header.ram_present = 1;
        this->header.battery_present = 1;
        break;
    case 0x1C: // +RUMBLE
        this->header.rumble_present = 1;
        break;
    case 0x1D: // +RUMBLE+RAM
        this->header.rumble_present = 1;
        this->header.ram_present = 1;
        break;
    case 0x1E: // +RUMBLE+RAM+BATTERY
        this->header.rumble_present = 1;
        this->header.ram_present = 1;
        this->header.battery_present = 1;
        break;
    case 0x22: // +SENSOR+RUMBLE+RAM+BATTERY
        this->header.rumble_present = 1;
        this->header.ram_present = 1;
        this->header.battery_present = 1;
        this->header.sensor_present = 1;
        break;
    case 0xFF: // +RAM+BATTERY
        this->header.ram_present = 1;
        this->header.battery_present = 1;
        break;
    }
    this->SRAM->persistent = this->header.battery_present;
    this->SRAM->dirty = this->header.RAM_size > 0;
    this->SRAM->requested_size = this->header.RAM_size;
    this->SRAM->ready_to_use = 0;

    GBC_read_ROM(this, inp, size);
    GBC_setup_mapper(this);
}
