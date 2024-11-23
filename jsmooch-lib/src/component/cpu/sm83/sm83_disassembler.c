//
// Created by Dave on 1/30/2024.
//

#include "stdio.h"
#include "string.h"
#include "sm83_disassembler.h"

#define R(a) return snprintf(w, sz, a)
#define Rn(a, n) return snprintf(w, sz, a, n)

u32 s8add(u32 PC, u32 val)
{
    u32 out = PC;
    out = (u32)((i32)out + (i8)val);
    return out & 0xFFFF;
}

static u32 SM83_disassembleCB(char *w, u32 opcode, size_t sz);

u32 SM83_disassemble(u32 PC, u32 (*peek)(u32), char *w, size_t sz) {
    u32 opcode = peek(PC);

    u32 lo = peek((PC+1) & 0xFFFF);
    u32 hi = peek((PC+2) & 0xFFFF);
    u32 word = (hi << 8) | lo;
    switch(opcode) {
        case 0x00: R("nop");
        case 0x01: Rn("ld   bc,$%04x", word);
        case 0x02: R("ld   (bc),a");
        case 0x03: R("inc  bc");
        case 0x04: R("inc  b");
        case 0x05: R("dec  b");
        case 0x06: Rn("ld   b,$%02x", lo);
        case 0x07: R("rlca");
        case 0x08: Rn("ld   ($%04x), sp", word);
        case 0x09: R("add  hl,bc");
        case 0x0a: R("ld   a,(bc)");
        case 0x0b: R("dec  bc");
        case 0x0c: R("inc  c");
        case 0x0d: R("dec  c");
        case 0x0e: Rn("ld   c,$%02x", lo);
        case 0x0f: R("rrca");
        case 0x10: R("stop");
        case 0x11: Rn("ld   de,$%04x", word);
        case 0x12: R("ld   (de),a");
        case 0x13: R("inc  de");
        case 0x14: R("inc  d");
        case 0x15: R("dec  d");
        case 0x16: Rn("ld   d,$%02x", lo);
        case 0x17: R("rla");
        case 0x18: Rn("jr   $%04x", s8add(PC + 2, lo));
        case 0x19: R("add  hl,de");
        case 0x1a: R("ld   a,(de)");
        case 0x1b: R("dec  de");
        case 0x1c: R("inc  e");
        case 0x1d: R("dec  e");
        case 0x1e: Rn("ld   e,$%02x", lo);
        case 0x1f: R("rra");
        case 0x20: Rn("jr   nz,$%04x", s8add(PC + 2, lo));
        case 0x21: Rn("ld   hl,$%04x", word);
        case 0x22: R("ldi  (hl+),a");
        case 0x23: R("inc  hl");
        case 0x24: R("inc  h");
        case 0x25: R("dec  h");
        case 0x26: Rn("ld   h,$%02x", lo);
        case 0x27: R("daa");
        case 0x28: Rn("jr   z,$%04x", s8add(PC + 2, lo));
        case 0x29: R("add  hl,hl");
        case 0x2a: R("ldi  a,(hl)");
        case 0x2b: R("dec  hl");
        case 0x2c: R("inc  l");
        case 0x2d: R("dec  l");
        case 0x2e: Rn("ld   l,$%02x", lo);
        case 0x2f: R("cpl");
        case 0x30: Rn("jr   nc,$%04x", s8add(PC + 2, lo));
        case 0x31: Rn("ld   sp,$%04x", word);
        case 0x32: R("ldd  (hl-),a");
        case 0x33: R("inc  sp");
        case 0x34: R("inc  (hl)");
        case 0x35: R("dec  (hl)");
        case 0x36: Rn("ld   (hl),$%02x", lo);
        case 0x37: R("scf");
        case 0x38: Rn("jr   c,$%04x", s8add(PC + 2, lo));
        case 0x39: R("add  hl,sp");
        case 0x3a: R("ldd  a,(hl)");
        case 0x3b: R("dec  sp");
        case 0x3c: R("inc  a");
        case 0x3d: R("dec  a");
        case 0x3e: Rn("ld   a,$%02x", lo);
        case 0x3f: R("ccf");
        case 0x40: R("ld   b,b");
        case 0x41: R("ld   b,c");
        case 0x42: R("ld   b,d");
        case 0x43: R("ld   b,e");
        case 0x44: R("ld   b,h");
        case 0x45: R("ld   b,l");
        case 0x46: R("ld   b,(hl)");
        case 0x47: R("ld   b,a");
        case 0x48: R("ld   c,b");
        case 0x49: R("ld   c,c");
        case 0x4a: R("ld   c,d");
        case 0x4b: R("ld   c,e");
        case 0x4c: R("ld   c,h");
        case 0x4d: R("ld   c,l");
        case 0x4e: R("ld   c,(hl)");
        case 0x4f: R("ld   c,a");
        case 0x50: R("ld   d,b");
        case 0x51: R("ld   d,c");
        case 0x52: R("ld   d,d");
        case 0x53: R("ld   d,e");
        case 0x54: R("ld   d,h");
        case 0x55: R("ld   d,l");
        case 0x56: R("ld   d,(hl)");
        case 0x57: R("ld   d,a");
        case 0x58: R("ld   e,b");
        case 0x59: R("ld   e,c");
        case 0x5a: R("ld   e,d");
        case 0x5b: R("ld   e,e");
        case 0x5c: R("ld   e,h");
        case 0x5d: R("ld   e,l");
        case 0x5e: R("ld   e,(hl)");
        case 0x5f: R("ld   e,a");
        case 0x60: R("ld   h,b");
        case 0x61: R("ld   h,c");
        case 0x62: R("ld   h,d");
        case 0x63: R("ld   h,e");
        case 0x64: R("ld   h,h");
        case 0x65: R("ld   h,l");
        case 0x66: R("ld   h,(hl)");
        case 0x67: R("ld   h,a");
        case 0x68: R("ld   l,b");
        case 0x69: R("ld   l,c");
        case 0x6a: R("ld   l,d");
        case 0x6b: R("ld   l,e");
        case 0x6c: R("ld   l,h");
        case 0x6d: R("ld   l,l");
        case 0x6e: R("ld   l,(hl)");
        case 0x6f: R("ld   l,a");
        case 0x70: R("ld   (hl),b");
        case 0x71: R("ld   (hl),c");
        case 0x72: R("ld   (hl),d");
        case 0x73: R("ld   (hl),e");
        case 0x74: R("ld   (hl),h");
        case 0x75: R("ld   (hl),l");
        case 0x76: R("halt");
        case 0x77: R("ld   (hl),a");
        case 0x78: R("ld   a,b");
        case 0x79: R("ld   a,c");
        case 0x7a: R("ld   a,d");
        case 0x7b: R("ld   a,e");
        case 0x7c: R("ld   a,h");
        case 0x7d: R("ld   a,l");
        case 0x7e: R("ld   a,(hl)");
        case 0x7f: R("ld   a,a");
        case 0x80: R("add  a,b");
        case 0x81: R("add  a,c");
        case 0x82: R("add  a,d");
        case 0x83: R("add  a,e");
        case 0x84: R("add  a,h");
        case 0x85: R("add  a,l");
        case 0x86: R("add  a,(hl)");
        case 0x87: R("add  a,a");
        case 0x88: R("adc  a,b");
        case 0x89: R("adc  a,c");
        case 0x8a: R("adc  a,d");
        case 0x8b: R("adc  a,e");
        case 0x8c: R("adc  a,h");
        case 0x8d: R("adc  a,l");
        case 0x8e: R("adc  a,(hl)");
        case 0x8f: R("adc  a,a");
        case 0x90: R("sub  a,b");
        case 0x91: R("sub  a,c");
        case 0x92: R("sub  a,d");
        case 0x93: R("sub  a,e");
        case 0x94: R("sub  a,h");
        case 0x95: R("sub  a,l");
        case 0x96: R("sub  a,(hl)");
        case 0x97: R("sub  a,a");
        case 0x98: R("sbc  a,b");
        case 0x99: R("sbc  a,c");
        case 0x9a: R("sbc  a,d");
        case 0x9b: R("sbc  a,e");
        case 0x9c: R("sbc  a,h");
        case 0x9d: R("sbc  a,l");
        case 0x9e: R("sbc  a,(hl)");
        case 0x9f: R("sbc  a,a");
        case 0xa0: R("and  a,b");
        case 0xa1: R("and  a,c");
        case 0xa2: R("and  a,d");
        case 0xa3: R("and  a,e");
        case 0xa4: R("and  a,h");
        case 0xa5: R("and  a,l");
        case 0xa6: R("and  a,(hl)");
        case 0xa7: R("and  a,a");
        case 0xa8: R("xor  a,b");
        case 0xa9: R("xor  a,c");
        case 0xaa: R("xor  a,d");
        case 0xab: R("xor  a,e");
        case 0xac: R("xor  a,h");
        case 0xad: R("xor  a,l");
        case 0xae: R("xor  a,(hl)");
        case 0xaf: R("xor  a,a");
        case 0xb0: R("or   a,b");
        case 0xb1: R("or   a,c");
        case 0xb2: R("or   a,d");
        case 0xb3: R("or   a,e");
        case 0xb4: R("or   a,h");
        case 0xb5: R("or   a,l");
        case 0xb6: R("or   a,(hl)");
        case 0xb7: R("or   a,a");
        case 0xb8: R("cp   a,b");
        case 0xb9: R("cp   a,c");
        case 0xba: R("cp   a,d");
        case 0xbb: R("cp   a,e");
        case 0xbc: R("cp   a,h");
        case 0xbd: R("cp   a,l");
        case 0xbe: R("cp   a,(hl)");
        case 0xbf: R("cp   a,a");
        case 0xc0: R("ret  nz");
        case 0xc1: R("pop  bc");
        case 0xc2: Rn("jp   nz,$%04x", word);
        case 0xc3: Rn("jp   $%04x", word);
        case 0xc4: Rn("call nz,$%04x", word);
        case 0xc5: R("push bc");
        case 0xc6: Rn("add  a,$%02x", lo);
        case 0xc7: R("rst  $0000");
        case 0xc8: R("ret  z");
        case 0xc9: R("ret");
        case 0xca: Rn("jp   z,$%04x", word);
        case 0xcb: return SM83_disassembleCB(w, lo, sz);
        case 0xcc: Rn("call z,$%04x", word);
        case 0xcd: Rn("call $%04x", word);
        case 0xce: Rn("adc  a,$%02x", lo);
        case 0xcf: R("rst  $0008");
        case 0xd0: R("ret  nc");
        case 0xd1: R("pop  de");
        case 0xd2: Rn("jp   nc,$%04x", word);
        case 0xd4: Rn("call nc,$%04x", word);
        case 0xd5: R("push de");
        case 0xd6: Rn("sub  a,$%02x", lo);
        case 0xd7: R("rst  $0010");
        case 0xd8: R("ret  c");
        case 0xd9: R("reti");
        case 0xda: Rn("jp   c,$%04x", word);
        case 0xdc: Rn("call c,$%04x", word);
        case 0xde: Rn("sbc  a,$%02x", lo);
        case 0xdf: R("rst  $0018");
        case 0xe0: Rn("ldh  ($ff%02x),a", lo);
        case 0xe1: R("pop  hl");
        case 0xe2: R("ldh  ($ff00+c),a");
        case 0xe5: R("push hl");
        case 0xe6: Rn("and  a,$%02x", lo);
        case 0xe7: R("rst  $0020");
        case 0xe8: Rn("add  sp,%d", lo);
        case 0xe9: R("jp   hl");
        case 0xea: Rn("ld   ($%04x),a", word);
        case 0xee: Rn("xor  a,$%02x", lo);
        case 0xef: R("rst  $0028");
        case 0xf0: Rn("ldh  a,($ff%02x)", lo);
        case 0xf1: R("pop  af");
        case 0xf2: R("ldh  a,($ff00+c)");
        case 0xf3: R("di");
        case 0xf5: R("push af");
        case 0xf6: Rn("or  a,$%02x", lo);
        case 0xf7: R("rst  $0030");
        case 0xf8: Rn("ld   hl,sp+%d", lo);
        case 0xf9: R("ld   sp,hl");
        case 0xfa: Rn("ld   a,($%04x)", word);
        case 0xfb: R("ei");
        case 0xfe: Rn("cp   a,$%02x", lo);
        case 0xff: R("rst  $0038");
        case 0x100: R("IRQ");
        default: R("ILLEGAL");
    }
}

