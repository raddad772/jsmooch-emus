#!/usr/bin/python3

import os
from typing import Optional, Tuple


class file_lines:
    def __init__(self, fpath: str, is_ares):
        with open(fpath) as infile:
            self.lines = infile.readlines()
        self.index = 0
        self.is_ares = is_ares

    def get_line(self) -> str:
        ln = ''
        while len(ln) < 1:
            if self.index >= len(self.lines):
                raise Exception('END at line ' + str(self.index) + '!')
            ln = self.lines[self.index].strip()
            self.index += 1
            if self.is_ares:
                if 'OpenGL: Failed to load librashader: shaders will be disabled' in ln:
                    ln = ''
                    continue
                if 'Loaded sonic' in ln or 'Loaded s1built' in ln:
                    ln = ''
                    continue
        return ln

    def advance_line(self):
        self.index += 1

    def deadvance_line(self):
        self.index += 1


def VDP_CP_ok(ares_line: str, my_line: str, ares_ln: int, my_ln: int) -> bool:
    # RD VDP CP DATA:3600
    ares_data = ares_line.split(':')[1]
    my_data = my_line.split(':')[1]
    ares_num = int(ares_data, 16)
    my_num = int(my_data, 16)
    if ares_num != my_num:
        diff = ares_num ^ my_num
        if diff == 8:
            print('\nWARNING DIFF (ARES:' + str(ares_ln) + ' MINE:' + str(my_ln) + '): RD CTRL VBLANK')
            return True
    print('BAD DIFF!', diff)
    return False


def comp_UDS(ares_line, my_line) -> bool:
    # Check on UDS/LDS
    ares_UDS = ares_line.split('(')[1].split(')')[0]
    my_UDS = my_line.split('(')[1].split(')')[0]
    if ares_UDS != my_UDS:
        return False
    ares_mask = 0
    if ares_UDS[0] == '1':
        ares_mask |= 0xFF00
    if ares_UDS[1] == '1':
        ares_mask |= 0x00FF
    my_mask = 0
    if my_UDS[0] == '1':
        my_mask |= 0xFF00
    if my_UDS[1] == '1':
        my_mask |= 0x00FF

    ares_val_s = ares_line.split(')')[1].strip()
    my_val_s = my_line.split(')')[1].strip()
    ares_val = int(ares_val_s, 16) & ares_mask
    my_val = int(my_val_s, 16) & my_mask
    return ares_val == my_val

def print_context(who: str, lines: file_lines, start: int, total:int):
    print('\nCONTEXT FOR ' + who)
    num = 0
    for idx in range(start, start+total):
        try:
            print(str(num) + ': ' + lines.lines[idx].strip())
        except:
            break
        num += 1
def VDP_FILL_SWITCH(ares_lines, my_lines, ares_line, my_line) -> bool:
    # We want to return true if next line works swapped
    next_ares_line = ares_lines.get_line()
    next_my_line = my_lines.get_line()
    if (next_my_line == ares_line) and (next_ares_line == my_line):
        return True
    ares_lines.deadvance_line()
    my_lines.deadvance_line()
    return False

def CPU_RD_chkswitch(ares_lines, my_lines, ares_line, my_line) -> bool:
    if comp_UDS(ares_line, my_line):
        return True

    # We want to return true if next line works swapped
    next_ares_line = ares_lines.get_line()
    next_my_line = my_lines.get_line()
    if (next_my_line == ares_line) and (next_ares_line == my_line):
        return True
    ares_lines.deadvance_line()
    my_lines.deadvance_line()
    return False


def get_PC(line: str) -> Optional[int]:
    if 'PC ' not in line:
        return None
    addr = int(line.split('PC ')[1], 16)
    return addr


def one_in_vblank(ares_PC, my_PC, addrs):
    if ares_PC in addrs and my_PC not in addrs:
        return True
    if my_PC in addrs and ares_PC not in addrs:
        return True
    return False

def CPU_WR_chkUDS(ares_lines, my_lines, ares_line, my_line) -> bool:
    if comp_UDS(ares_line, my_line):
        return True

    next_ares_line = ares_lines.get_line()
    next_my_line = my_lines.get_line()
    if (next_my_line == ares_line) and (next_ares_line == my_line):
        return True
    ares_lines.deadvance_line()
    my_lines.deadvance_line()
    return False


