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

        self.is_mem = False

        self.in_case = False
        self.last_case = 0
        self.has_footer = False
        self.has_custom_end = False
        self.no_addr_at_end = False
        self.no_RW_at_end = False
        self.no_T_at_end = False
        self.override_IRQ = False
        self.outstr = ''
        self.old_rd = 1
        self.old_wr = 0
        self.new_rd = 0
        self.new_wr = 0
        self.old_m = 0
        self.next_cyclewhat = None
        self.clear(indent)

    def clear(self, indent: str):
        self.indent1 = indent
        self.indent2 = '    ' + self.indent1
        self.indent3 = '    ' + self.indent2
        self.last_case: str = '0'
        self.has_footer = False
        self.no_addr_at_end = False
        self.no_RW_at_end = False
        self.has_custom_end = False
        self.override_IRQ = False
        self.old_rd = 1
        self.old_wr = 0
        self.no_T_at_end = False
        self.old_m = 0
        self.in_case = False
        self.addcycle('start cycle')
        self.indent4 = '    ' + self.indent3
        self.is_mem = False

    def set_down_rw(self):
        if self.new_rd != self.old_rd or self.new_wr != self.old_wr:
            x = ''
            if self.old_rd != self.new_rd:
                x = 'pins->RD = ' + str(self.new_rd) + '; '# + '// RD:' + str(self.new_rd) + '  WR:' + str(self.new_wr) + '. oldRD:' + str(self.old_rd) + '  oldWR:' + str(self.old_wr)
            if self.old_wr != self.new_wr:
                x += 'pins->WR = ' + str(self.new_wr) + ';'# + '// RD:' + str(self.new_rd) + '  WR:' + str(self.new_wr) + '. oldRD:' + str(self.old_rd) + '  oldWR:' + str(self.old_wr)
            self.addl(x)
            self.old_rd = self.new_rd
            self.old_wr = self.new_wr

    def addcycle(self, whatup: Optional[str]):
        if self.in_case:
            self.set_down_rw()
            self.outstr += self.indent4 + 'return; }\n'
        what = str(int(self.last_case) + 1)
        self.new_wr = 0
        self.new_rd = 0
        self.last_case = what
        self.in_case = True
        if whatup is not None:
            self.outstr += self.indent3 + 'case ' + what + ': {// ' + whatup + '\n'
        else:
            self.outstr += self.indent3 + 'case ' + what + ': {\n'

    def br_end(self, indent: '    '):
        self.addl('    pins->Addr = regs->MPR[regs->PC >> 13] | (regs->PC & 0x1FFF);')
        self.addl('    regs->PC = (regs->PC + 1) & 0xFFFF;')
        self.addl('    pins->RD = 1;')
        self.addl('    regs->P.T = 0;')
        self.addl('    HUC6280_poll_IRQs(regs, pins);')
        self.addl('    regs->TCU = 0;')
        self.addl('    return;')


    def addl(self, what: str):
        self.outstr += self.indent4 + what + '\n'

    def dummy_read(self, last=False):
        self.addr_to_PC()
        self.RW(1, 0)
        if not last:
            self.addcycle('dummy read...')

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
            self.RW(1, 0)
            self.set_down_rw()
        if not self.override_IRQ:
            self.poll_IRQs()
        else:
            self.override_IRQ_do()
        if not self.no_T_at_end:
            self.addl('regs->P.T = 0;')
        self.addl('regs->TCU = 0;')
        self.addl('return;')

    def RW(self, rd: int, wr: int, force: bool = False):
        self.new_rd = rd
        self.new_wr = wr

    def addr_to_PC(self):
        self.addl('pins->Addr = regs->MPR[regs->PC >> 13] | (regs->PC & 0x1FFF);')

    def addr_to_PC_then_inc(self):
        self.addr_to_PC()
        self.addl('regs->PC = (regs->PC + 1) & 0xFFFF;')

    def addr_to_S_then_dec(self):
        self.addl('pins->Addr = regs->S | 0x100;')
        self.addl('regs->S = (regs->S - 1) & 0xFF;')

    def addr_to(self, what: str):
        self.addl('pins->Addr = (' + what + ');')

    def finished(self):
        if not self.in_case:
            return ''
        self.regular_end()

        self.outstr += self.indent3 + '}\n' + self.indent2 + '}\n' + self.indent1 + '}\n'
        return self.indent2 + 'switch(regs->TCU) {\n' + self.outstr

    def setz(self, what):
        self.addl('regs->P.Z = (' + what + ') == 0;')

    def setn(self, what):
        self.addl('regs->P.N = ((' + what + ') & 0x80) >> 7;')

    def load16(self, dest, source, name=None, last=False):
        self.addl('pins->Addr = regs->MPR[(' + source + ')>>13] | ((' + source + ') & 0x1FFF);')
        self.RW(1, 0)
        if not last:
            self.addcycle('load16')
            self.RW(0, 0)
            if dest is not None:
                self.addl(dest + ' = pins->D;')

    def inc_PC(self):
        self.addl('regs->PC = (regs->PC + 1) & 0xFFFF;')

    def operand(self, dest=None, last=False):
        self.load16(dest, 'regs->PC', 'operand', last=last)
        self.inc_PC()

    def pull(self, wha: str, last: bool = False) -> None:
        self.addl('regs->S = (regs->S + 1) & 0xFF;')
        self.addl('pins->Addr = regs->MPR[1] | 0x100 | regs->S;')
        self.RW(1, 0)
        if not last:
            self.addcycle('pull')
        else:
            self.cleanup()
        self.addl(wha + ' = pins->D;')

    def push(self, wha: str, last: bool = False) -> None:
        self.addl('pins->Addr = regs->MPR[1] | 0x100 | regs->S;')
        self.addl('pins->D = ' + wha + ';')
        self.addl('regs->S = (regs->S - 1) & 0xFF;')
        self.RW(0, 1)
        if not last:
            self.addcycle('push')

    def load8(self, dest, source, last=False):
        self.addl('pins->Addr = regs->MPR[1] | (' + source + ');')
        self.RW(1, 0)
        if not last:
            self.addcycle('load8')
            if dest is not None:
                self.addl(dest + ' = pins->D;')

    def store8(self, addr, val, last=False):
        self.addl('pins->Addr = regs->MPR[1] | (' + addr + ');')
        self.addl('pins->D = ' + val + ';')
        self.RW(0, 1)
        if not last:
            self.addcycle('store8')

    def store16(self, addr, val, last=False):
        self.addl('pins->Addr = regs->MPR[(' + addr + ') >> 13] | ((' + addr + ') & 0x1FFF);')
        self.addl('pins->D = ' + val + ';')
        self.RW(0, 1)
        if not last:
            self.addcycle('store16')


def hex2(yo):
    # print(f'{number:04X}')
    return f'{yo:02X}'


def C_func_name(mo, tbit):
    if mo < 0x100:
        return 'HUC6280_ins_' + hex2(mo) + '_' + '_t' + str(tbit)
    if mo == 0x100:
        return 'HUC6280_ins_RESET_t' + str(tbit)
    if mo == 0x101:
        return 'HUC6280_ins_IRQ2_t' + str(tbit)
    if mo == 0x102:
        return 'HUC6280_ins_IRQ1_t' + str(tbit)
    if mo == 0x103:
        return 'HUC6280_ins_TIQ_t' + str(tbit)


def C_func_sig(mo, tbit):
    return 'static void ' + C_func_name(mo, tbit) + '(struct HUC6280_regs *regs, struct HUC6280_pins *pins)'


