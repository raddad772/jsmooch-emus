//
// Created by . on 11/25/25.
//

#include "c64_bus.h"
#include "c64_debugger.h"

namespace C64 {

void core::run_cpu() {
    if (vic2.AEC) return;
    if (cpu.pins.RW) {
        mem.write_main_bus(cpu.pins.Addr, cpu.pins.D);
        dbgloglog(this, C64_CAT_CPU_WRITE, DBGLS_INFO, "%04x   (write) %02x", cpu.pins.Addr, cpu.pins.D);
    }
    else {
        cpu.pins.D = open_bus = mem.read_main_bus(cpu.pins.Addr, open_bus, true);
        dbgloglog(this, C64_CAT_CPU_READ, DBGLS_INFO, "%04x   (read)  %02x", cpu.pins.Addr, cpu.pins.D);
    }
}

void core::run_block() {
    run_cpu();
    vic2.cycle();
    master_clock += 8;
}

void core::schedule_first() {
    vic2.schedule_first();
    scheduler.only_add_abs(0, 0, this, &run_cpu, nullptr);
}

void core::reset() {
    scheduler.clear();
    schedule_first();
}

core::core(jsm::regions in_region) : region(in_region), scheduler(&this->master_clock), vic2(this) {
    has.load_BIOS = true;
    has.save_state = false;
    has.set_audiobuf = false;
    switch (region) {
        case jsm::regions::USA:
        case jsm::regions::JAPAN:
            display_standard = jsm::display_standards::NTSC;
            break;
        case jsm::regions::EUROPE:
            display_standard = jsm::display_standards::PAL;
            break;
    }

}

}