#!/usr/bin/python3

import os
import glob
import json
from typing import Optional

FPATH = os.path.join(os.path.expanduser('~'), 'dev/jsmooch-emus/jsmooch-lib/src/component/cpu/huc6280')


class huc6280_switchgen:
    def __init__(self, indent: str):
        self.indent1 = indent
        self.indent2 = '    ' + self.indent1
        self.indent3 = '    ' + self.indent2
        self.indent4 = '    ' + self.indent3

        self.in_case = False
        self.last_case = 0
        self.has_footer = False
        self.has_custom_end = False
        self.no_addr_at_end = False
        self.no_RW_at_end = False
        self.no_T_at_end = False
        self.override_IRQ = False
        self.outstr = ''
        self.old_rw = 0
        self.old_m = 0
        self.next_cyclewhat = None
        self.clear(indent)

    def clear(self, indent: str):
        self.indent1 = indent
        self.indent2 = '    ' + self.indent1
        self.indent3 = '    ' + self.indent2
        self.in_case = False
        self.last_case: str = '0'
        self.has_footer = False
        self.no_addr_at_end = False
        self.no_RW_at_end = False
        self.has_custom_end = False
        self.override_IRQ = False
        self.old_rw = 0
        self.no_T_at_end = False
        self.old_m = 0
        self.indent4 = '    ' + self.indent3

    def addcycle(self, whatup: Optional[str]):
        if self.in_case:
            self.outstr += self.indent4 + 'break; }\n'
        what = str(int(self.last_case) + 1)
        self.last_case = what
        self.in_case = True
        if whatup is not None:
            self.outstr += self.indent3 + 'case ' + what + ': {// ' + whatup + '\n'
        else:
            self.outstr += self.indent3 + 'case ' + what + ': {\n'

    def addl(self, what: str):
        self.outstr += self.indent4 + what + '\n';

    def cleanup(self):
        self.has_footer = True
        self.addcycle('cleanup_custom')

    def poll_IRQs(self):
        self.addl('HUC6280_poll_IRQs(regs, pins);')

    def override_IRQ_do(self):
        self.addl('HUC6280_poll_NMI_only(regs, pins);')

    def regular_end(self):
        self.addl('// Following is auto-generated code for instruction finish')
        if not self.has_footer:
            self.addcycle('cleanup')
        if not self.no_addr_at_end:
            self.addr_to_PC_then_inc()
        if not self.no_RW_at_end:
            self.RW(0, 1)
        if not self.no_T_at_end:
            self.addl('regs->P.T = 0;')
        if not self.override_IRQ:
            self.poll_IRQs()
        else:
            self.override_IRQ_do()
        self.addl('regs->TCU = 0;')
        self.addl('break;')

    def RW(self, rw: int, mem: int, force: bool = False):
        if rw != self.old_rw or mem != self.old_m or force:
            x = ''
            if self.old_rw != rw:
                x = 'pins->RW = ' + str(rw) + '; '
            if self.old_m != mem:
                x += 'pins->M = ' + str(mem) + ';'
            self.addl(x)
            self.old_rw = rw
            self.old_m = mem

    def addr_to_PC(self):
        self.addl('pins->Addr = regs->MPR[regs->PC >> 13] | (regs->PC & 0x1FFF);')

    def addr_to_PC_then_inc(self):
        self.addr_to_PC()
        self.addl('regs->PC = (regs->PC + 1) & 0xFFFF;')

    def addr_to(self, what: str):
        self.addl('pins->Addr = (' + what + ');')

    def finished(self):
        if not self.in_case:
            return ''
        self.regular_end()

        self.outstr += self.indent3 + '}\n' + self.indent2 + '}\n' + self.indent1 + '}\n';
        return self.indent2 + 'switch(regs->TCU) {\n' + self.outstr

    def setz(self, what):
        self.addl('regs->P.Z = (' + what + ') == 0;')

    def setn(self, what):
        self.addl('regs->P.N = ((' + what + ') & 0x80) >> 7;')

    def load16(self, dest, source, name=None, last=False):
        self.addl('pins->Addr = regs->MPR[(' + source + ')>>13] | ((' + source + ') & 0x1FFF);')
        self.RW(0, 1)
        if not last:
            self.addcycle('store8')
            self.RW(0, 0)

            if name is None:
                self.addcycle('load16')
            else:
                self.addcycle(name)
            if dest is not None:
                self.addl(dest + ' = pins->D;')
            self.RW(0, 0)

    def inc_PC(self):
        self.addl('regs->PC = (regs->PC + 1) & 0xFFFF;')

    def operand(self, dest=None, last=False):
        self.load16(dest, 'regs->PC', 'operand', last=last)
        self.inc_PC()

    def push(self, wha: str, last: bool = False) -> None:
        self.addl('pins->Addr = regs->MPR[1] | 0x100 | regs->S;')
        self.addl('pins->D = ' + wha + ';')
        self.addl('regs->S = (regs->S - 1) & 0xFF;')
        if not last:
            self.addcycle('push')

    def load8(self, dest, source):
        self.addl('pins->Addr = regs->MPR[1] | (' + source + ');')
        self.RW(0, 1)
        self.addcycle('load8')
        self.addl(dest + ' = pins->D;')
        self.RW(0, 0)

    def store8(self, addr, val, last=False):
        self.addl('pins->Addr = regs->MPR[1] | (' + addr + ');')
        self.addl('pins->D = ' + val + ';')
        self.RW(1, 1)
        if not last:
            self.addcycle('store8')
            self.RW(0, 0)

    def store16(self, addr, val, last=False):
        self.addl('pins->Addr = ' + addr + ';')
        self.addl('pins->D = ' + val + ';')
        self.RW(1, 1)
        if not last:
            self.addcycle('store16')
            self.RW(0, 0)


