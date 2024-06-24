//
// Created by RadDad772 on 2/25/24.
//

#include <stdio.h>
#include <string.h>
#include "helpers/debug.h"
#include "z80_disassembler.h"
#include "z80.h"

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

static u32 read8(u32 *PC, struct jsm_debug_read_trace *rt) {

    u32 r = rt->read_trace(rt->ptr, *PC);
    *PC = ((*PC) + 1) & 0xFFFF;
    return r;
}

static u32 repl0(u32 *PC, const char *HL, const char *H, const char *L, struct jsm_debug_read_trace* rt, const char *reg, char *w) {
    u32 l;
    if (!strcmp(reg, "HL")) l = sprintf(w, "%s", HL);
    else if (!strcmp(reg, "L")) l = sprintf(w, "%s", L);
    else if (!strcmp(reg, "H")) l = sprintf(w, "%s", H);
    else if (!strcmp(reg, "(HL)")) {
        if (!strcmp(HL, "HL"))
            l = sprintf(w, "(HL)");
        else {
            l = sprintf(w, "(%s+%d)", HL, read8(PC, rt));
        }
    }
    else {
        l = sprintf(w, "%s", reg);
    }
    return l;
}

static u32 fetch(u32 *PC, struct jsm_debug_read_trace *rt) {
    u32 r = rt->read_trace(rt->ptr, *PC);
    *PC = ((*PC) + 1) & 0xFFFF;
    return r;
}

static u32 sread8(u32 *PC, struct jsm_debug_read_trace *rt) {
    u32 r = (u32)((i32)(i8)rt->read_trace(rt->ptr, *PC));
    *PC = ((*PC) + 1) & 0xFFFF;
    return ((*PC) + r) & 0xFFFF;
}

static u32 read16(u32* PC, struct jsm_debug_read_trace *rt) {
    u32 r = rt->read_trace(rt->ptr, *PC) + (rt->read_trace(rt->ptr, ((*PC)+1) & 0xFFFF) << 8);
    *PC = ((*PC) + 2) & 0xFFFF;
    return r;
}

