//
// Created by . on 11/20/25.
//

#include <cassert>
#include "vip_bus.h"

namespace VIP {

core::core(jsm::systems in_kind) : scheduler(&clock.master_cycle_count), kind(in_kind) {
    has.save_state = false;
    has.load_BIOS = true;
    has.set_audiobuf = false;
    switch (kind) {
        case jsm::systems::COSMAC_VIP_2k:
            RAM_mask = 0x7FF;
            break;
        case jsm::systems::COSMAC_VIP_4k:
            RAM_mask = 0xFFF;
            break;
        default:
            assert(1==2);
    }
}

    /* MEM MAP
 * 0000-7FFF RAM (2K)
 * 8000-8FFF 512-byte ROM
 * 9000-FFFF free
 */

void core::write_main_bus(u16 addr, u8 val) {
    addr |= u6b;
    if (addr < 0x8000) {
        if (kind == jsm::systems::COSMAC_VIP_2k) {
            addr &= 0xFFF;
            if (addr > 0x800) return;
        }
        RAM[addr & RAM_mask] = val;
        return;
    }
    if (addr < 0x9000) {
        printf("\nWARN write to ROM %04x: %02x", addr, val);
        return;
    }
    printf("\nUncaught write to %04x: %02x", addr, val);
}

u8 core::read_main_bus(u16 addr, u8 old, bool has_effect) {
    addr |= u6b;
    if (addr < 0x8000) {
        if (kind == jsm::systems::COSMAC_VIP_2k) {
            addr &= 0xFFF;
            if (addr > 0x800) return 0xFF;
        }
        return RAM[addr & RAM_mask];
    }
    if (addr < 0x9000) {
        return ROM[addr & 0x1FF];
    }
    printf("\nUnserviced read from %04x", addr);
    return 0xFF;
}

void core::reset() {
    cpu.reset();
    pixie.reset();
    u6b = 0x8000;
    scheduler.clear();
    schedule_first();
    printf("\nCosmac VIP reset!");
}

void core::service_N() {
    printf("\nN%d", cpu.pins.N);
    if ((cpu.pins.N == 2) && (cpu.pins.MRD)) {
        hex_keypad.latch = cpu.pins.D & 15;
    }
    if ((cpu.pins.N == 4) && (cpu.pins.MRD)) {
        printf("\ndisable ROM at 0000");
        u6b = 0;
    }
}

void core::do_cycle() {
    // service writes
    cpu.pins.EF &= ~4;
    if (hex_keypad.keys[hex_keypad.latch]) cpu.pins.EF |= 4;
    if (cpu.pins.MWR) {
        write_main_bus(cpu.pins.Addr, cpu.pins.D);
    }

    // Cycle 1802 *8
    cpu.cycle();

    // service reads
    if (cpu.pins.MRD) {
        cpu.pins.D = read_main_bus(cpu.pins.Addr, cpu.pins.D, true);
    }

    // service OUT
    if (cpu.pins.N) service_N();

    // do bus for cpu->1861
    pixie.bus.D = cpu.pins.D;
    pixie.bus.DMA_OUT = cpu.pins.DMA_OUT;
    pixie.bus.IRQ = cpu.pins.INTERRUPT;
    pixie.bus.EF1 = cpu.pins.EF & 1;
    pixie.bus.SC = cpu.pins.SC;

    // cycle 1861 *8
    pixie.cycle();

    // do bus for 1861->cpu
    //cpu.pins.D = pixie.bus.D;
    cpu.pins.DMA_OUT = pixie.bus.DMA_OUT;
    cpu.pins.INTERRUPT = pixie.bus.IRQ;
    cpu.pins.EF = (cpu.pins.EF & ~1) | pixie.bus.EF1;
}
}