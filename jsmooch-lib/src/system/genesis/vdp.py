#!/usr/bin/python3
import os


class file_lines:
    def __init__(self, fpath: str, is_ares: bool):
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


def main():
    bpath = os.path.expanduser('~')
    bfile = '_sonic_vdp.log'
    #bfile = '_sonic_cpu.log'

    IS_CPU = 'cpu' in bfile
    ares_lines = file_lines(os.path.join(bpath, 'ares' + bfile), True)
    my_lines = file_lines(os.path.join(bpath, 'js' + bfile), False)

    ares_index = 0
    my_index = 0
    line_num = 0

    last_ares_line = ''
    last_my_line = ''
    ares_line = ''
    my_line = ''

    ares_last_pc_line = ('', 0)
    my_last_pc_line = ('', 0)

    while True:
        last_ares_line = ares_line
        last_my_line = my_line
        ares_line = ares_lines.get_line()
        my_line = my_lines.get_line()

        if 'PC ' in ares_line:
            ares_last_pc_line = (ares_line, ares_lines.index-1)

        if 'PC ' in my_line:
            my_last_pc_line = (my_line, my_lines.index-1)

        if ares_line != my_line:
            if 'RD VDP CP DATA:' in ares_line and 'RD VDP CP DATA:' in my_line:
                if VDP_CP_ok(ares_line, my_line, ares_lines.index - 1, my_lines.index - 1):
                    continue
            if IS_CPU:
                if 'RD ' in ares_line and 'RD ' in my_line:
                    if CPU_RD_chkswitch(ares_lines, my_lines, ares_line, my_line):
                        continue
                if 'WR ' in ares_line and 'WR ' in my_line:
                    if CPU_WR_chkUDS(ares_lines, my_lines, ares_line, my_line):
                        continue

                # Skip a minor difference in how processor cores do reads
                if 'RD 000294' in ares_line and 'RD 000292' in my_line:
                    my_lines.get_line()
                    continue
                # Skip a dumb thing
                if 'PC 000300' in ares_last_pc_line[0]:
                    continue

                #if 'RD ' in ares_line and 'RD ' in my_line:
                #    continue
            else:
                #if 'RD VDP CP DATA' in ares_line:
                #    continue
                SKIPEMS = {65680,131310,131312,133023}
                if ares_lines.index in SKIPEMS:
                    continue
                pass
            print('\nLINE-1:ARES: ' + last_ares_line)
            print('LINE-1:MINE: ' + last_my_line)
            print('ARES(' + str(ares_lines.index) + '): ' + ares_line)
            print('MINE(' + str(my_lines.index) + '): ' + my_line)
            print('LINE+1:ARES: ' + ares_lines.get_line())
            print('LINE+1:MINE: ' + my_lines.get_line())
            if IS_CPU:
                print('\nLAST PC:ARES(' + str(ares_last_pc_line[1]) + '): ' + ares_last_pc_line[0])
                print('LAST PC:MINE(' + str(my_last_pc_line[1]) + '): ' + my_last_pc_line[0])
            break


if __name__ == '__main__':
    main()
