//
// Created by . on 4/19/25.
//

#include "wdc65816_disassembler.h"


enum  {
    RA_lo = 1,
    RA_hi = 2,
    RA = 4,
    RD = 8,
    RX = 0x10,
    RY = 0x20,
    RP = 0x40,
    RPBR = 0x80,
    RS = 0x100,
    RDBR = 0x200
} WDC_regs;


static inline u32 inc_addr16of24(u32 addr)
{
    return (addr & 0xFF0000) | ((addr + 1) & 0xFFFF);
}

static u32 read(u32 addr, jsm_debug_read_trace *rt)
{
    return rt->read_trace(rt->ptr, addr & 0xFFFFFF);
}

static u32 read_byte(u32 addr, jsm_debug_read_trace *rt) {
    return read(addr, rt);
}

static u32 read_word(u32 addr, jsm_debug_read_trace *rt) {
    u32 data = read(addr, rt);

    return data | (read(addr+1, rt) << 8);
}

static u32 read_long(u32 addr, jsm_debug_read_trace *rt) {
    u32 data = read_byte(addr, rt);
    return data | (read_word(addr+1, rt) << 8);
}

static u32 read_op_byte(u32 addr, jsm_debug_read_trace *rt) {
    return read_byte(addr, rt);
}

static u32 read_op_word(u32 addr, jsm_debug_read_trace *rt) {
    u32 data = read(addr, rt);
    addr = inc_addr16of24(addr);

    return data | (read(addr, rt) << 8);
}

static u32 read_op_long(u32 addr, jsm_debug_read_trace *rt) {
    u32 data = read_op_byte(addr, rt);
    addr = inc_addr16of24(addr);
    return data | (read_op_word(addr, rt) << 8);
}

#define ARGS char *buf, u32 addr, u32 e, u32 m, u32 x, WDC65816_regs *r, jsm_debug_read_trace *rt, u32 *ins_len, u32 *rp

static i32 do_absolute(ARGS)
{
    *ins_len = 3;
    u32 n = read_op_word(addr, rt);
    snprintf(buf, 50, "$%04x", n);
    return static_cast<i32>((r->DBR << 16) | n);
}

static i32 do_absolute_pc(ARGS)
{
    *ins_len = 3;
    u32 n = read_op_word(addr, rt);
    snprintf(buf, 50, "$%04x", n);
    return static_cast<i32>((addr & 0xFF0000) | n);
}

static i32 do_absolute_x(ARGS)
{
    *ins_len = 3;
    u32 n = read_op_word(addr, rt);
    snprintf(buf, 50, "$%04x,x", n);
    return static_cast<i32>((r->DBR << 16) + n + r->X);
}

static i32 do_absolute_y(ARGS)
{
    *ins_len = 3;
    u32 n = read_op_word(addr, rt);
    snprintf(buf, 50, "$%04x,y", n);
    return static_cast<i32>((r->DBR << 16) + n + r->Y);
}

static i32 do_absolute_long(ARGS)
{
    *ins_len = 4;
    u32 n = read_op_long(addr, rt);
    snprintf(buf, 50, "$%06x", n);
    return -1;
}

static i32 do_absolute_long_x(ARGS) {
    *ins_len = 4;
    u32 n = read_op_long(addr, rt);
    snprintf(buf, 50, "$%06x,x", n);
    return static_cast<i32>(n + r->X);
}

static i32 do_direct(ARGS) {
    *ins_len = 2;
    u32 n = read_op_byte(addr, rt);
    snprintf(buf, 50, "$%02x", n);
    return static_cast<i32>((r->D + n) & 0xFFFF);
}

static i32 do_direct_x(ARGS) {
    *ins_len = 2;
    u32 n = read_op_byte(addr, rt);
    snprintf(buf, 50, "$%02x,x", n);
    return static_cast<i32>((r->D + n + r->X) & 0xFFFF);
}

static i32 do_direct_y(ARGS) {
    *ins_len = 2;
    u32 n = read_op_byte(addr, rt);
    snprintf(buf, 50, "$%02x,y", n);
    return static_cast<i32>((r->D + n + r->Y) & 0xFFFF);
}

static i32 do_immediate(ARGS)
{
    *ins_len = 2;

    snprintf(buf, 50, "#$%02x", read_op_byte(addr, rt));
    return -1;
}

static i32 do_immediate_a(ARGS) {
    u32 n;
    if (m) {
        *ins_len = 2;
        n = read_op_byte(addr, rt);
        snprintf(buf, 50, "#$%02x", n);
    }
    else {
        *ins_len = 3;
        n = read_op_word(addr, rt);
        snprintf(buf, 50, "#$%04x", n);
    }
    return -1;
}

