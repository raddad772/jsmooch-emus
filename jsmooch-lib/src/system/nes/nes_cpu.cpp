//
// Created by Dave on 2/5/2024.
//

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/nesm6502_opcodes.h"
#include "mappers/mapper.h"
#include "nes.h"
#include "nes_cpu.h"

u32 NES_controllerport::data() const {
    if (device == nullptr) return 0;
    switch(kind) {
        case NES_JOYPAD:
            return static_cast<NES_joypad *>(device)->data() & 3;
        case NES_NONE:
        default:
            break;
    }
    return 0;
}

void NES_controllerport::latch(u32 what) const {
    if (!device) return;
    switch(kind) {
        case NES_JOYPAD:
            return static_cast<NES_joypad *>(device)->latch(what);
        case NES_NONE:
        default:
            printf("PLEASE IMP 2");
            break;
    }
}

r2A03::r2A03(NES* nes) : cpu(M6502::nesdecoded_opcodes), nes(nes)
{
    tracing = 0;

    cpu.reset();

    io.dma.addr = io.dma.running = 0;
    io.dma.bytes_left = io.dma.step = 0;

    controller_port1.device = static_cast<void *>(&joypad1);
    controller_port2.device = static_cast<void *>(&joypad2);
    controller_port1.kind = NES_JOYPAD;
    controller_port2.kind = NES_NONE;
}

u32 NES_CPU_read_trace(void *tr, u32 addr) {
    NES* nes = static_cast<NES *>(tr);
    return nes->bus.CPU_read(addr, 0, 0);
}

void r2A03::notify_NMI(u32 level) {
    cpu.pins.NMI = level > 0 ? 1 : 0;
}

void r2A03::notify_IRQ(u32 level, u32 from) {
    cpu.pins.IRQ = irq.set_level(level, from);
}

void r2A03::reset() {
    cpu.reset();
    irq.clear();
    nes->clock.cpu_frame_cycle = 0;
    nes->clock.cpu_master_clock = 0;
    io.dma.running = 0;
}

void r2A03::run_cycle() {
    if (io.dma.running) {
        io.dma.step++;
        if (io.dma.step == 1) {
            return;
        }
        io.dma.step = 0;
        nes->ppu.write_regs(0x2004, nes->bus.CPU_read(io.dma.addr, 0, 1));
        io.dma.bytes_left--;
        io.dma.addr = (io.dma.addr + 1) & 0xFFFF;
        if (io.dma.bytes_left == 0) {
            io.dma.running = 0;
        }
        return;
    }

    cpu.cycle();
    if (cpu.pins.RW) {
        nes->bus.CPU_write(cpu.pins.Addr, cpu.pins.D);
    }
    else {
        cpu.pins.D = open_bus = nes->bus.CPU_read(cpu.pins.Addr, open_bus, 1);
    }
}

u32 r2A03::read_reg(u32 addr, u32 val, u32 has_effect) const
{
    switch(addr) {
        case 0x4016: // JOYSER0
            return controller_port1.data();
        case 0x4017: // JOYSER1
            return controller_port2.data();
        default:
            return nes->apu.read_IO(addr, val, has_effect);
    }
}

void r2A03::write_reg(u32 addr, u32 val)
{
    switch(addr) {
        case 0x4014: //OAMDMA
            io.dma.addr = val << 8;
            io.dma.running = 1;
            io.dma.bytes_left = 256;
            io.dma.step = 0;
            return;
        case 0x4016: // JOYSER0
            controller_port1.latch(val&1);
            return;
        default:
            nes->apu.write_IO(addr, val);
    }

}
