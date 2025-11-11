//
// Created by . on 1/18/25.
//

#include "sh2_interpreter.h"

void SH2_SR_set(struct SH2_regs_SR *this, u32 val)
{
    this->data = val;
    this->M = (val >> 9) & 1;
    this->Q = (val >> 8) & 1;
    this->IMASK = (val >> 4) & 15;
    this->S = (val >> 1) & 1;
    this->T = val & 1;
}

u32 SH2_SR_get(struct SH2_regs_SR *this)
{
    return (this->M << 9) | (this->Q << 8) | (this->IMASK << 4) | (this->S << 1) | (this->T);
}

void SH2_init(struct SH2 *this, scheduler_t *scheduler)
{
    this->trace.my_cycles = 0;
    this->trace.ok = 0;
    this->trace.cycles = &this->trace.my_cycles;
    //this->clock.timer_cycles = 0;
    //this->clock.trace_cycles_blocks = 0;
    this->scheduler = scheduler;
    //this->pp_last_m = this->pp_last_n = -1;
    //do_sh2_decode();
    //jsm_string_init(&this->console, 5000);
    //SH2_init_interrupt_struct(this->interrupt_sources, this->interrupt_map);
    this->interrupt_highest_priority = 0;
    //printf("\nINS! %s\n", SH4_disassembled[0][0x6772]);

    this->fetch_ptr = NULL;
    this->read_ptr = NULL;
    this->write_ptr = NULL;
    this->fetch_ins = NULL;
    this->read = NULL;
    this->write = NULL;
}

void SH2_reset(struct SH2 *this)
{
    this->regs.VBR = 0;
    // Get PC from global table
    this->regs.PC = 0xA0000000;
    // Get R15 from global table

    SH2_SR_set(&this->regs.SR, (SH2_SR_get(&this->regs.SR) & 0b11110011) | 0b00000000000000000000000011110000);


}
