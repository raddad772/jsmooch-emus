//
// Created by . on 11/20/25.
//

#include "rca1802_disassembler.h"
#include "rca1802.h"

namespace RCA1802 {
static u8 dbg_read(jsm_debug_read_trace &trace, u16 &PC)
{
    u16 v = trace.read_trace(trace.ptr, PC);
    PC++;
    return v;
}

#define dasm(...) outstr.sprintf(__VA_ARGS__)
#define setreg(num) ctx.registers |= (num << 16);
static void disassemble_30(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx, u8 N)
{
    // Read one immediate
    u8 imm = dbg_read(trace, PC);
#define td(num, x) case num: dasm(x " $%02x", imm); break
    switch (0x30 | N) {
        td(0x30, "BR");
        td(0x31, "BQ");
        td(0x32, "BZ");
        td(0x33, "BDF");
        td(0x34, "B1");
        td(0x35, "B2");
        td(0x36, "B3");
        td(0x37, "B4");
        case 0x38: dasm("SKP"); break;
        td(0x39, "BNQ");
        td(0x3A, "BNZ");
        td(0x3B, "BNF");
        td(0x3C, "BN1");
        td(0x3D, "BN2");
        td(0x3E, "BN3");
        td(0x3F, "BN4");
    }
#undef td
}

static void disassemble_60(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx, u8 N) {
    if (N == 0) {
        dasm("IRX");
    }
    else if (N < 8) {
        dasm("OUT %d", N);
    }
    else if (N == 8) {
        dasm("INVALID/NOP");
    }
    else {
        dasm("INP %d", N - 8);
    }
}

static void disassemble_70(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx, u8 N) {
    switch (0x70 | N) {
        case 0x70: dasm("RET"); ctx.X = 1; ctx.P = 1; break;
        case 0x71: dasm("DIS"); ctx.X = 1; ctx.P = 1; break;
        case 0x72: dasm("LDXA"); ctx.X = 1; ctx.D = 1; break;
        case 0x73: dasm("STXD"); ctx.X = 1; ctx.D = 1; break;
        case 0x74: dasm("ADC"); ctx.X = 1; ctx.D = 1; break;
        case 0x75: dasm("SDB"); ctx.X = 1; ctx.D = 1; break;
        case 0x76: dasm("SHRC"); ctx.D = 1; break;
        case 0x77: dasm("SMB"); ctx.X = 1; ctx.D = 1; break;
        case 0x78: dasm("SAVE"); break;
        case 0x79: dasm("MARK"); setreg(2); break;
        case 0x7A: dasm("REQ"); break;
        case 0x7B: dasm("SEQ"); break;
        case 0x7C: dasm("ADCI $%02x", dbg_read(trace, PC)); break;
        case 0x7D: dasm("SDBI $%02x", dbg_read(trace, PC)); break;
        case 0x7E: dasm("SHLC"); ctx.D = 1; break;
        case 0x7F: dasm("SMBI $%02x", dbg_read(trace, PC)); break;
    }
}

static void disassemble_C0(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx, u8 N) {
    // skips are only 1 byte, long branches are 2
    // skip = &4
    static constexpr int is_skip[16] = {
        0, 0, 0, 0,
        0, 1, 1, 1,
        1, 0, 0, 0,
        1, 1, 1, 1
    };
    u16 imm=0;
    if (!is_skip[N]) {
        imm = static_cast<u16>(dbg_read(trace, PC)) << 8;
        imm |=  static_cast<u16>(dbg_read(trace, PC));
    }
    switch (0xC0 | N) {
#define tskip(num, x) case num: dasm(x); break;
#define tbranch(num, x) case num: dasm(x " $%04x", imm); break;
        tbranch(0xC0, "LBR");
        tbranch(0xC1, "LBQ");
        tbranch(0xC2, "LBZ");
        tbranch(0xC3, "LBDF");
        tskip(0xC4, "NOP");
        tskip(0xC5, "LSNQ");
        tskip(0xC6, "LSNZ");
        tskip(0xC7, "LSNF");
        tskip(0xC8, "LSKP");
        tbranch(0xC9, "LNBQ");
        tbranch(0xCA, "LNBZ");
        tbranch(0xCB, "LBNF");
        tskip(0xCC, "LSIE");
        tskip(0xCD, "LSQ");
        tskip(0xCE, "LSZ");
        tskip(0xCF, "LSDF");
#undef tskip
#undef tbranch
    }
}

static void disassemble_F0(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx, u8 N) {
    switch (0xF0 | N) {
        case 0xF0: dasm("LDX"); ctx.X = 1; ctx.D = 1; break;
        case 0xF1: dasm("OR"); ctx.X = 1; ctx.D = 1; break;
        case 0xF2: dasm("AND"); ctx.X = 1; ctx.D = 1; break;
        case 0xF3: dasm("XOR"); ctx.X = 1; ctx.D = 1; break;
        case 0xF4: dasm("ADD"); ctx.X = 1; ctx.D = 1; break;
        case 0xF5: dasm("SD"); ctx.X = 1; ctx.D = 1; break;
        case 0xF6: dasm("SHR"); ctx.D = 1; break;
        case 0xF7: dasm("SMB"); ctx.X = 1; ctx.D = 1; break;
        case 0xF8: dasm("LDI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
        case 0xF9: dasm("ORI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
        case 0xFA: dasm("ANI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
        case 0xFB: dasm("XRI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
        case 0xFC: dasm("ADI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
        case 0xFD: dasm("SDI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
        case 0xFE: dasm("SHL"); ctx.D = 1; break;
        case 0xFF: dasm("SMI $%02x", dbg_read(trace, PC)); ctx.D = 1; break;
    }
}

void disassemble(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx) {
    u32 opcode = dbg_read(trace, PC);
    u32 I = opcode & 0xF0;
    u32 N = opcode & 15;
    ctx.u = 0;
    switch (I) {
        case 0x00:
            if (N == 0) dasm("IDLE");
            else { dasm("LDN %d", N); ctx.N = 1; ctx.D = 1;  setreg(N); }
            break;
        case 0x10:
            dasm("INC %d", N);
            ctx.N = 1;
            setreg(N);
            break;
        case 0x20:
            dasm("DEC %d", N);
            ctx.N = 1;
            setreg(N);
            break;
        case 0x30:
            disassemble_30(PC, trace, outstr, ctx, N);
            break;
        case 0x40:
            dasm("LDA %d", N);
            ctx.N = 1;
            ctx.D = 1;
            setreg(N);
            break;
        case 0x50:
            dasm("STR %d", N);
            ctx.N = 1;
            ctx.D = 1;
            setreg(N);
            break;
        case 0x60:
            disassemble_60(PC, trace, outstr, ctx, N);
            break;
        case 0x70:
            disassemble_70(PC, trace, outstr, ctx, N);
            break;
        case 0x80:
            dasm("GLO %d", N);
            ctx.N = 1;
            ctx.D = 1;
            break;
        case 0x90:
            dasm("GHI %d", N);
            ctx.N = 1;
            ctx.D = 1;
            break;
        case 0xA0:
            dasm("PLO %d", N);
            ctx.N = 1;
            ctx.D = 1;
            setreg(N);
            break;
        case 0xB0:
            dasm("PHI %d", N);
            ctx.N = 1;
            ctx.D = 1;
            setreg(N);
            break;
        case 0xC0:
            disassemble_C0(PC, trace, outstr, ctx, N);
            break;
        case 0xD0:
            dasm("SEP %d", N);
            break;
        case 0xE0:
            dasm("SEX %d", N);
            break;
        case 0xF0:
            disassemble_F0(PC, trace, outstr, ctx, N);
            break;
    }
#undef dasm
}


void disassemble_entry(core *cpu, disassembly_entry& entry) {
    u16 PC = entry.addr;
    ctxt ctx;
    disassemble(PC, cpu->trace.strct, entry.dasm, ctx);
    entry.ins_size_bytes = PC - entry.addr;
}
}