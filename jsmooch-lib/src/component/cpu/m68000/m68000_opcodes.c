//
// Created by . on 5/15/24.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "m68000_opcodes.h"

struct M68k_ins_t M68k_decoded[65536];
char M68k_disassembled[65536][30];
char M68k_mnemonic[65536][30];

static void start_read16(struct M68k* this, u32 addr)
{
    this->state = M68kS_read16;
    this->bus_cycle.TCU = 0;
    this->bus_cycle.addr = addr;
}


#define ADDRAND1 { this->pins.LDS = (this->bus_cycle.addr & 1); this->pins.UDS = this->pins.LDS ^ 1; }
#define M68KINS(x) static void M68K_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    switch(this->instruction.TCU) {

#define STEP0 case 0: {\
            this->instruction.done = 0;

#define STEP(x) break; }\
                case x: {

#define INS_END \
            this->instruction.done = 1; \
            break; }\
    }\
    this->instruction.TCU++;            \
}

M68KINS(RESET)
        // $0 (32-bit), indirect, -> SSP
        // $4 (32-bit) -> PC
        // SR->interrupt level = 7
        STEP0
        this->instruction.data32 = 0;
        start_read16(this, 0);
        STEP(1)
        // First word will be
        this->instruction.data32 = this->instruction.data << 16;
        start_read16(this, 2);
        STEP(2)
        this->regs.SSP = this->instruction.data32 | this->instruction.data;
        this->instruction.data32 = 0;
        start_read16(this, 4);
        STEP(3)
        this->instruction.data32 = this->instruction.data << 16;
        start_read16(this, 6);
        STEP(4)
        this->regs.PC = this->instruction.data32 | this->instruction.data;
        this->regs.SR.I = 7;
        // Start filling prefetch queue
        start_read16(this, this->regs.PC);
        this->regs.PC += 2;
        STEP(5)
        this->regs.IRC = this->instruction.data;
        this->regs.IR = this->regs.IRC;
        start_read16(this, this->regs.PC);
        this->regs.PC += 2;
        STEP(6)
        this->regs.IRC = this->instruction.data;
        this->regs.IRD = this->regs.IR;
INS_END

// IRD holds currently-executing instruction,
// IR holds currently-decoding instruction,
// IRC holds last pre-fetched word


M68KINS(TAS)
        STEP0
            // TODO: set FCs
            this->pins.RW = 0;
            this->instruction.done = 0;
        STEP(1)
            this->pins.Addr = this->instruction.addr & 0xFFFFFE;
        STEP(2)
            this->pins.AS = 1;
            ADDRAND1;
        STEP(4)
            if (!this->pins.DTACK) this->instruction.TCU--;
        STEP(7)
            this->pins.UDS = this->pins.LDS = 0;
            if (this->state == M68kS_read8)
                this->instruction.data = this->pins.D[this->instruction.addr & 1];
            else
                this->instruction.data = ((this->pins.D[0] << 8) | (this->pins.D[1] & 0xFF)) & 0xFFFF;
            this->pins.DTACK = 0;
        STEP(8)
            // TODO ALU thing
        STEP(14)
            this->pins.RW = 1;
        STEP(15)
            this->pins.D[this->instruction.addr & 1] = this->instruction.data;
        STEP(16)
            ADDRAND1;
        STEP(19)
            this->pins.AS = 0;
            this->pins.UDS = this->pins.LDS = 0;
            this->pins.RW = 0;
            this->pins.DTACK = 0;
            this->instruction.done = 1;
INS_END

#define BADINS(x) static void M68K_ins_##x(struct M68k* this, struct M68k_ins_t *ins) { \
    printf("\nUnimplemented instruction %04x", ins->opcode);\
}


static u32 M68K_disasm_BAD(struct M68k* this, struct M68k_ins_t *ins, char *ptr, u32 max_len)
{
    printf("\nERROR UNIMPLEMENTED DISASSEMBLY %04x", ins->opcode);
    return 0;
}


