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
    cpu.pins.RDY = vic2.signals.BA;
    if (!cpu.pins.RW) { // Read cycle
        if (cpu.pins.RDY) return;
        cpu.pins.D = open_bus = mem.read_main_bus(cpu.pins.Addr, open_bus, true);
        dbgloglog(this, C64_CAT_CPU_READ, DBGLS_INFO, "%04x   (read)  %02x", cpu.pins.Addr, cpu.pins.D);
    }
    cpu.cycle(); // Write cycle
    if (cpu.pins.RW) {
        mem.write_main_bus(cpu.pins.Addr, cpu.pins.D);
        dbgloglog(this, C64_CAT_CPU_WRITE, DBGLS_INFO, "%04x   (write) %02x", cpu.pins.Addr, cpu.pins.D);
    }

}

void core::run_block() {
    vic2.cycle();
    run_cpu();
    cia1.cycle();
    cia2.cycle();
    sid.cycle();
    master_clock++;
}

void core::reset() {
    vic2.reset();
    mem.reset();
    sid.reset();
    cpu.reset();
    cia1.reset();
    cia2.reset();
    cia1.pins.PRA_in = 0xFF;
    cia1.pins.PRB_in = 0xFF;
    cia2.pins.PRA_in = 0xFF;
    cia2.pins.PRB_in = 0xFF;
    if (reset_PC_to != -1) {
        cpu.force_jump(reset_PC_to);
        printf("\nIMPL RESET_PC_TO!");
    }

    //scheduler.clear();
    //schedule_first();
}

static u8 read_vic2(void *ptr, u16 addr) {
    auto *core = static_cast<C64::core *>(ptr);
    addr |= ((core->cia2.read_PRA() & 3) ^ 3) << 14;
    // $1000-1FFF 0001  or $9000-9FFFF 1001
    const u32 upper = (addr >> 12) & 0b1111;
    if ((upper == 0b1001) || (upper == 0b0001)) {
        // Enable character ROM only!
        return core->mem.CHARGEN[addr & 0xFFF];
    }
    return core->mem.RAM[addr];
}

static void CIA_update_IRQ(void *ptr, u32 device_num, u8 level) {
    auto *core = static_cast<C64::core *>(ptr);
    core->update_IRQ(device_num, level);
}

static void CIA_update_NMI(void *ptr, u32 device_num, u8 level) {
    auto *core = static_cast<C64::core *>(ptr);
    core->update_NMI(device_num, level);
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

    cia1.update_irq.ptr = this;
    cia2.update_irq.ptr = this;
    cia1.update_irq.device_num = 2;
    cia2.update_irq.device_num = 1;
    cia1.update_irq.func = &CIA_update_IRQ;
    cia2.update_irq.func = &CIA_update_NMI;
}

u8 core::read_io1(u8 addr, u8 old, bool has_effect) {
    return 0xFF;
}

u8 core::read_io2(u8 addr, u8 old, bool has_effect)
{
    //printf("\nREAD IO2 %02x", addr);
    return 0xFF;
}

void core::write_io1(u8 addr, u8 val) {
    //printf("\nWRITE IO1 %02x:%02x", addr, val);

}

void core::write_io2(u8 addr, u8 val) {
    //printf("\nWRITE IO2 %02x:%02x", addr, val);
}


}