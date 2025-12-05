//
// Created by . on 7/24/24.
//

#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "mac_regs.h"
#include "mac_bus.h"
#include "mac_display.h"

namespace mac {
static constexpr u16 ulmask[4] = {
        0, // UDS = 0 LDS = 0
        0xFF, // UDS = 0 LDS = 1
        0xFF00, // UDS = 1 LDS = 0
        0xFFFF // UDS = 1 LDS = 1
};

u32 read_trace_m68k(void *ptr, u32 addr, u32 UDS, u32 LDS);

core::core(variants variant) : iwm{this, variant} {
    has.load_BIOS = true;
    has.max_loaded_files = 1;
    has.max_loaded_folders = 1;
    has.save_state = false;
    has.set_audiobuf = false;
    kind = variant;
    switch(variant) {
        case mac128k:
            RAM_size = 128 * 1024;
            ROM_size = 64 * 1024;
            snprintf(label, sizeof(label), "Mac Classic 128K");
            break;
        case mac512k:
            RAM_size = 512 * 1024;
            ROM_size = 64 * 1024;
            snprintf(label, sizeof(label), "Mac Classic 512K");
            break;
        case macplus_1mb:
            RAM_size = 1 * 1024 * 1024;
            ROM_size = 128 * 1024;
            snprintf(label, sizeof(label), "Mac Plus 1MB");
            break;
        default:
            assert(1==2);
    }
    RAM = static_cast<u16 *>(calloc(1, RAM_size));
    RAM_mask = (RAM_size >> 1) - 1; // in words
    ROM = static_cast<u16 *>(calloc(1, ROM_size));
    ROM_mask = (ROM_size >> 1) - 1;
    kind = variant;

    jsm_debug_read_trace dt;
    dt.read_trace_m68k = &read_trace_m68k;
    dt.ptr = this;
    cpu.setup_tracing(&dt, &clock.master_cycles);
    
    jsm.described_inputs = false;
    jsm.cycles_left = 0;
}