static void M68K_ins_NOINS(struct M68k* this, struct M68k_ins_t *ins)
{
    printf("\nERROR UNIMPLEMENTED M68K INSTRUCTION %04x at PC %06x", ins->opcode, this->regs.PC);
}

#undef M68KINS
#undef INS_END
#undef STEP0
#undef STEP

static int M68k_already_decoded = 0;

#define MF(x) &M68K_ins_##x
#define MD(x) &M68K_disasm_##x

struct m68k_str_ret
{
    u32 t_max, s_max, ea_mode, ea_reg, size_max, rm;

};

static void bind_opcode(const char* inpt, u32 sz, M68k_ins_func exec_func, M68k_disassemble_t disasm_func, u32 op_or, struct M68k_EA *ea1, struct M68k_EA *ea2)
{
    const char *ptr;
    u32 orer = 0x8000;
    u32 out = 0;

    for (ptr = inpt; orer != 0; ptr++) {
        if (*ptr == ' ') continue;
        if (*ptr == '1') out |= orer;
        orer >>= 1;
    }

    out |= op_or;

    assert(out < 65536); assert(out >= 0);

    struct M68k_ins_t *ins = &M68k_decoded[out];
    assert(ins->disasm == MD(BAD));
    *ins = (struct M68k_ins_t) {
        .opcode = out,
        .disasm = disasm_func,
        .exec = exec_func,
        .sz = sz
    };
    if (ea1)
        memcpy(&ins->ea1, ea1, sizeof(struct M68k_EA));
    if (ea2)
        memcpy(&ins->ea2, ea2, sizeof(struct M68k_EA));
}

static struct M68k_EA immediate(u32 n) {
    return (struct M68k_EA){.kind = M68k_AM_immediate, .ea_register = n };
}

static struct M68k_EA data_reg_direct(u32 n)
{
    return (struct M68k_EA){.kind = M68k_AM_data_register_direct, .ea_register = n};
}

static struct M68k_EA addr_reg_direct(u32 n)
{
    return (struct M68k_EA){.kind = M68k_AM_address_register_direct, .ea_register = n};
}


static struct M68k_EA addr_reg_indirect_with_predecrement(u32 n)
{
    return (struct M68k_EA){.kind = M68k_AM_address_register_indirect_with_predecrement, .ea_register = n};
}

static struct M68k_EA mk_ea(u32 mode, u32 reg) {
    return (struct M68k_EA) {.kind=mode, .ea_register=reg};
}

