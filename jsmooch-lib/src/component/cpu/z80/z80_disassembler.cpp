//
// Created by RadDad772 on 2/25/24.
//

#include "helpers/debug.h"
#include "z80_disassembler.h"
#include "z80.h"
namespace Z80 {
static const char *Z80D_tabl_r[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
static const char *Z80D_tabl_rp[] = {"BC", "DE", "HL", "SP"};
static const char *Z80D_tabl_rp2[] = {"BC", "DE", "HL", "AF"};
static const char *Z80D_tabl_cc[] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
static const char *Z80D_tabl_alu[] = {"ADD A, ", "ADC A, ", "SUB", "SBC A, ", "AND", "XOR", "OR", "CP"};
static const char *Z80D_tabl_rot[] = {"RLC", "RRC", "RL", "RR", "SLA", "SRA", "SSL", "SRL"};
static const char *Z80D_tabl_im[] = {"0", "0/1", "1", "2", "0", "0/1", "1", "2"};
static const char *Z80D_tabl_bli[8][10] = {{}, {}, {}, {},
{"LDI", "CPI", "INI", "OUTI"}, // 4, 0...3
{"LDD", "CPD", "IND", "OUTD"}, // 5, 0...3
{"LDIR", "CPIR", "INIR", "OTIR"}, // 6, 0...3
{"LDDR", "CPDR", "INDR", "OTDR"}, // 7, 0...3
};

static u32 read8(u32 *PC, jsm_debug_read_trace *rt) {

    u32 r = rt->read_trace(rt->ptr, *PC);
    *PC = ((*PC) + 1) & 0xFFFF;
    return r;
}

static u32 repl0(u32 *PC, const char *HL, const char *H, const char *L, jsm_debug_read_trace* rt, const char *reg, char *w, size_t sz) {
    u32 l;
    if (!strcmp(reg, "HL")) l = snprintf(w, sz, "%s", HL);
    else if (!strcmp(reg, "L")) l = snprintf(w, sz, "%s", L);
    else if (!strcmp(reg, "H")) l = snprintf(w, sz, "%s", H);
    else if (!strcmp(reg, "(HL)")) {
        if (!strcmp(HL, "HL"))
            l = snprintf(w, sz, "(HL)");
        else {
            l = snprintf(w, sz, "(%s+%d)", HL, read8(PC, rt));
        }
    }
    else {
        l = snprintf(w, sz, "%s", reg);
    }
    return l;
}

static u32 fetch(u32 *PC, jsm_debug_read_trace *rt) {
    u32 r = rt->read_trace(rt->ptr, *PC);
    *PC = ((*PC) + 1) & 0xFFFF;
    return r;
}

static u32 sread8(u32 *PC, jsm_debug_read_trace *rt) {
    u32 r = static_cast<u32>(static_cast<i32>(static_cast<i8>(rt->read_trace(rt->ptr, *PC))));
    *PC = ((*PC) + 1) & 0xFFFF;
    return ((*PC) + r) & 0xFFFF;
}

static u32 read16(u32* PC, jsm_debug_read_trace *rt) {
    u32 r = rt->read_trace(rt->ptr, *PC) + (rt->read_trace(rt->ptr, ((*PC)+1) & 0xFFFF) << 8);
    *PC = ((*PC) + 2) & 0xFFFF;
    return r;
}


void disassemble_entry(core *th, disassembly_entry* entry)
{
    entry->dasm.quickempty();
    entry->context.quickempty();
    u32 PC = entry->addr;
    u32 IR = th->read_trace.read_trace(th->read_trace.ptr, PC);
    char buf[100];
    buf[0] = 0;
    disassemble(&PC, IR, &th->read_trace, buf, sizeof(buf));
    entry->dasm.sprintf("%s", buf);
    entry->ins_size_bytes = PC - entry->addr;
}

u32 Z80_disassemble(u32 *PC, u32 IR, jsm_debug_read_trace *rt, char *w, size_t sz)
{
    u32 l = 0;
    u32 opcode = IR;
    *PC = ((*PC) + 1) & 0xFFFF;
    if (IR == S_DECODE) {
        return snprintf(w, sz, "DECODE");
    }
    else if (IR == S_RESET) {
        return snprintf(w, sz, "RESET");
    }
    else if (IR == S_IRQ) {
        return snprintf(w, sz, "IRQ");
    }

    char H[10], L[10], HL[10], IXY[10];

    u32 current_prefix = 0x00;
    u32 current_byte = opcode;

    u32 decoded_bytes = 0;
    snprintf(H, 10, "H");
    snprintf(L, 10, "L");
    snprintf(HL, 10, "HL");
    // So we don"t loop forever
    // Decode regular
    while(decoded_bytes < 16) {
        // First decide what to do: refresh prefix or decode
        if (current_byte == 0xDD) {
            current_prefix = 0xDD;
            decoded_bytes++;
            current_byte = fetch(PC, rt);
            continue;
        }

        if (current_byte == 0xFD) {
            current_prefix = 0xFD;
            decoded_bytes++;
            current_byte = fetch(PC, rt);
            continue;
        }

        if ((current_byte == 0xCB) && (current_prefix == 0)) {
            // prefix = CB for reglar
            current_prefix = 0xCB;
            current_byte = fetch(PC, rt);
            decoded_bytes++;
            continue;
        }
        else if ((current_byte == 0xCB) && ((current_prefix == 0xDD) || (current_prefix == 0xFD))) {
            current_prefix = (current_prefix << 8) | 0xCB;
            current_byte = fetch(PC, rt);
            decoded_bytes++;
            continue;
        }
        else if (current_byte == 0xED) {
            current_prefix = 0xED; // lose IX/IY
            decoded_bytes++;
            current_byte = fetch(PC, rt);
            continue;
        }

        u32 x = (current_byte & 0xC0) >> 6;
        u32 y = (current_byte & 0x38) >> 3;
        u32 z = (current_byte & 7);
        u32 p = (y >> 1);
        u32 q = y % 2;
        snprintf(HL, 10, "HL");
        snprintf(H, 10, "H");
        snprintf(L, 10, "L");
        switch(current_prefix) {
            case 0xDD:
            case 0xFD:
            case 0x00:
                if (current_prefix == 0xDD) {
                    //console.log("DOIN IX");
                    snprintf(HL, 10, "IX");
                    snprintf(L, 10, "IXL");
                    snprintf(H, 10, "IXH");
                }
                else if (current_prefix == 0xFD) {
                    //console.log("DOIN IY");
                    snprintf(HL, 10, "IY");
                    snprintf(L, 10, "IYL");
                    snprintf(H, 10, "IYH");
                }
                switch (x) {
                    case 0: // counter = 0
                        switch (z) {
                            case 0: // counter=0 z=0
                                switch (y) {
                                    case 0:
                                        l += snprintf(w, sz, "NOP");
                                        break;
                                    case 1:
                                        l += snprintf(w, sz, "EX AF, AF'");
                                        break;
                                    case 2:
                                        l += snprintf(w, sz, "DJNZa %04x", sread8(PC, rt));
                                        break;
                                    case 3:
                                        l += snprintf(w, sz, "JR %04x", sread8(PC, rt));
                                        break;
                                    case 4:
                                    case 5:
                                    case 6:
                                    case 7:
                                        l += snprintf(w, sz, "JR %s, %04x", Z80D_tabl_cc[y - 4], sread8(PC, rt));
                                        break;
                                }
                                break;
                            case 1: // counter = 0, z = 1
                                if (q == 0) {
                                    l += snprintf(w, sz, "LD ");
                                    w += 3;
                                    u32 a = repl0(PC, HL, H, L, rt, Z80D_tabl_rp[p], w, sz-3);
                                    l += a;
                                    w += a;
                                    l += snprintf(w, sz - l, ", %04x", read16(PC, rt));
                                }
                                else {
                                    l += snprintf(w, sz, "ADD ");
                                    w += 4;
                                    u32 a = repl0(PC, HL, H, L, rt, "HL", w, sz-4);
                                    l += a;
                                    w += a;
                                    l += snprintf(w, sz - l, ", %s", Z80D_tabl_rp[p]);
                                }
                                break;
                            case 2: // counter = 0, z = 2
                                switch (p) {
                                    case 0:
                                        if (q == 0) l += snprintf(w, sz, "LD (BC), A");
                                        else l += snprintf(w, sz, "LD A, (BC)");
                                        break;
                                    case 1:
                                        if (q == 0) l += snprintf(w, sz, "LD (DE), A");
                                        else l += snprintf(w, sz, "LD A, (DE)");
                                        break;
                                    case 2:
                                        if (q == 0) {
                                            l += snprintf(w, sz, "LD (%04x), ", read16(PC, rt));
                                            w += l;
                                            l += repl0(PC, HL, H, L, rt, "HL", w, sz - l);
                                        }
                                        else {
                                            l += snprintf(w, sz, "LD ");
                                            w += 3;
                                            u32 a = repl0(PC, HL, H, L, rt, "HL", w, sz - l);
                                            w += a;
                                            l += a;
                                            snprintf(w, sz - l, ",(%04x)", read16(PC, rt));
                                        }
                                        break;
                                    case 3:
                                        if (q == 0) l += snprintf(w, sz, "LD (%04x), A", read16(PC, rt));
                                        else l += snprintf(w, sz, "LD A, (%04x)", read16(PC, rt));
                                        break;
                                }
                                break;
                            case 3: // counter = 0, z = 3
                                if (q == 0) l += snprintf(w, sz, "INC ");
                                else l += snprintf(w, sz, "DEC ");
                                l += repl0(PC, HL, H, L, rt, Z80D_tabl_rp[p], w+4, sz - l);
                                break;
                            case 4: // counter = 0 z = 3
                                l += snprintf(w, sz, "INC ");
                                l += repl0(PC, HL, H, L, rt, Z80D_tabl_r[y], w+4, sz - l);
                                break;
                            case 5:
                                l += snprintf(w, sz, "DEC ");
                                l += repl0(PC, HL, H, L, rt, Z80D_tabl_r[y], w+4, sz - l);
                                break;
                            case 6: {
                                l += snprintf(w, sz, "LD ");
                                w += 3;
                                u32 a = repl0(PC, HL, H, L, rt, Z80D_tabl_r[y], w, sz - l);
                                l += a;
                                w += a;
                                l += snprintf(w, sz, ", %02x", read8(PC, rt));
                                break; }
                            case 7: {
                                const char *att[] = {"RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"};
                                l += snprintf(w, sz, "%s", att[y]);
                                break;
                            }
                        }
                        break;
                    case 1: // counter = 1
                        if ((z == 6) && (y == 6)) l += snprintf(w, sz, "HALT");
                        else {
                            l += snprintf(w, sz, "LD ");
                            w += 3;
                            u32 a = repl0(PC, HL, H, L, rt, Z80D_tabl_r[y], w, sz - l);
                            l += a;
                            w += a;
                            l += snprintf(w, sz - l, ", ");
                            w += 2;
                            l += repl0(PC, HL, H, L, rt, Z80D_tabl_r[z], w, sz - l);
                        }
                        break;
                    case 2: {// counter = 2
                        u32 a = snprintf(w, sz, "%s ", Z80D_tabl_alu[y]);
                        l += a;
                        w += a;
                        l += repl0(PC, HL, H, L, rt, Z80D_tabl_r[z], w, sz - l);
                        break;
                    }
                    case 3: // counter = 3
                        switch (z) {
                            case 0: // counter=3 z=0
                                l += snprintf(w, sz, "RET %s", Z80D_tabl_cc[y]);
                                break;
                            case 1: // counter=3 z=1
                                if (q == 0) {
                                    l += snprintf(w, sz, "POP ");
                                    w += 4;
                                    l += repl0(PC, HL, H, L, rt, Z80D_tabl_rp2[p], w, sz - l);
                                }
                                else {
                                    switch(p) {
                                        case 0:
                                            l += snprintf(w, sz, "RET");
                                            break;
                                        case 1:
                                            l += snprintf(w, sz, "EXX");
                                            break;
                                        case 2:
                                            l += snprintf(w, sz, "JP HL");
                                            break;
                                        default:
                                            l += snprintf(w, sz, "LD SP, ");
                                            w += 7;
                                            l += repl0(PC, HL, H, L, rt, "HL", w, sz - l);
                                    }
                                }
                                break;
                            case 2:
                                l += snprintf(w, sz, "JP %s, %04x", Z80D_tabl_cc[y], read16(PC, rt));
                                break;
                            case 3: // counter=3 z=3
                                switch (y) {
                                    case 0:
                                        l += snprintf(w, sz, "JP %04x", read16(PC, rt));
                                        break;
                                    case 1:
                                        l += snprintf(w, sz, "INVALID1");
                                        break;
                                    case 2:
                                        l += snprintf(w, sz, "OUT (%02x), A", read8(PC, rt));
                                        break;
                                    case 3:
                                        l += snprintf(w, sz, "IN (%02X), A", read8(PC, rt));
                                        break;
                                    case 4:
                                        l += snprintf(w, sz, "EX (SP), ");
                                        w += 9;
                                        l += repl0(PC, HL, H, L, rt, "HL", w, sz - l);
                                        break;
                                    case 5:
                                        l += snprintf(w, sz, "EX DE, ");
                                        w += 7;
                                        l += repl0(PC, HL, H, L, rt, "HL", w, sz - l);
                                        break;
                                    case 6:
                                        l += snprintf(w, sz, "DI");
                                        break;
                                    case 7:
                                        l += snprintf(w, sz, "EI");
                                        break;
                                }
                                break;
                            case 4: // counter=3 z=4
                                l += snprintf(w, sz, "CALL %s, %04x", Z80D_tabl_cc[y], read16(PC, rt));
                                break;
                            case 5: // counter=3 z=5
                                if (q == 0) {
                                    l += snprintf(w, sz, "PUSH ");
                                    w += 5;
                                    l += repl0(PC, HL, H, L, rt, Z80D_tabl_rp2[p], w, sz - l);
                                }
                                else
                                    l += snprintf(w, sz, "CALL %04x", read16(PC, rt));
                                break;
                            case 6: // counter=3 z=6
                                l += snprintf(w, sz, "%s %02x", Z80D_tabl_alu[y], read8(PC, rt));
                                break;
                            case 7: // counter=3 z=7
                                l += snprintf(w, sz, "RST %02x", y * 8);
                                break;
                        }
                    default: ;
                }
                break;
            case 0xCB: // prefix 0xCB
                switch(x) {
                    case 0:
                        l += snprintf(w, sz, "%s %s", Z80D_tabl_rot[y], Z80D_tabl_r[z]);
                        break;
                    case 1:
                        l += snprintf(w, sz, "TEST %d, %s", y, Z80D_tabl_r[z]);
                        break;
                    case 2:
                        l += snprintf(w, sz, "RES %d, %s", y, Z80D_tabl_r[z]);
                        break;
                    case 3:
                        l += snprintf(w, sz, "SET %d, %s", y, Z80D_tabl_r[z]);
                        break;
                }
                break;
            case 0xED:
                switch(x) {
                    case 0:
                    case 3:
                        l += snprintf(w, sz, "INVALID NONI NOP");
                        break;
                    case 1: // 0xED counter=1
                        switch(z) {
                            case 0: // 0xED counter=1 z=0
                                if (y == 6) l += snprintf(w, sz, "IN (C)");
                                else l += snprintf(w, sz, "IN %s, (C)", Z80D_tabl_r[y]);
                                break;
                            case 1:
                                if (y == 6) l += snprintf(w, sz, "OUT (C)");
                                else l += snprintf(w, sz, "OUT %s, (C)", Z80D_tabl_r[y]);
                                break;
                            case 2: // 0xED counter=1 z=2
                                if (q == 0) l += snprintf(w, sz, "SBC HL, %s", Z80D_tabl_rp[p]);
                                else l += snprintf(w, sz, "ADC HL, %s", Z80D_tabl_rp[p]);
                                break;
                            case 3: // 0xED counter=1 z=3
                                if (q == 0) l += snprintf(w, sz, "LD (%04x), %s", read16(PC, rt), Z80D_tabl_rp[p]);
                                else l += snprintf(w, sz, "LD %s, (%04x)", Z80D_tabl_rp[p], read16(PC, rt));
                                break;
                            case 4:
                                l += snprintf(w, sz, "NEG");
                                break;
                            case 5:
                                if (y == 1) l += snprintf(w, sz, "RETI");
                                else l += snprintf(w, sz, "RETN");
                                break;
                            case 6: // 0xED counter=1 z=6
                                l += snprintf(w, sz, "IM %s", Z80D_tabl_im[y]);
                                break;
                            case 7: // 0xED counter=1 z=7
                                l += snprintf(w, sz, "%s", (char[8][10]){"LD I, A", "LD R, A", "LD A, I", "LD A, R", "RRD", "RLD", "NOP", "NOP"}[y]);
                                break;
                        }
                        break;
                    case 2: // 0xED counter=2
                        if ((z <= 3) && (y >= 4)) {
                            l += snprintf(w, sz, "%s", Z80D_tabl_bli[y][z]);
                        }
                        else
                            l += snprintf(w, sz, "INVALID#2 NONI NOP");
                        break;
                }
                break;
            case 0xDDCB:
            case 0xFDCB:
                if (current_prefix == 0xDDCB) strcpy(IXY, "IX");
                else strcpy(IXY, "IY");
                char d[20];
                snprintf(d, sizeof(d), "$%02x", current_byte);
                current_byte = fetch(PC, rt);
                decoded_bytes++;
                x = (current_byte & 0xC0) >> 6;
                y = (current_byte & 0x38) >> 3;
                z = (current_byte & 7);
                switch(x) {
                    case 0: // CBd counter=0
                        if (z != 6) l += snprintf(w, sz, "LD %s, %s(%s+%s)", Z80D_tabl_r[z], Z80D_tabl_rot[y], IXY, d);
                        else l += snprintf(w, sz, "%s (%s+%s)", Z80D_tabl_rot[y], IXY, d);
                        break;
                    case 1: // CBd counter=1
                        l += snprintf(w, sz, "TEST %d, (%s+%s)", y, IXY, d);
                        break;
                    case 2: // CBd counter=2
                        if (z != 6)
                            l += snprintf(w, sz, "LD %s, RES %d, (%s+%s)", Z80D_tabl_r[z], y, IXY, d);
                        else
                            l += snprintf(w, sz, "RES %d, (%s+%s)", y, IXY, d);
                        break;
                    case 3: // CBd counter=3
                        if (z != 6)
                            l += snprintf(w, sz, "LD %s, SET %d, (%s+%s)", Z80D_tabl_r[z], y, IXY, d);
                        else
                            l += snprintf(w, sz, "SET %d, (%s+%s)", y, IXY, d);
                        break;
                }
                break;
            default:
                l += snprintf(w, sz, "HOW DID WE GET HERE GOVNA");
                break;
        }
        if (l != 0) break;
        decoded_bytes++;
    }
    return l;
}
}