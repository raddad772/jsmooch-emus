#!/usr/bin/python3
from typing import Optional
import copy
import os

JSMOOCH_LIB_PATH = os.path.expanduser('~') + '/dev/jsmooch-emus/jsmooch-lib/src'

DC_PATH = JSMOOCH_LIB_PATH + '/system/dreamcast/generated'
SH4_PATH = JSMOOCH_LIB_PATH + '/component/cpu/sh4/generated'


class OutOptions:
    def __init__(self):
        self.decl_indent = 2
        self.switch_indent = 2
        self.init_indent = 1


def di(num: int) -> str:
    return '    ' * num


class BFIK_t:
    def __init__(self):
        self.AND = 0
        self.OR = 1


BFIK = BFIK_t()


class BitField:
    def __init__(self, name, start, end, default_value=0, kind=BFIK.AND):
        self.start: int = start
        self.end: int = end
        if start > end:
            raise Exception("Start must come before end " + name)
        if start < 0:
            raise Exception("Start must be 0 or more")
        if start > 31:
            raise Exception("Start must be 0...31")
        if end > 31:
            raise Exception("End must be 0...31")
        self.name: str = name
        self.default_value: int = default_value
        self.kind: int = kind

    @property
    def mask(self) -> int:
        outv = 0
        if self.kind == BFIK.AND:
            for i in range(self.start, self.end + 1):
                outv = (outv << 1) | 1
            outv = outv << self.start
        else:
            outv = self.default_value << self.start
        return outv


class Register:
    def __init__(self):
        self.name = ''
        self.has_bitflags = False
        self.has_and_bits = False
        self.has_or_bits = False
        self.access_datatypes = []  # u32, float, flags
        self.access_bits = []  # 8, 16, 32, 64
        self.read = False
        self.write = False
        self.write_mask = None
        self.w_cond = None
        self.r_cond = None
        self.w_override = None
        self.r_override = None
        self.w_exclude = False
        self.r_exclude = False
        self.default_value = None
        self.quickboot_value = None
        self.and_bitfields = []
        self.or_bitfields = []
        self.address = -1
        self.on_write = None
        self.data_sz = 'u32'

    @property
    def and_bitfield(self) -> Optional[int]:
        if not self.has_and_bits:
            return 0xFFFFFFFF
        o = 0
        bf: BitField
        for bf in self.and_bitfields:
            o = o | bf.mask
        return o

    @property
    def or_bitfield(self) -> Optional[int]:
        if not self.has_or_bits:
            return 0
        o = 0
        bf: BitField
        for bf in self.or_bitfields:
            o = o | bf.mask
        return o

    @property
    def hex_address(self) -> str:
        return '0x{:08X}'.format(self.address)

    def init(self, oo: OutOptions) -> str:
        o = ''
        if self.default_value is None:
            return o

        indent1 = di(oo.init_indent)

        return o

    def decl(self, oo: OutOptions) -> str:
        o = ''
        indent1 = di(oo.decl_indent)
        indent2 = di(oo.decl_indent + 1)
        indent3 = di(oo.decl_indent + 2)
        if not self.has_and_bits:
            return '\n' + indent1 + self.data_sz + ' ' + self.name + ';  // ' + self.hex_address
        o = '\n' + indent1 + 'union {  // ' + self.name + '\n'
        o += indent2 + 'struct {\n'
        my_bitfields = sorted(self.and_bitfields, key=lambda x: x.start, reverse=False)
        bf: BitField
        last_end = -1
        for bf in my_bitfields:
            if (bf.start - last_end) > 1:
                insert_bits = (bf.start - last_end) - 1
                o += indent3 + self.data_sz + ' : ' + str(insert_bits) + ';\n'
            o += indent3 + self.data_sz + ' ' + bf.name + ' : ' + str((bf.end - bf.start) + 1) + ';\n'
            last_end = bf.end
        o += indent2 + '};\n'
        o += indent2 + self.data_sz + ' u;\n'
        o += indent1 + '} ' + self.name + ';  // ' + self.hex_address

        return o

    def switch_write(self, oo: OutOptions, section: str, pref: str, in_var: str) -> str:
        if (not self.write) or self.w_exclude:
            return ''
        indent1 = di(oo.switch_indent)
        indent2 = di(oo.switch_indent + 1)
        o = ''
        o += '\n' + indent1 + 'case ' + self.hex_address + ': '
        o += '{ '

        if self.w_override is not None:
            o += self.w_override
            if o[-1:] != ';':
                o += ';'
            o += ' '
        else:
            if self.w_cond is not None:
                o += 'if (' + self.w_cond + ') { '
            o += pref + section + '.' + self.name
            if self.has_and_bits:
                o += '.u'
            o += ' = '

            if self.has_or_bits:
                o += '('
            if self.write_mask is not None:
                o += '('

            if self.has_and_bits:
                o += in_var + ' & 0x{:08X}'.format(self.and_bitfield)
            elif self.write_mask is not None:
                o += in_var + ' & ' + self.write_mask + ')'
            else:
                o += in_var

            if self.has_or_bits:
                o += ') | 0x{:08X}'.format(self.or_bitfield)
            o += '; '

            if self.on_write is not None:
                o += self.on_write
                if o[-1:] != ';':
                    o += ';'
                o += ' '

            if self.w_cond is not None:
                o += '} '

        o += 'return; }'
        return o

    def switch_read(self, oo: OutOptions, section: str, pref: str):
        if (not self.read) or self.r_exclude:
            return ''
        indent1 = di(oo.switch_indent)
        o = ''
        o += '\n' + indent1 + 'case ' + self.hex_address + ': '

        if self.r_override is not None:
            o += '{ ' + self.r_override
            if o[-1:] != ';':
                o += ';'
            o += ' }'
        else:
            o += ' { return ' + pref + section + '.' + self.name
            if self.has_and_bits:
                o += '.u'

            if self.has_or_bits:
                o += ' | 0x{:08X}'.format(self.or_bitfield)

            o += '; }'
        return o