void do_M68k_decode() {
    if (M68k_already_decoded) return;
    M68k_already_decoded = 1;
    for (u32 i = 0; i < 65536; i++) {
        M68k_decoded[i] = (struct M68k_ins_t) {
                .opcode = i,
                .exec = MF(NOINS),
                .disasm = MD(BAD)
        };
        M68k_disassembled[i][0] = 0;
        M68k_mnemonic[i][0] = 0;
    }

// &M68k_disasm_##func
#define OE(opcstr, func) bind_opcode(opcstr, 0, &M68K_ins_##func, &M68K_disasm_BAD, 0, NULL, NULL, NULL)
#define OEo(opcstr, func) bind_opcode(opcstr, opcode_or, &M68K_ins_##func, &M68K_disasm_BAD, 0, NULL, NULL, NULL)
#define OE_ea(opcstr, func, sz, orval, ea1) bind_opcode(opcstr, sz, &M68K_ins_##func, &M68K_disasm_BAD, orval, &ea1, NULL, NULL)
#define OE_ea_ea(opcstr, func, sz, orval, ea1, ea2) bind_opcode(opcstr, sz, &M68K_ins_##func, &M68K_disasm_BAD, orval, &ea1, &ea2, NULL)
#define l0_7(x) for (x = 0; x < 8; x++)
struct M68k_EA op1, op2;

#define loop2(x, y) for (u32 x = 0; x < 8; x++) {\
for (u32 y = 0; y < 8; y++)
#define loop3(x, y, z) for (u32 x = 0; x < 8; x++) {\
for (u32 y = 0; y < 8; y++) {\
for (u32 z = 0; z < 8; z++)
#define OPOR(x) u32 opcode_or = (x)

    loop2(s, t) {
            struct M68k_EA o1 = data_reg_direct(s);
            struct M68k_EA o2 = data_reg_direct(t);
            struct M68k_EA o3 = addr_reg_indirect_with_predecrement(s);
            struct M68k_EA o4 = addr_reg_indirect_with_predecrement(t);
            OE_ea_ea("1100...100000...", ABCD, 0, (t << 9) | s, o1, o2);
            OE_ea_ea("1100...100001...", ABCD, 0, (t << 9) | s, o3, o4);
        }}

    loop3(mode, dreg, reg) {
                if(mode == 7 && reg >= 5) continue;
                op1 = mk_ea(mode, reg);
                op2 = data_reg_direct(dreg);
                OPOR((dreg << 9) | (mode << 3) | reg);
                OE_ea_ea("1101...000......", ADD, 1, opcode_or, op1, op2);
                OE_ea_ea("1101...001......", ADD, 2, opcode_or, op1, op2);
                OE_ea_ea("1101...010......", ADD, 4, opcode_or, op1, op2);

                if ((mode == 7 && reg >= 2) || (mode <= 1)) continue;
                op1 = data_reg_direct(dreg);
                op2 = mk_ea(mode, reg);
                OE_ea_ea("1101...000......", ADD, 1, opcode_or, op1, op2);
                OE_ea_ea("1101...001......", ADD, 2, opcode_or, op1, op2);
                OE_ea_ea("1101...010......", ADD, 4, opcode_or, op1, op2);
    }}}

    loop3(areg, mode, reg) {
                if(mode == 7 && reg >= 5) continue;
                op2 = addr_reg_direct(areg);
                op1 = mk_ea(mode, reg);

                OE_ea_ea("1101...011......", ADDA, 2, (areg << 9) | (mode << 3) | reg, op1, op2);
                OE_ea_ea("1101...111......", ADDA, 4, (areg << 9) | (mode << 3) | reg, op1, op2);
    }}}

    loop2(mode, reg) {
            if(mode == 1 || (mode == 7 && reg >= 2)) continue;

            op2 = mk_ea(mode, reg);
            OE_ea("0000011000......", ADDI, 1, (mode << 3) | reg, op2);
            OE_ea("0000011001......", ADDI, 2, (mode << 3) | reg, op2);
            OE_ea("0000011010......", ADDI, 4, (mode << 3) | reg, op2);
        }}

    loop3(data, mode, reg) {
                if(mode == 7 && reg >= 2) continue;
                OPOR(data << 9 | mode << 3 | reg << 0);
                struct M68k_EA imm = immediate(data ? data : 8);
                if (mode != 1) {
                    op2 = mk_ea(mode, reg);
                    OE_ea_ea("0101...000......", ADDQ, 1, opcode_or, imm, op2);
                    OE_ea_ea("0101...001......", ADDQ, 2, opcode_or, imm, op2);
                    OE_ea_ea("0101...010......", ADDQ, 4, opcode_or, imm, op2);
                }
                else {
                    op2 = addr_reg_direct(reg);
                    OE_ea_ea("0101...001......", ADDQ, 2, opcode_or, imm, op2);
                    OE_ea_ea("0101...010......", ADDQ, 4, opcode_or, imm, op2);
                }
    }}}

    loop2(xreg, yreg) {
            OPOR((xreg << 9) | yreg);

            op2 = data_reg_direct(xreg);
            op1 = data_reg_direct(yreg);
            OE_ea_ea("1101...100000...", ADDX, 1, opcode_or, op1, op2);
            OE_ea_ea("1101...101000...", ADDX, 2, opcode_or, op1, op2);
            OE_ea_ea("1101...110000...", ADDX, 4, opcode_or, op1, op2);

            op2 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, xreg);
            op1 = mk_ea(M68k_AM_address_register_indirect_with_predecrement, yreg);
            OE_ea_ea("1101...100001...", ADDX, 1, opcode_or, op1, op2);
            OE_ea_ea("1101...101001...", ADDX, 2, opcode_or, op1, op2);
            OE_ea_ea("1101...110001...", ADDX, 4, opcode_or, op1, op2);
        }}

    loop3(dreg, mode, reg) { // AND "1100 ---0 ++-- ----"
                if(mode == 1 || (mode == 7 && reg >= 5)) continue;
                op1 = mk_ea(mode, reg);
                op2 = data_reg_direct(dreg);
                OPOR((dreg << 9) | (mode << 3) | reg);

                OE_ea_ea("1100...000......", AND, 1, opcode_or, op1, op2);
                OE_ea_ea("1100...001......", AND, 2, opcode_or, op1, op2);
                OE_ea_ea("1100...010......", AND, 4, opcode_or, op1, op2);
            }}}

    loop3(dreg, mode, reg) {
                if(mode <= 1 || (mode == 7 && reg >= 2)) continue;
                op1 = data_reg_direct(dreg);
                op2 = mk_ea(mode, reg);
                OPOR((dreg << 9) | (mode << 3) | reg);

                OE_ea_ea("1100...100......", AND, 1, opcode_or, op1, op2);
                OE_ea_ea("1100...101......", AND, 2, opcode_or, op1, op2);
                OE_ea_ea("1100...110......", AND, 4, opcode_or, op1, op2);
            }}}

    loop2(mode, reg) {
            if(mode == 1 || (mode == 7 && reg >= 2)) continue;
            op2 = mk_ea(mode, reg);
            OPOR((mode << 3) | reg);
            OE_ea("0000 0010 00.. ....", ANDI, 1, opcode_or, op2);
            OE_ea("0000 0010 01.. ....", ANDI, 2, opcode_or, op2);
            OE_ea("0000 0010 10.. ....", ANDI, 4, opcode_or, op2);
        }}

    OE("0000 0010 0011 1100", ANDI_TO_CCR);
    OE("0000 0010 0111 1100", ANDI_TO_SR);

    loop2(data, dreg) {
            op1 = immediate(data ? data : 8);
            op2 = data_reg_direct(dreg);
        OPOR((data << 9) | dreg);
        OE_ea_ea("1110 ...1 0000 0...", ASL, 1, opcode_or, op1, op2);
        OE_ea_ea("1110 ...1 0100 0...", ASL, 2, opcode_or, op1, op2);
        OE_ea_ea("1110 ...1 1000 0...", ASL, 4, opcode_or, op1, op2);

        OE_ea_ea("1110 ...0 0000 0...", ASR, 1, opcode_or, op1, op2);
        OE_ea_ea("1110 ...0 0100 0...", ASR, 2, opcode_or, op1, op2);
        OE_ea_ea("1110 ...0 1000 0...", ASR, 4, opcode_or, op1, op2);
        }}

    loop2(sreg, dreg) {
            op1 = data_reg_direct(sreg);
            op2 = data_reg_direct(dreg);
        OPOR((sreg << 9) | dreg);
        OE_ea_ea("1110 ...1 0010 0...", ASL, 1, opcode_or, op1, op2);
        OE_ea_ea("1110 ...1 0110 0...", ASL, 2, opcode_or, op1, op2);
        OE_ea_ea("1110 ...1 1010 0...", ASL, 4, opcode_or, op1, op2);

        OE_ea_ea("1110 ...0 0010 0...", ASR, 1, opcode_or, op1, op2);
        OE_ea_ea("1110 ...0 0110 0...", ASR, 2, opcode_or, op1, op2);
        OE_ea_ea("1110 ...0 1010 0...", ASR, 4, opcode_or, op1, op2);
        }}

    loop2(mode, reg) {
            if ((mode <= 1) || ((mode == 7) && (reg >= 2))) continue;
            op2 = mk_ea(mode, reg);
            OPOR((mode << 3) | reg);
            OE_ea("1110 0001 11.. ....", ASL, 0, opcode_or, op2);

            OE_ea("1110 0000 11.. ....", ASR, 0, opcode_or, op2);
        }}

        for (u32 test = 2; test < 16; test++) {
        for (u32 displacement = 0; displacement < 256; displacement++) {
            OPOR((test << 8) | displacement);
            op1 = immediate(test);
            op2 = immediate(displacement);
            OE_ea_ea("0110 .... .... ....", BCC, 0, opcode_or, op1, op2);
        }
    }

    loop3(dreg, mode, reg) {
            if ((mode == 1) || (mode == 7 && reg >= 2)) continue;
                op1 = data_reg_direct(dreg);
                op2 = mk_ea(mode, reg);
            OPOR((dreg << 9) | (mode << 3) | reg);
            if (mode == 0) OE_ea_ea("0000 ...1 01.. ....", BCHG, 4, opcode_or, op1, op2);
            else           OE_ea_ea("0000 ...1 01.. ....", BCHG, 1, opcode_or, op1, op2);
            }}}

    loop2(mode, reg) {
        if ((mode == 1) || ((mode == 7) && (reg >= 2))) continue;

            op2 = mk_ea(mode, reg);
        OPOR((mode << 3) | reg);
        if (mode == 0) OE_ea("0000 1000 01.. ....", BCHG, 4, opcode_or, op2);
        else           OE_ea("0000 1000 01.. ....", BCHG, 1, opcode_or, op2);
        }}

    loop3(dreg, mode, reg) {
                if(mode == 1 || (mode == 7 && reg >= 2)) continue;
                op1 = data_reg_direct(dreg);
                op2 = mk_ea(mode, reg);
                OPOR((dreg << 9) | (mode << 3) | reg);
                if (mode == 0) OE_ea_ea("0000 ...1 11.. ....", BSET, 4, opcode_or, op1, op2);
                else           OE_ea_ea("0000 ...1 11.. ....", BSET, 1, opcode_or, op1, op2);
            }}}

    loop2(mode, reg) {
            if(mode == 1 || (mode == 7 && reg >= 2)) continue;
            OPOR((mode << 3) | reg);
            op2 = mk_ea(mode, reg);
            if (mode == 0) OE_ea("0000 1000 11.. ....", BSET, 4, opcode_or, op2);
            else           OE_ea("0000 1000 11.. ....", BSET, 1, opcode_or, op2);
        }}

    for (u32 displacement = 0; displacement < 256; displacement++) {
        OPOR(displacement);
        OEo("0110 0001 .... ....", BSR);
    }

    loop3(dreg, mode, reg) {
                if (mode == 1 || (mode == 7 && reg >= 5)) continue;
                op1 = data_reg_direct(dreg);
                op2 = mk_ea(mode, reg);
                OPOR((dreg << 9) | (mode << 3) | reg);

                OE_ea_ea("0000 ...1 00.. ....", BTST, mode == 0 ? 4 : 1, opcode_or, op1, op2);
            }}}
    loop2(mode, reg) {
        if (mode == 1 || (mode == 7 && reg >= 4)) continue;
        OPOR((mode << 3) | reg);
            op2 = mk_ea(mode, reg);
        OE_ea("0000 1000 00.. ....", BTST, mode == 0 ? 4 : 1, opcode_or, op2);
        }}

    loop3(dreg, mode, reg) {
        if (mode == 1 || (mode == 7 && reg >= 5)) continue;
        OPOR((dreg << 9) | (mode << 3) | reg);
                op2 = data_reg_direct(dreg);
                op1 = mk_ea(mode, reg);
        OE_ea_ea("0100 ...1 10.. ....", CHK, 4, opcode_or, op1, op2);
        }}}

    loop2(mode, reg) {
        if (mode == 1 || (mode == 7 && reg >= 2)) continue;
            op2 = mk_ea(mode, reg);
        OPOR((mode << 3) | reg);
        OE_ea("0100 0010 00.. ....", CLR, 1, opcode_or, op2);
        OE_ea("0100 0010 01.. ....", CLR, 2, opcode_or, op2);
        OE_ea("0100 0010 10.. ....", CLR, 4, opcode_or, op2);

        }}

    loop3(dreg, mode, reg) {
        if (mode == 7 && reg >= 5) continue;
        OPOR((dreg << 9) | (mode << 3) | reg);
                op2 = data_reg_direct(dreg);
                op1 = mk_ea(mode, reg);

        if (mode != 1)
            OE_ea_ea("1011 ...0 00.. ....", CMP, 1, opcode_or, op2, op1);
        OE_ea_ea("1011 ...0 01.. ....", CMP, 2, opcode_or, op2, op1);
        OE_ea_ea("1011 ...0 10.. ....", CMP, 4, opcode_or, op2, op1);

            }}}

    loop3(areg, mode, reg) {
        if (mode == 7 && reg >= 5) continue;
        OPOR((areg << 9) | (mode << 3) | reg);
                op2 = addr_reg_direct(areg);
                op1 = mk_ea(mode, reg);
        OE_ea_ea("1011 ...0 11.. ....", CMPA, 2, opcode_or, op1, op2);
        OE_ea_ea("1011 ...1 11.. ....", CMPA, 4, opcode_or, op1, op2);
            }}}

    loop2(mode, reg) {
            if (mode == 1 || (mode == 7 && reg >= 2)) continue;
            op2 = mk_ea(mode, reg);
            OPOR((mode << 3) | reg);
            OE_ea("0000 1100 00.. ....", CMPI, 1, opcode_or, op2);
            OE_ea("0000 1100 01.. ....", CMPI, 2, opcode_or, op2);
            OE_ea("0000 1100 10.. ....", CMPI, 4, opcode_or, op2);
        }}

    loop2(xreg, yreg) {
            op1 = mk_ea(M68k_AM_address_register_indirect_with_postincrement, yreg);
            op2 = mk_ea(M68k_AM_address_register_indirect_with_postincrement, xreg);
            OPOR((xreg << 9) | yreg);
            OE_ea_ea("1011 ...1 0000 1...", CMPM, 1, opcode_or, op1, op2);
            OE_ea_ea("1011 ...1 0100 1...", CMPM, 2, opcode_or, op1, op2);
            OE_ea_ea("1011 ...1 1000 1...", CMPM, 4, opcode_or, op1, op2);
        }}



        /*
    DBcc
    DIVS
    DIVU
    EOR
    EORI
    EORI to CCR
    EORI to SR
    EXG
    EXT
    ILLEGAL
    JMP
    JSR
    LEA
    LINK
    LSL
    LSR
    MOVE
    MOVEA
    MOVE to CCR
    MOVE op1 SR? <-- not privileged
    MOVE to SR
    MOVE USP
    MOVEM
    MOVEP
    MOVEQ
    MULS
    MULU
    NBCD
    NEG
    NEGX
    NOP
    NOT
    OR
    ORI
    ORI to CCR
    ORI to SR
    PEA
    RESET
    ROL
    ROR
    ROXL
    ROXR
    RTE
    RTR
    RTS
    SBCD
    Sec
    STOP
    SUB
    SUBA
    SUBI
    SUBQ
    SUBX
    SWAP
    TAS
    TRAP
    TRAPV
    TST
    UNLK
     */
}
