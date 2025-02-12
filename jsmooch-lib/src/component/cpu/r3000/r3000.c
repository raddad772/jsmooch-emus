//
// Created by . on 2/11/25.
//
#include <stdlib.h>
#include <string.h>

#include "r3000_instructions.h"
#include "r3000.h"

static struct R3000_pipeline_item *pipe_push(struct R3000_pipeline *this)
{
    if (this->num_items == 2) return &this->empty_item;
    this->num_items++;
    switch(this->num_items) {
        case 1:
            return &this->item0;
        case 2:
            return &this->item1;
        default:
            NOGOHERE;
    }
}

static void pipe_item_clear(struct R3000_pipeline_item *this)
{
    this->target = -1;
    this->value = this->new_PC = 0;
}

static void pipe_clear(struct R3000_pipeline *this)
{
    pipe_item_clear(&this->item0);
    pipe_item_clear(&this->item1);
    pipe_item_clear(&this->current);
    this->num_items = 0;
}

struct R3000_pipeline_item *R3000_pipe_move_forward(struct R3000_pipeline *this)
{
    if (this->num_items == 0) return &this->empty_item;
    this->current = this->item0;
    this->item0 = this->item1;

    pipe_item_clear(&this->item1);
    this->num_items--;

    return &this->current;
}

