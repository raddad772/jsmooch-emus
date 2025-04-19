//
// Created by . on 4/16/25.
//
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "spc700.h"
#include "spc700_boot_rom.h"

#include "system/snes/snes_clock.h"

extern SPC700_ins_func SPC700_decoded_opcodes[0x100];

void SPC700_init(struct SPC700 *this, struct SNES_clock *clock)
{
    memset(this, 0, sizeof(*this));
    this->clock = clock;
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


SPC700_ins_func get_decoded_opcode(u32 opcode)
{
    return SPC700_decoded_opcodes[opcode];
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
        this->clock->apu.has += this->regs.opc_cycles;
        this->cycles -= this->regs.opc_cycles;
        advance_timers(this, this->regs.opc_cycles);
    }
}

void SPC700_sync_to(struct SPC700 *this, i64 to_what)
{
    long double apu_clock = (long double)to_what * this->clock->apu.ratio;
    i64 cycles = (i64)floorl((apu_clock - this->clock->apu.has) / 24.0) + 1;
    if (cycles < 1) return;
    SPC700_cycle(this, cycles);
}