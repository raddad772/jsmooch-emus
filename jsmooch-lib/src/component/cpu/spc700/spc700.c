//
// Created by . on 4/16/25.
//
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "spc700.h"
#include "spc700_boot_rom.h"

#include "system/snes/snes_clock.h"

extern SPC700_ins_func spc700_decoded_opcodes[0x100];

void SPC700_init(struct SPC700 *this, u64 *clock_ptr)
{
    memset(this, 0, sizeof(*this));
    this->clock = clock_ptr;
}

void SPC700_reset(struct SPC700 *this)
{
    this->io.ROM_readable = 1;
    for (u32 i = 0; i < 3; i++) {
        this->timers[i].out = 0xF;
    }

    this->regs.PC = SPC700_boot_rom[62] + (SPC700_boot_rom[63] << 8);
    this->regs.IR = SPC700_boot_rom[this->regs.PC - 0xFFC0];
    this->regs.PC++;

    this->regs.SP = 0xEF;
    this->regs.P.v = 2;
}


static SPC700_ins_func get_decoded_opcode(u32 opcode)
{
    return spc700_decoded_opcodes[opcode];
}

static void advance_timers(struct SPC700 *this, i32 cycles)
{
    this->timers[0].divider += cycles;
    this->timers[1].divider += cycles;
    this->timers[2].divider += cycles;

    if (!this->io.T0_enable)
        this->timers[0].divider &= 127;
    else {
        while(this->timers[0].divider >= 128) {
            this->timers[0].counter = (this->timers[0].counter + 1) & 255;
            if (this->timers[0].counter == this->timers[0].target) {
                this->timers[0].counter = 0;
                this->timers[0].out = (this->timers[0].out + 1) & 15;
            }
            this->timers[0].divider -= 128;
        }
    }

    if (!this->io.T1_enable)
        this->timers[1].divider &= 127;
    else {
        while(this->timers[1].divider >= 128) {
            this->timers[1].counter = (this->timers[1].counter + 1) & 255;
            if (this->timers[1].counter == this->timers[1].target) {
                this->timers[1].counter = 0;
                this->timers[1].out = (this->timers[1].out + 1) & 15;
            }
            this->timers[1].divider -= 128;
        }
    }

    if (!this->io.T2_enable)
        this->timers[2].divider &= 15;
    else {
        while(this->timers[2].divider >= 16) {
            this->timers[2].counter = (this->timers[2].counter + 1) & 255;
            if (this->timers[2].counter == this->timers[2].target) {
                this->timers[2].counter = 0;
                this->timers[2].out = (this->timers[2].out + 1) & 15;
            }
        }
    }
}

void SPC700_cycle(struct SPC700 *this, i64 how_many) {
    this->cycles += how_many;
    while(this->cycles > 0) {
        this->regs.opc_cycles = 0;
        if (this->STP || this->WAI) {
            static int a = 1;
            if (a) {
                a = 0;
                printf("\nWARN STP OR WAI ON SPC700");
            }
            return;
        }

        SPC700_ins_func fptr = get_decoded_opcode(this->regs.IR);
        fptr(this);
        (*this->clock) += this->regs.opc_cycles;
        this->cycles -= this->regs.opc_cycles;
        advance_timers(this, this->regs.opc_cycles);
    }
}

void SPC700_sync_to(struct SPC700 *this, i64 to_what)
{
    /*long double apu_clock = (long double)to_what * this->ratio;
    i64 cycles = (i64)floorl((apu_clock - this->clock->apu.has) / 24.0) + 1;
    if (cycles < 1) return;
    SPC700_cycle(this, cycles);*/
}

static u8 DSP_read_reg(struct SPC700 *this, u32 addr)
{
    return this->dsp_regs[addr & 0x7F];
}

static void DSP_write_reg(struct SPC700 *this, u32 addr, u32 val)
{
    this->dsp_regs[addr & 0x7F] = val;
}