def parse_regs_file(fname: str, out_path: str) -> None:
    fl = []
    with open(fname, 'r') as infil:
        fl = infil.readlines()
    fl = [s.strip() for s in fl]

    sections = {}
    section = ''
    lindex = 0
    cur_section = []
    while (True):
        if lindex >= len(fl):
            break
        cur_line = fl[lindex]
        lindex += 1

        if len(cur_line) == 0:
            continue

        if cur_line[:1] == '.':
            section = cur_line[1:]
            if section not in sections:
                sections[section] = []
            cur_section = sections[section]
            continue

        if cur_line[:2] == '0x':
            o = Register()
            cur_section.append(o)
            try:
                addr, name = cur_line.split(':')
            except:
                print(cur_line)
                raise
            o.name = name.strip()
            if len(addr) != 10:
                raise Exception("Must specify all 8 digits of register")
            addr = int(addr, 0)
            o.address = addr

            cur_line = fl[lindex]
            lindex += 1
            o.access_datatypes = [fc.strip() for fc in cur_line.split(",")]
            if 'u16' in o.access_datatypes:
                o.data_sz = 'u16'
            elif 'u8' in o.access_datatypes:
                o.data_sz = 'u8'
            o.has_bitflags = 'flags' in o.access_datatypes

            cur_line = fl[lindex]
            lindex += 1
            o.access_bits = []
            accesses = [fc.strip() for fc in cur_line.split(",")]
            for ac in accesses:
                if ac[:7] == 'access_':
                    o.access_bits.append(int(ac[7:]))
                elif ac == 'r':
                    o.read = True
                elif ac == 'w':
                    o.write = True
                elif ac == 'rw':
                    o.read = o.write = True
                else:
                    raise Exception("Unknown meaning of " + ac + " (" + o.name + ")")

            cur_line = fl[lindex]
            lindex += 1
            while '=' in cur_line:
                # Either default= or boot=
                if cur_line[:7] == "w_cond=":
                    o.w_cond = cur_line[7:].strip()
                elif cur_line[:11] == 'w_override=':
                    o.w_override = cur_line[11:].strip()
                elif cur_line[:11] == 'r_override=':
                    o.r_override = cur_line[11:].strip()
                elif cur_line[:8] == 'exclude=':
                    o.w_exclude = 'w' in cur_line[8:]
                    o.r_exclude = 'r' in cur_line[8:]
                elif cur_line[:9] == 'on_write=':
                    o.on_write = cur_line[9:]
                else:
                    try:
                        kind, val = cur_line.split("=")
                    except:
                        print("EXCEPT ON " + cur_line + ' (' + o.name + ')')
                        raise
                    if kind == 'default':
                        o.default_value = int(val, 0)
                    elif kind == 'boot':
                        o.quickboot_value = int(val, 0)
                    elif kind == 'write_mask':
                        val = val.strip()
                        if val[:4] == 'bits':
                            startend = val[4:].strip()
                            if '-' in startend:
                                start, end = startend.split('-')
                            else:
                                start = end = startend
                            start = int(start, 10)
                            end = int(end, 10)
                            bf = BitField('', start, end)
                            o.write_mask = '0x{:08X}'.format(bf.mask)
                        else:
                            o.write_mask = val.strip()
                    else:
                        raise Exception("Unknown kind! " + kind)

                if lindex >= len(fl): break
                cur_line = fl[lindex]
                lindex += 1
                if lindex >= len(fl): break

            if lindex >= len(fl): break

            if not o.has_bitflags:
                lindex -= 1
                continue

            while len(cur_line) > 0:
                cls = cur_line.split(':')

                name = cls[0].strip()
                try:
                    startend = cls[1].strip()
                except:
                    print("WHAT?", cls)
                    print("WHAT2?", o.name)
                    raise
                if '-' in startend:
                    start, end = startend.split('-')
                else:
                    start = end = startend
                try:
                    start = int(start, 10)
                except ValueError:
                    print("REG: " + o.name + " LINE: " + cur_line)
                    raise
                end = int(end, 10)

                if len(name) > 0 and name[0] != '_':
                    o.and_bitfields.append(BitField(name, start, end))
                    o.has_and_bits = True

                if len(cls) == 3:  # OR bits present...
                    o.or_bitfields.append(BitField(name, start, end, default_value=int(cls[2], 0), kind=BFIK.OR))
                    o.has_or_bits = True

                cur_line = fl[lindex]
                lindex += 1
                if lindex >= len(fl):
                    break
    oo = OutOptions()

    decl = ''
    switch_write = ''
    switch_read = ''
    for section in sections:
        se = sections[section]
        sections[section] = sorted(se, key=lambda x: x.address, reverse=False)

    out_decls = {}
    out_reads = {}
    out_writes = {}

    for sstr in sections:
        section, fname = sstr.split(',')
        fname = fname.strip().split('=')[1]

        if section not in out_decls:
            out_decls[section] = ''
        if fname not in out_reads:
            out_reads[fname] = ''
        if fname not in out_writes:
            out_writes[fname] = ''
        for register in sections[sstr]:
            out_decls[section] += register.decl(oo)
            out_writes[fname] += register.switch_write(oo, section, 'this->', 'val')
            out_reads[fname] += register.switch_read(oo, section, 'this->')

    for section in out_decls:
        with open(out_path + '/' + section + '_decls.h', 'w') as outfile:
            outfile.write(out_decls[section])

    for fname in out_reads:
        with open(out_path + '/' + fname + '_reads.c', 'w') as outfile:
            outfile.write(out_reads[fname])

    for fname in out_writes:
        with open(out_path + '/' + fname + '_writes.c', 'w') as outfile:
            outfile.write(out_writes[fname])


def main():
    parse_regs_file('dreamcast_regs.txt', DC_PATH)
    parse_regs_file('sh4_regs.txt', SH4_PATH)


if __name__ == '__main__':
    main()
