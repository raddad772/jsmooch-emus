//
// Created by Dave on 2/4/2024.
//
#include <stdio.h>
#include "huc6280.h"
#include "huc6280_disassembler.h"

static u16 dbg_read(struct jsm_debug_read_trace *trace, u32 *PC)
{
    u16 v = trace->read_trace(trace->ptr, *PC);
    (*PC)++;
    return v;
}

static u16 dbg_read16(struct jsm_debug_read_trace *trace, u32 *PC)
{
    u16 v = dbg_read(trace,PC);
    v |= dbg_read(trace,PC) << 8;
    return v;
}

#define SARG const char *ins, struct jsm_debug_read_trace *trace, u32 *PC, struct jsm_string *outstr, struct HUC6280 *cpu
#define SARG2x const char *ins, const char *s2, struct jsm_debug_read_trace *trace, u32 *PC, struct jsm_string *outstr, struct HUC6280 *cpu
#define SARGBIT const char *ins, u32 bitnum, struct jsm_debug_read_trace *trace, u32 *PC, struct jsm_string *outstr, struct HUC6280 *cpu

static u32 longaddr(struct HUC6280 *cpu, u32 addr)
{
    return cpu->regs.MPR[(addr & 0xE000) >> 13] | (addr & 0x1FFF);
}

#define zpa(x) longaddr(cpu, x | 0x2000)
#define la(x) longaddr(cpu, x)


static void immediate_absolute(SARG)
{
    u32 immediate = dbg_read(trace, PC);
    u32 absolute = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s, #$%02x, $%04x", ins, immediate, absolute);
}

static void immediate_absolute_x(SARG)
{
    u32 immediate = dbg_read(trace, PC);
    u32 absolute = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s, #$%02x, $%06x, x", ins, immediate, absolute);
}

static void absolute(SARG)
{
    u16 v = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s$%06x", ins, v);
}

static void absolute_x(SARG) {
    u16 v = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s$%06x,x", ins, v);
};

static void absolute_y(SARG) {
    u16 v = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s$%06x,y", ins, v);
};

static void block_move(SARG)
{
    u32 source = dbg_read16(trace, PC);
    u32 target = dbg_read16(trace, PC);
    u32 length = dbg_read16(trace, PC);
    jsm_string_sprintf(outstr, "%ss:$%06x, t:$%06x, l:$%4x", ins, la(source), la(target), length);

}

static void immediate_zero_page_x(SARG)
{
    u32 immediate = dbg_read(trace, PC);
    u32 zp = zpa(dbg_read(trace, PC));
    jsm_string_sprintf(outstr, "%s#$02x, $%06x,x", ins, immediate, zp);

}

static void immediate_zero_page(SARG)
{
    u32 immediate = dbg_read(trace, PC);
    u32 zp = zpa(dbg_read(trace, PC));
    jsm_string_sprintf(outstr, "%s#$02x, %%06x", ins, immediate, zp);
}

static void indirect_long_x(SARG)
{
    u32 addr = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s($%06x,x)", ins, addr);
}

static void indirect_long(SARG)
{
    u32 addr = la(dbg_read16(trace, PC));
    jsm_string_sprintf(outstr, "%s($%06x)", ins, addr);
}

static void zero_page_bit_relative(SARGBIT)
{
    u32 zp = zpa(dbg_read(trace, PC));
    u16 v = (u16)(i8)dbg_read(trace, PC);
    u16 pc = v + (*PC);
    jsm_string_sprintf(outstr, "%s$%02x:%c, $%06x", ins, zp, bitnum + 0x30, la(pc));
}

static void branch(SARG) {
    u16 v = (u16)(i8)dbg_read(trace, PC);
    u16 pc = v + (*PC);
    jsm_string_sprintf(outstr, "%s$%06x", ins, la(pc));
}

static void immediate(SARG)
{
    jsm_string_sprintf(outstr, "%s#$%02x", ins, dbg_read(trace, PC));
}

static void implied(SARG)
{
    jsm_string_sprintf(outstr, "%s", ins);
}

