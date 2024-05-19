#!/usr/bin/python3

import os
from typing import Dict, Optional


class ins_var:
    def __init__(self, letter) -> None:
        self.letter = letter
        self.looped = False
        self.loop_min = 0
        self.loop_max = 8

        self.shift_amount = -1

        self.hi_bit = -1
        self.lo_bit = 500
        self.storage = 'u32'

    def add_bit(self, num: int) -> None:
        if num > self.hi_bit:
            self.hi_bit = num

        if num < self.lo_bit:
            self.lo_bit = num

    def finish(self) -> None:
        if self.hi_bit == -1:
            raise ValueError("No bits found for variable")
        self.shift_amount = self.lo_bit
        self.loop_min = 0
        self.loop_max = 1 << ((self.hi_bit - self.lo_bit) + 1)

    def get_loop_inner(self) -> str:
        return self.storage + ' ' + self.letter + ' = ' + str(self.loop_min) + '; ' + self.letter + ' < ' + str(self.loop_max) + '; ' + self.letter + '++'

class instruction_block:
    def __init__(self):
        self.name = ''
        self.data_sizes = []
        self.opcode_str_01_only = ''
        self.vars: Dict[str, ins_var] = {}
        self.template: Optional[str] = None
        self.bitcode: Optional[str] = None
        self.opcode_or: str = '0'
        self.line: int = 0
        self.indent1: str = '    '
        self.indent2: str = '        '
        self.indent: str = self.indent1

        self.op1 = None
        self.op2 = None
        self.op3 = None

        self.skip_if = None

    def readline(self, line) -> bool:
        ln = line.strip()
        if '#' in ln:
            ln = ln.split('#')[0]

        if len(ln) < 1: return

        if ln[0] == '.':
            ln = ln[1:]
            things = ln.split(' ')
            self.name = things[0]
            try:
                self.data_sizes = [int(s) for s in things[1].split(',')]
            except:
                print('WHAT?', ln)
            print(things)
            if 'template' in things[2]:
                self.template = things[2].split('(')[1].split(')')[0]
            else: # just a bitcode
                self.bitcode = ''.join(things[2:])
                self.parse_bitcode()
            self.line += 1
            return True

        if self.line == 1 and not self.bitcode:
            self.bitcode = ln
            self.parse_bitcode()
            self.line += 1
            return True

        self.line += 1
        return True

    def parse_bitcode(self):
        self.bitcode = ''.join(self.bitcode.split(' '))
        for i in range(0, 16):
            try:
                c = self.bitcode[i]
            except:
                print('WHAT!?!?!')
            if c == '0':
                self.opcode_str_01_only += '0'
                continue
            elif c == '1':
                self.opcode_str_01_only += '1'
                continue
            self.opcode_str_01_only += '.'
            if c not in self.vars:
                self.vars[c] = ins_var(c)
            self.vars[c].add_bit(15 - i)
        for bvar in self.vars.values():
            bvar.finish()

    def finalize(self):
        return

    def bind_str(self, sz) -> str:
        # static void bind_opcode(const char* inpt, u32 sz, M68k_ins_func exec_func, M68k_disassemble_t disasm_func, u32 op_or, struct M68k_EA *ea1, struct M68k_EA *ea2)
        # bind_opcode(opcstr, opcode_or, &M68K_ins_##func, &M68K_disasm_BAD, 0, NULL, NULL, NULL)
        o = 'bind_opcode("' + self.opcode_str_01_only + '", ' + str(sz) + ', &M68K_ins_' + self.name + ', + &M68K_disasm_BAD, ' + self.opcode_or + ', '
        if self.op1 is not None:
            o += '&op1, '
        else:
            o += 'NULL, '

        if self.op2 is not None:
            o += '&op2'
        else:
            o += 'NULL'

        return o

    def out_code(self) -> str:
        out = ''
        if self.template is None:
            out += self.indent + self.bind_str()
        for r in self.
        out += self.indent + 'for (' +


JSMOOCH_LIB_PATH = os.path.expanduser('~') + '/dev/jsmooch-emus/jsmooch-lib/src'
M68K_PATH = JSMOOCH_LIB_PATH + '/component/cpu/m68k/generated'


def main():
    lines = []
    code_blocks = []
    code_block: Optional[instruction_block] = None

    with open('instructions.txt', 'r') as infile:
        lines = infile.readlines()
    for line in lines:
        if line[0] == '.':
            if code_block is not None:
                code_block.finalize()
                code_blocks.append(code_block)
            code_block = instruction_block()
        if code_block is not None:
            code_block.readline(line)

if __name__ == '__main__':
    main()