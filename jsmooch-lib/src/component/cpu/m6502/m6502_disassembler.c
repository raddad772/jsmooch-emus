//
// Created by Dave on 2/4/2024.
//
#include "m6502.h"
#include "m6502_disassembler.h"

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

#define SARG const char *ins, struct jsm_debug_read_trace *trace, u32 *PC, struct jsm_string *outstr
static void absolute(SARG)
{
    u16 v = dbg_read16(trace, PC);
    jsm_string_sprintf(outstr, "%s$%04x", ins, v);
}

static void absolute_x(SARG) {
    u16 v = dbg_read16(trace, PC);
    jsm_string_sprintf(outstr, "%s$%04x,x", ins, v);
};

static void absolute_y(SARG) {
    u16 v = dbg_read16(trace, PC);
    jsm_string_sprintf(outstr, "%s$%04x,y", ins, v);
};

static void branch(SARG) {
    u16 v = (u16)(i8)dbg_read(trace, PC);
    u16 pc = v + (*PC);
    jsm_string_sprintf(outstr, "%s$%04x", ins, pc);
}

static void immediate(SARG)
{
    jsm_string_sprintf(outstr, "%s#$%02x", ins, dbg_read(trace, PC));
}

static void implied(SARG)
{
    jsm_string_sprintf(outstr, "%s", ins);
}

static void indirect(SARG)
{
    jsm_string_sprintf(outstr, "%s($%04x)", ins, dbg_read16(trace, PC));
}

static void indirect_x(SARG)
{
    jsm_string_sprintf(outstr, "%s($%04x,x)", ins, dbg_read16(trace, PC));
}

static void indirect_y(SARG) {
    jsm_string_sprintf(outstr, "%s($%04x),y", ins, dbg_read16(trace, PC));
}

static void zero_page(SARG)
{
    jsm_string_sprintf(outstr, "%s$%02x", ins, dbg_read(trace, PC));
}

static void zero_page_x(SARG)
{
    jsm_string_sprintf(outstr, "%s$%02x,x", ins, dbg_read(trace, PC));
}

static void zero_page_y(SARG)
{
    jsm_string_sprintf(outstr, "%s$%02x,y", ins, dbg_read(trace, PC));
}