static u8 readIO(struct SPC700 *this, u32 addr)
{
    u32 val;
    switch(addr) {
        case 0xF0: // TEST register we do not emulate
            return 0x0A;
        case 0xF1: // CONTROL - I/O and timer control
            val = this->io.ROM_readable << 7;
            val += 0x30;
            val += this->io.T2_enable << 2;
            val += this->io.T1_enable << 1;
            val += this->io.T0_enable;
            return val;
        case 0xF2:
            return this->io.DSPADDR;
        case 0xF3:
            return DSP_read_reg(this, this->io.DSPADDR);
        case 0xF4:
            return this->io.CPUI[0];
        case 0xF5:
            return this->io.CPUI[1];
        case 0xF6:
            return this->io.CPUI[2];
        case 0xF7:
            return this->io.CPUI[3];
        case 0xF8:
        case 0xF9:
            return this->RAM[addr];
        case 0xFA: // Read-only
        case 0xFB:
        case 0xFC:
            return 0;
        case 0xFD:
            val = this->timers[0].out;
            this->timers[0].out = 0;
            return val;
        case 0xFE:
            val = this->timers[1].out;
            this->timers[1].out = 0;
            return val;
        case 0xFF:
            val = this->timers[2].out;
            this->timers[2].out = 0;
            return val;
        default:
            printf("\nUNEMULATED SPC REG %04x", addr);
            return 0;
    }
}

static void writeIO(struct SPC700 *this, u32 addr, u32 val)
{
    switch(addr) {
        case 0xF0: // TEST register, should not be written
            if (val != 0x0A) {
                printf("\nWARN SPC700 WRITE REG 0xF0 %02x", val);
            }
            return;
        case 0xF1: // CONTROL reg
            this->io.ROM_readable = (val >> 7) & 1;
            if (val & 0x20) this->io.CPUI[2] = this->io.CPUI[3] = 0;
            if (val & 0x10) this->io.CPUI[0] = this->io.CPUI[1] = 0;
            this->io.T2_enable = (val >> 2) & 1;
            this->io.T1_enable = (val >> 1) & 1;
            this->io.T0_enable = val & 1;
            return;
        case 0xF2:
            this->io.DSPADDR = val;
            return;
        case 0xF3:
            DSP_write_reg(this, this->io.DSPADDR, val);
            return;
        case 0xF4:
            this->io.CPUO[0] = val;
            return;
        case 0xF5:
            this->io.CPUO[1] = val;
            return;
        case 0xF6:
            this->io.CPUO[2] = val;
            return;
        case 0xF7:
            this->io.CPUO[3] = val;
            return;
        case 0xF8:
        case 0xF9:
            this->RAM[addr] = val;
            return;
        case 0xFA:
            this->timers[0].target = val;
            return;
        case 0xFB:
            this->timers[1].target = val;
            return;
        case 0xFC:
            this->timers[2].target = val;
            return;
        case 0xFD:
        case 0xFE:
        case 0xFF:
            // Read-only
            return;
        
    }
}

u8 SPC700_read8(struct SPC700 *this, u32 addr)
{
    if ((addr >= 0x00F1) && (addr <= 0x00FF)) return readIO(this, addr);
    if ((addr >= 0xFFC0) && this->io.ROM_readable) return SPC700_boot_rom[addr - 0xFFC0];
    return this->RAM[addr & 0xFFFF];
}

u8 SPC700_read8D(struct SPC700 *this, u32 addr)
{
    return SPC700_read8(this, (addr & 0xFF) + this->regs.P.DO);
}

void SPC700_write8(struct SPC700 *this, u32 addr, u32 val)
{
    if ((addr >= 0x00F1) && (addr <= 0x00FF))
        writeIO(this, addr, val);
    this->RAM[addr & 0xFFFF] = val;
}

void SPC700_write8D(struct SPC700 *this, u32 addr, u32 val)
{
    SPC700_write8(this, addr, val);
}
