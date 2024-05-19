#!/usr/bin/python3

import os
from typing import Dict, Optional


TEMPLATES_DONE = set()

class ins_var:
    def __init__(self, letter) -> None:
        self.letter = letter
        self.looped = False
        self.loop_min = 0
        self.loop_max = 8

        self.shift_amount = -1

        self.num_digits = 0

        self.hi_bit = -1
        self.lo_bit = 500
        self.storage = 'u32'

    def add_bit(self, num: int) -> None:
        self.num_digits += 1
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
        self.line: int = 0
        self.indent1: str = '    '
        self.indent2: str = '        '
        self.indent: str = self.indent1
        self.num_fors = 0
        self.outshifts = None

        self.op1 = None
        self.op2 = None
        self.op3 = None

        self.skip_if = None
        self.skip1 = None
        self.SKIPAFART = False
        self.skipS0 = None
        self.skipS1 = None

    def readline(self, line) -> bool:
        ln = line.strip()
        if '#' in ln:
            ln = ln.split('#')[0]

        if len(ln) < 1: return True

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

        if ln[:7] == 'skip_if':
            self.skip_if = ln[8:]
            self.line += 1
            return True
        if ln[:5] == 'skip1':
            self.skip1 = ln[6:]
            self.line += 1
            return True

        if ln[:8] == 'skip_S=0':
            self.skipS0 = ln[12:]
            self.line += 1
            return True

        if ln[:8] == 'skip_S=1':
            self.skipS1 = ln[12:]
            self.line += 1
            return True

        if ln == 'SKIPAFART':
            self.SKIPAFART = True
            self.line += 1
            return True

        self.line += 1
        print('UNRECOGNIZED LINE: ', ln)
        return False

    # X template. X=0 data_register_direct
    # X=1 address_register_indirect_with_predecrement

    # dr_sea_S = EA=address_register_indirect_with_displacement

    def parse_bitcode(self):
        self.bitcode = ''.join(self.bitcode.split(' '))
        for i in range(0, 16):
            try:
                c = self.bitcode[i]
            except:
                print('WHAT!?!?!', self.bitcode)
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
        #print(self.out_code())
        return

    def make_W_string(self, val) -> str:
        o = self.opcode_str_01_only
        if 'w' in self.vars:
            w: ins_var = self.vars['w']
            sa = 15 - w.shift_amount
            nd = w.num_digits
            if nd == 1:
                o_l = o[:sa]
                o_r = o[sa:]
            else:
                o_l = o[:sa-1]
                o_r = o[sa+1:]
            if val == 0: r = '0'
            elif val == 1: r = '1'
            elif val == 2: r = '10'
            elif val == 3: r = '11'
            else: raise Exception('HUH?')
            if (len(r) == 1) and (nd == 2): r = '0' + r
            if (len(r) == 2) and (nd == 1): raise Exception('WRONG DIGITS!?', r, self.vars, self.name)
            o = o_l + r + o_r


        return o[0:4] + ' ' + o[4:8] + ' ' + o[8:12] + ' ' + o[12:16]

    def bind_str(self, sz, val) -> str:
        o = ''
        if self.skip1 is not None:
            r = [s.strip() for s in self.skip1.split('if')]
            skip = r[0]
            when = r[1]
            skip = '(' + skip.replace('=', '!=') + ')'
            when = when.replace('(','').replace(')','')
            if when[:4] == 'sz==':
                eq = int(when[4:])
                if sz == eq:
                    o += 'if ' + skip + ' '
            else:
                raise Exception('Unsupported skip1', self.name)
            # if(mode == 1) unbind(opcode | 0 << 6);
            # m==1 (sz==1)


        o += 'bind_opcode("'
        o += self.make_W_string(val) + '", ' + str(sz) + ', &M68K_ins_' + self.name + ', &M68K_disasm_BAD, ' + self.outshifts + ', '
        if self.op1 is not None:
            o += '&op1, '
        else:
            o += 'NULL, '

        if self.op2 is not None:
            o += '&op2'
        else:
            o += 'NULL'
        o += ')'
        return o

    def make_outshifts(self) -> None:
        mvars = {}
        for nm, deets in self.vars.items():
            if nm in {'w', 'S', 'X'}: continue
            if deets.shift_amount > 0:
                mvars[deets.shift_amount] = '(' + nm + ' << ' + str(deets.shift_amount) + ')'
            else:
                mvars[deets.shift_amount] = nm
        if len(mvars) < 1:
            self.outshifts = '0'
            return

        mvars = [s[1] for s in sorted([(k,v) for k,v in mvars.items()], key=lambda x: -x[0])]

        if len(mvars) == 1:
            self.outshifts = mvars[0]
            return

        self.outshifts = ''

        for i in range(0, len(mvars)):
            if i != 0:
                self.outshifts += ' | '
            self.outshifts += mvars[i]

    def open_fors(self) -> str:
        out = ''
        self.num_fors = 0
        for nm, deets in self.vars.items():
            if nm in {'w', 'S', 'X'}: continue
            deets: ins_var
            out += '\n' + self.indent + 'for (' + deets.get_loop_inner() + ') {'
            self.num_fors += 1
            self.indent += '    '
        return out

    def close_fors(self) -> str:
        out = ''
        if self.num_fors > 0:
            for i in range(0, self.num_fors):
                self.indent = self.indent[4:]
                out += '\n' + self.indent + '}'

        return out

    def out_code(self) -> str:
        out = ''
        self.make_outshifts()

        if self.template is None:
            if len(self.data_sizes) > 1:
                raise Exception("HUH?")
            else:
                out += '\n' + self.indent + self.bind_str(self.data_sizes[0], 0) + ';'
        else:
            out += self.do_template()
            out += self.close_fors()

        return out

    def do_template(self) -> str:
        o = None
        if self.template == 'ea1':
            o = self.template_output_1op('m, r')
        elif self.template == 'ea_ar':
            o = self.template_output_2op('m, r', 'M68k_AM_address_register_direct, a')
        elif self.template == 'ea_dr':
            o = self.template_output_2op('m, r', 'M68k_AM_data_register_direct, d')
        elif self.template == 'dr':
            o = self.template_output_1op('M68k_AM_data_register_direct, d')
        elif self.template == 'dr_ea':
            o = self.template_output_2op('M68k_AM_data_register_direct, d', 'm, r')
        elif self.template == 'dr_dr':
            o = self.template_output_2op('M68k_AM_data_register_direct, s', 'M68k_AM_data_register_direct, d')
        elif self.template == 'ar':
            o = self.template_output_1op('M68k_AM_address_register_direct, a')
        elif self.template == 'ar_ar': # using s and d where s is operand 1 and d is operand 2
            o = self.template_output_2op('M68k_AM_address_register_direct, s', 'M68k_AM_address_register_direct, d')
        elif self.template == 'ar_dr': # using s and d where s is operand 1 and d is operand 2
            o = self.template_output_2op('M68k_AM_address_register_direct, s', 'M68k_AM_data_register_direct, d')
        elif self.template == '_A':
            o = self.template_output_2op('m, r', 'M68k_AM_address_register_direct, a')
        elif self.template == '_M':
            o = self.template_output_2op('M68k_AM_address_register_indirect_with_postincrement, y', 'M68k_AM_address_register_indirect_with_postincrement, x')
        elif self.template == 'imm256_dr':  # imm, data
            o = self.template_output_2op('M68k_AM_immediate, i', 'M68k_AM_data_register_direct, d')
        elif self.template == 'imm_dr':
            o = self.template_output_2op('M68k_AM_immediate, i ? i : 8', 'M68k_AM_data_register_direct, d')
        elif self.template == 'displacement':
            o = self.template_output_1op('M68k_AM_immediate, d')
        elif self.template == 'ADDQ':
            o = self.template_ADDQ()
        elif self.template == 'B1':
            o = self.template_B1()
        elif self.template == 'B2':
            o = self.template_B2()
        elif self.template == 'test_displace_jump':
            o = self.template_output_2op('M68k_AM_immediate, t', 'M68k_AM_immediate, d')
        elif self.template == 'test_ea':
            o = self.template_output_2op('M68k_AM_immediate, t', 'm, r')
        elif self.template == 'DBCC':
            o = self.template_output_2op('M68k_AM_immediate, c', 'M68k_AM_data_register_direct, d')
        elif self.template == 'X':
            o = self.template_X()
        elif self.template == 'dr_sea_S':
            o = self.template_dr_sea_S()
        elif self.template == 'MOVE':
            o = self.template_MOVE()
        elif self.template == 'dreg_to_ea_S':
            o = self.template_dreg_to_ea_S()

        if o is not None:
            TEMPLATES_DONE.add(self.template)
        else:
            o = 'BAD TEMPLATE'

        return o

    def mk_op(self, n, v):
        #         out += ls + 'op2 = mk_ea(M68k_AM_data_register_direct, d);'
        if n == 1:
            self.op1 = 1
        else:
            self.op2 = 1
        return 'op' + str(n) + ' = mk_ea(' + v + ');'

    def template_output_1op(self, op1str) -> str:
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        out += ls + self.mk_op(1, op1str)
        out += self.bind_data_sizes(ls)
        return out

    def template_output_2op(self, op1str, op2str) -> str:
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        out += ls + self.mk_op(1, op1str)
        out += ls + self.mk_op(2, op2str)
        out += self.bind_data_sizes(ls)
        return out

    def template_B2(self) -> str:
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        out += ls + self.mk_op(1, 'M68k_AM_data_register_direct, d')
        out += ls + self.mk_op(2, 'm, r')
        out += ls + 'if (m == 0) ' + self.bind_str(4, 0) + ';'
        out += ls + 'else        ' + self.bind_str(1, 0) + ';'
        return out

    def template_B1(self) -> str:
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        out += ls + self.mk_op(1, 'm, r')
        out += ls + 'if (m == 0) ' + self.bind_str(4, 0) + ';'
        out += ls + 'else        ' + self.bind_str(1, 0) + ';'
        return out

    def template_MOVE(self) -> str:
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        # 00ww aaab bbcc cddd
        out += ls + self.mk_op(1, 'c, d')
        out += ls + self.mk_op(2, 'b, a')
        out += ls + self.bind_str(1, 1) + ';'
        out += ls + self.bind_str(2, 3) + ';'
        out += ls + self.bind_str(4, 2) + ';'
        return out

    def template_dr_sea_S(self) -> str:
        # # where dreg is the first argument, and set EA is second. S=0 EA is first argument S=1 EA is second argument
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        # S=0, EA is first. S=1, EA is second. other is dreg
        out += '\n' + ls + self.mk_op(1, 'M68k_AM_address_register_indirect_with_displacement, r')
        out += ls + self.mk_op(2, 'M68k_AM_data_register_direct, d')
        out += self.bind_data_sizes(ls)

        out += '\n' + ls + self.mk_op(1, 'M68k_AM_data_register_direct, d')
        out += ls + self.mk_op(2, 'M68k_AM_address_register_indirect_with_displacement, r')
        if len(self.outshifts) == 1:
            self.outshifts = '0x80'
        else:
            self.outshifts += ' | 0x80'
        out += self.bind_data_sizes(ls)
        return out

    def template_dreg_to_ea_S(self) -> str:
        # .ADD 1,2,4 template(dreg_to_ea_S)  # dreg_to_ea_S means S=0 first operand EA, S=1 second operand EA
        # 1101 dddS wwmm mrrr
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)
        out += '\n' + ls + 'if (!(' + self.skipS0 + ')) {'

        out += ls + '    ' + self.mk_op(1, 'm, r')
        out += ls + '    ' + self.mk_op(2, 'M68k_AM_data_register_direct, d')
        pstr = ''
        if self.SKIPAFART:
            pstr = 'if (m != 1) '
        out += ls + '    ' + pstr + self.bind_str(1, 0) + ';'
        out += ls + '    ' + self.bind_str(2, 1) + ';'
        out += ls + '    ' + self.bind_str(4, 2) + ';'
        out += ls + '}'
        out += ls + 'if (!(' + self.skipS1 + ')) {'
        out += ls + '    ' + self.mk_op(1, 'M68k_AM_data_register_direct, d')
        out += ls + '    ' + self.mk_op(2, 'm, r')
        if len(self.outshifts) == 1:
            self.outshifts = '0x100'
        else:
            self.outshifts += ' | 0x100'
        out += ls + '    ' + self.bind_str(1, 0) + ';'
        out += ls + '    ' + self.bind_str(2, 1) + ';'
        out += ls + '    ' + self.bind_str(4, 2) + ';'
        out += ls + '}'

        return out


    def template_X(self) -> str:
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)

        out += '\n' + ls + self.mk_op(1, 'M68k_AM_data_register_direct, y')
        out += ls + self.mk_op(2, 'M68k_AM_data_register_direct, x')
        out += self.bind_data_sizes(ls)

        out += '\n' + ls + self.mk_op(1, 'M68k_AM_address_register_indirect_with_predecrement, y')
        out += ls + self.mk_op(2, 'M68k_AM_address_register_indirect_with_predecrement, x')
        if len(self.outshifts) == 1:
            self.outshifts = '8'
        else:
            self.outshifts += ' | 8'
        out += self.bind_data_sizes(ls)
        return out

    def template_ADDQ(self) -> str:
        self.skip_if = '((m == 7) && (r >= 2))'
        out = self.open_fors()
        ls = '\n' + self.indent
        out += self.loop_skip_if(ls)

        self.op2 = 1

        out += ls + self.mk_op(1, 'M68k_AM_immediate, d ? d : 8')
        out += ls + 'if (m == 1) {'
        out += ls + '    op2 = mk_ea(M68k_AM_address_register_direct, r);'
        out += ls + '    ' + self.bind_str(2, 1) + ';'
        out += ls + '    ' + self.bind_str(4, 2) + ';'
        out += ls + '}'
        out += ls + 'else {'
        out += ls + '    op2 = mk_ea(m, r);'
        out += ls + '    ' + self.bind_str(1, 0) + ';'
        out += ls + '    ' + self.bind_str(2, 1) + ';'
        out += ls + '    ' + self.bind_str(4, 2) + ';'
        out += ls + '}'
        return out

    def loop_skip_if(self, ls) -> str:
        if self.skip_if is None: return ''
        return ls + 'if ' + self.skip_if + ' continue;'

    def bind_data_sizes(self, ls) -> str:
        out = ''
        for i in range(0, len(self.data_sizes)):
            out += ls + self.bind_str(self.data_sizes[i], i) + ';'
        return out