void M6502_disassemble(u32 *PC, struct jsm_debug_read_trace *trace, struct jsm_string *outstr)
{
#define SPCS "   "
#define dasm(id, prefix, mode) case id: mode(prefix SPCS, trace, PC, outstr); break;

    u32 opcode = dbg_read(trace, PC);
    // const char *ins, struct jsm_debug_read_trace *trace, u32 *PC, struct jsm_string *outstr
    switch(opcode) {
        dasm(0x00, "brk", immediate)
        dasm(0x01, "ora", indirect_x)
        dasm(0x04, "nop", zero_page)
        dasm(0x05, "ora", zero_page)
        dasm(0x06, "asl", zero_page)
        dasm(0x08, "php", implied)
        dasm(0x09, "ora", immediate)
        dasm(0x0a, "asl", implied)
        dasm(0x0c, "nop", absolute)
        dasm(0x0d, "ora", absolute)
        dasm(0x0e, "asl", absolute)
        dasm(0x10, "bpl", branch)
        dasm(0x11, "ora", indirect_y)
        dasm(0x14, "nop", zero_page_x)
        dasm(0x15, "ora", zero_page_x)
        dasm(0x16, "asl", zero_page_x)
        dasm(0x18, "clc", implied)
        dasm(0x19, "ora", absolute_y)
        dasm(0x1a, "nop", implied)
        dasm(0x1c, "nop", absolute_x)
        dasm(0x1d, "ora", absolute_x)
        dasm(0x1e, "asl", absolute_x)
        dasm(0x20, "jsr", absolute)
        dasm(0x21, "and", indirect_x)
        dasm(0x24, "bit", zero_page)
        dasm(0x25, "and", zero_page)
        dasm(0x26, "rol", zero_page)
        dasm(0x28, "plp", implied)
        dasm(0x29, "and", immediate)
        dasm(0x2a, "rol", implied)
        dasm(0x2c, "bit", absolute)
        dasm(0x2d, "and", absolute)
        dasm(0x2e, "rol", absolute)
        dasm(0x30, "bmi", branch)
        dasm(0x31, "and", indirect_y)
        dasm(0x34, "nop", zero_page_x)
        dasm(0x35, "and", zero_page_x)
        dasm(0x36, "rol", zero_page_x)
        dasm(0x38, "sec", implied)
        dasm(0x39, "and", absolute_y)
        dasm(0x3a, "nop", implied)
        dasm(0x3c, "nop", absolute_x)
        dasm(0x3d, "and", absolute_x)
        dasm(0x3e, "rol", absolute_x)
        dasm(0x40, "rti", implied)
        dasm(0x41, "eor", indirect_x)
        dasm(0x44, "nop", zero_page)
        dasm(0x45, "eor", zero_page)
        dasm(0x46, "lsr", zero_page)
        dasm(0x48, "pha", implied)
        dasm(0x49, "eor", immediate)
        dasm(0x4a, "lsr", implied)
        dasm(0x4c, "jmp", absolute)
        dasm(0x4d, "eor", absolute)
        dasm(0x4e, "lsr", absolute)
        dasm(0x50, "bvc", branch)
        dasm(0x51, "eor", indirect_y)
        dasm(0x54, "nop", zero_page_x)
        dasm(0x55, "eor", zero_page_x)
        dasm(0x56, "lsr", zero_page_x)
        dasm(0x58, "cli", implied)
        dasm(0x59, "eor", absolute_y)
        dasm(0x5a, "nop", implied)
        dasm(0x5c, "nop", absolute_x)
        dasm(0x5d, "eor", absolute_x)
        dasm(0x5e, "lsr", absolute_x)
        dasm(0x60, "rts", implied)
        dasm(0x61, "adc", indirect_x)
        dasm(0x64, "nop", zero_page)
        dasm(0x65, "adc", zero_page)
        dasm(0x66, "ror", zero_page)
        dasm(0x68, "pla", implied)
        dasm(0x69, "adc", immediate)
        dasm(0x6a, "ror", implied)
        dasm(0x6c, "jmp", indirect)
        dasm(0x6d, "adc", absolute)
        dasm(0x6e, "ror", absolute)
        dasm(0x70, "bvs", branch)
        dasm(0x71, "adc", indirect_y)
        dasm(0x74, "nop", zero_page_x)
        dasm(0x75, "adc", zero_page_x)
        dasm(0x76, "ror", zero_page_x)
        dasm(0x78, "sei", implied)
        dasm(0x79, "adc", absolute_y)
        dasm(0x7a, "nop", implied)
        dasm(0x7c, "nop", absolute_x)
        dasm(0x7d, "adc", absolute_x)
        dasm(0x7e, "ror", absolute_x)
        dasm(0x80, "nop", immediate)
        dasm(0x81, "sta", indirect_x)
        dasm(0x82, "nop", immediate)
        dasm(0x84, "sty", zero_page)
        dasm(0x85, "sta", zero_page)
        dasm(0x86, "stx", zero_page)
        dasm(0x88, "dey", implied)
        dasm(0x89, "nop", immediate)
        dasm(0x8a, "txa", implied)
        dasm(0x8c, "sty", absolute)
        dasm(0x8d, "sta", absolute)
        dasm(0x8e, "stx", absolute)
        dasm(0x90, "bcc", branch)
        dasm(0x91, "sta", indirect_y)
        dasm(0x94, "sty", zero_page_x)
        dasm(0x95, "sta", zero_page_x)
        dasm(0x96, "stx", zero_page_y)
        dasm(0x98, "tya", implied)
        dasm(0x99, "sta", absolute_y)
        dasm(0x9a, "txs", implied)
        dasm(0x9d, "sta", absolute_x)
        dasm(0xa0, "ldy", immediate)
        dasm(0xa1, "lda", indirect_x)
        dasm(0xa2, "ldx", immediate)
        dasm(0xa4, "ldy", zero_page)
        dasm(0xa5, "lda", zero_page)
        dasm(0xa6, "ldx", zero_page)
        dasm(0xa8, "tay", implied)
        dasm(0xa9, "lda", immediate)
        dasm(0xaa, "tax", implied)
        dasm(0xac, "ldy", absolute)
        dasm(0xad, "lda", absolute)
        dasm(0xae, "ldx", absolute)
        dasm(0xb0, "bcs", branch)
        dasm(0xb1, "lda", indirect_y)
        dasm(0xb4, "ldy", zero_page_x)
        dasm(0xb5, "lda", zero_page_x)
        dasm(0xb6, "ldx", zero_page_y)
        dasm(0xb8, "clv", implied)
        dasm(0xb9, "lda", absolute_y)
        dasm(0xba, "tsx", implied)
        dasm(0xbc, "ldy", absolute_x)
        dasm(0xbd, "lda", absolute_x)
        dasm(0xbe, "ldx", absolute_y)
        dasm(0xc0, "cpy", immediate)
        dasm(0xc1, "cmp", indirect_x)
        dasm(0xc2, "nop", immediate)
        dasm(0xc4, "cpy", zero_page)
        dasm(0xc5, "cmp", zero_page)
        dasm(0xc6, "dec", zero_page)
        dasm(0xc8, "iny", implied)
        dasm(0xc9, "cmp", immediate)
        dasm(0xca, "dex", implied)
        dasm(0xcc, "cpy", absolute)
        dasm(0xcd, "cmp", absolute)
        dasm(0xce, "dec", absolute)
        dasm(0xd0, "bne", branch)
        dasm(0xd1, "cmp", indirect_y)
        dasm(0xd4, "nop", zero_page_x)
        dasm(0xd5, "cmp", zero_page_x)
        dasm(0xd6, "dec", zero_page_x)
        dasm(0xd8, "cld", implied)
        dasm(0xd9, "cmp", absolute_y)
        dasm(0xda, "nop", implied)
        dasm(0xdc, "nop", absolute_x)
        dasm(0xdd, "cmp", absolute_x)
        dasm(0xde, "dec", absolute_x)
        dasm(0xe0, "cpx", immediate)
        dasm(0xe1, "sbc", indirect_x)
        dasm(0xe2, "nop", immediate)
        dasm(0xe4, "cpx", zero_page)
        dasm(0xe5, "sbc", zero_page)
        dasm(0xe6, "inc", zero_page)
        dasm(0xe8, "inx", implied)
        dasm(0xe9, "sbc", immediate)
        dasm(0xea, "nop", implied)
        dasm(0xec, "cpx", absolute)
        dasm(0xed, "sbc", absolute)
        dasm(0xee, "inc", absolute)
        dasm(0xf0, "beq", branch)
        dasm(0xf1, "sbc", indirect_y)
        dasm(0xf4, "nop", zero_page_x)
        dasm(0xf5, "sbc", zero_page_x)
        dasm(0xf6, "inc", zero_page_x)
        dasm(0xf8, "sed", implied)
        dasm(0xf9, "sbc", absolute_y)
        dasm(0xfa, "nop", implied)
        dasm(0xfc, "nop", absolute_x)
        dasm(0xfd, "sbc", absolute_x)
        dasm(0xfe, "inc", absolute_x)

        default:
            jsm_string_sprintf(outstr, "UKN" SPCS "$%02x", opcode);
        break;
    }
#undef op
}

void M6502_disassemble_entry(struct M6502 *this, struct disassembly_entry* entry)
{
    jsm_string_quickempty(&entry->dasm);
    jsm_string_quickempty(&entry->context);
    u32 PC = entry->addr;
    M6502_disassemble(&PC, &this->trace.strct, &entry->dasm);
    entry->ins_size_bytes = PC - entry->addr;
}