static void str2x(SARG2x)
{
    jsm_string_sprintf(outstr, "%s%s", ins, s2);
}

static void indirect(SARG)
{
    jsm_string_sprintf(outstr, "%s($%06x)", ins, la(dbg_read16(trace, PC)));
}

static void indirect_x(SARG)
{
    jsm_string_sprintf(outstr, "%s($%06x,x)", ins, la(dbg_read16(trace, PC)));
}

static void indirect_y(SARG) {
    u32 zpa = dbg_read(trace, PC);
    u32 zer = zpa;
    u32 a = dbg_read16(trace, &zer);
    jsm_string_sprintf(outstr, "%s($%02x),y   [$%06x ($%04x)]", ins, zpa, a, la(a));
}

static void zero_page_bit(SARGBIT)
{
    jsm_string_sprintf(outstr, "%s$%06x", ins, zpa(dbg_read(trace, PC)));
}

static void zero_page(SARG)
{
    jsm_string_sprintf(outstr, "%s$%06x", ins, zpa(dbg_read(trace, PC)));
}

static void zero_page_x(SARG)
{
    jsm_string_sprintf(outstr, "%s$%06x,x", ins, zpa(dbg_read(trace, PC)));
}

static void zero_page_y(SARG)
{
    jsm_string_sprintf(outstr, "%s$%06x,y", ins, zpa(dbg_read(trace, PC)));
}

#undef zpa
#undef la