def hex2(yo):
    # print(f'{number:04X}')
    return f'{yo:02X}'


def C_func_name(mn, mo, tbit):
    return 'HUC6280_ins_' + hex2(mo) + '_' + mn + '_t' + str(tbit)


def C_func_sig(mn, mo, tbit):
    return 'static void ' + C_func_name(mn, mo, tbit) + '(struct HUC6280_regs *regs, struct HUC6280_pins *pins)'


def IndirectYReadMemory(ag: huc6280_switchgen, ins_func) -> str:
    ag.addcycle('start!')
    ag.addl('regs->TR[2] = regs->A;')
    ag.load8('regs->A', 'regs->X')

    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.addl('regs->TA = (regs->TA + regs->Y) & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA')
    ins_func(ag, 'regs->A', 'regs->TR[0]')
    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def Break(ag: huc6280_switchgen):
    ag.operand()
    ag.addcycle('idle')
    ag.push('regs->PC >> 8')
    ag.push('regs->PC & 0xFF')
    ag.push('regs->P.u | 0x10')
    ag.addl('regs->P.T = 0; regs->P.D = 0; regs->P.I = 1;')
    ag.load16('regs->PC', '0xFFF6')
    ag.load16('regs->TA', '0xFFF7', last=True)
    ag.addl('regs->PC |= regs->TA << 8;')
    return ag.finished()


def StoreImplied(ag: huc6280_switchgen, data: str) -> str:
    ag.addcycle('start')
    ag.operand('regs->TR[0]')
    ag.addcycle('idle')
    ag.store16(addr=data, val='regs->TR[0]', last=True)

    return ag.finished()

def Clear(ag: huc6280_switchgen, dt):
    ag.addcycle('yo')
    ag.addl(dt + ' = 0;')

    return ag.finished()

def ResetMemoryBit(ag: huc6280_switchgen, bitnum):
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TR[0] &= ~(1 << ' + bitnum + ');')
    ag.store8('regs->TA', 'regs->TR[0]', last=True)
    return ag.finished()


def Push(ag: huc6280_switchgen, dt: str):
    ag.addcycle('idle')
    ag.push(dt, True)
    return ag.finished()


def Immediate(ag: huc6280_switchgen, ins_func, dt):
    ag.operand("regs->TA", True)
    ins_func(ag, dt, 'regs->TA')
    return ag.finished()

def AbsoluteModify(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None):
    ag.addcycle('YES!')
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA')
    ins_func(ag, 'regs->TR[1]', 'regs->TR[0]')
    ag.store16('regs->TA', 'regs->TR[1]', last=True)

    return ag.finished()

def Implied(ag: huc6280_switchgen, ins_func, dt):
    ag.addcycle('only!')
    ins_func(ag, dt, dt)

    return ag.finished()

