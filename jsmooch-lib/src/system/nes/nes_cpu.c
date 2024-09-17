//
// Created by Dave on 2/5/2024.
//

#include <stdio.h>
#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/nesm6502_opcodes.h"
#include "helpers/debugger/debugger.h"
#include "nes.h"
#include "nes_cpu.h"

#define RT struct r2A03* this

void IRQ_multiplexer_init(struct IRQ_multiplexer* this)
{
    this->IF = 0;
    this->current_level = 0;
}

u32 IRQ_multiplexer_set_level(struct IRQ_multiplexer* this, u32 level, u32 from)
{
    if (level == 0)
        this->IF &= (from ^ 0xFFFF);
    else
        this->IF |= from;
    return this->current_level = this->IF != 0;
}

void IRQ_multiplexer_clear(struct IRQ_multiplexer* this)
{
    this->IF = 0;
}

u32 IRQ_multiplexer_query_level(struct IRQ_multiplexer* this)
{
    return this->current_level;
}

void NES_controllerport_init(struct NES_controllerport* this)
{
    this->device = 0;
    this->kind = NES_NONE;
}

u32 NES_controllerport_data(struct NES_controllerport* this)
{
    if (this->device == NULL) return 0;
    switch(this->kind) {
        case NES_JOYPAD:
            return NES_joypad_data((struct NES_joypad*)this->device) & 3;
        case NES_NONE:
        default:
            break;
    }
    return 0;
}

void NES_controllerport_latch(struct NES_controllerport* this, u32 what) {
    if (this->device == NULL) return;
    switch(this->kind) {
        case NES_JOYPAD:
            return NES_joypad_latch((struct NES_joypad*)this->device, what);
        case NES_NONE:
        default:
            printf("PLEASE IMP 2");
            break;
    }
}

void r2A03_init(struct r2A03* this, struct NES* nes)
{
    M6502_init(&this->cpu, nesM6502_decoded_opcodes);
    this->nes = nes;

    this->tracing = 0;
    IRQ_multiplexer_init(&this->irq);

    M6502_reset(&this->cpu);

    this->io.dma.addr = this->io.dma.running = 0;
    this->io.dma.bytes_left = this->io.dma.step = 0;

    NES_controllerport_init(&this->controller_port1);
    NES_controllerport_init(&this->controller_port2);
    NES_joypad_init(&this->joypad1, 1);
    NES_joypad_init(&this->joypad2, 2);
    this->controller_port1.device = (void *)&this->joypad1;
    this->controller_port2.device = (void *)&this->joypad2;
    this->controller_port1.kind = NES_JOYPAD;
    this->controller_port2.kind = NES_NONE;
}

u32 NES_CPU_read_trace(void *tr, u32 addr) {
    struct NES* nes = (struct NES*)tr;
    return nes->bus.CPU_read(nes, addr, 0, 0);
}

void r2A03_notify_NMI(RT, u32 level) {
    this->cpu.pins.NMI = level > 0 ? 1 : 0;
}

void r2A03_notify_IRQ(RT, u32 level, u32 from) {
    //if (level) printf("\nf: %llu IRQ NOTIFY %d AT ly:%d", this->nes->clock.master_frame, level, this->nes->clock.ppu_y);
    fflush(stdout);
    this->cpu.pins.IRQ = IRQ_multiplexer_set_level(&this->irq, level, from);
}

void r2A03_reset(RT) {
    M6502_reset(&this->cpu);
    IRQ_multiplexer_clear(&this->irq);
    this->nes->clock.cpu_frame_cycle = 0;
    this->nes->clock.cpu_master_clock = 0;
    this->io.dma.running = 0;
}

void r2A03_run_cycle(RT) {
    if (this->io.dma.running) {
        this->io.dma.step++;
        if (this->io.dma.step == 1) {
            return;
        }
        this->io.dma.step = 0;
        NES_bus_PPU_write_regs(this->nes, 0x2004, this->nes->bus.CPU_read(this->nes, this->io.dma.addr, 0, 1));
        this->io.dma.bytes_left--;
        this->io.dma.addr = (this->io.dma.addr + 1) & 0xFFFF;
        if (this->io.dma.bytes_left == 0) {
            this->io.dma.running = 0;
        }
        return;
    }
    // TODO: put both of these either before or after the cycle, and re-do emu sync issues if any occur
    M6502_cycle(&this->cpu);
    if (!this->cpu.pins.RW) {
        this->cpu.pins.D = this->open_bus = this->nes->bus.CPU_read(this->nes, this->cpu.pins.Addr, this->open_bus, 1);
    }
    if (this->cpu.pins.RW) {
        this->nes->bus.CPU_write(this->nes, this->cpu.pins.Addr, this->cpu.pins.D);
    }
}

u32 NES_bus_CPU_read_reg(struct NES* nes, u32 addr, u32 val, u32 has_effect)
{
    struct r2A03* this = &nes->cpu;
    u32 r;

    switch(addr) {
        case 0x4016: // JOYSER0
            r = NES_controllerport_data(&this->controller_port1);
            return r;
        case 0x4017: // JOYSER1
            r = NES_controllerport_data(&this->controller_port2);
            return r;
    }
    return NES_APU_read_IO(&nes->apu, addr, val, has_effect);
    //return this->bus.apu.read_IO(addr, val, has_effect);  TODO
    return val;
}

void NES_bus_CPU_write_reg(struct NES* nes, u32 addr, u32 val)
{
    struct r2A03* this = &nes->cpu;
    switch(addr) {
        case 0x4014: //OAMDMA
            this->io.dma.addr = val << 8;
            this->io.dma.running = 1;
            this->io.dma.bytes_left = 256;
            this->io.dma.step = 0;
            return;
        case 0x4016: // JOYSER0
            NES_controllerport_latch(&this->controller_port1, val&1);
            return;
    }
    NES_APU_write_IO(&nes->apu, addr, val);
    //this->bus.apu.write_IO(addr, val);
}
