#!/usr/bin/python3

import os
import glob
import json

FPATH = os.path.join(os.path.expanduser('~'), 'dev/external/ProcessorTests/680x0/map/68000.official.json')


class ins_t:
    def __init__(self, ins_name, full_name):
        self.opcodes = set()
        self.ins_name = ins_name
        self.full_name = full_name


def main():
    instructions = {}
    with open(FPATH) as infile:
        js = json.load(infile)

    opcode_to_disasm = dict()

    for opcode, disasm in js.items():
        suffix = None
        r = disasm.split(' ')
        iname = r[0]
        if iname == 'None':
            continue

        if iname not in instructions:
            instructions[iname] = ins_t(iname, disasm)
        instructions[iname].opcodes.add(int(opcode, 16))
        opcode_to_disasm[int(opcode, 16)] = disasm

    o = '\n#include "helpers/int.h"'
    o += '\n'
    o += '\n#ifndef JSMOOCH_LIB_M68K_GENTEST_ITEM_H'
    o += '\n#define JSMOOCH_LIB_M68K_GENTEST_ITEM_H'
    o += '\n'
    o += '\nstruct m68k_gentest_item {'
    o += '\n    char name[50];'
    o += '\n    char full_name[50];'
    o += '\n    u32 num_opcodes;'
    o += '\n    u32 *opcodes;'
    o += '\n};'
    o += '\n'
    o += '\n#define M68K_NUM_GENTEST_ITEMS ' + str(len(instructions))
    o += '\nextern struct m68k_gentest_item m68k_gentests[M68K_NUM_GENTEST_ITEMS];'
    o += '\n'
    o += '\n#endif'
    o += '\n'

    OUTPATH = os.path.join(os.path.expanduser('~'), 'dev/mame-m68k-test-gen/src/mame/testcpu/')
    mfil = os.path.join(OUTPATH, 'test_out.h')
    if os.path.exists(mfil):
        os.unlink(mfil)
    with open(mfil, 'w') as outfile:
        outfile.write(o)

    o = '#include "' + mfil + '"'
    o += '\n'
    o += '\n'
    o += '\nstruct m68k_gentest_item m68k_gentests[M68K_NUM_GENTEST_ITEMS] = {'

    for iname, ins in instructions.items():
        ins: ins_t
        iname: str

        o += '\n    (struct m68k_gentest_item) { .name="' + ins.ins_name + '", .num_opcodes = ' + str(
            len(ins.opcodes)) + ', .opcodes = (u32 []) { '
        for opcode in ins.opcodes:
            o += str(opcode) + ','
        o += '} },'
    o += '\n};\n'
    o += 'char m68k_gentest_disasm[65536][50] = {\n'
    per_line = 4
    on_line = 0
    for i in range(0, 65536):
        if on_line == 0:
            o += '\n    '
        if i in opcode_to_disasm:
            if len(opcode_to_disasm[i]) > 45:
                print('WOW LONG ONE!')
            o += '"' + opcode_to_disasm[i] + '",'
        else:
            o += '"BAD",'
        if on_line != (per_line-1):
            o += ' '
        on_line += 1
        if on_line == per_line: on_line = 0
    o += '\n};\n'

    mfil = os.path.join(OUTPATH, 'test_out.c')
    if os.path.exists(mfil):
        os.unlink(mfil)
    with open(mfil, 'w') as outfile:
        outfile.write(o)


if __name__ == '__main__':
    main()