def BranchIfBitReset(ag: huc6280_switchgen, bitnum: str):
    ag.addcycle('YO!')
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addl('regs->TA = (regs->PC + (u16)(i8)regs->TR[0]);')
    ag.addl('regs->TR[0] = (regs->TR[1] & (1 << ' + bitnum + ')) != 0;')
    ag.addl('if (!regs->TR[0]) regs->TCU += 2;')
    ag.addcycle('idle');
    ag.addcycle('idle');
    ag.cleanup()
    ag.addl('if (regs->TR[0]) regs->PC = regs->TA;')
    return ag.finished()

def Branch(ag: huc6280_switchgen, expr):
    ag.addcycle('C1')
    ag.addl('regs->TR[0] = ' + expr + ';');
    ag.operand('regs->TA')
    ag.addl("if (!regs->TR[0]) regs->TCU += 2;");
    ag.addcycle('idle')
    ag.cleanup()
    ag.addl('if (regs->TR[0]) regs->PC = (regs->PC + regs->TA) & 0xFFFF;')
    return ag.finished()

def IndirectYRead(ag: huc6280_switchgen, ins_func, dt):
    ag.addcycle('BOO')
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.addl('regs->TA = (regs->TA + regs->Y) & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA', last=True)
    ins_func(ag, dt, 'regs->TR[0]')
    return ag.finished()

def AbsoluteRead(ag: huc6280_switchgen, ins_func, dt, inx: Optional[str] = None):
    ag.addcycle('YOULL NEVER FIND ME')
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + ' + inx + ') & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA', last=True);
    ins_func(ag, dt, 'regs->TR[0]')

    return ag.finished()

def NOP(ag: huc6280_switchgen):
    ag.addcycle('yo')

    return ag.finished()