    core::~core() {
    if (RAM) free(RAM);
    if (ROM) free(ROM);
    RAM = nullptr;
    ROM = nullptr;
}

void core::set_sound_output(u32 set_to)
{
    sound.io.on = set_to;
}

void core::step_CPU()
{
    //if (io.m68k.stuck) dbg_printf("\nSTUCK cyc %lld", *cpu.trace.cycles);

    cpu.cycle();
    if (cpu.pins.FC == 7) {
        // Auto-vector interrupts!
        cpu.pins.VPA = 1;
        return;
    }
    if (cpu.pins.AS && (!cpu.pins.DTACK)) {
        if (!cpu.pins.RW) { // read
            io.cpu.last_read_data = cpu.pins.D = mainbus_read(cpu.pins.Addr, cpu.pins.UDS, cpu.pins.LDS, io.cpu.last_read_data, true);
            cpu.pins.DTACK = 1;
            if (::dbg.trace_on && ::dbg.traces.m68000.mem && ((cpu.pins.FC & 1) || ::dbg.traces.m68000.ifetch)) {
                dbg_printf(DBGC_READ "\nr.M68k(%lld)   %06x  v:%04x" DBGC_RST, clock.master_cycles, cpu.pins.Addr, cpu.pins.D);
            }
        }
        else { // write
            mainbus_write(cpu.pins.Addr, cpu.pins.UDS, cpu.pins.LDS, cpu.pins.D);
            cpu.pins.DTACK = 1;
            if (::dbg.trace_on && ::dbg.traces.m68000.mem && (cpu.pins.FC & 1)) {
                dbg_printf(DBGC_WRITE "\nw.M68k(%lld)   %06x  v:%04x" DBGC_RST, clock.master_cycles, cpu.pins.Addr, cpu.pins.D);
            }
        }
    }
}

void core::set_cpu_irq()
{
    // TODO: ???
    //M68k_set_interrupt_level(&cpu, io.irq.via | (io.irq.scc << 1) | (io.irq.iswitch << 2));
    cpu.set_interrupt_level(0);
}

void core::step_eclock()
{

    if (io.eclock == 0) {
        via.step();
    }

    io.eclock = (io.eclock + 1) % 10;
    if (io.eclock == 0) { // my RTC works off the e clock for some reason
        rtc.cycle_counter++;
        if (rtc.cycle_counter >= 783360) {
            rtc.cycle_counter = 0;
            rtc.seconds++;
            via.regs.IFR |= 1;
        }
    }

    iwm.clock();
}

void core::step_bus()
{

    // Step everything in the mac by 2
    step_CPU();         // CPU, 1 clock
    display.step2();    // Display, 2 clocks
    step_eclock();      // e-clock devices like VIA, 1 clocks, but only fires once every 10
    clock.master_cycles += 2;
}


u16 core::mainbus_read_iwm(u32 addr, u16 mask, u16 old, bool has_effect)
{
    uint16_t result = iwm.do_read(addr, mask, old, has_effect);
    return ((result << 8) | result) & mask;
}

void core::mainbus_write_iwm(u32 addr, u16 mask, u16 val)
{
        iwm.do_write(addr, mask, val);
}

u16 core::mainbus_read_scc(u32 addr, u16 mask, u16 old, bool has_effect)
{
    /*if ((addr >= sccWBase) && (addr < (sccWBase + 10))) {
        printf("\nRead to SCC write addr:%06x cyc:%lld", addr, clock.master_cycles);
        return old;
    }*/
    u32 oaddr = addr;
    if (addr >= sccWBase) addr -= sccWBase;
    else if (addr >= sccRBase) addr -= sccRBase;

    switch(addr) {
        case scc_aData:
            printf("\nSCC aData read, unsupported, cyc:%lld", clock.master_cycles);
            return scc.regs.aData << 8;
        case scc_aCtl:
            printf("\nSCC aCtl read, unsupported, cyc:%lld", clock.master_cycles);
            return scc.regs.aCtl << 8;
        case scc_bData:
            printf("\nSCC bData read, unsupported, cyc:%lld", clock.master_cycles);
            return scc.regs.bData << 8;
        case scc_bCtl:
            printf("\nSCC bCtl read, unsupported, cyc:%lld", clock.master_cycles);
            return scc.regs.bCtl << 8;
        default:
    }

    printf("\nUnhandled SCC read addr:%06x cyc:%lld", oaddr, clock.master_cycles);
    return 0xFFFF;
}

void core::mainbus_write_scc(u32 addr, u16 mask, u16 val)
{
    val >>= 8;
    /*if ((addr >= sccRBase) && (addr < (sccRBase + 10))) {
        printf("\nWrite to SCC read addr:%06x val:%02x cyc:%lld", addr, val, clock.master_cycles);
       return;
    }*/
    u32 oaddr = addr;
    if (addr >= sccWBase) addr -= sccWBase;
    else if (addr >= sccRBase) addr -= sccRBase;

    switch(addr) {
        case scc_aData:
            printf("\nSCC aData write, unsupported, val:%02x cyc:%lld", val, clock.master_cycles);
            scc.regs.aData = val;
            return;
        case scc_aCtl:
            printf("\nSCC aCtl write, unsupported, val:%02x cyc:%lld", val, clock.master_cycles);
            scc.regs.aCtl = val;
            return;
        case scc_bData:
            printf("\nSCC bData write, unsupported, val:%02x cyc:%lld", val, clock.master_cycles);
            scc.regs.bData = val;
            return;
        case scc_bCtl:
            printf("\nSCC bCtl write, unsupported, val:%02x cyc:%lld", val, clock.master_cycles);
            scc.regs.bCtl = val;
            return;
        default:
    }

    printf("\nUnhandled SCC write addr:%06x val:%04x cyc:%lld", oaddr, val & mask, clock.master_cycles);
}

void core::write_RAM(u32 addr, u16 mask, u16 val)
{
    RAM[(addr >> 1) & RAM_mask] = (RAM[(addr >> 1) & RAM_mask] & ~mask) | (val & mask);
}

u16 core::read_ROM(u32 addr, u16 mask, u16 old)
{
    return ROM[(addr >> 1) & ROM_mask] & mask;
}

u16 core::read_RAM(u32 addr, u16 mask, u16 old) const {
    return RAM[(addr >> 1) & RAM_mask];
}

void core::mainbus_write(u32 addr, u32 UDS, u32 LDS, u16 val)
{
    u16 mask = ulmask[(UDS << 1) | LDS];
    if (io.ROM_overlay) {
        if (addr < 0x100000) { // ROM
            return;
        }
        if ((addr >= 0x200000) && (addr < 0x300000)) { // ROM
            return;

        }
        if ((addr >= 0x400000) && (addr < 0x500000)) { // ROM
            return;
        }

        if ((addr >= 0x600000) && (addr < 0x800000)) {
            return write_RAM(addr - 0x600000, mask, val);
        }
    }
    else {

        if (addr < 0x400000) {
            return write_RAM(addr, mask, val);
        }
        if (addr < 0x500000) { // ROM
            return;
        }
    }


    if ((addr >= 0x900000) && (addr <= 0x9FFFFF)) {
        return mainbus_write_scc(addr, mask, val);
    }

    if ((addr >= 0xB00000) && (addr <= 0xBFFFFF)) {
        return mainbus_write_scc(addr, mask, val);
    }


    if ((addr >= 0xC00000) && (addr < 0xE00000)) {
        return mainbus_write_iwm(addr, mask, val);
    }

    if ((addr >= 0xE80000) && (addr < 0xF00000)) {
        return via.write(addr, mask, val);
    }

    if ((addr >= 0xF80000) && (addr < 0xF8FFFF))
        return;

    printf("\nUnhandled mainbus write addr:%06x val:%04x sz:%d cyc:%lld", addr, val, UDS && LDS ? 2 : 1, clock.master_cycles);
}

u16 core::mainbus_read(u32 addr, u32 UDS, u32 LDS, u16 old, bool has_effect)
{
    u16 mask = ulmask[(UDS << 1) | LDS];
    // 0-10k, 20-30k = ROM
    // 40-50k = ROM
    if (io.ROM_overlay) {
        if (addr < 0x100000)
            return read_ROM(addr, mask, old);
        if ((addr >= 0x200000) && (addr < 0x300000))
            return read_ROM(addr - 0x200000, mask, old);
        if ((addr >= 0x400000) && (addr < 0x500000))
            return read_ROM(addr, mask, old);
        if ((addr >= 0x600000) && (addr < 0x800000))
            return read_RAM(addr - 0x600000, mask, old);
    }
    else {
        if (addr < 0x400000)
            return read_RAM(addr, mask, old);

        if (addr < 0x500000)
            return read_ROM(addr - 0x400000, mask, old);
    }

    if ((addr >= 0x900000) && (addr <= 0x9FFFFF)) {
        //if (mask == 0xFFFF) return 0xFFFF;
        return mainbus_read_scc(addr, mask, old, has_effect);
    }

    if ((addr >= 0xBF0000) && (addr <= 0xBFFFFF)) {
        return mainbus_read_scc(addr, mask, old, has_effect);
    }

    if ((addr >= 0xC00000) && (addr < 0xE00000))
        return mainbus_read_iwm(addr, mask, old, has_effect);

    if ((addr >= 0xE80000) && (addr < 0xF00000))
        return via.read(addr, mask, old, has_effect);

    if ((addr >= 0xF80000) && (addr < 0xF8FFFF)) // phase read
        return 0xFFFF;

    printf("\nUnhandled mainbus read addr:%06x cyc:%lld", addr, clock.master_cycles);
    return 0xFFFF;
}

}