static i32 do_immediate_x(ARGS) {
    u32 n;
    if (m) {
        *ins_len = 2;
        n = read_op_byte(addr, rt);
        snprintf(buf, 50, "#$%02x", n);
    }
    else {
        *ins_len = 3;
        n = read_op_word(addr, rt);
        snprintf(buf, 50, "#$%04x", n);
    }
    return -1;
}

static i32 do_implied(ARGS) {
    buf[0] = 0;
    return -1;
}

static i32 do_indexed_indirect_x(ARGS) {
    *ins_len = 2;
    u32 n = read_op_byte(addr, rt);
    snprintf(buf, 50, "($%02x,x)", n);
    return static_cast<i32>((r->DBR << 16) | read_word((r->D + n + r->X) & 0xFFFF, rt));
}

static i32 do_indirect(ARGS) {
    *ins_len = 2;
    u32 n = read_op_byte(addr, rt);

    snprintf(buf, 50, "($%02x)", n);
    return static_cast<i32>((r->DBR << 16) + read_word((r->D + n) & 0xFFFF, rt));
}

static i32 do_indirect_pc(ARGS) {
    *ins_len = 3;
    u32 n = read_op_word(addr, rt);

    snprintf(buf, 50, "($%04x)", n);
    return static_cast<i32>((addr & 0xFF0000) | read_word(n, rt));
}

static i32 do_indirect_x(ARGS) {
    *ins_len = 3;
    u32 n = read_op_word(addr, rt);

    snprintf(buf, 50, "($%04x,x)", n);

    return static_cast<i32>((addr & 0xFF0000) | read_word((addr & 0xFF0000) | ((n + r->X) & 0xFFFF), rt));
}

static i32 do_indirect_indexed_y(ARGS) {
    *ins_len = 2;

    u32 n = read_op_byte(addr, rt);

    snprintf(buf, 50, "($%02x),y", n);

    return static_cast<i32>((r->DBR << 16) | read_word((r->D + n) & 0xFFFF, rt) + r->Y);
}

static i32 do_indirect_long(ARGS) {
    *ins_len = 2;
    u32 n = read_op_byte(addr, rt);

    snprintf(buf, 50, "[$%02x]", n);

    return static_cast<i32>(read_long((r->D + n) & 0xFFFF, rt));
}

static i32 do_indirect_long_pc(ARGS) {
    *ins_len = 3;

    u32 n = read_op_word(addr, rt);

    snprintf(buf, 50, "[$%04x]", n);

    return static_cast<i32>(read_long(n, rt));
}

static i32 do_indirect_long_y(ARGS) {
    *ins_len = 2;

    u32 n = read_op_byte(addr, rt);

    snprintf(buf, 50, "[$%02x],y", n);
    return static_cast<i32>(read_long((r->D + n) & 0xFFFF, rt) + r->Y);
}


static i32 do_move(ARGS) {
    *ins_len = 3;
    u32 n0 = read_op_byte(addr, rt);
    addr = inc_addr16of24(addr);
    u32 n1 = read_byte(addr, rt);

    snprintf(buf, 50, "$%02x=$%02x", n0, n1);
    return -1;
}

static i32 do_relative(ARGS) {
    *ins_len = 2;
    u32 n = (addr & 0xFF0000) | ((addr + 1 + static_cast<i32>(static_cast<i8>(read_op_byte(addr, rt)))) & 0xFFFF);
    snprintf(buf, 50, "$%04x", n & 0xFFFF);
    return static_cast<i32>(n);
}

static i32 do_relative_word(ARGS) {
    *ins_len = 3;
    u32 n = static_cast<u32>(static_cast<i16>(read_op_word(addr, rt)));
    n = (addr & 0xFF0000) | ((addr + 3 + static_cast<u32>(static_cast<i16>(n))) & 0xFFFF);

    snprintf(buf, 50, "$%06x", n);
    return static_cast<i32>(n);
}

static i32 do_stack(ARGS) {
    *ins_len = 2;

    u32 n = read_op_byte(addr, rt);

    snprintf(buf, 50, "$%02x,s", n);

    return static_cast<i32>((r->S + n) & 0xFFFF);
}

static i32 do_stack_indirect(ARGS) {
    *ins_len = 2;

    u32 n = read_op_byte(addr, rt);

    snprintf(buf, 50, "($%02x,s),y", n);

    return static_cast<i32>((r->DBR << 16) + read_word((n + r->S) & 0xFFFF, rt) + r->Y);
}