def ZeroPageRead(ag: huc6280_switchgen, ins_func, dt, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA')
    ins_func(ag, dt, 'regs->TR[0]')
    return ag.finished()

def ZeroPageModify(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA')
    ins_func(ag, 'regs->TR[1]', 'regs->TR[0]')
    ag.store8('regs->TA', 'regs->TR[1]', last=True)

    return ag.finished()


def Swap(ag: huc6280_switchgen, data1, data2) -> str:
    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.addl('regs->TA = ' + data1 + ';')
    ag.addl(data1 + ' = ' + data2 + ';')
    ag.addl(data2 + ' = regs->TA;')

    return ag.finished()


def IndirectRead(ag: huc6280_switchgen, ins_func, dta: Optional[str] = None, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx is not None:
        ag.addcycle('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.load16('regs->TR[0]', 'regs->TA')
    ins_func(ag, dta, 'regs->TR[0]')
    ag.cleanup()
    ag.addl(dta + " = regs->TR[0];")

    return ag.finished()


def IndirectReadMemory(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.addcycle('start!')
    ag.addl('regs->TR[2] = regs->A;')
    ag.load8('regs->A', 'regs->X')

    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.load16('regs->TR[0]', 'regs->TA')

    ins_func(ag, 'regs->A', 'regs->TR[0]')
    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def AbsoluteReadMemory(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.addcycle('start!')
    ag.addl('regs->TR[2] = regs->A;')
    ag.load8('regs->A', 'regs->X')

    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFFFF;')

    ag.load16('regs->TR[0]', 'regs->TA')
    ins_func(ag, 'regs->A', 'regs->TR[0]')

    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def ImmediateMemory(ag: huc6280_switchgen, ins_func) -> str:
    ag.addcycle('start!')
    ag.addl('regs->TR[2] = regs->A;')
    ag.load8('regs->A', 'regs->X')
    ag.operand('regs->TA')
    ins_func(ag, 'regs->A', 'regs->TA')

    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def ZeroPageReadMemory(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.addcycle('start!')
    ag.addl('regs->TR[2] = regs->A;')
    ag.load8('regs->A', 'regs->X')
    ag.operand('regs->TA')  # ZP = operand()
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ' )) & 0xFF;')
    ag.load8('regs->A', 'regs->TA')
    ins_func(ag, 'regs->A', 'regs->TR[0]')

    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()

def al_TRB(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('regs->P.Z = (regs->A & (' + source + ')) == 0;')
    ag.addl('regs->P.V = ((' + source + ') >> 6) & 1;')
    ag.addl('regs->P.N = ((' + source + ') >> 7) & 1;')
    if dest is not None:
        ag.addl(dest + ' = ~regs->A & (' + source + ');')

def al_ORA(ag: huc6280_switchgen, dest: str, source: str) -> None:
    ag.addl(dest + ' = regs->A | (' + source + ');')
    ag.setz(dest)
    ag.setn(dest)


def al_AND(ag: huc6280_switchgen, dest: str, source: str) -> None:
    ag.addl(dest + ' = regs->A & (' + source + ');')
    ag.setz(dest)
    ag.setn(dest)


def al_EOR(ag: huc6280_switchgen, dest: str, source: str) -> None:
    ag.addl(dest + ' = regs->A ^ (' + source + ');')
    ag.setz(dest)
    ag.setn(dest)


def al_TSB(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('u32 o = (' + source + ') & regs->A;')
    ag.addl('regs->P.Z = o == 0;')
    ag.addl('regs->P.N = ((' + source + ') >> 7) & 1;')
    ag.addl('regs->P.V = ((' + source + ') >> 6) & 1;')
    if dest is not None:
        ag.addl(dest + ' = (' + source + ') | regs->A;')


def al_ASL(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('regs->P.C = ((' + source + ') >> 7) & 1;')
    ag.addl(source + ' = (' + source + ' << 1) & 0xFF;')
    ag.setz(source)
    ag.setn(source)
    if dest is not None:
        ag.addl(dest + ' = ' + source + ';')


def al_ADC(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('i16 out = (i16)regs->A + (i16)(' + source + ') + (i16)regs->P.C;')
    ag.addl('if (!regs->P.D) {')
    ag.addl('    regs->P.C = (out >> 8) & 1;')
    ag.addl('    regs->P.V = ((~(regs->A ^ (' + source + ')) & (regs->A & out)) >> 7) & 1;')
    ag.addl('    regs->TCU++;')
    ag.addl('}')
    ag.addl('else {')
    ag.addl('    u8 lo = (regs->A & 15) + ((' + source + ') & 15) + regs->P.C;')
    ag.addl('    if (lo > 9) out += 6;')
    ag.addl('    if (out > 0x9F) out += 0x60;')
    ag.addl('    regs->P.C = out > 0xFF;')
    ag.addl('}')
    ag.setz('out')
    ag.setn('out')
    if dest is not None:
        ag.addl(dest + ' = out;')
    ag.addcycle('idle')


def write_instruction(outfile, opcode, tbit):
    mn = ''
    r = ''
    am = ''
    ag = huc6280_switchgen('')
    didit = False
    if tbit == 1:
        al = None
        if opcode < 0x20:
            al = al_ORA
            mn = 'ORA'
        elif opcode < 0x40:
            al = al_AND
            mn = 'AND'
        elif opcode < 0x60:
            al = al_EOR
            mn = 'EOR'
        elif opcode < 0x80:
            al = al_ADC
            mn = 'ADC'
        if al is not None:
            myopc = opcode & 0x1F
            if myopc == 0x01:
                r = IndirectReadMemory(ag, al, 'regs->X')
                am = 'indirect_read_memory'
            elif myopc == 0x05:
                r = ZeroPageReadMemory(ag, al)
                am = 'zero_page_read_memory'
            elif myopc == 0x09:
                r = ImmediateMemory(ag, al)
                am = 'immediate_memory'
            elif myopc == 0x0D:
                r = AbsoluteReadMemory(ag, al)
                am = 'absolute_read_memory'
            elif myopc == 0x11:
                r = IndirectYReadMemory(ag, al)
                am = 'indirect_y_read_memory'
            elif myopc == 0x12:
                r = IndirectReadMemory(ag, al, None)
                am = 'indirect_read_memory'
            elif myopc == 0x15:
                r = ZeroPageReadMemory(ag, al, 'regs->X')
                am = 'zero_page_read_memory'
            elif myopc == 0x19:
                r = AbsoluteReadMemory(ag, al, 'regs->Y')
                am = 'absolute_read_memory'
            elif myopc == 0x1D:
                r = AbsoluteReadMemory(ag, al, 'regs->X')
                am = 'absolute_read_memory'

    if len(r) < 1:
        if opcode == 0x00:
            r = Break(ag)
        elif opcode == 0x01:
            r = IndirectRead(ag, al_ORA, 'regs->A', 'regs->X')
        elif opcode == 0x02:
            r = Swap(ag, 'regs->X', 'regs->Y')
        elif opcode == 0x03:
            r = StoreImplied(ag, '0x1FE000')
        elif opcode == 0x04:
            r = ZeroPageModify(ag, al_TSB)
        elif opcode == 0x05:
            r = ZeroPageRead(ag, al_ORA, 'regs->A', None)
        elif opcode == 0x06:
            r = ZeroPageModify(ag, al_ASL)
        elif opcode == 0x07:
            r = ResetMemoryBit(ag, '0')
        elif opcode == 0x08:
            r = Push(ag, 'regs->P.u')
        elif opcode == 0x09:
            r = Immediate(ag, al_ORA, 'regs->A')
        elif opcode == 0x0A:
            r = Implied(ag, al_ASL, 'regs->A')
        elif opcode == 0x0B:
            r = NOP(ag)
        elif opcode == 0x0C:
            r = AbsoluteModify(ag, al_TSB)
        elif opcode == 0x0D:
            r = AbsoluteRead(ag, al_ORA, 'regs->A')
        elif opcode == 0x0E:
            r = AbsoluteModify(ag, al_ASL)
        elif opcode == 0x0F:
            r = BranchIfBitReset(ag, '0')
        elif opcode == 0x10:
            r = Branch(ag, '!regs->P.N')
        elif opcode == 0x11:
            r = IndirectYRead(ag, al_ORA, 'regs->A')
        elif opcode == 0x12:
            r = IndirectRead(ag, al_ORA, 'regs->A')
        elif opcode == 0x13:
            r = StoreImplied(ag, '0x1FEE02')
        elif opcode == 0x14:
            r = ZeroPageModify(ag, al_TRB)
        elif opcode == 0x15:
            r = ZeroPageRead(ag, al_ORA, 'regs->A', 'regs->X')
        elif opcode == 0x16:
            r = ZeroPageModify(ag, al_ASL, 'regs->X')
        elif opcode == 0x17:
            r = ResetMemoryBit(ag, '1')
        elif opcode == 0x18:
            r = Clear(ag, 'regs->P.C')
        elif opcode == 0x19:
            r = AbsoluteRead(ag, al_ORA, 'regs->A', 'regs->Y')
        '''elif opcode == 0x1A:
            r =
        elif opcode == 0x1B:
            r =
        elif opcode == 0x1C:
            r =
        elif opcode == 0x1D:
            r =
        elif opcode == 0x1E:
            r =
        elif opcode == 0x1F:
            r =
        elif opcode == 0x20:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r =
        elif opcode == 0x0:
            r =
        elif opcode == 0x1:
            r =
        elif opcode == 0x2:
            r =
        elif opcode == 0x3:
            r =
        elif opcode == 0x4:
            r =
        elif opcode == 0x5:
            r =
        elif opcode == 0x6:
            r =
        elif opcode == 0x7:
            r =
        elif opcode == 0x8:
            r =
        elif opcode == 0x9:
            r =
        elif opcode == 0xA:
            r =
        elif opcode == 0xB:
            r =
        elif opcode == 0xC:
            r =
        elif opcode == 0xD:
            r =
        elif opcode == 0xE:
            r =
        elif opcode == 0xF:
            r ='''

    if len(r) > 0:
        mystr = '// ' + am + '\n' + C_func_sig(mn, opcode, tbit) + '\n{\n' + r + '\n\n'
        outfile.write(mystr)


def main():
    outinsfile = os.path.join(FPATH, 'huc6280_opcodes.c')
    if os.path.isfile(outinsfile):
        os.unlink(outinsfile)
    with open(outinsfile, 'w') as outfile:
        outfile.write('#include <assert.h>\n')
        outfile.write('#include "helpers/int.h"\n')
        outfile.write('#include "huc6280_opcodes.h"\n')
        outfile.write('#include "huc6280.h"\n')
        outfile.write('\n')
        outfile.write('// This file auto-generated byhuc6280_gen.py in JSMooCh\n')

        outfile.write('\n')
        outfile.write('static void M6502_ins_NONE(struct HUC6280_regs *regs, struct HUC6280_pins *pins)\n')
        outfile.write('{\n')
        outfile.write('    assert(1==0);\n')
        outfile.write('}\n')

        for tbit in range(0, 2):
            for i in range(0, 0x100 + 3):
                write_instruction(outfile, i, tbit)


if __name__ == '__main__':
    main()