def IndirectYReadMemory(ag: huc6280_switchgen, ins_func) -> str:
    ag.is_mem = True
    ag.addl('regs->TR[2] = regs->A;')

    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.addl('regs->TA = (regs->TA + regs->Y) & 0xFFFF;')
    ag.load16('regs->TR[3]', 'regs->TA')
    ag.load8('regs->A', 'regs->X')
    ins_func(ag, 'regs->A', 'regs->TR[3]')
    ag.addcycle('idle')
    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def Break(ag: huc6280_switchgen):
    ag.operand()
    ag.push('regs->PC >> 8')
    ag.push('regs->PC & 0xFF')
    ag.addl('regs->P.T = 0;')
    ag.push('regs->P.u | 0x10')
    ag.addl('regs->P.D = 0; regs->P.I = 1;')
    ag.load16('regs->PC', '0xFFF6')
    ag.load16('regs->TA', '0xFFF7')
    ag.addl('regs->PC |= regs->TA << 8;')
    return ag.finished()


def StoreImplied(ag: huc6280_switchgen, addr: str) -> str:
    ag.operand('regs->TR[0]')
    ag.addcycle('idle')

    ag.addl('pins->Addr = ' + addr + ';')
    ag.addl('pins->D = regs->TR[0];')
    ag.RW(0, 1)

    return ag.finished()


def Clear(ag: huc6280_switchgen, dt):
    ag.dummy_read(last=True)
    ag.cleanup()
    ag.addl(dt + ' = 0;')

    return ag.finished()


def SetMemoryBit(ag: huc6280_switchgen, bitnum):
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.addl('regs->TR[0] |= 1 << ' + bitnum + ';')
    ag.store8('regs->TA', 'regs->TR[0]', last=True)

    return ag.finished()


def Transfer(ag: huc6280_switchgen, src, tgt) -> str:
    ag.dummy_read(last=True)
    ag.cleanup()
    ag.addl(tgt + ' = ' + src + ';')
    ag.addl('regs->P.Z = ' + tgt + ' == 0;')
    ag.addl('regs->P.N = (' + tgt + ' >> 7) & 1;')

    return ag.finished()


def TransferXS(ag: huc6280_switchgen) -> str:
    ag.dummy_read(last=True)
    ag.cleanup()
    ag.addl('regs->S = regs->X;')

    return ag.finished()


def ResetMemoryBit(ag: huc6280_switchgen, bitnum):
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.addl('regs->TR[0] &= ~(1 << ' + bitnum + ');')
    ag.store8('regs->TA', 'regs->TR[0]', last=True)
    return ag.finished()


def JumpIndirect(ag: huc6280_switchgen, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + ' + inx + ') & 0xFFFF;')
    ag.load16('regs->PC', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA')
    ag.addl('regs->PC |= regs->TR[0] << 8;')
    #ag.addcycle('idle')

    return ag.finished()


def Pull(ag: huc6280_switchgen, dt: str):
    ag.dummy_read()
    ag.addcycle('idle')
    ag.pull(dt, True)
    ag.setz(dt)
    ag.setn(dt)
    return ag.finished()


def Push(ag: huc6280_switchgen, dt: str):
    ag.dummy_read()
    ag.push(dt, True)
    return ag.finished()


def Immediate(ag: huc6280_switchgen, ins_func, dt):
    ag.operand("regs->TA", True)
    ag.cleanup()
    ins_func(ag, dt, 'pins->D')
    return ag.finished()


def AbsoluteWrite(ag: huc6280_switchgen, dt, inx: Optional[str] = None):
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + ' + inx + ' ) & 0xFFFF;')
    ag.store16('regs->TA', dt, last=True)

    return ag.finished()


def AbsoluteModify(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None):
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA')
    ag.addcycle('idle')
    ins_func(ag, 'regs->TR[1]', 'regs->TR[0]')
    ag.store16('regs->TA', 'regs->TR[1]', last=True)

    return ag.finished()


def Implied(ag: huc6280_switchgen, ins_func, dt):
    ag.dummy_read(last=True)
    ag.cleanup()
    ins_func(ag, dt, dt)

    return ag.finished()


def CallAbsolute(ag: huc6280_switchgen):
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.push('regs->PC >> 8')
    ag.push('regs->PC & 0xFF')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addl('regs->PC = regs->TA;')
    return ag.finished()


def S_RESET(ag: huc6280_switchgen) -> str:
    ag.addcycle('3')
    ag.RW(0, 0, force=True)
    ag.addr_to_S_then_dec()

    ag.addcycle('4')
    ag.addr_to_S_then_dec()

    ag.addcycle('5')
    ag.addr_to_S_then_dec()

    ag.addcycle('6')
    ag.addr_to_S_then_dec()

    ag.addcycle('7')
    ag.addl('regs->MPR[7] = 0;')
    ag.addl('regs->MPL = 0;')
    ag.addl('regs->P.I = 1;')
    ag.addl('regs->P.D = regs->P.T = 0;')
    ag.addl('regs->IRQD.u = 0;')
    ag.addl('regs->timer_startstop = 0;')
    ag.addl('regs->clock_div = 12;')
    ag.addl('pins->Addr = 0x1FFE;')
    ag.RW(1, 0)

    ag.addcycle('8')
    ag.addl('regs->PC = pins->D;')
    ag.addl('pins->Addr++;')
    ag.RW(1, 0)

    ag.addcycle('9')
    ag.cleanup()
    ag.addl('regs->PC |= pins->D << 8;')

    return ag.finished()