u32 Z80_disassemble(u32 PC, u32 IR, struct jsm_debug_read_trace *rt, char *w)
{
    u32 l = 0;
    u32 opcode = IR;
    PC = (PC + 1) & 0xFFFF;
    if (IR == Z80_S_DECODE) {
        return sprintf(w, "DECODE");
    }
    else if (IR == Z80_S_RESET) {
        return sprintf(w, "RESET");
    }
    else if (IR == Z80_S_IRQ) {
        return sprintf(w, "IRQ");
    }

    char H[10], L[10], HL[10], IXY[10];

    u32 current_prefix = 0x00;
    u32 current_byte = opcode;

    u32 decoded_bytes = 0;
    sprintf(H, "H");
    sprintf(L, "L");
    sprintf(HL, "HL");
    // So we don"t loop forever
    // Decode regular
    while(decoded_bytes < 16) {
        // First decide what to do: update prefix or decode
        if (current_byte == 0xDD) {
            current_prefix = 0xDD;
            decoded_bytes++;
            current_byte = fetch(&PC, rt);
            continue;
        }

        if (current_byte == 0xFD) {
            current_prefix = 0xFD;
            decoded_bytes++;
            current_byte = fetch(&PC, rt);
            continue;
        }

        if ((current_byte == 0xCB) && (current_prefix == 0)) {
            // prefix = CB for reglar
            current_prefix = 0xCB;
            current_byte = fetch(&PC, rt);
            decoded_bytes++;
            continue;
        }
        else if ((current_byte == 0xCB) && ((current_prefix == 0xDD) || (current_prefix == 0xFD))) {
            current_prefix = (current_prefix << 8) | 0xCB;
            current_byte = fetch(&PC, rt);
            decoded_bytes++;
            continue;
        }
        else if (current_byte == 0xED) {
            current_prefix = 0xED; // lose IX/IY
            decoded_bytes++;
            current_byte = fetch(&PC, rt);
            continue;
        }

        u32 x = (current_byte & 0xC0) >> 6;
        u32 y = (current_byte & 0x38) >> 3;
        u32 z = (current_byte & 7);
        u32 p = (y >> 1);
        u32 q = y % 2;
        sprintf(HL, "HL");
        sprintf(H, "H");
        sprintf(L, "L");
        switch(current_prefix) {
            case 0xDD:
            case 0xFD:
            case 0x00:
                if (current_prefix == 0xDD) {
                    //console.log("DOIN IX");
                    sprintf(HL, "IX");
                    sprintf(L, "IXL");
                    sprintf(H, "IXH");
                }
                else if (current_prefix == 0xFD) {
                    //console.log("DOIN IY");
                    sprintf(HL, "IY");
                    sprintf(L, "IYL");
                    sprintf(H, "IYH");
                }
                switch (x) {
                    case 0: // counter = 0
                        switch (z) {
                            case 0: // counter=0 z=0
                                switch (y) {
                                    case 0:
                                        l += sprintf(w, "NOP");
                                        break;
                                    case 1:
                                        l += sprintf(w, "EX AF, AF'");
                                        break;
                                    case 2:
                                        l += sprintf(w, "DJNZa %04x", sread8(&PC, rt));
                                        break;
                                    case 3:
                                        l += sprintf(w, "JR %04x", sread8(&PC, rt));
                                        break;
                                    case 4:
                                    case 5:
                                    case 6:
                                    case 7:
                                        l += sprintf(w, "JR %s, %04x", Z80D_tabl_cc[y - 4], sread8(&PC, rt));
                                        break;
                                }
                                break;
                            case 1: // counter = 0, z = 1
                                if (q == 0) {
                                    l += sprintf(w, "LD ");
                                    w += 3;
                                    u32 a = repl0(&PC, HL, H, L, rt, Z80D_tabl_rp[p], w);
                                    l += a;
                                    w += a;
                                    l += sprintf(w, ", %04x", read16(&PC, rt));
                                }
                                else {
                                    l += sprintf(w, "ADD ");
                                    w += 4;
                                    u32 a = repl0(&PC, HL, H, L, rt, "HL", w);
                                    l += a;
                                    w += a;
                                    l += sprintf(w, ", %s", Z80D_tabl_rp[p]);
                                }
                                break;
                            case 2: // counter = 0, z = 2
                                switch (p) {
                                    case 0:
                                        if (q == 0) l += sprintf(w, "LD (BC), A");
                                        else l += sprintf(w, "LD A, (BC)");
                                        break;
                                    case 1:
                                        if (q == 0) l += sprintf(w, "LD (DE), A");
                                        else l += sprintf(w, "LD A, (DE)");
                                        break;
                                    case 2:
                                        if (q == 0) {
                                            l += sprintf(w, "LD (%04x), ", read16(&PC, rt));
                                            w += l;
                                            l += repl0(&PC, HL, H, L, rt, "HL", w);
                                        }
                                        else {
                                            l += sprintf(w, "LD ");
                                            w += 3;
                                            u32 a = repl0(&PC, HL, H, L, rt, "HL", w);
                                            w += a;
                                            l += a;
                                            sprintf(w, ",(%04x)", read16(&PC, rt));
                                        }
                                        break;
                                    case 3:
                                        if (q == 0) l += sprintf(w, "LD (%04x), A", read16(&PC, rt));
                                        else l += sprintf(w, "LD A, (%04x)", read16(&PC, rt));
                                        break;
                                }
                                break;
                            case 3: // counter = 0, z = 3
                                if (q == 0) l += sprintf(w, "INC ");
                                else l += sprintf(w, "DEC ");
                                l += repl0(&PC, HL, H, L, rt, Z80D_tabl_rp[p], w+4);
                                break;
                            case 4: // counter = 0 z = 3
                                l += sprintf(w, "INC ");
                                l += repl0(&PC, HL, H, L, rt, Z80D_tabl_r[y], w+4);
                                break;
                            case 5:
                                l += sprintf(w, "DEC ");
                                l += repl0(&PC, HL, H, L, rt, Z80D_tabl_r[y], w+4);
                                break;
                            case 6:
                                l += sprintf(w, "LD ");
                                w += 3;
                                u32 a = repl0(&PC, HL, H, L, rt, Z80D_tabl_r[y], w);
                                l += a;
                                w += a;
                                l += sprintf(w, ", %02x", read8(&PC, rt));
                                break;
                            case 7: {
                                const char *att[] = {"RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"};
                                l += sprintf(w, "%s", att[y]);
                                break;
                            }
                        }
                        break;
                    case 1: // counter = 1
                        if ((z == 6) && (y == 6)) l += sprintf(w, "HALT");
                        else {
                            l += sprintf(w, "LD ");
                            w += 3;
                            u32 a = repl0(&PC, HL, H, L, rt, Z80D_tabl_r[y], w);
                            l += a;
                            w += a;
                            l += sprintf(w, ", ");
                            w += 2;
                            l += repl0(&PC, HL, H, L, rt, Z80D_tabl_r[z], w);
                        }
                        break;
                    case 2: {// counter = 2
                        u32 a = sprintf(w, "%s ", Z80D_tabl_alu[y]);
                        l += a;
                        w += a;
                        l += repl0(&PC, HL, H, L, rt, Z80D_tabl_r[z], w);
                        break;
                    }
                    case 3: // counter = 3
                        switch (z) {
                            case 0: // counter=3 z=0
                                l += sprintf(w, "RET %s", Z80D_tabl_cc[y]);
                                break;
                            case 1: // counter=3 z=1
                                if (q == 0) {
                                    l += sprintf(w, "POP ");
                                    w += 4;
                                    l += repl0(&PC, HL, H, L, rt, Z80D_tabl_rp2[p], w);
                                }
                                else {
                                    switch(p) {
                                        case 0:
                                            l += sprintf(w, "RET");
                                            break;
                                        case 1:
                                            l += sprintf(w, "EXX");
                                            break;
                                        case 2:
                                            l += sprintf(w, "JP HL");
                                            break;
                                        default:
                                            l += sprintf(w, "LD SP, ");
                                            w += 7;
                                            l += repl0(&PC, HL, H, L, rt, "HL", w);
                                    }
                                }
                                break;
                            case 2:
                                l += sprintf(w, "JP %s, %04x", Z80D_tabl_cc[y], read16(&PC, rt));
                                break;
                            case 3: // counter=3 z=3
                                switch (y) {
                                    case 0:
                                        l += sprintf(w, "JP %04x", read16(&PC, rt));
                                        break;
                                    case 1:
                                        l += sprintf(w, "INVALID1");
                                        break;
                                    case 2:
                                        l += sprintf(w, "OUT (%02x), A", read8(&PC, rt));
                                        break;
                                    case 3:
                                        l += sprintf(w, "IN (%02X), A", read8(&PC, rt));
                                        break;
                                    case 4:
                                        l += sprintf(w, "EX (SP), ");
                                        w += 9;
                                        l += repl0(&PC, HL, H, L, rt, "HL", w);
                                        break;
                                    case 5:
                                        l += sprintf(w, "EX DE, ");
                                        w += 7;
                                        l += repl0(&PC, HL, H, L, rt, "HL", w);
                                        break;
                                    case 6:
                                        l += sprintf(w, "DI");
                                        break;
                                    case 7:
                                        l += sprintf(w, "EI");
                                        break;
                                }
                                break;
                            case 4: // counter=3 z=4
                                l += sprintf(w, "CALL %s, %04x", Z80D_tabl_cc[y], read16(&PC, rt));
                                break;
                            case 5: // counter=3 z=5
                                if (q == 0) {
                                    l += sprintf(w, "PUSH ");
                                    w += 5;
                                    l += repl0(&PC, HL, H, L, rt, Z80D_tabl_rp2[p], w);
                                }
                                else
                                    l += sprintf(w, "CALL %04x", read16(&PC, rt));
                                break;
                            case 6: // counter=3 z=6
                                l += sprintf(w, "%s %02x", Z80D_tabl_alu[y], read8(&PC, rt));
                                break;
                            case 7: // counter=3 z=7
                                l += sprintf(w, "RST %02x", y * 8);
                                break;
                        }
                }
                break;
            case 0xCB: // prefix 0xCB
                switch(x) {
                    case 0:
                        l += sprintf(w, "%s %s", Z80D_tabl_rot[y], Z80D_tabl_r[z]);
                        break;
                    case 1:
                        l += sprintf(w, "BIT %d, %s", y, Z80D_tabl_r[z]);
                        break;
                    case 2:
                        l += sprintf(w, "RES %d, %s", y, Z80D_tabl_r[z]);
                        break;
                    case 3:
                        l += sprintf(w, "SET %d, %s", y, Z80D_tabl_r[z]);
                        break;
                }
                break;
            case 0xED:
                switch(x) {
                    case 0:
                    case 3:
                        l += sprintf(w, "INVALID NONI NOP");
                        break;
                    case 1: // 0xED counter=1
                        switch(z) {
                            case 0: // 0xED counter=1 z=0
                                if (y == 6) l += sprintf(w, "IN (C)");
                                else l += sprintf(w, "IN %s, (C)", Z80D_tabl_r[y]);
                                break;
                            case 1:
                                if (y == 6) l += sprintf(w, "OUT (C)");
                                else l += sprintf(w, "OUT %s, (C)", Z80D_tabl_r[y]);
                                break;
                            case 2: // 0xED counter=1 z=2
                                if (q == 0) l += sprintf(w, "SBC HL, %s", Z80D_tabl_rp[p]);
                                else l += sprintf(w, "ADC HL, %s", Z80D_tabl_rp[p]);
                                break;
                            case 3: // 0xED counter=1 z=3
                                if (q == 0) l += sprintf(w, "LD (%04x), %s", read16(&PC, rt), Z80D_tabl_rp[p]);
                                else l += sprintf(w, "LD %s, (%04x)", Z80D_tabl_rp[p], read16(&PC, rt));
                                break;
                            case 4:
                                l += sprintf(w, "NEG");
                                break;
                            case 5:
                                if (y == 1) l += sprintf(w, "RETI");
                                else l += sprintf(w, "RETN");
                                break;
                            case 6: // 0xED counter=1 z=6
                                l += sprintf(w, "IM %s", Z80D_tabl_im[y]);
                                break;
                            case 7: // 0xED counter=1 z=7
                                l += sprintf(w, "%s", (char[8][10]){"LD I, A", "LD R, A", "LD A, I", "LD A, R", "RRD", "RLD", "NOP", "NOP"}[y]);
                                break;
                        }
                        break;
                    case 2: // 0xED counter=2
                        if ((z <= 3) && (y >= 4)) {
                            l += sprintf(w, "%s", Z80D_tabl_bli[y][z]);
                        }
                        else
                            l += sprintf(w, "INVALID#2 NONI NOP");
                        break;
                }
                break;
            case 0xDDCB:
            case 0xFDCB:
                if (current_prefix == 0xDDCB) strcpy(IXY, "IX");
                else strcpy(IXY, "IY");
                char d[20];
                sprintf(d, "$%02x", current_byte);
                current_byte = fetch(&PC, rt);
                decoded_bytes++;
                x = (current_byte & 0xC0) >> 6;
                y = (current_byte & 0x38) >> 3;
                z = (current_byte & 7);
                switch(x) {
                    case 0: // CBd counter=0
                        if (z != 6) l += sprintf(w, "LD %s, %s(%s+%s)", Z80D_tabl_r[z], Z80D_tabl_rot[y], IXY, d);
                        else l += sprintf(w, "%s (%s+%s)", Z80D_tabl_rot[y], IXY, d);
                        break;
                    case 1: // CBd counter=1
                        l += sprintf(w, "BIT %d, (%s+%s)", y, IXY, d);
                        break;
                    case 2: // CBd counter=2
                        if (z != 6)
                            l += sprintf(w, "LD %s, RES %d, (%s+%s)", Z80D_tabl_r[z], y, IXY, d);
                        else
                            l += sprintf(w, "RES %d, (%s+%s)", y, IXY, d);
                        break;
                    case 3: // CBd counter=3
                        if (z != 6)
                            l += sprintf(w, "LD %s, SET %d, (%s+%s)", Z80D_tabl_r[z], y, IXY, d);
                        else
                            l += sprintf(w, "SET %d, (%s+%s)", y, IXY, d);
                        break;
                }
                break;
            default:
                l += sprintf(w, "HOW DID WE GET HERE GOVNA");
                break;
        }
        if (l != 0) break;
        decoded_bytes++;
    }
    return l;
}
