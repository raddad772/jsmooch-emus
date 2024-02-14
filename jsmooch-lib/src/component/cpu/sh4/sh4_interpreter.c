//
// Created by Dave on 2/10/2024.
//

#include "stdio.h"
#include "sh4_interpreter.h"
#include "sh4_interpreter_opcodes.h"

#define NMI_VEC VBR + 0x0600

// Endianness is little.

static void set_user_mode(struct SH4* this)
{

}

static void set_privileged_mode(struct SH4* this)
{

}

void SH4_regs_SR_reset(struct SH4_regs_SR* this)
{

}

u32 SH4_regs_SR_get(struct SH4_regs_SR* this)
{
    return (this->data & 0b10001111111111110111110000001100) | (this->MD << 30) | (this->RB << 29) | (this->BL << 28) | (this->FD << 15) |
            (this->M << 9) | (this->Q << 8) | (this->IMASK << 4) | (this->S << 1) | (this->T);
}

void SH4_regs_FPSCR_reset(struct SH4_regs_FPSCR* this)
{

}

u32 SH4_regs_FPSCR_get(struct SH4_regs_FPSCR* this)
{
    return (this->data & 0b11111111110000000000000000000000) | (this->FR << 21) | (this->SZ << 20) | (this->PR << 19) |
            (this->DN << 18) | (this->Cause << 12) | (this->Enable << 7) | (this->Flag << 2) | (this->RM);
}

void SH4_regs_FPSCR_set(struct SH4_regs_FPSCR* this, u32 val)
{
    this->data = val;
    this->FR = (val >> 21) & 1;
    this->SZ = (val >> 20) & 1;
    this->PR = (val >> 19) & 1;
    this->DN = (val >> 18) & 63;
    this->Cause = (val >> 12) & 63;
    this->Enable = (val >> 7) & 63;
    this->Flag = (val >> 2) & 63;
    this->RM = val & 3;
}

static void SH4_pprint(struct SH4* this, struct SH4_ins_t *ins)
{
    u32 had_any = 0;
    if (ins->Rn != -1) {
        printf("R%d:%08x", ins->Rn, this->regs.R[ins->Rn]);
    }
    if (ins->Rm != -1) {
        if (had_any) printf(" ");
        printf(" R%d:%08x", ins->Rm, this->regs.R[ins->Rm]);
    }
}

void SH4_run_cycles(struct SH4* this, u32 howmany) {
    // fetch
    i64 addr;
    for (u32 i = 0; i < howmany; i++) {
        if (this->delay_slot > -1) addr = this->delay_slot;
        else addr = this->regs.PC;
        this->delay_slot = -1;
        u32 opcode = this->read16(this->mptr, addr);
        struct SH4_ins_t *ins = &SH4_decoded[opcode];
        printf("\naddr:%08x opcode:%04x %s   ", (u32)addr, opcode, SH4_disassembled[opcode]);
        SH4_pprint(this, ins);

        ins->exec(this, ins);
    }
}

void SH4_init(struct SH4* this)
{
    this->regs.currently_banked_rb = 1;
    this->last_md = 1;
    this->last_rb = 1;
    this->delay_slot = -1;
    SH4_reset(this);

    this->mptr = NULL;
    this->read8 = NULL;
    this->read16 = NULL;
    this->read32 = NULL;
    this->write8 = NULL;
    this->write16 = NULL;
    this->write32 = NULL;
}

static void swap_register_banks(struct SH4* this)
{
    for (u32 i = 0; i < 8; i++) {
        u32 t = this->regs.R[i];
        this->regs.R[i] = this->regs.R_[i];
        this->regs.R_[i] = t;
    }
}

void SH4_update_mode(struct SH4* this)
{
    // If we've switched into or out of privileged
    u32 should_be = 0;
    if (!this->regs.SR.MD) should_be = 0;
    else should_be = this->regs.SR.RB;
    if (should_be != this->regs.currently_banked_rb) {
        this->regs.currently_banked_rb = should_be;
        swap_register_banks(this);
    }
}

void SH4_SR_set(struct SH4* this, u32 val)
{
    this->regs.SR.data = val;
    this->regs.SR.MD = (val >> 30) & 1;
    this->regs.SR.RB = (val >> 29) & 1;
    this->regs.SR.BL = (val >> 28) & 1;
    this->regs.SR.FD = (val >> 15) & 1;
    this->regs.SR.M = (val >> 9) & 1;
    this->regs.SR.Q = (val >> 8) & 1;
    this->regs.SR.IMASK = (val >> 4) & 15;
    this->regs.SR.S = (val >> 1) & 1;
    this->regs.SR.T = val & 1;
    SH4_update_mode(this);
}

void SH4_reset(struct SH4* this)
{
    /* for SR,
     * MD bit = 1, RB bit = 1, BL bit = 1, FD bit = 0,
I3ñI0 = 1111 (H'F), reserved bits = 0, others
undefined
     */
    this->regs.VBR = 0;
    this->regs.PC = 0xA0000000;
    SH4_regs_FPSCR_set(&this->regs.FPSCR, 0x00040001);
    SH4_SR_set(this, (SH4_regs_SR_get(&this->regs.SR) &  0b11110011) | 0b01110000000000000000000011110000);
}



/* syscalls - https://mc.pp.se/dc/syscalls.html
 * Crazy Taxi
 * instructions - http://shared-ptr.com/sh_insns.html
 * originaldave_ — Today at 6:28 PM
how do you HLE the functions, is there documentation available?
[6:28 PM]
did you disassemble them and figure them out
[6:28 PM]
I kinda have this idea, that Dreamcast is, with the exception of a few emulators, not very well-documente

Senryoku — Today at 6:30 PM
Basically the bios loads two files "IP.bin" at 0x8000 and "1STREAD.BIN" (this one can have different names depending on the game) at 0x10000 and then jumps to, uuh, 0x8300 I think? I'd have to check that
[6:30 PM]
So I do that manually


 */