//
// Created by . on 11/25/25.
//

#include "c64_bus.h"
#include "c64_debugger.h"

namespace C64 {

void core::run_cpu() {
    // AEC always stomps on CPU
    if (vic2.signals.AEC) return;

    // BA only stomps on CPU if it does a read
    if (cpu.pins.RW) {
        mem.write_main_bus(cpu.pins.Addr, cpu.pins.D);
        dbgloglog(this, C64_CAT_CPU_WRITE, DBGLS_INFO, "%04x   (write) %02x", cpu.pins.Addr, cpu.pins.D);
    }
    if (!vic2.signals.BA || cpu.pins.RW) // If BA is off, or we're in a write cycle, GO!
        cpu.cycle();
    if (!cpu.pins.RW) {
        cpu.pins.D = open_bus = mem.read_main_bus(cpu.pins.Addr, open_bus, true);
        dbgloglog(this, C64_CAT_CPU_READ, DBGLS_INFO, "%04x   (read)  %02x", cpu.pins.Addr, cpu.pins.D);
    }
}

void core::run_block() {
    vic2.cycle();
    run_cpu();
    master_clock++;
}

void core::reset() {
    vic2.reset();
    mem.reset();
    sid.reset();
    cpu.reset();

    //scheduler.clear();
    //schedule_first();
}

static u8 read_vic2(void *ptr, u16 addr) {
    auto *core = static_cast<C64::core *>(ptr);
    // TODO: when cia2 is working
    //addr |= ((core->cia2.portA & 3) ^ 3) << 14;
    // $1000-1FFF 0001  or $9000-9FFFF 1001
    const u32 upper = (addr >> 12) & 0b1111;
    if ((upper == 0b1001) || (upper == 0b0001)) {
        // Enable character ROM only!
        return core->mem.CHARGEN[addr & 0xFFF];
    }
    return core->mem.RAM[addr];
}



//scheduler(&this->master_clock),
core::core(jsm::regions in_region) : mem(this), vic2(this), region(in_region) {
    has.load_BIOS = true;
    has.save_state = false;
    has.set_audiobuf = true;
    switch (region) {
        case jsm::regions::USA:
        case jsm::regions::JAPAN:
            display_standard = jsm::display_standards::NTSC;
            break;
        case jsm::regions::EUROPE:
            display_standard = jsm::display_standards::PAL;
            break;
    }
    vic2.read_mem_ptr = this;
    vic2.read_mem = &read_vic2;
}

u8 core::read_cia1(u8 addr, u8 old, bool has_effect) {
    printf("\nREAD CIA1 %02x", addr);
    return 0xFF;
}

u8 core::read_cia2(u8 addr, u8 old, bool has_effect) {
    printf("\nREAD CIA2 %02x", addr);
    return 0xFF;
}

u8 core::read_io1(u8 addr, u8 old, bool has_effect) {
    printf("\nREAD IO1 %02x", addr);
    return 0xFF;
}

u8 core::read_io2(u8 addr, u8 old, bool has_effect)
{
    printf("\nREAD IO2 %02x", addr);
    return 0xFF;
}

void core::write_cia1(u8 addr, u8 val) {
    printf("\nWRITE CIA1 %02x:%02x", addr, val);
}

void core::write_cia2(u8 addr, u8 val) {
    printf("\nWRITE CIA2 %02x:%02x", addr, val);

}

void core::write_io1(u8 addr, u8 val) {
    printf("\nWRITE IO1 %02x:%02x", addr, val);

}

void core::write_io2(u8 addr, u8 val) {
    printf("\nWRITE IO2 %02x:%02x", addr, val);
}


}