static u32 SM83_disassembleCB(char *w, u32 opcode, size_t sz) {
    switch(opcode) {
        case 0x00: R("rlc  b");
        case 0x01: R("rlc  c");
        case 0x02: R("rlc  d");
        case 0x03: R("rlc  e");
        case 0x04: R("rlc  h");
        case 0x05: R("rlc  l");
        case 0x06: R("rlc  (hl)");
        case 0x07: R("rlc  a");
        case 0x08: R("rrc  b");
        case 0x09: R("rrc  c");
        case 0x0a: R("rrc  d");
        case 0x0b: R("rrc  e");
        case 0x0c: R("rrc  h");
        case 0x0d: R("rrc  l");
        case 0x0e: R("rrc  (hl)");
        case 0x0f: R("rrc  a");
        case 0x10: R("rl   b");
        case 0x11: R("rl   c");
        case 0x12: R("rl   d");
        case 0x13: R("rl   e");
        case 0x14: R("rl   h");
        case 0x15: R("rl   l");
        case 0x16: R("rl   (hl)");
        case 0x17: R("rl   a");
        case 0x18: R("rr   b");
        case 0x19: R("rr   c");
        case 0x1a: R("rr   d");
        case 0x1b: R("rr   e");
        case 0x1c: R("rr   h");
        case 0x1d: R("rr   l");
        case 0x1e: R("rr   (hl)");
        case 0x1f: R("rr   a");
        case 0x20: R("sla  b");
        case 0x21: R("sla  c");
        case 0x22: R("sla  d");
        case 0x23: R("sla  e");
        case 0x24: R("sla  h");
        case 0x25: R("sla  l");
        case 0x26: R("sla  (hl)");
        case 0x27: R("sla  a");
        case 0x28: R("sra  b");
        case 0x29: R("sra  c");
        case 0x2a: R("sra  d");
        case 0x2b: R("sra  e");
        case 0x2c: R("sra  h");
        case 0x2d: R("sra  l");
        case 0x2e: R("sra  (hl)");
        case 0x2f: R("sra  a");
        case 0x30: R("swap b");
        case 0x31: R("swap c");
        case 0x32: R("swap d");
        case 0x33: R("swap e");
        case 0x34: R("swap h");
        case 0x35: R("swap l");
        case 0x36: R("swap (hl)");
        case 0x37: R("swap a");
        case 0x38: R("srl  b");
        case 0x39: R("srl  c");
        case 0x3a: R("srl  d");
        case 0x3b: R("srl  e");
        case 0x3c: R("srl  h");
        case 0x3d: R("srl  l");
        case 0x3e: R("srl  (hl)");
        case 0x3f: R("srl  a");
        case 0x40: R("bit  0,b");
        case 0x41: R("bit  0,c");
        case 0x42: R("bit  0,d");
        case 0x43: R("bit  0,e");
        case 0x44: R("bit  0,h");
        case 0x45: R("bit  0,l");
        case 0x46: R("bit  0,(hl)");
        case 0x47: R("bit  0,a");
        case 0x48: R("bit  1,b");
        case 0x49: R("bit  1,c");
        case 0x4a: R("bit  1,d");
        case 0x4b: R("bit  1,e");
        case 0x4c: R("bit  1,h");
        case 0x4d: R("bit  1,l");
        case 0x4e: R("bit  1,(hl)");
        case 0x4f: R("bit  1,a");
        case 0x50: R("bit  2,b");
        case 0x51: R("bit  2,c");
        case 0x52: R("bit  2,d");
        case 0x53: R("bit  2,e");
        case 0x54: R("bit  2,h");
        case 0x55: R("bit  2,l");
        case 0x56: R("bit  2,(hl)");
        case 0x57: R("bit  2,a");
        case 0x58: R("bit  3,b");
        case 0x59: R("bit  3,c");
        case 0x5a: R("bit  3,d");
        case 0x5b: R("bit  3,e");
        case 0x5c: R("bit  3,h");
        case 0x5d: R("bit  3,l");
        case 0x5e: R("bit  3,(hl)");
        case 0x5f: R("bit  3,a");
        case 0x60: R("bit  4,b");
        case 0x61: R("bit  4,c");
        case 0x62: R("bit  4,d");
        case 0x63: R("bit  4,e");
        case 0x64: R("bit  4,h");
        case 0x65: R("bit  4,l");
        case 0x66: R("bit  4,(hl)");
        case 0x67: R("bit  4,a");
        case 0x68: R("bit  5,b");
        case 0x69: R("bit  5,c");
        case 0x6a: R("bit  5,d");
        case 0x6b: R("bit  5,e");
        case 0x6c: R("bit  5,h");
        case 0x6d: R("bit  5,l");
        case 0x6e: R("bit  5,(hl)");
        case 0x6f: R("bit  5,a");
        case 0x70: R("bit  6,b");
        case 0x71: R("bit  6,c");
        case 0x72: R("bit  6,d");
        case 0x73: R("bit  6,e");
        case 0x74: R("bit  6,h");
        case 0x75: R("bit  6,l");
        case 0x76: R("bit  6,(hl)");
        case 0x77: R("bit  6,a");
        case 0x78: R("bit  7,b");
        case 0x79: R("bit  7,c");
        case 0x7a: R("bit  7,d");
        case 0x7b: R("bit  7,e");
        case 0x7c: R("bit  7,h");
        case 0x7d: R("bit  7,l");
        case 0x7e: R("bit  7,(hl)");
        case 0x7f: R("bit  7,a");
        case 0x80: R("res  0,b");
        case 0x81: R("res  0,c");
        case 0x82: R("res  0,d");
        case 0x83: R("res  0,e");
        case 0x84: R("res  0,h");
        case 0x85: R("res  0,l");
        case 0x86: R("res  0,(hl)");
        case 0x87: R("res  0,a");
        case 0x88: R("res  1,b");
        case 0x89: R("res  1,c");
        case 0x8a: R("res  1,d");
        case 0x8b: R("res  1,e");
        case 0x8c: R("res  1,h");
        case 0x8d: R("res  1,l");
        case 0x8e: R("res  1,(hl)");
        case 0x8f: R("res  1,a");
        case 0x90: R("res  2,b");
        case 0x91: R("res  2,c");
        case 0x92: R("res  2,d");
        case 0x93: R("res  2,e");
        case 0x94: R("res  2,h");
        case 0x95: R("res  2,l");
        case 0x96: R("res  2,(hl)");
        case 0x97: R("res  2,a");
        case 0x98: R("res  3,b");
        case 0x99: R("res  3,c");
        case 0x9a: R("res  3,d");
        case 0x9b: R("res  3,e");
        case 0x9c: R("res  3,h");
        case 0x9d: R("res  3,l");
        case 0x9e: R("res  3,(hl)");
        case 0x9f: R("res  3,a");
        case 0xa0: R("res  4,b");
        case 0xa1: R("res  4,c");
        case 0xa2: R("res  4,d");
        case 0xa3: R("res  4,e");
        case 0xa4: R("res  4,h");
        case 0xa5: R("res  4,l");
        case 0xa6: R("res  4,(hl)");
        case 0xa7: R("res  4,a");
        case 0xa8: R("res  5,b");
        case 0xa9: R("res  5,c");
        case 0xaa: R("res  5,d");
        case 0xab: R("res  5,e");
        case 0xac: R("res  5,h");
        case 0xad: R("res  5,l");
        case 0xae: R("res  5,(hl)");
        case 0xaf: R("res  5,a");
        case 0xb0: R("res  6,b");
        case 0xb1: R("res  6,c");
        case 0xb2: R("res  6,d");
        case 0xb3: R("res  6,e");
        case 0xb4: R("res  6,h");
        case 0xb5: R("res  6,l");
        case 0xb6: R("res  6,(hl)");
        case 0xb7: R("res  6,a");
        case 0xb8: R("res  7,b");
        case 0xb9: R("res  7,c");
        case 0xba: R("res  7,d");
        case 0xbb: R("res  7,e");
        case 0xbc: R("res  7,h");
        case 0xbd: R("res  7,l");
        case 0xbe: R("res  7,(hl)");
        case 0xbf: R("res  7,a");
        case 0xc0: R("set  0,b");
        case 0xc1: R("set  0,c");
        case 0xc2: R("set  0,d");
        case 0xc3: R("set  0,e");
        case 0xc4: R("set  0,h");
        case 0xc5: R("set  0,l");
        case 0xc6: R("set  0,(hl)");
        case 0xc7: R("set  0,a");
        case 0xc8: R("set  1,b");
        case 0xc9: R("set  1,c");
        case 0xca: R("set  1,d");
        case 0xcb: R("set  1,e");
        case 0xcc: R("set  1,h");
        case 0xcd: R("set  1,l");
        case 0xce: R("set  1,(hl)");
        case 0xcf: R("set  1,a");
        case 0xd0: R("set  2,b");
        case 0xd1: R("set  2,c");
        case 0xd2: R("set  2,d");
        case 0xd3: R("set  2,e");
        case 0xd4: R("set  2,h");
        case 0xd5: R("set  2,l");
        case 0xd6: R("set  2,(hl)");
        case 0xd7: R("set  2,a");
        case 0xd8: R("set  3,b");
        case 0xd9: R("set  3,c");
        case 0xda: R("set  3,d");
        case 0xdb: R("set  3,e");
        case 0xdc: R("set  3,h");
        case 0xdd: R("set  3,l");
        case 0xde: R("set  3,(hl)");
        case 0xdf: R("set  3,a");
        case 0xe0: R("set  4,b");
        case 0xe1: R("set  4,c");
        case 0xe2: R("set  4,d");
        case 0xe3: R("set  4,e");
        case 0xe4: R("set  4,h");
        case 0xe5: R("set  4,l");
        case 0xe6: R("set  4,(hl)");
        case 0xe7: R("set  4,a");
        case 0xe8: R("set  5,b");
        case 0xe9: R("set  5,c");
        case 0xea: R("set  5,d");
        case 0xeb: R("set  5,e");
        case 0xec: R("set  5,h");
        case 0xed: R("set  5,l");
        case 0xee: R("set  5,(hl)");
        case 0xef: R("set  5,a");
        case 0xf0: R("set  6,b");
        case 0xf1: R("set  6,c");
        case 0xf2: R("set  6,d");
        case 0xf3: R("set  6,e");
        case 0xf4: R("set  6,h");
        case 0xf5: R("set  6,l");
        case 0xf6: R("set  6,(hl)");
        case 0xf7: R("set  6,a");
        case 0xf8: R("set  7,b");
        case 0xf9: R("set  7,c");
        case 0xfa: R("set  7,d");
        case 0xfb: R("set  7,e");
        case 0xfc: R("set  7,h");
        case 0xfd: R("set  7,l");
        case 0xfe: R("set  7,(hl)");
        case 0xff: R("set  7,a");
        default: R("ILLEGAL_CB");
    }
}