JSMOOCH_LIB_PATH = os.path.expanduser('~') + '/dev/jsmooch-emus/jsmooch-lib/src'
M68K_PATH = JSMOOCH_LIB_PATH + '/component/cpu/m68000/generated'


def main():
    lines = []
    code_blocks = []
    code_block: Optional[instruction_block] = None

    ins_names = set()
    template_names = set()

    with open('instructions.txt', 'r') as infile:
        lines = infile.readlines()
    for line in lines:
        if line[0] == '.':
            if code_block is not None:
                code_block.finalize()
                ins_names.add(code_block.name)
                if code_block.template is not None:
                    template_names.add(code_block.template)
                code_blocks.append(code_block)
            code_block = instruction_block()
        if code_block is not None:
            a = code_block.readline(line)
            if not a:
                return

    mfo = M68K_PATH + '/generated_decodes.c'
    if os.path.exists(mfo):
        os.unlink(mfo)
    lines = []
    for code_block in code_blocks:
        rls = code_block.out_code().split('\n')
        for r in rls:
            lines.append(r + '\n')

    with open(mfo, 'w') as ofile:
        ofile.writelines(lines)

    for ins_name in sorted(list(ins_names)):
        #&M68K_ins_' + self.name + ', &M68K_disasm_' + self.name + ', ' + self.outshifts + ', '
        print('M68KINS(' + ins_name + ')')
        print('        STEP0')
        print('        BADINS;')
        print('INS_END')
        print('')

if __name__ == '__main__':
    main()