def S_IRQ(ag: huc6280_switchgen, vector: str) -> str:
    ag.dummy_read()
    ag.dummy_read()
    ag.push('regs->PC >> 8')
    ag.push('regs->PC & 0xFF')
    ag.push('regs->P.u & 0xEF')
    ag.addl('regs->P.I = 1;')
    ag.addl('regs->P.D = 0;')
    ag.addl('regs->P.T = 0;')
    ag.addl('regs->TA = ' + vector + ';')
    ag.load16('regs->PC', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA', last=True)
    ag.addl('regs->PC |= regs->TR[0] << 8;')

    return ag.finished()


def BranchIfBitSet(ag: huc6280_switchgen, bitnum: str) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.operand('regs->TR[2]')
    ag.addcycle('idle')
    ag.load8(None, 'regs->TA')
    ag.addl('if ((pins->D & ' + str(1 << int(bitnum)) + ') == 0) {')
    ag.br_end('    ')
    ag.addl('}')
    ag.addl('regs->PC = (regs->PC + (u32)(i8)regs->TR[2]) & 0xFFFF;')
    ag.addcycle('idle')
    return ag.finished()


def BranchIfBitReset(ag: huc6280_switchgen, bitnum: str):
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.operand('regs->TR[2]')
    ag.addcycle('idle')
    ag.load8(None, 'regs->TA')
    ag.addl('if ((pins->D & ' + str(1 << int(bitnum)) + ') != 0) {')
    ag.br_end('    ')
    ag.addl('}')
    ag.addl('regs->PC = (regs->PC + (u32)(i8)regs->TR[2]) & 0xFFFF;')
    ag.addcycle('idle')
    return ag.finished()


def al_TII(ag: huc6280_switchgen) -> None:
    ag.addl('regs->TR[0] = (regs->TR[0] + 1) & 0xFFFF;')  # source
    ag.addl('regs->TR[1] = (regs->TR[1] + 1) & 0xFFFF;')  # target


def al_TAI(ag: huc6280_switchgen) -> None:
    ag.addl('regs->TR[0] += regs->TR[3] ? -1 : 1;')
    ag.addl('regs->TR[0] &= 0xFFFF;')
    ag.addl('regs->TR[1] = (regs->TR[1] + 1) & 0xFFFF;')


def al_TDD(ag: huc6280_switchgen) -> None:
    ag.addl('regs->TR[0] = (regs->TR[0] - 1) & 0xFFFF;')
    ag.addl('regs->TR[1] = (regs->TR[1] - 1) & 0xFFFF;')


def al_TIA(ag: huc6280_switchgen) -> None:
    ag.addl('regs->TR[0] = (regs->TR[0] + 1) & 0xFFFF;')
    ag.addl('regs->TR[1] += regs->TR[3] ? -1 : 1;')
    ag.addl('regs->TR[1] &= 0xFFFF;')


def al_TIN(ag: huc6280_switchgen) -> None:
    ag.addl('regs->TR[0] = (regs->TR[0] + 1) & 0xFFFF;')


def BlockMove(ag: huc6280_switchgen, ifunc) -> str:
    ag.dummy_read()
    ag.addcycle('idle')
    ag.push('regs->Y')
    ag.push('regs->A')
    ag.push('regs->X')
    ag.operand('regs->TR[0]')  # 0=source
    ag.operand('regs->TR[6]')
    ag.addl('regs->TR[0] |= regs->TR[6] << 8;')
    ag.operand('regs->TR[1]')  # 1=target
    ag.operand('regs->TR[5]')
    ag.addl('regs->TR[1] |= regs->TR[5] << 8;')
    ag.operand('regs->TR[2]')  # 2=length
    ag.operand('regs->TR[5]')
    ag.addl('regs->TR[2] |= regs->TR[5] << 8;')
    ag.addcycle('idle')
    ag.addl('pins->BM = 1;')
    ag.addl('regs->TR[3] = 0;')  # 3=alternate
    ag.addcycle('idle')

    # Now we start the loop ...
    ag.load16('regs->TR[4]', 'regs->TR[0]')
    ag.addcycle('idle')
    ag.store16('regs->TR[1]', 'regs->TR[4]')
    ifunc(ag)
    ag.addl('regs->TR[3] ^= 1;')
    ag.addcycle('idle in loop')
    ag.addcycle('idle in loop')
    ag.addl('regs->TR[2] = (regs->TR[2] - 1) & 0xFFFF;')
    ag.addl('if (regs->TR[2]) regs->TCU -= 6; // TESTME!')
    ag.addcycle('idle out loop')
    ag.pull('regs->X')
    ag.addl('pins->BM = 0;')
    ag.pull('regs->A')
    ag.pull('regs->Y', last=True)

    return ag.finished()


def IndirectYWrite(ag: huc6280_switchgen, dt: str) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addl('regs->TR[0] |= regs->TR[1] << 8;')
    ag.addcycle('idle')
    ag.addl('regs->TA = (regs->TR[0] + regs->Y) & 0xFFFF;')
    ag.store16('regs->TA', dt, last=True)

    return ag.finished()


def IndirectWrite(ag: huc6280_switchgen, dt: str, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ' )) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.addcycle('idle')
    ag.store16('regs->TA', dt, last=True)
    return ag.finished()


def TestAbsolute(ag: huc6280_switchgen, inx: Optional[str] = None) -> str:
    ag.operand('regs->TR[0]')
    ag.operand('regs->TA')
    ag.operand('regs->TR[1]')
    ag.addl('regs->TA |= regs->TR[1] << 8;')
    ag.addcycle('idle')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + ' + inx + ' ) & 0xFFFF;')
    ag.load16('regs->TR[2]', 'regs->TA')
    ag.cleanup()
    ag.addl('regs->P.Z = (regs->TR[2] & regs->TR[0]) == 0;')
    ag.addl('regs->P.V = (regs->TR[2] >> 6) & 1;')
    ag.addl('regs->P.N = (regs->TR[2] >> 7) & 1;')

    return ag.finished()


def TestZeroPage(ag: huc6280_switchgen, inx: Optional[str] = None) -> str:
    ag.operand('regs->TR[0]')
    ag.operand('regs->TR[1]')
    ag.addcycle('idle')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TR[1] = (regs->TR[1] + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TA', 'regs->TR[1]')
    ag.addl('regs->P.Z = (regs->TR[0] & regs->TA) == 0;')
    ag.addl('regs->P.V = (regs->TA >> 6) & 1;')
    ag.addl('regs->P.N = (regs->TA >> 7) & 1;')

    return ag.finished()


def Branch(ag: huc6280_switchgen, expr) -> str:
    ag.operand('regs->TA')
    if expr != '1':
        ag.addl('if (!' + expr + ') {')
        ag.br_end('    ')
        ag.addl('}')
    ag.addl('regs->TA = (regs->PC + (u32)(i8)pins->D) & 0xFFFF;')
    if expr != '1':
        ag.dummy_read(last=True)
    ag.addcycle('idle')
    ag.cleanup()
    ag.addl('regs->PC = regs->TA;')
    return ag.finished()


def IndirectYRead(ag: huc6280_switchgen, ins_func, dt):
    ag.operand('regs->TA')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.addl('regs->TA = (regs->TA + regs->Y) & 0xFFFF;')
    ag.load16('regs->TR[2]', 'regs->TA', last=True)
    ag.cleanup()
    ins_func(ag, dt, 'pins->D')
    return ag.finished()


def AbsoluteRead(ag: huc6280_switchgen, ins_func, dt, inx: Optional[str] = None):
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + ' + inx + ') & 0xFFFF;')
    ag.load16('regs->TR[0]', 'regs->TA', last=True)
    ag.cleanup()
    ins_func(ag, dt, 'pins->D')

    return ag.finished()


def NOP(ag: huc6280_switchgen):
    ag.dummy_read(last=True)
    return ag.finished()


def ZeroPageRead(ag: huc6280_switchgen, ins_func, dt, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA', last=True)
    ag.cleanup()
    ins_func(ag, dt, 'pins->D')
    return ag.finished()


def PullP(ag: huc6280_switchgen) -> str:
    ag.dummy_read()
    ag.addcycle('idle')
    ag.pull('regs->P.u', last=True)
    ag.addl('regs->P.u &= 0xEF;')
    ag.no_T_at_end = True

    return ag.finished()


def ZeroPageModify(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addcycle('idle')
    ins_func(ag, 'regs->TR[1]', 'regs->TR[0]')
    ag.store8('regs->TA', 'regs->TR[1]', last=True)

    return ag.finished()


def Swap(ag: huc6280_switchgen, data1, data2) -> str:
    ag.dummy_read()
    ag.cleanup()
    ag.addl('regs->TA = ' + data1 + ';')
    ag.addl(data1 + ' = ' + data2 + ';')
    ag.addl(data2 + ' = regs->TA;')

    return ag.finished()


def IndirectRead(ag: huc6280_switchgen, ins_func, dta: Optional[str] = None, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')
    ag.addcycle('idle')
    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')
    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')
    ag.load16('regs->TR[0]', 'regs->TA', last=True)
    ag.cleanup()
    ag.addl('regs->TR[0] = pins->D;')
    ins_func(ag, dta, 'regs->TR[0]')

    return ag.finished()


def IndirectReadMemory(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.is_mem = True
    ag.addl('regs->TR[2] = regs->A;')
    ag.operand('regs->TA')
    if inx:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFF;')

    ag.addcycle('idle')

    ag.load8('regs->TR[0]', 'regs->TA')
    ag.addl('regs->TA = (regs->TA + 1) & 0xFF;')
    ag.load8('regs->TR[1]', 'regs->TA')
    ag.addcycle('idle')

    ag.addl('regs->TA = regs->TR[0] | (regs->TR[1] << 8);')

    ag.load16('regs->TR[0]', 'regs->TA')
    ag.load8('regs->A', 'regs->X')

    ins_func(ag, 'regs->A', 'regs->TR[0]')
    ag.addcycle('idle')
    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def AbsoluteReadMemory(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.is_mem = True
    ag.addl('regs->TR[2] = regs->A;')

    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ')) & 0xFFFF;')

    ag.load16('regs->TR[0]', 'regs->TA')
    ag.load8('regs->A', 'regs->X')
    ins_func(ag, 'regs->A', 'regs->TR[0]')
    ag.addcycle('idle')

    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def ImmediateMemory(ag: huc6280_switchgen, ins_func) -> str:
    ag.is_mem = True
    ag.addl('regs->TR[2] = regs->A;')
    ag.operand('regs->TA')
    ag.load8('regs->A', 'regs->X')
    ins_func(ag, 'regs->A', 'regs->TA')
    ag.addcycle('idle')

    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def Set(ag: huc6280_switchgen, dt) -> str:
    ag.dummy_read(last=True)
    ag.cleanup()
    ag.addl(dt + ' = 1;')
    if dt == 'regs->P.T':
        ag.no_T_at_end = True
    return ag.finished()


def BranchSubroutine(ag: huc6280_switchgen) -> str:
    ag.operand('regs->TA')
    ag.addl('regs->TA = (u32)(i8)regs->TA;')
    ag.addcycle('idle')
    ag.addl('regs->TR[0] = (regs->PC - 1) & 0xFFFF;')
    ag.push('regs->TR[0] >> 8')
    ag.addl('regs->PC += regs->TA;')
    ag.push('regs->TR[0] & 0xFF')
    ag.addcycle('idle')
    ag.addcycle('idle')

    return ag.finished()


def ChangeSpeedLow(ag: huc6280_switchgen) -> str:
    ag.dummy_read()
    ag.addl('regs->clock_div = 12;')

    return ag.finished()


def ChangeSpeedHigh(ag: huc6280_switchgen) -> str:
    ag.dummy_read()
    ag.addl('regs->clock_div = 3;')

    return ag.finished()


def ReturnFromSubroutine(ag: huc6280_switchgen) -> str:
    ag.dummy_read()
    ag.addcycle('idle')
    ag.pull('regs->PC')
    ag.pull('regs->TA')
    ag.addl('regs->PC |= regs->TA << 8;')
    ag.addcycle('idle')
    ag.addl('regs->PC = (regs->PC + 1) & 0xFFFF;')
    return ag.finished()


def ZeroPageWrite(ag: huc6280_switchgen, dt: str, inx: Optional[str] = None) -> str:
    ag.operand('regs->TA')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + ' + inx + ') & 0xFF;')
    ag.addcycle('idle')
    ag.store8('regs->TA', dt, last=True)

    return ag.finished()


def TransferAccumulatorToMPR(ag: huc6280_switchgen) -> str:
    ag.operand('regs->TA')

    ag.addcycle('idle')
    ag.addcycle('idle')
    ag.addl('if (regs->TA) {')
    ag.addl('    regs->MPL = regs->A;')
    ag.addl('    for (u32 i = 0; i < 8; i++) {')
    ag.addl('        u32 shifted = 1 << i;')
    ag.addl('        if (regs->TA & shifted)')
    ag.addl('            regs->MPR[i] = regs->MPL << 13;')
    ag.addl('    }')
    ag.addl('}')

    return ag.finished()


def TransferMPRToAccumulator(ag: huc6280_switchgen) -> str:
    ag.operand('regs->TR[0]')
    ag.addcycle('idle')
    ag.cleanup()
    ag.addl('if (regs->TR[0]) {')
    ag.addl('    regs->MPL = 0;')
    ag.addl('    for (u32 i = 0; i < 8; i++) { // inspired by Ares handling')
    ag.addl('        u32 shift = 1 << i;')
    ag.addl('        if (regs->TR[0] & shift)')
    ag.addl('            regs->MPL |= (regs->MPR[i] >> 13);')
    ag.addl('    }')
    ag.addl('}')
    ag.addl('regs->A = regs->MPL;')
    return ag.finished()


def ReturnFromInterrupt(ag: huc6280_switchgen) -> str:
    ag.dummy_read()
    ag.addcycle('idle')
    ag.pull('regs->P.u')
    ag.addl('regs->P.u &= 0xEF;')
    ag.pull('regs->PC')
    ag.pull('regs->TA', last=True)
    ag.addl('regs->PC |= regs->TA << 8;')
    ag.addcycle('idle')
    ag.no_T_at_end = True
    return ag.finished()


def JumpAbsolute(ag: huc6280_switchgen) -> str:
    ag.operand('regs->TA')
    ag.operand('regs->TR[0]')
    ag.addl('regs->TA |= regs->TR[0] << 8;')
    ag.cleanup()
    ag.addl('regs->PC = regs->TA;')

    return ag.finished()


def ZeroPageReadMemory(ag: huc6280_switchgen, ins_func, inx: Optional[str] = None) -> str:
    ag.is_mem = True
    ag.addl('regs->TR[2] = regs->A;')
    ag.operand('regs->TA')
    ag.addcycle('idle')
    if inx is not None:
        ag.addl('regs->TA = (regs->TA + (' + inx + ' )) & 0xFF;')


    ag.load8('regs->A', 'regs->TA')
    ag.load8('regs->TR[0]', 'regs->X')


    ins_func(ag, 'regs->A', 'regs->TR[0]')
    ag.addcycle('idle')

    ag.store8('regs->X', 'regs->A', True)
    ag.addl('regs->A = regs->TR[2];')

    return ag.finished()


def al_CMP(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    '''
  n9 o = A - i;
  C = !o.bit(8);
  Z = n8(o) == 0;
  N = o.bit(7);
    '''
    ag.addl('u32 a = (regs->A - (' + source + ')) & 0x1FF;')
    ag.addl('regs->P.C = ((a >> 8) & 1) ^ 1;')
    ag.addl('regs->P.Z = (a & 0xFF) == 0;')
    ag.addl('regs->P.N = (a >> 7) & 1;')


def al_CPX(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('u32 o = regs->X - (' + source + ');')
    ag.addl('regs->P.C = ((o >> 8) & 1) ^ 1;')
    ag.addl('regs->P.Z = (o & 0xFF) == 0;')
    ag.addl('regs->P.N = (o >> 7) & 1;')


def al_CPY(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('u32 o = regs->Y - (' + source + ');')
    ag.addl('regs->P.C = ((o >> 8) & 1) ^ 1;')
    ag.addl('regs->P.Z = (o & 0xFF) == 0;')
    ag.addl('regs->P.N = (o >> 7) & 1;')


def al_TRB(ag: huc6280_switchgen, dest: Optional[str], source: str):
    ag.addl('regs->P.Z = (regs->A & (' + source + ')) == 0;')
    ag.addl('regs->P.V = ((' + source + ') >> 6) & 1;')
    ag.addl('regs->P.N = ((' + source + ') >> 7) & 1;')
    if dest is not None:
        ag.addl(dest + ' = ~regs->A & (' + source + ');')


def al_DEC(ag: huc6280_switchgen, dest: str, source: str):
    ag.addl(dest + ' = ((' + source + ') - 1) & 0xFF;')
    ag.setz(dest)
    ag.setn(dest)


def al_INC(ag: huc6280_switchgen, dest: str, source: str):
    ag.addl(dest + ' = ((' + source + ') + 1) & 0xFF;')
    ag.setz(dest)
    ag.setn(dest)


def al_BIT(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('regs->P.V = ((' + source + ') >> 6) & 1;')
    ag.addl('regs->P.N = ((' + source + ') >> 7) & 1;')
    ag.addl('regs->P.Z = ((' + source + ') & regs->A) == 0;')
    #if dest is not None:
    #    ag.addl(dest + ' = (regs->A & (' + source + ')) == 0;')


def al_ORA(ag: huc6280_switchgen, dest: str, source: str) -> None:
    ag.addl(dest + ' = regs->A | (' + source + ');')
    ag.setz(dest)
    ag.setn(dest)


def al_LSR(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('regs->P.C = (' + source + ') & 1;')
    if dest is None:
        ag.addl('u32 o;')
        dest = 'o'
    ag.addl(dest + ' = (' + source + ') >> 1;')
    ag.setz(dest)
    ag.setn(dest)


def al_LD(ag: huc6280_switchgen, dest: Optional[str], source: str):
    ag.addl('regs->P.Z = ' + source + ' == 0;')
    ag.addl('regs->P.N = (' + source + ' >> 7) & 1;')
    if dest is not None:
        ag.addl(dest + ' = ' + source + ';')


def al_ROR(ag: huc6280_switchgen, dest: Optional[str], source: str):
    ag.addl('u32 c = regs->P.C << 7;')
    ag.addl('regs->P.C = (' + source + ') & 1;')
    ag.addl('c = (((' + source + ') >> 1) | c) & 0xFF;')
    ag.addl('regs->P.Z = c == 0;')
    ag.addl('regs->P.N = (c >> 7) & 1;')
    if dest is not None:
        ag.addl(dest + ' = c;')


def al_ROL(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('u32 c = regs->P.C;')
    ag.addl('regs->P.C = ((' + source + ') >> 7) & 1;')
    ag.addl(source + ' = ((' + source + ' << 1) & 0xFF) | c;')
    ag.addl('regs->P.Z = ' + source + ' == 0;')
    ag.addl('regs->P.N = ((' + source + ') >> 7) & 1;')
    if dest is not None:
        ag.addl(dest + ' = (' + source + ');')


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


def al_SBC(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl(source + ' ^= 0xFF;')
    ag.addl('i16 out = (i16)regs->A + (i16)(' + source + ') + (i16)regs->P.C;')
    ag.addl('if (!regs->P.D) {')
    ag.addl('    regs->P.C = out > 0xFF;')
    ag.addl('    regs->P.V = ((~(regs->A ^ (' + source + ')) & (regs->A ^ out)) >> 7) & 1;')
    ag.addl('    out &= 0xFF;')
    ag.setz('out')
    ag.setn('out')
    if dest is not None:
        ag.addl(dest + ' = out;')
    ag.br_end('    ')
    ag.addl('}')
    ag.addl('else { // if decimal')
    ag.addl('    out = (regs->A & 15) + ((' + source + ') & 15) + regs->P.C;')
    ag.addl('    if (out <= 15) out -= 6;')
    ag.addl('    out = ((' + source + ') & 0xF0) + (regs->A & 0xF0) + (out > 15 ? 0x10 : 0) + (out & 15);')
    ag.addl('    if (out <= 0xFF) out -= 0x60;')
    ag.addl('    regs->P.C = out > 0xFF;')
    ag.addl('    out &= 0xFF;')
    ag.setz('out')
    ag.setn('out')
    if dest is not None:
        ag.addl(dest + ' = out;')
    ag.addl('}')

    if ag.is_mem:
        ag.addcycle('idle')
    else:
        ag.dummy_read()
    return

    ag.addl('u8 r = ' + source + ' ^ 0xFF;')
    ag.addl('i16 out = (i16)regs->A + (i16)r + (i16)regs->P.C;')
    ag.addl('if (!regs->P.D) {')
    ag.addl('    regs->P.C = (out >> 8) & 1;')
    ag.addl('    regs->P.V = ((~(regs->A ^ (' + source + ')) & (regs->A & out)) >> 7) & 1;')
    ag.addl('    regs->TCU++;')
    ag.addl('}')
    ag.addl('else {')
    ag.addl('    u8 lo = (regs->A & 15) + ((' + source + ') & 15) + regs->P.C;')
    ag.addl('    regs->P.C = out > 0xFF;')
    ag.addl('    if (out <= 0xFF) out -= 0x60;')
    ag.addl('    if (lo <= 0x0F) out -= 6;')
    ag.addl('}')
    ag.setz('out')
    ag.setn('out')
    if dest is not None:
        ag.addl(dest + ' = out;')
    ag.addcycle('idle')


def al_ADC(ag: huc6280_switchgen, dest: Optional[str], source: str) -> None:
    ag.addl('i16 out = (i16)regs->A + (i16)(' + source + ') + (i16)regs->P.C;')
    ag.addl('if (!regs->P.D) {')
    ag.addl('    regs->P.C = out > 0xFF;')
    ag.addl('    regs->P.V = ((~(regs->A ^ (' + source + ')) & (regs->A ^ out)) >> 7) & 1;')
    ag.addl('    out &= 0xFF;')
    ag.setz('out')
    ag.setn('out')

    if dest is not None:
        ag.addl(dest + ' = out;')
    if ag.is_mem:
        ag.addl('    regs->TCU++;')
    else:
        ag.br_end('    ')
    ag.addl('}')
    ag.addl('else { // if decimal')
    ag.addl('    out = (regs->A & 15) + ((' + source + ') & 15) + regs->P.C;')
    ag.addl('    if (out > 9) out += 6;')
    ag.addl('    out = ((' + source + ') & 0xF0) + (regs->A & 0xF0) + (out > 15 ? 0x10 : 0) + (out & 15);')
    ag.addl('    if (out > 0x9F) out += 0x60;')
    ag.addl('    regs->P.C = out > 0xFF;')
    ag.addl('    out &= 0xFF;')
    ag.setz('out')
    ag.setn('out')
    if dest is not None:
        ag.addl(dest + ' = out;')
    ag.addl('}')

    if ag.is_mem:
        ag.addcycle('idle')
    else:
        ag.dummy_read()


def write_instruction_table(outfile):
    num_on_line = 0
    out = 'HUC6280_ins_func HUC6280_decoded_opcodes[2][0x104] = {\n    '
    for tbit in range(0, 2):
        for i in range(0, 0x100 + 4):
            if num_on_line == 0:
                out += '    '
            else:
                out += '  '

            out += '&' + C_func_name(i, tbit)
            if i == 0x103 and tbit == 1:
                pass
            else:
                out += ','
            num_on_line += 1
            if num_on_line >= 8:
                num_on_line = 0
                out += '\n  '
    out += '\n};\n'
    outfile.write(out)


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
        elif opcode > 0xE0:
            ag.is_mem = True
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
            r = Push(ag, '(regs->P.u | 0x10) & 0xDF')
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
            r = StoreImplied(ag, '0x1FE002')
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
        elif opcode == 0x1A:
            r = Implied(ag, al_INC, 'regs->A')
        elif opcode == 0x1B:
            r = NOP(ag)
        elif opcode == 0x1C:
            r = AbsoluteModify(ag, al_TRB)
        elif opcode == 0x1D:
            r = AbsoluteRead(ag, al_ORA, 'regs->A', 'regs->X')
        elif opcode == 0x1E:
            r = AbsoluteModify(ag, al_ASL, 'regs->X')
        elif opcode == 0x1F:
            r = BranchIfBitReset(ag, '1')
        elif opcode == 0x20:
            r = CallAbsolute(ag)
        elif opcode == 0x21:
            r = IndirectRead(ag, al_AND, 'regs->A', 'regs->X')
        elif opcode == 0x22:
            r = Swap(ag, 'regs->A', 'regs->X')
        elif opcode == 0x23:
            r = StoreImplied(ag, '0x1FE003')
        elif opcode == 0x24:
            r = ZeroPageRead(ag, al_BIT, 'regs->A')
        elif opcode == 0x25:
            r = ZeroPageRead(ag, al_AND, 'regs->A')
        elif opcode == 0x26:
            r = ZeroPageModify(ag, al_ROL)
        elif opcode == 0x27:
            r = ResetMemoryBit(ag, '2')
        elif opcode == 0x28:
            r = PullP(ag)
        elif opcode == 0x29:
            r = Immediate(ag, al_AND, 'regs->A')
        elif opcode == 0x2A:
            r = Implied(ag, al_ROL, 'regs->A')
        elif opcode == 0x2B:
            r = NOP(ag)
        elif opcode == 0x2C:
            r = AbsoluteRead(ag, al_BIT, 'regs->A')
        elif opcode == 0x2D:
            r = AbsoluteRead(ag, al_AND, 'regs->A')
        elif opcode == 0x2E:
            r = AbsoluteModify(ag, al_ROL)
        elif opcode == 0x2F:
            r = BranchIfBitReset(ag, '2')
        elif opcode == 0x30:
            r = Branch(ag, 'regs->P.N')
        elif opcode == 0x31:
            r = IndirectYRead(ag, al_AND, 'regs->A')
        elif opcode == 0x32:
            r = IndirectRead(ag, al_AND, 'regs->A')
        elif opcode == 0x33:
            r = NOP(ag)
        elif opcode == 0x34:
            r = ZeroPageRead(ag, al_BIT, 'regs->A', 'regs->X')
        elif opcode == 0x35:
            r = ZeroPageRead(ag, al_AND, 'regs->A', 'regs->X')
        elif opcode == 0x36:
            r = ZeroPageModify(ag, al_ROL, 'regs->X')
        elif opcode == 0x37:
            r = ResetMemoryBit(ag, '3')
        elif opcode == 0x38:
            r = Set(ag, 'regs->P.C')
        elif opcode == 0x39:
            r = AbsoluteRead(ag, al_AND, 'regs->A', 'regs->Y')
        elif opcode == 0x3A:
            r = Implied(ag, al_DEC, 'regs->A')
        elif opcode == 0x3B:
            r = NOP(ag)
        elif opcode == 0x3C:
            r = AbsoluteRead(ag, al_BIT, 'regs->A', 'regs->X')
        elif opcode == 0x3D:
            r = AbsoluteRead(ag, al_AND, 'regs->A', 'regs->X')
        elif opcode == 0x3E:
            r = AbsoluteModify(ag, al_ROL, 'regs->X')
        elif opcode == 0x3F:
            r = BranchIfBitReset(ag, '3')
        elif opcode == 0x40:
            r = ReturnFromInterrupt(ag)
        elif opcode == 0x41:
            r = IndirectRead(ag, al_EOR, 'regs->A', 'regs->X')
        elif opcode == 0x42:
            r = Swap(ag, 'regs->A', 'regs->Y')
        elif opcode == 0x43:
            r = TransferMPRToAccumulator(ag)
        elif opcode == 0x44:
            r = BranchSubroutine(ag)
        elif opcode == 0x45:
            r = ZeroPageRead(ag, al_EOR, 'regs->A')
        elif opcode == 0x46:
            r = ZeroPageModify(ag, al_LSR)
        elif opcode == 0x47:
            r = ResetMemoryBit(ag, '4')
        elif opcode == 0x48:
            r = Push(ag, 'regs->A')
        elif opcode == 0x49:
            r = Immediate(ag, al_EOR, 'regs->A')
        elif opcode == 0x4A:
            r = Implied(ag, al_LSR, 'regs->A')
        elif opcode == 0x4B:
            r = NOP(ag)
        elif opcode == 0x4C:
            r = JumpAbsolute(ag)
        elif opcode == 0x4D:
            r = AbsoluteRead(ag, al_EOR, 'regs->A')
        elif opcode == 0x4E:
            r = AbsoluteModify(ag, al_LSR)
        elif opcode == 0x4F:
            r = BranchIfBitReset(ag, '4')
        elif opcode == 0x50:
            r = Branch(ag, '!regs->P.V')
        elif opcode == 0x51:
            r = IndirectYRead(ag, al_EOR, 'regs->A')
        elif opcode == 0x52:
            r = IndirectRead(ag, al_EOR, 'regs->A')
        elif opcode == 0x53:
            r = TransferAccumulatorToMPR(ag)
        elif opcode == 0x54:
            r = ChangeSpeedLow(ag)
        elif opcode == 0x55:
            r = ZeroPageRead(ag, al_EOR, 'regs->A', 'regs->X')
        elif opcode == 0x56:
            r = ZeroPageModify(ag, al_LSR, 'regs->X')
        elif opcode == 0x57:
            r = ResetMemoryBit(ag, '5')
        elif opcode == 0x58:
            r = Clear(ag, 'regs->P.I')
        elif opcode == 0x59:
            r = AbsoluteRead(ag, al_EOR, 'regs->A', 'regs->Y')
        elif opcode == 0x5A:
            r = Push(ag, 'regs->Y')
        elif opcode == 0x5B:
            r = NOP(ag)
        elif opcode == 0x5C:
            r = NOP(ag)
        elif opcode == 0x5D:
            r = AbsoluteRead(ag, al_EOR, 'regs->A', 'regs->X')
        elif opcode == 0x5E:
            r = AbsoluteModify(ag, al_LSR, 'regs->X')
        elif opcode == 0x5F:
            r = BranchIfBitReset(ag, '5')
        elif opcode == 0x60:
            r = ReturnFromSubroutine(ag)
        elif opcode == 0x61:
            r = IndirectRead(ag, al_ADC, 'regs->A', 'regs->X')
        elif opcode == 0x62:
            r = Clear(ag, 'regs->A')
        elif opcode == 0x63:
            r = NOP(ag)
        elif opcode == 0x64:
            r = ZeroPageWrite(ag, '0')
        elif opcode == 0x65:
            r = ZeroPageRead(ag, al_ADC, 'regs->A')
        elif opcode == 0x66:
            r = ZeroPageModify(ag, al_ROR)
        elif opcode == 0x67:
            r = ResetMemoryBit(ag, '6')
        elif opcode == 0x68:
            r = Pull(ag, 'regs->A')
        elif opcode == 0x69:
            r = Immediate(ag, al_ADC, 'regs->A')
        elif opcode == 0x6A:
            r = Implied(ag, al_ROR, 'regs->A')
        elif opcode == 0x6B:
            r = NOP(ag)
        elif opcode == 0x6C:
            r = JumpIndirect(ag)
        elif opcode == 0x6D:
            r = AbsoluteRead(ag, al_ADC, 'regs->A')
        elif opcode == 0x6E:
            r = AbsoluteModify(ag, al_ROR)
        elif opcode == 0x6F:
            r = BranchIfBitReset(ag, '6')
        elif opcode == 0x70:
            r = Branch(ag, 'regs->P.V')
        elif opcode == 0x71:
            r = IndirectYRead(ag, al_ADC, 'regs->A')
        elif opcode == 0x72:
            r = IndirectRead(ag, al_ADC, 'regs->A')
        elif opcode == 0x73:
            r = BlockMove(ag, al_TII)
        elif opcode == 0x74:
            r = ZeroPageWrite(ag, '0', 'regs->X')
        elif opcode == 0x75:
            r = ZeroPageRead(ag, al_ADC, 'regs->A', 'regs->X')
        elif opcode == 0x76:
            r = ZeroPageModify(ag, al_ROR, 'regs->X')
        elif opcode == 0x77:
            r = ResetMemoryBit(ag, '7')
        elif opcode == 0x78:
            r = Set(ag, 'regs->P.I')
        elif opcode == 0x79:
            r = AbsoluteRead(ag, al_ADC, 'regs->A', 'regs->Y')
        elif opcode == 0x7A:
            r = Pull(ag, 'regs->Y')
        elif opcode == 0x7B:
            r = NOP(ag)
        elif opcode == 0x7C:
            r = JumpIndirect(ag, 'regs->X')
        elif opcode == 0x7D:
            r = AbsoluteRead(ag, al_ADC, 'regs->A', 'regs->X')
        elif opcode == 0x7E:
            r = AbsoluteModify(ag, al_ROR, 'regs->X')
        elif opcode == 0x7F:
            r = BranchIfBitReset(ag, '7')
        elif opcode == 0x80:
            r = Branch(ag, '1')
        elif opcode == 0x81:
            r = IndirectWrite(ag, 'regs->A', 'regs->X')
        elif opcode == 0x82:
            r = Clear(ag, 'regs->X')
        elif opcode == 0x83:
            r = TestZeroPage(ag)
        elif opcode == 0x84:
            r = ZeroPageWrite(ag, 'regs->Y')
        elif opcode == 0x85:
            r = ZeroPageWrite(ag, 'regs->A')
        elif opcode == 0x86:
            r = ZeroPageWrite(ag, 'regs->X')
        elif opcode == 0x87:
            r = SetMemoryBit(ag, '0')
        elif opcode == 0x88:
            r = Implied(ag, al_DEC, 'regs->Y')
        elif opcode == 0x89:
            r = Immediate(ag, al_BIT, 'regs->A')
        elif opcode == 0x8A:
            r = Transfer(ag, 'regs->X', 'regs->A')
        elif opcode == 0x8B:
            r = NOP(ag)
        elif opcode == 0x8C:
            r = AbsoluteWrite(ag, 'regs->Y')
        elif opcode == 0x8D:
            r = AbsoluteWrite(ag, 'regs->A')
        elif opcode == 0x8E:
            r = AbsoluteWrite(ag, 'regs->X')
        elif opcode == 0x8F:
            r = BranchIfBitSet(ag, '0')
        elif opcode == 0x90:
            r = Branch(ag, '!regs->P.C')
        elif opcode == 0x91:
            r = IndirectYWrite(ag, 'regs->A')
        elif opcode == 0x92:
            r = IndirectWrite(ag, 'regs->A')
        elif opcode == 0x93:
            r = TestAbsolute(ag)
        elif opcode == 0x94:
            r = ZeroPageWrite(ag, 'regs->Y', 'regs->X')
        elif opcode == 0x95:
            r = ZeroPageWrite(ag, 'regs->A', 'regs->X')
        elif opcode == 0x96:
            r = ZeroPageWrite(ag, 'regs->X', 'regs->Y')
        elif opcode == 0x97:
            r = SetMemoryBit(ag, '1')
        elif opcode == 0x98:
            r = Transfer(ag, 'regs->Y', 'regs->A')
        elif opcode == 0x99:
            r = AbsoluteWrite(ag, 'regs->A', 'regs->Y')
        elif opcode == 0x9A:
            r = TransferXS(ag)
        elif opcode == 0x9B:
            r = NOP(ag)
        elif opcode == 0x9C:
            r = AbsoluteWrite(ag, '0')
        elif opcode == 0x9D:
            r = AbsoluteWrite(ag, 'regs->A', 'regs->X')
        elif opcode == 0x9E:
            r = AbsoluteWrite(ag, '0', 'regs->X')
        elif opcode == 0x9F:
            r = BranchIfBitSet(ag, '1')
        elif opcode == 0xA0:
            r = Immediate(ag, al_LD, 'regs->Y')
        elif opcode == 0xA1:
            r = IndirectRead(ag, al_LD, 'regs->A', 'regs->X')
        elif opcode == 0xA2:
            r = Immediate(ag, al_LD, 'regs->X')
        elif opcode == 0xA3:
            r = TestZeroPage(ag, 'regs->X')
        elif opcode == 0xA4:
            r = ZeroPageRead(ag, al_LD, 'regs->Y')
        elif opcode == 0xA5:
            r = ZeroPageRead(ag, al_LD, 'regs->A')
        elif opcode == 0xA6:
            r = ZeroPageRead(ag, al_LD, 'regs->X')
        elif opcode == 0xA7:
            r = SetMemoryBit(ag, '2')
        elif opcode == 0xA8:
            r = Transfer(ag, 'regs->A', 'regs->Y')
        elif opcode == 0xA9:
            r = Immediate(ag, al_LD, 'regs->A')
        elif opcode == 0xAA:
            r = Transfer(ag, 'regs->A', 'regs->X')
        elif opcode == 0xAB:
            r = NOP(ag)
        elif opcode == 0xAC:
            r = AbsoluteRead(ag, al_LD, 'regs->Y')
        elif opcode == 0xAD:
            r = AbsoluteRead(ag, al_LD, 'regs->A')
        elif opcode == 0xAE:
            r = AbsoluteRead(ag, al_LD, 'regs->X')
        elif opcode == 0xAF:
            r = BranchIfBitSet(ag, '2')
        elif opcode == 0xB0:
            r = Branch(ag, 'regs->P.C')
        elif opcode == 0xB1:
            r = IndirectYRead(ag, al_LD, 'regs->A')
        elif opcode == 0xB2:
            r = IndirectRead(ag, al_LD, 'regs->A')
        elif opcode == 0xB3:
            r = TestAbsolute(ag, 'regs->X')
        elif opcode == 0xB4:
            r = ZeroPageRead(ag, al_LD, 'regs->Y', 'regs->X')
        elif opcode == 0xB5:
            r = ZeroPageRead(ag, al_LD, 'regs->A', 'regs->X')
        elif opcode == 0xB6:
            r = ZeroPageRead(ag, al_LD, 'regs->X', 'regs->Y')
        elif opcode == 0xB7:
            r = SetMemoryBit(ag, '3')
        elif opcode == 0xB8:
            r = Clear(ag, 'regs->P.V')
        elif opcode == 0xB9:
            r = AbsoluteRead(ag, al_LD, 'regs->A', 'regs->Y')
        elif opcode == 0xBA:
            r = Transfer(ag, 'regs->S', 'regs->X')
        elif opcode == 0xBB:
            r = NOP(ag)
        elif opcode == 0xBC:
            r = AbsoluteRead(ag, al_LD, 'regs->Y', 'regs->X')
        elif opcode == 0xBD:
            r = AbsoluteRead(ag, al_LD, 'regs->A', 'regs->X')
        elif opcode == 0xBE:
            r = AbsoluteRead(ag, al_LD, 'regs->X', 'regs->Y')
        elif opcode == 0xBF:
            r = BranchIfBitSet(ag, '3')
        elif opcode == 0xC0:
            r = Immediate(ag, al_CPY, 'regs->Y')
        elif opcode == 0xC1:
            r = IndirectRead(ag, al_CMP, 'regs->A', 'regs->X')
        elif opcode == 0xC2:
            r = Clear(ag, 'regs->Y')
        elif opcode == 0xC3:
            r = BlockMove(ag, al_TDD)
        elif opcode == 0xC4:
            r = ZeroPageRead(ag, al_CPY, 'regs->Y')
        elif opcode == 0xC5:
            r = ZeroPageRead(ag, al_CMP, 'regs->A')
        elif opcode == 0xC6:
            r = ZeroPageModify(ag, al_DEC)
        elif opcode == 0xC7:
            r = SetMemoryBit(ag, '4')
        elif opcode == 0xC8:
            r = Implied(ag, al_INC, 'regs->Y')
        elif opcode == 0xC9:
            r = Immediate(ag, al_CMP, 'regs->A')
        elif opcode == 0xCA:
            r = Implied(ag, al_DEC, 'regs->X')
        elif opcode == 0xCB:
            r = NOP(ag)
        elif opcode == 0xCC:
            r = AbsoluteRead(ag, al_CPY, 'regs->Y')
        elif opcode == 0xCD:
            r = AbsoluteRead(ag, al_CMP, 'regs->A')
        elif opcode == 0xCE:
            r = AbsoluteModify(ag, al_DEC)
        elif opcode == 0xCF:
            r = BranchIfBitSet(ag, '4')
        elif opcode == 0xD0:
            r = Branch(ag, '!regs->P.Z')
        elif opcode == 0xD1:
            r = IndirectYRead(ag, al_CMP, 'regs->A')
        elif opcode == 0xD2:
            r = IndirectRead(ag, al_CMP, 'regs->A')
        elif opcode == 0xD3:
            r = BlockMove(ag, al_TIN)
        elif opcode == 0xD4:
            r = ChangeSpeedHigh(ag)
        elif opcode == 0xD5:
            r = ZeroPageRead(ag, al_CMP, 'regs->A', 'regs->X')
        elif opcode == 0xD6:
            r = ZeroPageModify(ag, al_DEC, 'regs->X')
        elif opcode == 0xD7:
            r = SetMemoryBit(ag, '5')
        elif opcode == 0xD8:
            r = Clear(ag, 'regs->P.D')
        elif opcode == 0xD9:
            r = AbsoluteRead(ag, al_CMP, 'regs->A', 'regs->Y')
        elif opcode == 0xDA:
            r = Push(ag, 'regs->X')
        elif opcode == 0xDB:
            r = NOP(ag)
        elif opcode == 0xDC:
            r = NOP(ag)
        elif opcode == 0xDD:
            r = AbsoluteRead(ag, al_CMP, 'regs->A', 'regs->X')
        elif opcode == 0xDE:
            r = AbsoluteModify(ag, al_DEC, 'regs->X')
        elif opcode == 0xDF:
            r = BranchIfBitSet(ag, '5')
        elif opcode == 0xE0:
            r = Immediate(ag, al_CPX, 'regs->X')
        elif opcode == 0xE1:
            r = IndirectRead(ag, al_SBC, 'regs->A', 'regs->X')
        elif opcode == 0xE2:
            r = NOP(ag)
        elif opcode == 0xE3:
            r = BlockMove(ag, al_TIA)
        elif opcode == 0xE4:
            r = ZeroPageRead(ag, al_CPX, 'regs->X')
        elif opcode == 0xE5:
            r = ZeroPageRead(ag, al_SBC, 'regs->A')
        elif opcode == 0xE6:
            r = ZeroPageModify(ag, al_INC)
        elif opcode == 0xE7:
            r = SetMemoryBit(ag, '6')
        elif opcode == 0xE8:
            r = Implied(ag, al_INC, 'regs->X')
        elif opcode == 0xE9:
            r = Immediate(ag, al_SBC, 'regs->A')
        elif opcode == 0xEA:
            r = NOP(ag)
        elif opcode == 0xEB:
            r = NOP(ag)
        elif opcode == 0xEC:
            r = AbsoluteRead(ag, al_CPX, 'regs->X')
        elif opcode == 0xED:
            r = AbsoluteRead(ag, al_SBC, 'regs->A')
        elif opcode == 0xEE:
            r = AbsoluteModify(ag, al_INC)
        elif opcode == 0xEF:
            r = BranchIfBitSet(ag, '6')
        elif opcode == 0xF0:
            r = Branch(ag, 'regs->P.Z')
        elif opcode == 0xF1:
            r = IndirectYRead(ag, al_SBC, 'regs->A')
        elif opcode == 0xF2:
            r = IndirectRead(ag, al_SBC, 'regs->A')
        elif opcode == 0xF3:
            r = BlockMove(ag, al_TAI)
        elif opcode == 0xF4:
            r = Set(ag, 'regs->P.T')
        elif opcode == 0xF5:
            r = ZeroPageRead(ag, al_SBC, 'regs->A', 'regs->X')
        elif opcode == 0xF6:
            r = ZeroPageModify(ag, al_INC, 'regs->X')
        elif opcode == 0xF7:
            r = SetMemoryBit(ag, '7')
        elif opcode == 0xF8:
            r = Set(ag, 'regs->P.D')
        elif opcode == 0xF9:
            r = AbsoluteRead(ag, al_SBC, 'regs->A', 'regs->Y')
        elif opcode == 0xFA:
            r = Pull(ag, 'regs->X')
        elif opcode == 0xFB:
            r = NOP(ag)
        elif opcode == 0xFC:
            r = NOP(ag)
        elif opcode == 0xFD:
            r = AbsoluteRead(ag, al_SBC, 'regs->A', 'regs->X')
        elif opcode == 0xFE:
            r = AbsoluteModify(ag, al_INC, 'regs->X')
        elif opcode == 0xFF:
            r = BranchIfBitSet(ag, '7')
        elif opcode == 0x100:  # RESET
            r = S_RESET(ag)
        elif opcode == 0x101:  # IRQ2
            r = S_IRQ(ag, '0xFFF6')
        elif opcode == 0x102:  # IRQ
            r = S_IRQ(ag, '0xFFF8')
        elif opcode == 0x103: # TIQ
            r = S_IRQ(ag, '0xFFFA')

    if len(r) > 0:
        mystr = '// ' + am + '\n' + C_func_sig(opcode, tbit) + '\n{\n' + r + '\n\n'
        outfile.write(mystr)


def main():
    outinsfile = os.path.join(FPATH, 'huc6280_opcodes.c')
    if os.path.isfile(outinsfile):
        os.unlink(outinsfile)
    with open(outinsfile, 'w') as outfile:
        outfile.write('#include <printf.h>\n')
        outfile.write('#include <assert.h>\n')
        outfile.write('#include "helpers/int.h"\n')
        outfile.write('#include "huc6280_opcodes.h"\n')
        outfile.write('#include "huc6280.h"\n')
        outfile.write('\n')
        outfile.write('// This file auto-generated byhuc6280_gen.py in JSMooCh\n')

        outfile.write('\n')
        #outfile.write('static voidHUC6280_ins_NONE(struct HUC6280_regs *regs, struct HUC6280_pins *pins)\n')
        #outfile.write('{\n')
        #outfile.write('    assert(1==0);\n')
        #outfile.write('}\n')

        for tbit in range(0, 2):
            for i in range(0, 0x100 + 4):
                write_instruction(outfile, i, tbit)

        outfile.write('\n\n')

        write_instruction_table(outfile)


if __name__ == '__main__':
    main()
