//
// Created by . on 6/5/24.
//

#include <cstdio>
#include <cassert>

#include "m68000_instructions.h"
#include "m68000_disassembler.h"
#include "generated/generated_disasm.h"

#define jss(...) out.sprintf(__VA_ARGS__)
namespace M68k {
static constexpr char conditions[16][3] = {
        "t ", "f ", "hi", "ls", "cc", "cs", "ne", "eq",
        "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le",
};

static constexpr char movem_from_mem[16][3] = {
        "a7", "a6", "a5", "a4", "a3", "a2", "a1", "a0",
        "d7", "d6", "d5", "d4", "d3", "d2", "d1", "d0",
};

static constexpr char movem_to_mem[16][3] = {
        "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
        "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
};


static u32 read_disp(u32 &PC, jsm_debug_read_trace &rt)
{
    const u32 val = rt.read_trace_m68k(rt.ptr, PC, 1, 1) & 0xFFFF;
    PC += 2;
    return val;
}

static u32 read_index(u32 &PC, jsm_debug_read_trace &rt)
{
    const u32 val = rt.read_trace_m68k(rt.ptr, PC, 1, 1) & 0xFF;
    PC += 2;
    return val;
}

static u32 read_pc(u32 &PC, jsm_debug_read_trace &rt)
{
    const u32 val = rt.read_trace_m68k(rt.ptr, PC, 1, 1);
    PC += 2;
    return val;
}

static u32 read_pc32(u32 &PC, jsm_debug_read_trace &rt)
{
    u32 val = rt.read_trace_m68k(rt.ptr, PC, 1, 1) << 16;
    PC += 2;
    val |= rt.read_trace_m68k(rt.ptr, PC, 1, 1);
    PC += 2;
    return val;
}
//ins_suffix("add", sz, out);
void ins_suffix(const char* ins, u32 sz, jsm_string &out, const char *spaces)
{
    switch(sz) {
        case 1:
            jss("%s.b%s", ins, spaces);
            break;
        case 2:
            jss("%s.w%s", ins, spaces);
            break;
        case 4:
            jss("%s.l%s", ins, spaces);
            break;
        default:
            assert(1==0);
            break;
    }
}


static void dodea(EA *ea, u32 IR, jsm_string &out, u32 sz, u32 &PC, jsm_debug_read_trace &rt)
{
    u32 v;
    switch(ea->kind) {
        case 0: // data reg direct
            jss("d%d", ea->reg);
            return;
        case 1: // address reg direct
            jss("a%d", ea->reg);
            return;
        case 2: // address reg indirect
            jss("(a%d)", ea->reg);
            return;
        case 3: // address reg indirect with postincrement
            jss("(a%d)+", ea->reg);
            return;
        case 4: // ^ predecrement
            jss("-(a%d)", ea->reg);
            return;
        case 5: // ^ displacement
            jss("($%04x,a%d)", read_disp(PC, rt), ea->reg);
            return;
        case 6: // ^ index
            //($, hex(read(displacement),ax,dx)
            //PC -= 2;
            v =  read_pc(PC, rt);
            jss("($%02x,a%d,d%d)", v & 0xFF, ea->reg, (v >> 12) & 7);
            return;
        case 7: // absolute short
            jss("$%04x.w", read_pc(PC, rt));
            return;
        case 8: // absolute long
            jss("$%06x.l", read_pc32(PC, rt));
            return;
        case 9: // program counter with displacement
            jss("(d%d,PC)", ea->reg);
            return;
        case 10: // program counter with index
            jss("($%02x,d%d,PC)", read_index(PC, rt), ea->reg);
            return;
        case 11: {// AM quick immediate
            jss("%d", ea->reg);
            return; }
        case AM_immediate: {// other immediates
            //1 or two words depending on size
            u32 n;
            switch(sz) {
                case 1:
                    n = read_pc(PC, rt);
                    jss("$%x", n);
                    break;
                case 2:
                    n = read_pc(PC, rt);
                    jss("$%x", n);
                    break;
                case 4:
                    n = read_pc32(PC, rt);
                    jss("$%x", n);
                    break;
            }
            return; }
        default:
            jss("???");
            printf("\nHOW DID I GET HERE!?");
            return;
    }
}

static u32 readabyte(u32 addr, jsm_debug_read_trace &rt)
{
    u32 v;
    if (addr & 1) { // second one
        v = rt.read_trace_m68k(rt.ptr, addr, 0, 1) & 0xFF;
    }
    else {
        v = rt.read_trace_m68k(rt.ptr, addr, 1, 0) >> 8;
    }
    return v;
}

// print_imm(&PC, rt, ins.sz, 1);
static void print_imm(u32 &PC, jsm_debug_read_trace &rt, u32 sz, u32 do_comma, jsm_string &out)
{
    u32 v;
    switch(sz) {
        case 1:
            v = readabyte(PC, rt);
            PC+=2;
            if (do_comma) jss("$%02x,", v);
            else jss("$%02x", v);
            break;
        case 2:
            v = rt.read_trace_m68k(rt.ptr, PC, 1, 1);
            PC+=2;
            if (do_comma) jss("$%04x,", v);
            else jss("$%04x", v);
            break;
        case 4:
            v = read_pc32(PC, rt);
            if (do_comma) jss("$%08x,", v);
            else jss("$%08x", v);
            break;
        default:
            printf("\nWHAAAT!?");
            assert(1==0);
    }
}

void disasm_BADINS(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    //printf("\nERROR UNIMPLEMENTED DISASSEMBLY %04x", ins.opcode);
    out.sprintf("%04x  UNKNOWN", ins.opcode);
}

#define dea(ea, sz) dodea(ea, 0, out, sz, PC, rt)
#define dea2(sz)     dea(&ins.ea[0], sz); jss(","); dea(&ins.ea[1], sz)

void disasm_ABCD(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("abcd    ");
    dea2(1);
}

void disasm_ADD(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("add", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ADDA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("adda", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_ADDI(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("addi", ins.sz, out, "  ");
    print_imm(PC, rt, ins.sz, 1, out);
    dea(&ins.ea[0], ins.sz);
}

void disasm_ADDQ(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("addq", ins.sz, out, "  ");
    dea2(ins.sz);
    //dea(&ins.ea[1], ins.sz);
}

void disasm_ADDQ_ar(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("addq", ins.sz, out, "  ");
    out.sprintf("%d,a%d", ins.ea[0].reg, ins.ea[1].reg);
}


void disasm_ADDX(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("addx", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_AND(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("and", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ANDI(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("andi", ins.sz, out, "  ");
    print_imm(PC, rt, ins.sz, 1, out);
    dea(&ins.ea[0], ins.sz);
}

void disasm_ANDI_TO_CCR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("andi    %02x,ccr", read_pc(PC, rt));
}

void disasm_ANDI_TO_SR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("andi    %04x,sr", read_pc32(PC, rt));
}

void disasm_ASL_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("asl", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ASL_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("asl", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ASL_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("asl", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_ASR_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("asr", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ASR_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("asr", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ASR_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("asr", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_BCC(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("b%s     $%02x", conditions[ins.ea[0].reg], ins.ea[1].reg);
}

void disasm_BCHG_dr_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("bchg", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_BCHG_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("bchg", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_BCLR_dr_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("bclr", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_BCLR_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("bclr", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_BRA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("bra     $%02x", ins.ea[0].reg);
}

void disasm_BSET_dr_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("bset", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_BSET_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("bset", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}


void disasm_BSR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("bsr     $%02x", ins.ea[0].reg);
}

void disasm_BTST_dr_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("btst", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_BTST_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("btst", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
    jss(",#$%x", read_pc(PC, rt));
}

void disasm_CHK(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("chk.w   ");
    dea2(2);
}

void disasm_CLR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("clr", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_CMP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("cmp", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_CMPA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("cmpa", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_CMPI(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("cmpi", ins.sz, out, "  ");
    print_imm(PC, rt, ins.sz, 1, out);
    dea(&ins.ea[0], ins.sz);
}

void disasm_CMPM(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("cmpm", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_DBCC(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    //printf("\nDASM INS %04x", ins.opcode);
    if (ins.opcode == 0x082d) {
        int a =4;
        a++;
    }
    jss("db%s    d%d,$%04x", conditions[ins.ea[0].reg], ins.ea[1].reg, read_disp(PC, rt));
}

void disasm_DIVS(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("divs.w  ");
    dea2(2);
}

void disasm_DIVU(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("divu.w  ");
    dea2(2);
}

void disasm_EOR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("eor", ins.sz, out, "   ");
    switch(ins.variant) {
        case 0:
        dea2(ins.sz);
            return;
        case 1:
            dea(&ins.ea[0], ins.sz);
            return;
        default:
            printf("\nWHAAAT?");
            assert(1==0);
            return;
    }
}

void disasm_EORI(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("eori", ins.sz, out, "  ");
    print_imm(PC, rt, ins.sz, 1, out);
    dea(&ins.ea[0], ins.sz);
}

void disasm_EORI_TO_CCR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("eori    %02x,ccr", read_pc(PC, rt));
}

void disasm_EORI_TO_SR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("eori    %04x,sr", read_pc32(PC, rt));
}

void disasm_EXG(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("exg     ");
    dea2(4);
}

void disasm_EXT(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("ext", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_ILLEGAL(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("illegal ");
}

void disasm_JMP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("jmp     ");
    dea(&ins.ea[0], 4);
}

void disasm_JSR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("jsr     ");
    dea(&ins.ea[0], 4);
}

void disasm_LEA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("lea     ");
    dea2(4);
}

void disasm_LINK(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("link    ");
    dea(&ins.ea[0], 4);
    jss(",");
    print_imm(PC, rt, ins.sz, 0, out);
}

void disasm_LSL_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("lsl", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_LSL_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("lsl", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_LSL_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("lsl", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_LSR_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("lsr", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_LSR_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("lsr", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_LSR_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("lsr", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_MOVE(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("move", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_MOVEA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("movea", ins.sz, out, " ");
    dea2(ins.sz);
}

void disasm_MOVEM_TO_MEM(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("movem", ins.sz, out, " ");
    u32 lst = read_pc(PC, rt);
    for (u32 i = 0; i < 16; i++) {
        if (lst & 1)
            jss("%s,", movem_to_mem[i]);
        lst >>= 1;
    }
    dea(&ins.ea[0], ins.sz);
}

void disasm_MOVEM_TO_REG(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("movem", ins.sz, out, " ");
    PC -= 2;
    u32 lst = read_pc(PC, rt);
    for (u32 i = 0; i < 16; i++) {
        if (lst & 1)
            jss("%s,", movem_from_mem[15 - i]);
        lst >>= 1;
    }
    dea(&ins.ea[0], ins.sz);
}

void disasm_MOVEP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("movep", ins.sz, out, " ");
    dea2(ins.sz);
}

void disasm_MOVEQ(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("moveq   #$%02x,d%d", ins.ea[0].reg, ins.ea[1].reg);
}

void disasm_MOVE_FROM_SR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("move    sr,");
    dea(&ins.ea[0], 2);
}

void disasm_MOVE_TO_CCR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("move   ");
    dea(&ins.ea[0], 1);
    jss(",ccr");
}

void disasm_MOVE_TO_SR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("move   ");
    dea(&ins.ea[0], 2);
    jss(",sr");
}

void disasm_MOVE_FROM_USP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("move    usp,");
    dea(&ins.ea[0], ins.sz);
}

void disasm_MOVE_TO_USP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("move    ");
    dea(&ins.ea[0], ins.sz);
    jss(",usp");
}

void disasm_MULS(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("muls.w  ");
    dea2(2);
}

void disasm_MULU(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("mulu.w  ");
    dea2(2);
}

void disasm_NBCD(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("nbcd    ");
    dea(&ins.ea[0], 1);
}

void disasm_NEG(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("neg", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_NEGX(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("negx", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_NOP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("nop     ");
}

void disasm_NOT(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("not", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_OR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("or", ins.sz, out, "    ");
    dea2(ins.sz);
}

void disasm_ORI(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("ori", ins.sz, out, "   ");
    print_imm(PC, rt, ins.sz, 1, out);
    dea(&ins.ea[0], ins.sz);
}

void disasm_ORI_TO_CCR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("ori     #$%02x,ccr", read_index(PC, rt));
}

void disasm_ORI_TO_SR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("ori     #$%04x,sr", read_pc(PC, rt));
}

void disasm_PEA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("pea     ");
    dea(&ins.ea[0], 4);
}

void disasm_RESET_POWER(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("reset(pwr)");
}


void disasm_RESET(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("reset   ");
}

void disasm_ROL_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("rol", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ROL_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("rol", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ROL_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("rol", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_ROR_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("ror", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ROR_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("ror", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_ROR_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("ror", ins.sz, out, "   ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_ROXL_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("roxl", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_ROXL_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("roxl", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_ROXL_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("roxl", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_ROXR_qimm_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("roxr", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_ROXR_dr_dr(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("roxr", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_ROXR_ea(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("roxr", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_RTE(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("rte     ");
}

void disasm_RTR(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("rtr     ");
}

void disasm_RTS(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("rts     ");
}

void disasm_SBCD(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("sbcd    ");
    dea2(1);
}


void disasm_SCC(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("s%s     ", conditions[ins.ea[0].reg]);
    dea(&ins.ea[1], 1);
}

void disasm_STOP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("stop    #");
    print_imm(PC, rt, 2, 0, out);
}

void disasm_SUB(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("sub", ins.sz, out, "   ");
    dea2(ins.sz);
}

void disasm_SUBA(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("suba", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_SUBI(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("subi", ins.sz, out, "  ");
    print_imm(PC, rt, ins.sz, 1, out);
    dea(&ins.ea[0], ins.sz);
}

void disasm_SUBQ(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    switch(ins.sz) {
        case 1:
            jss("subq.b  #$%01x", ins.ea[0].reg);
            break;
        case 2:
            jss("subq.w  #$%02x,", ins.ea[0].reg);
            break;
        case 4:
            jss("subq.l  #$%04x,", ins.ea[0].reg);
            break;
        default:
            assert(1==0);
    }
    dea(&ins.ea[1], ins.sz);
}

void disasm_SUBQ_ar(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    switch(ins.sz) {
        case 2:
            jss("subq.w  #$%02x,", ins.ea[0].reg);
            break;
        case 4:
            jss("subq.l  #$%04x,", ins.ea[0].reg);
            break;
        default:
            assert(1==0);
    }
    dea(&ins.ea[1], ins.sz);
}

void disasm_SUBX(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("subx", ins.sz, out, "  ");
    dea2(ins.sz);
}

void disasm_SWAP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("swap    ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_TAS(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("tas     ");
    dea(&ins.ea[0], 1);
}

void disasm_TRAP(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("trap    #%d", ins.ea[0].reg);
}

void disasm_TRAPV(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("trapv      ");
}


void disasm_TST(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    ins_suffix("tst", ins.sz, out, "  ");
    dea(&ins.ea[0], ins.sz);
}

void disasm_UNLK(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("unlk    a%d", ins.ea[0].reg);
}


void disasm_ALINE(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("illegal  a-line");
}

void disasm_FLINE(ins_t &ins, u32 &PC, jsm_debug_read_trace &rt, jsm_string &out)
{
    jss("illegal  f-line");
}

void disassemble(u32 PC, u16 IR, jsm_debug_read_trace &rt, jsm_string &out)
{
    u16 opcode = IR;
    printf("\nDAMS %04x", IR);
    ins_t &ins = decoded[opcode];
    u32 mPC = (PC+2)&0xFFFFFF;
    ins.disasm(ins, mPC, rt, out);
}

}