u32 WDC65816_disassemble(u32 addr, WDC65816_regs &r, u32 e, u32 m, u32 x, jsm_debug_read_trace &rt, jsm_string &out, WDC65816_ctxt *ct)
{
    char buf[50];
    const char *mnemonic;
    u32 opcode = read_byte(addr, &rt);
    addr = inc_addr16of24(addr);
    u32 tct = 0;
    u32 *rptr = &tct;
    if (ct) rptr = &ct->regs;

    i32 effective = -1;
    u32 ins_len = 0;

#define dasm(num, mnm, func) case num: mnemonic = mnm; effective = do_##func(buf, addr, e, m, x, &r, &rt, &ins_len, rptr); break

    switch(opcode) {
        dasm(0x00, "brk", immediate);
        dasm(0x01, "ora", indexed_indirect_x);
        dasm(0x02, "cop", immediate);
        dasm(0x03, "ora", stack);
        dasm(0x04, "tsb", direct);
        dasm(0x05, "ora", direct);
        dasm(0x06, "asl", direct);
        dasm(0x07, "ora", indirect_long);
        dasm(0x08, "php", implied);
        dasm(0x09, "ora", immediate_a);
        dasm(0x0a, "asl", implied);
        dasm(0x0b, "phd", implied);
        dasm(0x0c, "tsb", absolute);
        dasm(0x0d, "ora", absolute);
        dasm(0x0e, "asl", absolute);
        dasm(0x0f, "ora", absolute_long);
        dasm(0x10, "bpl", relative);
        dasm(0x11, "ora", indirect_indexed_y);
        dasm(0x12, "ora", indirect);
        dasm(0x13, "ora", stack_indirect);
        dasm(0x14, "trb", direct);
        dasm(0x15, "ora", direct_x);
        dasm(0x16, "asl", direct_x);
        dasm(0x17, "ora", indirect_long_y);
        dasm(0x18, "clc", implied);
        dasm(0x19, "ora", absolute_y);
        dasm(0x1a, "inc", implied);
        dasm(0x1b, "tas", implied);
        dasm(0x1c, "trb", absolute);
        dasm(0x1d, "ora", absolute_x);
        dasm(0x1e, "asl", absolute_x);
        dasm(0x1f, "ora", absolute_long_x);
        dasm(0x20, "jsr", absolute_pc);
        dasm(0x21, "and", indexed_indirect_x);
        dasm(0x22, "jsl", absolute_long);
        dasm(0x23, "and", stack);
        dasm(0x24, "bit", direct);
        dasm(0x25, "and", direct);
        dasm(0x26, "rol", direct);
        dasm(0x27, "and", indirect_long);
        dasm(0x28, "plp", implied);
        dasm(0x29, "and", immediate_a);
        dasm(0x2a, "rol", implied);
        dasm(0x2b, "pld", implied);
        dasm(0x2c, "bit", absolute);
        dasm(0x2d, "and", absolute);
        dasm(0x2e, "rol", absolute);
        dasm(0x2f, "and", absolute_long);
        dasm(0x30, "bmi", relative);
        dasm(0x31, "and", indirect_indexed_y);
        dasm(0x32, "and", indirect);
        dasm(0x33, "and", stack_indirect);
        dasm(0x34, "bit", direct_x);
        dasm(0x35, "and", direct_x);
        dasm(0x36, "rol", direct_x);
        dasm(0x37, "and", indirect_long_y);
        dasm(0x38, "sec", implied);
        dasm(0x39, "and", absolute_y);
        dasm(0x3a, "dec", implied);
        dasm(0x3b, "tsa", implied);
        dasm(0x3c, "bit", absolute_x);
        dasm(0x3d, "and", absolute_x);
        dasm(0x3e, "rol", absolute_x);
        dasm(0x3f, "and", absolute_long_x);
        dasm(0x40, "rti", implied);
        dasm(0x41, "eor", indexed_indirect_x);
        dasm(0x42, "wdm", immediate);
        dasm(0x43, "eor", stack);
        dasm(0x44, "mvp", move);
        dasm(0x45, "eor", direct);
        dasm(0x46, "lsr", direct);
        dasm(0x47, "eor", indirect_long);
        dasm(0x48, "pha", implied);
        dasm(0x49, "eor", immediate_a);
        dasm(0x4a, "lsr", implied);
        dasm(0x4b, "phk", implied);
        dasm(0x4c, "jmp", absolute_pc);
        dasm(0x4d, "eor", absolute);
        dasm(0x4e, "lsr", absolute);
        dasm(0x4f, "eor", absolute_long);
        dasm(0x50, "bvc", relative);
        dasm(0x51, "eor", indirect_indexed_y);
        dasm(0x52, "eor", indirect);
        dasm(0x53, "eor", stack_indirect);
        dasm(0x54, "mvn", move);
        dasm(0x55, "eor", direct_x);
        dasm(0x56, "lsr", direct_x);
        dasm(0x57, "eor", indirect_long_y);
        dasm(0x58, "cli", implied);
        dasm(0x59, "eor", absolute_y);
        dasm(0x5a, "phy", implied);
        dasm(0x5b, "tad", implied);
        dasm(0x5c, "jml", absolute_long);
        dasm(0x5d, "eor", absolute_x);
        dasm(0x5e, "lsr", absolute_x);
        dasm(0x5f, "eor", absolute_long_x);
        dasm(0x60, "rts", implied);
        dasm(0x61, "adc", indexed_indirect_x);
        dasm(0x62, "per", relative_word);
        dasm(0x63, "adc", stack);
        dasm(0x64, "stz", direct);
        dasm(0x65, "adc", direct);
        dasm(0x66, "ror", direct);
        dasm(0x67, "adc", indirect_long);
        dasm(0x68, "pla", implied);
        dasm(0x69, "adc", immediate_a);
        dasm(0x6a, "ror", implied);
        dasm(0x6b, "rtl", implied);
        dasm(0x6c, "jmp", indirect_pc);
        dasm(0x6d, "adc", absolute);
        dasm(0x6e, "ror", absolute);
        dasm(0x6f, "adc", absolute_long);
        dasm(0x70, "bvs", relative);
        dasm(0x71, "adc", indirect_indexed_y);
        dasm(0x72, "adc", indirect);
        dasm(0x73, "adc", stack_indirect);
        dasm(0x74, "stz", absolute_x);
        dasm(0x75, "adc", absolute_x);
        dasm(0x76, "ror", absolute_x);
        dasm(0x77, "adc", indirect_long_y);
        dasm(0x78, "sei", implied);
        dasm(0x79, "adc", absolute_y);
        dasm(0x7a, "ply", implied);
        dasm(0x7b, "tda", implied);
        dasm(0x7c, "jmp", indirect_x);
        dasm(0x7d, "adc", absolute_x);
        dasm(0x7e, "ror", absolute_x);
        dasm(0x7f, "adc", absolute_long_x);
        dasm(0x80, "bra", relative);
        dasm(0x81, "sta", indexed_indirect_x);
        dasm(0x82, "brl", relative_word);
        dasm(0x83, "sta", stack);
        dasm(0x84, "sty", direct);
        dasm(0x85, "sta", direct);
        dasm(0x86, "stx", direct);
        dasm(0x87, "sta", indirect_long);
        dasm(0x88, "dey", implied);
        dasm(0x89, "bit", immediate_a);
        dasm(0x8a, "txa", implied);
        dasm(0x8b, "phb", implied);
        dasm(0x8c, "sty", absolute);
        dasm(0x8d, "sta", absolute);
        dasm(0x8e, "stx", absolute);
        dasm(0x8f, "sta", absolute_long);
        dasm(0x90, "bcc", relative);
        dasm(0x91, "sta", indirect_indexed_y);
        dasm(0x92, "sta", indirect);
        dasm(0x93, "sta", stack_indirect);
        dasm(0x94, "sty", direct_x);
        dasm(0x95, "sta", direct_x);
        dasm(0x96, "stx", direct_y);
        dasm(0x97, "sta", indirect_long_y);
        dasm(0x98, "tya", implied);
        dasm(0x99, "sta", absolute_y);
        dasm(0x9a, "txs", implied);
        dasm(0x9b, "txy", implied);
        dasm(0x9c, "stz", absolute);
        dasm(0x9d, "sta", absolute_x);
        dasm(0x9e, "stz", absolute_x);
        dasm(0x9f, "sta", absolute_long_x);
        dasm(0xa0, "ldy", immediate_x);
        dasm(0xa1, "lda", indexed_indirect_x);
        dasm(0xa2, "ldx", immediate_x);
        dasm(0xa3, "lda", stack);
        dasm(0xa4, "ldy", direct);
        dasm(0xa5, "lda", direct);
        dasm(0xa6, "ldx", direct);
        dasm(0xa7, "lda", indirect_long);
        dasm(0xa8, "tay", implied);
        dasm(0xa9, "lda", immediate_a);
        dasm(0xaa, "tax", implied);
        dasm(0xab, "plb", implied);
        dasm(0xac, "ldy", absolute);
        dasm(0xad, "lda", absolute);
        dasm(0xae, "ldx", absolute);
        dasm(0xaf, "lda", absolute_long);
        dasm(0xb0, "bcs", relative);
        dasm(0xb1, "lda", indirect_indexed_y);
        dasm(0xb2, "lda", indirect);
        dasm(0xb3, "lda", stack_indirect);
        dasm(0xb4, "ldy", direct_x);
        dasm(0xb5, "lda", direct_x);
        dasm(0xb6, "ldx", direct_y);
        dasm(0xb7, "lda", indirect_long_y);
        dasm(0xb8, "clv", implied);
        dasm(0xb9, "lda", absolute_y);
        dasm(0xba, "tsx", implied);
        dasm(0xbb, "tyx", implied);
        dasm(0xbc, "ldy", absolute_x);
        dasm(0xbd, "lda", absolute_x);
        dasm(0xbe, "ldx", absolute_y);
        dasm(0xbf, "lda", absolute_long_x);
        dasm(0xc0, "cpy", immediate_x);
        dasm(0xc1, "cmp", indexed_indirect_x);
        dasm(0xc2, "rep", immediate);
        dasm(0xc3, "cmp", stack);
        dasm(0xc4, "cpy", direct);
        dasm(0xc5, "cmp", direct);
        dasm(0xc6, "dec", direct);
        dasm(0xc7, "cmp", indirect_long);
        dasm(0xc8, "iny", implied);
        dasm(0xc9, "cmp", immediate_a);
        dasm(0xca, "dex", implied);
        dasm(0xcb, "wai", implied);
        dasm(0xcc, "cpy", absolute);
        dasm(0xcd, "cmp", absolute);
        dasm(0xce, "dec", absolute);
        dasm(0xcf, "cmp", absolute_long);
        dasm(0xd0, "bne", relative);
        dasm(0xd1, "cmp", indirect_indexed_y);
        dasm(0xd2, "cmp", indirect);
        dasm(0xd3, "cmp", stack_indirect);
        dasm(0xd4, "pei", indirect);
        dasm(0xd5, "cmp", direct_x);
        dasm(0xd6, "dec", direct_x);
        dasm(0xd7, "cmp", indirect_long_y);
        dasm(0xd8, "cld", implied);
        dasm(0xd9, "cmp", absolute_y);
        dasm(0xda, "phx", implied);
        dasm(0xdb, "stp", implied);
        dasm(0xdc, "jmp", indirect_long_pc);
        dasm(0xdd, "cmp", absolute_x);
        dasm(0xde, "dec", absolute_x);
        dasm(0xdf, "cmp", absolute_long_x);
        dasm(0xe0, "cpx", immediate_x);
        dasm(0xe1, "sbc", indexed_indirect_x);
        dasm(0xe2, "sep", immediate);
        dasm(0xe3, "sbc", stack);
        dasm(0xe4, "cpx", direct);
        dasm(0xe5, "sbc", direct);
        dasm(0xe6, "inc", direct);
        dasm(0xe7, "sbc", indirect_long);
        dasm(0xe8, "inx", implied);
        dasm(0xe9, "sbc", immediate_a);
        dasm(0xea, "nop", implied);
        dasm(0xeb, "xba", implied);
        dasm(0xec, "cpx", absolute);
        dasm(0xed, "sbc", absolute);
        dasm(0xee, "inc", absolute);
        dasm(0xef, "sbc", absolute_long);
        dasm(0xf0, "beq", relative);
        dasm(0xf1, "sbc", indirect_indexed_y);
        dasm(0xf2, "sbc", indirect);
        dasm(0xf3, "sbc", stack_indirect);
        dasm(0xf4, "pea", absolute);
        dasm(0xf5, "sbc", direct_x);
        dasm(0xf6, "inc", direct_x);
        dasm(0xf7, "sbc", indirect_long_y);
        dasm(0xf8, "sed", implied);
        dasm(0xf9, "sbc", absolute_y);
        dasm(0xfa, "plx", implied);
        dasm(0xfb, "xce", implied);
        dasm(0xfc, "jsr", indirect_x);
        dasm(0xfd, "sbc", absolute_x);
        dasm(0xfe, "inc", absolute_x);
        dasm(0xff, "sbc", absolute_long_x);
        default:
            printf("\nIMPOSSIBLE!");
            abort();
    }

#undef dasm
    u32 l = out.sprintf("%s %s", mnemonic, buf);
    if (effective != -1) {
        effective &= 0xFFFFFF;
        while (l < 14) l += out.sprintf(" ");
        out.sprintf("[%06x]", effective);
    }
    
    return ins_len;
}