class thing:
    def __init__(self):
        self.my_line: str = ''
        self.ares_line: str = ''
        self.last_ares_line: str = ''
        self.last_my_line: str = ''
        self.ares_lines: file_lines
        self.ares_last_pc_line: Tuple[str, int] = ('', 0)
        self.my_last_pc_line: Tuple[str, int] = ('', 0)
        bpath = os.path.expanduser('~')
        bfile = '_sonic_vdp.log'
        # bfile = '_sonic_cpu.log'

        self.IS_CPU = 'cpu' in bfile
        #self.IS_CPU = True
        self.ares_lines = file_lines(os.path.join(bpath, 'ares' + bfile), True)
        self.my_lines = file_lines(os.path.join(bpath, 'js' + bfile), False)
        self.dobreak = False

    def feed_my_line(self):
        self.last_my_line = self.my_line
        try:
            self.my_line = self.my_lines.get_line()
        except:
            self.dobreak = True

    def feed_ares_line(self):
        self.last_ares_line = self.ares_line
        try:
            self.ares_line = self.ares_lines.get_line()
        except:
            self.dobreak = True

    def main(self):
        diffs = []

        while True:
            self.feed_my_line()
            self.feed_ares_line()
            if self.dobreak:
                break

            if 'PC ' in self.ares_line:
                self.ares_last_pc_line = (self.ares_line, self.ares_lines.index - 1)

            if 'PC ' in self.my_line:
                self.my_last_pc_line = (self.my_line, self.my_lines.index - 1)

            if self.ares_line != self.my_line:
                if 'RD VDP CP DATA:' in self.ares_line and 'RD VDP CP DATA:' in self.my_line:
                    if VDP_CP_ok(self.ares_line, self.my_line, self.ares_lines.index - 1, self.my_lines.index - 1):
                        continue
                if self.IS_CPU:
                    if 'RD ' in self.ares_line and 'RD ' in self.my_line:
                        if CPU_RD_chkswitch(self.ares_lines, self.my_lines, self.ares_line, self.my_line):
                            continue
                    if 'WR ' in self.ares_line and 'WR ' in self.my_line:
                        if CPU_WR_chkUDS(self.ares_lines, self.my_lines, self.ares_line, self.my_line):
                            continue

                    # Skip VBlank differences
                    ares_PC = get_PC(self.ares_line)
                    my_PC = get_PC(self.my_line)
                    # if ares_PC is not None and my_pc is not None:
                    VBLANK_WAIT_ADDRS = {0x29a4, 0x2900, 0x29a8}
                    if ares_PC in VBLANK_WAIT_ADDRS and my_PC in VBLANK_WAIT_ADDRS:
                        continue

                    if one_in_vblank(ares_PC, my_PC, VBLANK_WAIT_ADDRS):
                        num = 0
                        while one_in_vblank(ares_PC, my_PC, VBLANK_WAIT_ADDRS):
                            if my_PC in VBLANK_WAIT_ADDRS:
                                self.feed_my_line()
                                my_PC = get_PC(self.my_line)
                                if my_PC is None:
                                    print('VBLANK SYNC FAILED2!')
                                    break
                            else:
                                self.feed_ares_line()
                                ares_PC = get_PC(self.ares_line)
                                if ares_PC is None:
                                    print('VBLANK SYNC FAILED2!')
                                    break

                            if self.dobreak:
                                print("VBLANK SYNC FAILED DOBREAK!")
                                break
                            num += 1
                            if num > 10:
                                print('VBLANK SYNC FAILED')
                                break
                        print('VBLANK SYNC DONE WITH ' + str(num))
                        self.my_lines.index -= 1
                        self.ares_lines.index -= 1
                        continue

                    # Skip a dumb thing
                    '''if 'PC 000300' in ares_last_pc_line[0]:
                        continue'''

                    # if 'RD ' in self.ares_line and 'RD ' in self.my_line:
                    #    continue
                else:
                    # if 'RD VDP CP DATA' in self.ares_line:
                    #    continue
                    '''if VDP_FILL_SWITCH(self.ares_lines, self.my_lines, self.ares_line, self.my_line):
                        continue'''
                    SKIPEMS = {}
                    if self.ares_lines.index in SKIPEMS:
                        continue
                    pass

                ares_context_index = self.ares_lines.index - 10
                my_context_index = self.my_lines.index - 10

                diffs.append([self.ares_line, self.my_line])
                print('\nLINE-1:ARES: ' + self.last_ares_line)
                print('ARES(' + str(self.ares_lines.index) + '): ' + self.ares_line)
                try:
                    print('LINE+1:ARES: ' + self.ares_lines.get_line())
                except:
                    pass

                print('\nLINE-1:MINE: ' + self.last_my_line)
                print('MINE(' + str(self.my_lines.index) + '): ' + self.my_line)
                try:
                    print('LINE+1:MINE: ' + self.my_lines.get_line())
                except:
                    pass
                if self.IS_CPU:
                    print('\nLAST PC:ARES(' + str(self.ares_last_pc_line[1]) + '): ' + self.ares_last_pc_line[0])
                    print('LAST PC:MINE(' + str(self.my_last_pc_line[1]) + '): ' + self.my_last_pc_line[0])

                print_context('ARES', self.ares_lines, ares_context_index, 12)
                print_context('MINE', self.my_lines, my_context_index, 12)
                # continue
                break
        print('# diffs ' + str(len(diffs)))


if __name__ == '__main__':
    a = thing()
    a.main()