void HUC6280_disassemble(struct HUC6280 *cpu, u32 *PC, struct jsm_debug_read_trace *trace, struct jsm_string *outstr)
{
#define SPCS "   "
#define dasm(id, prefix, mode) case id: mode(prefix SPCS, trace, PC, outstr, cpu); break;
#define dasm2s(id, prefix, str) case id: str2x(prefix SPCS, str, trace, PC, outstr, cpu); break;
#define dasmbit(id, prefix, mode, bitnum) case id: mode(prefix SPCS, bitnum, trace, PC, outstr, cpu); break;
    u32 opcode = dbg_read(trace, PC);
    // const char *ins, struct jsm_debug_read_trace *trace, u32 *PC, struct jsm_string *outstr
    switch(opcode) {
        dasm(0x00, "brk", implied)
        dasm(0x01, "ora", indirect_x)
        dasm(0x02, "sxy", implied)
        dasm(0x03, "st0", immediate)
        dasm(0x04, "tsb", zero_page)
        dasm(0x05, "ora", zero_page)
        dasm(0x06, "asl", zero_page)
        dasmbit(0x07, "rmb", zero_page_bit, 0)
        dasm(0x08, "php", implied)
        dasm(0x09, "ora", immediate)
        dasm(0x0a, "asl", implied)
        dasm2s(0x0b, "nop", "$0b")
        dasm(0x0c, "tsb", absolute)
        dasm(0x0d, "ora", absolute)
        dasm(0x0e, "asl", absolute)
        dasmbit(0x0f, "bbr", zero_page_bit_relative, 0)
        dasm(0x10, "bpl", branch)
        dasm(0x11, "ora", indirect_y)
        dasm(0x12, "ora", indirect)
        dasm(0x13, "st1", immediate)
        dasm(0x14, "trb", zero_page)
        dasm(0x15, "ora", zero_page_x)
        dasm(0x16, "asl", zero_page_x)
        dasmbit(0x17, "rmb", zero_page_bit, 1)
        dasm(0x18, "clc", implied)
        dasm(0x19, "ora", absolute_y)
        dasm(0x1a, "inc", implied)
        dasm2s(0x1b, "nop", "$1b")
        dasm(0x1c, "trb", absolute)
        dasm(0x1d, "ora", absolute_x)
        dasm(0x1e, "asl", absolute_x)
        dasmbit(0x1f, "bbr", zero_page_bit_relative, 1)
        dasm(0x20, "jsr", absolute)
        dasm(0x21, "and", indirect_x)
        dasm(0x22, "sax", implied)
        dasm(0x23, "st2", immediate)
        dasm(0x24, "bit", zero_page)
        dasm(0x25, "and", zero_page)
        dasm(0x26, "rol", zero_page)
        dasmbit(0x27, "rmb", zero_page_bit, 2)
        dasm(0x28, "plp", implied)
        dasm(0x29, "and", immediate)
        dasm(0x2a, "rol", implied)
        dasm2s(0x2b, "nop", "$2b")
        dasm(0x2c, "bit", absolute)
        dasm(0x2d, "and", absolute)
        dasm(0x2e, "rol", absolute)
        dasmbit(0x2f, "bbr", zero_page_bit_relative, 2)
        dasm(0x30, "bmi", branch)
        dasm(0x31, "and", indirect_y)
        dasm(0x32, "and", indirect)
        dasm(0x34, "bit", zero_page_x)
        dasm(0x35, "and", zero_page_x)
        dasm(0x36, "rol", zero_page_x)
        dasmbit(0x37, "rmb", zero_page_bit, 3)
        dasm(0x38, "sec", implied)
        dasm(0x39, "and", absolute_y)
        dasm(0x3a, "dec", implied)
        dasm2s(0x3b, "nop", "$3b")
        dasm(0x3c, "bit", absolute_x)
        dasm(0x3d, "and", absolute_x)
        dasm(0x3e, "rol", absolute_x)
        dasmbit(0x3f, "bbr", zero_page_bit_relative, 3)
        dasm(0x40, "rti", implied)
        dasm(0x41, "eor", indirect_x)
        dasm(0x42, "say", implied)
        dasm(0x43, "tma", immediate)
        dasm(0x44, "bsr", branch)
        dasm(0x45, "eor", zero_page)
        dasm(0x46, "lsr", zero_page)
        dasmbit(0x47, "rmb", zero_page_bit, 4)
        dasm(0x48, "pha", implied)
        dasm(0x49, "eor", immediate)
        dasm(0x4a, "lsr", implied)
        dasm2s(0x4b, "nop", "$4b")
        dasm(0x4c, "jmp", absolute)
        dasm(0x4d, "eor", absolute)
        dasm(0x4e, "lsr", absolute)
        dasmbit(0x4f, "bbr", zero_page_bit_relative, 4)
        dasm(0x50, "bvc", branch)
        dasm(0x51, "eor", indirect_y)
        dasm(0x52, "eor", indirect)
        dasm(0x53, "tam", immediate)
        dasm(0x54, "csl", implied)
        dasm(0x55, "eor", zero_page_x)
        dasm(0x56, "lsr", zero_page_x)
        dasmbit(0x57, "rmb", zero_page_bit, 5)
        dasm(0x58, "cli", implied)
        dasm(0x59, "eor", absolute_y)
        dasm(0x5a, "phy", implied)
        dasm2s(0x5b, "nop", "$5b")
        dasm2s(0x5c, "nop", "$5c")
        dasm(0x5d, "eor", absolute_x)
        dasm(0x5e, "lsr", absolute_x)
        dasmbit(0x5f, "bbr", zero_page_bit_relative, 5)
        dasm(0x60, "rts", implied)
        dasm(0x61, "adc", indirect_x)
        dasm(0x62, "cla", implied)
        dasm2s(0x63, "nop", "$63")
        dasm(0x64, "stz", zero_page)
        dasm(0x65, "adc", zero_page)
        dasm(0x66, "ror", zero_page)
        dasmbit(0x67, "rmb", zero_page_bit, 6)
        dasm(0x68, "pla", implied)
        dasm(0x69, "adc", immediate)
        dasm(0x6a, "ror", implied)
        dasm2s(0x6b, "nop", "$6b")
        dasm(0x6c, "jmp", indirect_long)
        dasm(0x6d, "adc", absolute)
        dasm(0x6e, "ror", absolute)
        dasmbit(0x6f, "bbr", zero_page_bit_relative, 6)
        dasm(0x70, "bvs", branch)
        dasm(0x71, "adc", indirect_y)
        dasm(0x72, "adc", indirect)
        dasm(0x73, "tii", block_move)
        dasm(0x74, "stz", zero_page_x)
        dasm(0x75, "adc", zero_page_x)
        dasm(0x76, "ror", zero_page_x)
        dasmbit(0x77, "rmb", zero_page_bit, 7)
        dasm(0x78, "sei", implied)
        dasm(0x79, "adc", absolute_y)
        dasm(0x7a, "ply", implied)
        dasm2s(0x7b, "nop", "$7b")
        dasm(0x7c, "jmp", indirect_long_x)
        dasm(0x7d, "adc", absolute_x)
        dasm(0x7e, "ror", absolute_x)
        dasmbit(0x7f, "bbr", zero_page_bit_relative, 7)
        dasm(0x80, "bra", branch)
        dasm(0x81, "sta", indirect_x)
        dasm(0x82, "clx", implied)
        dasm(0x83, "tst", immediate_zero_page)
        dasm(0x84, "sty", zero_page)
        dasm(0x85, "sta", zero_page)
        dasm(0x86, "stx", zero_page)
        dasmbit(0x87, "smb", zero_page_bit, 0)
        dasm(0x88, "dey", implied)
        dasm(0x89, "bit", immediate)
        dasm(0x8a, "txa", implied)
        dasm2s(0x8b, "nop", "$8b")
        dasm(0x8c, "sty", absolute)
        dasm(0x8d, "sta", absolute)
        dasm(0x8e, "stx", absolute)
        dasmbit(0x8f, "bbs", zero_page_bit_relative, 0)
        dasm(0x90, "bcc", branch)
        dasm(0x91, "sta", indirect_y)
        dasm(0x92, "sta", indirect)
        dasm(0x93, "tst", immediate_absolute)
        dasm(0x94, "sty", zero_page_x)
        dasm(0x95, "sta", zero_page_x)
        dasm(0x96, "stx", zero_page_y)
        dasmbit(0x97, "smb", zero_page_bit, 1)
        dasm(0x98, "tya", implied)
        dasm(0x99, "sta", absolute_y)
        dasm(0x9a, "txs", implied)
        dasm2s(0x9b, "nop", "$9b")
        dasm(0x9c, "stz", absolute)
        dasm(0x9d, "sta", absolute_x)
        dasm(0x9e, "stz", absolute_x)
        dasmbit(0x9f, "bbs", zero_page_bit_relative, 1)
        dasm(0xa0, "ldy", immediate)
        dasm(0xa1, "lda", indirect_x)
        dasm(0xa2, "ldx", immediate)
        dasm(0xa3, "tst", immediate_zero_page_x) // ("x")
        dasm(0xa4, "ldy", zero_page)
        dasm(0xa5, "lda", zero_page)
        dasm(0xa6, "ldx", zero_page)
        dasmbit(0xa7, "smb", zero_page_bit, 2)
        dasm(0xa8, "tay", implied)
        dasm(0xa9, "lda", immediate)
        dasm(0xaa, "tax", implied)
        dasm2s(0xab, "nop", "$ab")
        dasm(0xac, "ldy", absolute)
        dasm(0xad, "lda", absolute)
        dasm(0xae, "ldx", absolute)
        dasmbit(0xaf, "bbs", zero_page_bit_relative, 2)
        dasm(0xb0, "bcs", branch)
        dasm(0xb1, "lda", indirect_y)
        dasm(0xb2, "lda", indirect)
        dasm(0xb3, "tst", immediate_absolute_x) // ("x")
        dasm(0xb4, "ldy", zero_page_x)
        dasm(0xb5, "lda", zero_page_x)
        dasm(0xb6, "ldx", zero_page_y)
        dasmbit(0xb7, "smb", zero_page_bit, 3)
        dasm(0xb8, "clv", implied)
        dasm(0xb9, "lda", absolute_y)
        dasm(0xba, "tsx", implied)
        dasm2s(0xbb, "nop", "$bb")
        dasm(0xbc, "ldy", absolute_x)
        dasm(0xbd, "lda", absolute_x)
        dasm(0xbe, "ldx", absolute_y)
        dasmbit(0xbf, "bbs", zero_page_bit_relative, 3)
        dasm(0xc0, "cpy", immediate)
        dasm(0xc1, "cmp", indirect_x)
        dasm(0xc2, "cly", implied)
        dasm(0xc3, "tdd", block_move)
        dasm(0xc4, "cpy", zero_page)
        dasm(0xc5, "cmp", zero_page)
        dasm(0xc6, "dec", zero_page)
        dasmbit(0xc7, "smb", zero_page_bit, 4)
        dasm(0xc8, "iny", implied)
        dasm(0xc9, "cmp", immediate)
        dasm(0xca, "dex", implied)
        dasm2s(0xcb, "nop", "$cb")
        dasm(0xcc, "cpy", absolute)
        dasm(0xcd, "cmp", absolute)
        dasm(0xce, "dec", absolute)
        dasmbit(0xcf, "bbs", zero_page_bit_relative, 4)
        dasm(0xd0, "bne", branch)
        dasm(0xd1, "cmp", indirect_y)
        dasm(0xd2, "cmp", indirect)
        dasm(0xd3, "tin", block_move)
        dasm(0xd4, "csh", implied)
        dasm(0xd5, "cmp", zero_page_x)
        dasm(0xd6, "dec", zero_page_x)
        dasmbit(0xd7, "smb", zero_page_bit, 5)
        dasm(0xd8, "cld", implied)
        dasm(0xd9, "cmp", absolute_y)
        dasm(0xda, "phx", implied)
        dasm2s(0xdb, "nop", "$db")
        dasm2s(0xdc, "nop", "$dc")
        dasm(0xdd, "cmp", absolute_x)
        dasm(0xde, "dec", absolute_x)
        dasmbit(0xdf, "bbs", zero_page_bit_relative, 5)
        dasm(0xe0, "cpx", immediate)
        dasm(0xe1, "sbc", indirect_x)
        dasm2s(0xe2, "nop", "$e2")
        dasm(0xe3, "tia", block_move)
        dasm(0xe4, "cpx", zero_page)
        dasm(0xe5, "sbc", zero_page)
        dasm(0xe6, "inc", zero_page)
        dasmbit(0xe7, "smb", zero_page_bit, 6)
        dasm(0xe8, "inx", implied)
        dasm(0xe9, "sbc", immediate)
        dasm(0xea, "nop", implied)
        dasm2s(0xeb, "nop", "$eb")
        dasm(0xec, "cpx", absolute)
        dasm(0xed, "sbc", absolute)
        dasm(0xee, "inc", absolute)
        dasmbit(0xef, "bbs", zero_page_bit_relative, 6)
        dasm(0xf0, "beq", branch)
        dasm(0xf1, "sbc", indirect_y)
        dasm(0xf2, "sbc", indirect)
        dasm(0xf3, "tai", block_move)
        dasm(0xf4, "set", implied)
        dasm(0xf5, "sbc", zero_page_x)
        dasm(0xf6, "inc", zero_page_x)
        dasmbit(0xf7, "smb", zero_page_bit, 7)
        dasm(0xf8, "sed", implied)
        dasm(0xf9, "sbc", absolute_y)
        dasm(0xfa, "plx", implied)
        dasm2s(0xfb, "nop", "$fb")
        dasm2s(0xfc, "nop", "$fc")
        dasm(0xfd, "sbc", absolute_x)
        dasm(0xfe, "inc", absolute_x)
        dasmbit(0xff, "bbs", zero_page_bit_relative, 7)
        default:
            jsm_string_sprintf(outstr, "UKN" SPCS "$%02x", opcode);
            break;
    }
#undef op
}

void HUC6280_disassemble_entry(struct HUC6280 *this, struct disassembly_entry* entry)
{
    jsm_string_quickempty(&entry->dasm);
    jsm_string_quickempty(&entry->context);
    u32 PC = entry->addr;
    HUC6280_disassemble(this, &PC, &this->trace.strct, &entry->dasm);
    entry->ins_size_bytes = PC - entry->addr;
}