static void do_decode_table(struct R3000 *this) {
    for (u32 op1 = 0; op1 < 0x3F; op1++) {
        struct R3000_opcode *mo = &this->decode_table[op1];
        R3000_ins o = &R3000_fNA;
        enum R3000_MN m = R3000_MN_NA;
        i32 a = 0;
        switch (op1) {
            case 0: {// SPECIAL
                for (u32 op2 = 0; op2 < 0x3F; op2++) {
                    a = 0;
                    switch (op2) {
                        case 0: // SLL
                            m = R3000_MN_J;
                            o = &R3000_fSLL;
                            break;
                        case 0x02: // SRL
                            m = R3000_MN_SRL;
                            o = &R3000_fSRL;
                            break;
                        case 0x03: // SRA
                            m = R3000_MN_SRA;
                            o = &R3000_fSRA;
                            break;
                        case 0x4: // SLLV
                            m = R3000_MN_SLLV;
                            o = &R3000_fSLLV;
                            break;
                        case 0x06: // SRLV
                            m = R3000_MN_SRLV;
                            o = &R3000_fSRLV;
                            break;
                        case 0x07: // SRAV
                            m = R3000_MN_SRAV;
                            o = &R3000_fSRAV;
                            break;
                        case 0x08: // JR
                            m = R3000_MN_JR;
                            o = &R3000_fJR;
                            break;
                        case 0x09: // JALR
                            m = R3000_MN_JALR;
                            o = &R3000_fJALR;
                            break;
                        case 0x0C: // SYSCALL
                            m = R3000_MN_SYSCALL;
                            o = &R3000_fSYSCALL;
                            break;
                        case 0x0D: // BREAK
                            m = R3000_MN_BREAK;
                            o = &R3000_fBREAK;
                            break;
                        case 0x10: // MFHI
                            m = R3000_MN_MFHI;
                            o = &R3000_fMFHI;
                            break;
                        case 0x11: // MTHI
                            m = R3000_MN_MTHI;
                            o = &R3000_fMTHI;
                            break;
                        case 0x12: // MFLO
                            m = R3000_MN_MFLO;
                            o = &R3000_fMFLO;
                            break;
                        case 0x13: // MTLO
                            m = R3000_MN_MTLO;
                            o = &R3000_fMTLO;
                            break;
                        case 0x18: // MULT
                            m = R3000_MN_MULT;
                            o = &R3000_fMULT;
                            break;
                        case 0x19: // MULTU
                            m = R3000_MN_MULTU;
                            o = &R3000_fMULTU;
                            break;
                        case 0x1A: // DIV
                            m = R3000_MN_DIV;
                            o = &R3000_fDIV;
                            break;
                        case 0x1B: // DIVU
                            m = R3000_MN_DIVU;
                            o = &R3000_fDIVU;
                            break;
                        case 0x20: // ADD
                            m = R3000_MN_ADD;
                            o = &R3000_fADD;
                            break;
                        case 0x21: // ADDU
                            m = R3000_MN_ADDU;
                            o = &R3000_fADDU;
                            break;
                        case 0x22: // SUB
                            m = R3000_MN_SUB;
                            o = &R3000_fSUB;
                            break;
                        case 0x23: // SUBU
                            m = R3000_MN_SUBU;
                            o = &R3000_fSUBU;
                            break;
                        case 0x24: // AND
                            m = R3000_MN_AND;
                            o = &R3000_fAND;
                            break;
                        case 0x25: // OR
                            m = R3000_MN_OR;
                            o = &R3000_fOR;
                            break;
                        case 0x26: // XOR
                            m = R3000_MN_XOR;
                            o = &R3000_fXOR;
                            break;
                        case 0x27: // NOR
                            m = R3000_MN_NOR;
                            o = &R3000_fNOR;
                            break;
                        case 0x2A: // SLT
                            m = R3000_MN_SLT;
                            o = &R3000_fSLT;
                            break;
                        case 0x2B: // SLTU
                            m = R3000_MN_SLTU;
                            o = &R3000_fSLTU;
                            break;
                        default:
                            m = R3000_MN_NA;
                            o = &R3000_fNA;
                            break;
                    }
                    mo = &this->decode_table[op2 + 0x3F];
                    mo->func = o;
                    mo->opcode = op2;
                    mo->mn = m;
                    mo->arg = a;
                }
                continue;
            }
            case 0x01: // BcondZ
                m = R3000_MN_BcondZ;
                o = &R3000_fBcondZ;
                break;
            case 0x02: // J
                m = R3000_MN_J;
                o = &R3000_fJ;
                break;
            case 0x03: // JAL
                m = R3000_MN_JAL;
                o = &R3000_fJAL;
                break;
            case 0x04: // BEQ
                m = R3000_MN_BEQ;
                o = &R3000_fBEQ;
                break;
            case 0x05: // BNE
                m = R3000_MN_BNE;
                o = &R3000_fBNE;
                break;
            case 0x06: // BLEZ
                m = R3000_MN_BLEZ;
                o = &R3000_fBLEZ;
                break;
            case 0x07: // BGTZ
                m = R3000_MN_BGTZ;
                o = &R3000_fBGTZ;
                break;
            case 0x08: // ADDI
                m = R3000_MN_ADDI;
                o = &R3000_fADDI;
                break;
            case 0x09: // ADDIU
                m = R3000_MN_ADDIU;
                o = &R3000_fADDIU;
                break;
            case 0x0A: // SLTI
                m = R3000_MN_SLTI;
                o = &R3000_fSLTI;
                break;
            case 0x0B: // SLTIU
                m = R3000_MN_SLTIU;
                o = &R3000_fSLTIU;
                break;
            case 0x0C: // ANDI
                m = R3000_MN_ANDI;
                o = &R3000_fANDI;
                break;
            case 0x0D: // ORI
                m = R3000_MN_ORI;
                o = &R3000_fORI;
                break;
            case 0x0E: // XORI
                m = R3000_MN_XORI;
                o = &R3000_fXORI;
                break;
            case 0x0F: // LUI
                m = R3000_MN_LUI;
                o = &R3000_fLUI;
                break;
            case 0x13: // COP3
            case 0x12: // COP2
            case 0x11: // COP1
            case 0x10: // COP0
                m = R3000_MN_COPx;
                o = &R3000_fCOP;
                a = (op1 - 0x10);
                break;
            case 0x20: // LB
                m = R3000_MN_LB;
                o = &R3000_fLB;
                break;
            case 0x21: // LH
                m = R3000_MN_LH;
                o = &R3000_fLH;
                break;
            case 0x22: // LWL
                m = R3000_MN_LWL;
                o = &R3000_fLWL;
                break;
            case 0x23: // LW
                m = R3000_MN_LW;
                o = &R3000_fLW;
                break;
            case 0x24: // LBU
                m = R3000_MN_LBU;
                o = &R3000_fLBU;
                break;
            case 0x25: // LHU
                m = R3000_MN_LHU;
                o = &R3000_fLHU;
                break;
            case 0x26: // LWR
                m = R3000_MN_LWR;
                o = &R3000_fLWR;
                break;
            case 0x28: // SB
                m = R3000_MN_SB;
                o = &R3000_fSB;
                break;
            case 0x29: // SH
                m = R3000_MN_SH;
                o = &R3000_fSH;
                break;
            case 0x2A: // SWL
                m = R3000_MN_SWL;
                o = &R3000_fSWL;
                break;
            case 0x2B: // SW
                m = R3000_MN_SW;
                o = &R3000_fSW;
                break;
            case 0x2E: // SWR
                m = R3000_MN_SWR;
                o = &R3000_fSWR;
                break;
            case 0x33: // LWC3
            case 0x32: // LWC2
            case 0x31: // LWC1
            case 0x30: // LWC0
                m = R3000_MN_LWCx;
                o = &R3000_fLWC;
                a = op1 - 0x30;
                break;
            case 0x3B: // SWC3
            case 0x3A: // SWC2
            case 0x39: // SWC1
            case 0x38: // SWC0
                m = R3000_MN_SWCx;
                o = &R3000_fSWC;
                a = op1 - 0x38;
                break;
        }
        mo->opcode = op1;
        mo->mn = m;
        mo->func = o;
        mo->arg = a;
    }
}

void R3000_init(struct R3000 *this, u64 *master_clock, u32 *waitstates)
{
    memset(this, 0, sizeof(*this));
    this->clock = master_clock;
    this->waitstates = waitstates;

    do_decode_table(this);

    pipe_clear(&this->pipe);

    jsm_string_init(&this->console, 200);
    jsm_string_init(&this->trace.str, 100);

    GTE_init(&this->gte);
}

void R3000_delete(struct R3000 *this)
{
    jsm_string_delete(&this->console);
    jsm_string_delete(&this->trace.str);
}

