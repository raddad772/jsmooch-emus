#!/usr/bin/python3

import os

def compare_files(good_file, my_file):
    good_lines = []
    my_lines = []
    my_line_num = -1
    good_line_num = -1
    with open(good_file, 'r') as gf:
        with open(my_file, 'r') as mf:
            my_line = mf.readline()
            while True:
                my_line_num += 1
                my_line = mf.readline()
                if (my_line_num % 1000000) == 0:
                    print('Line ' + str(int(my_line_num / 1000000)) + ' million')
                if my_line is None: break
                if len(my_line) < 3:
                    break
                mls = my_line.split(':')
                my_pc = mls[0].strip()
                my_opc = mls[1].strip().split(' ')[0].strip()
                my_lines.append(my_line)

                good_line_num += 1
                good_line = gf.readline()
                gls = good_line.split(':')
                good_pc = gls[0].strip()
                good_opc = gls[1].strip().split(' ')[0].strip()
                good_lines.append(good_line)

                if (good_pc != my_pc) or (good_opc != my_opc):
                    print(' DIFF!')
                    print('G.PC:' + good_pc + ' OPC:' + good_opc)
                    print('M.PC:' + my_pc + ' OPC:' + my_opc)
                    print('my_line  (' + str(my_line_num) + '): ' + my_line.strip())
                    print('good_line(' + str(good_line_num) + '): ' + good_line.strip())
                    BACKLINES = -10
                    print('\nMy last ' + str(0 - BACKLINES) + ' lines:')
                    tp = my_lines[BACKLINES:]
                    for p in tp:
                        print(p.strip())

                    print('\nDuckStation last ' + str(0 - BACKLINES) + ' lines:')
                    tp = good_lines[BACKLINES:]
                    for p in tp:
                        print(p.strip())
                    break
        print('\n\nWent over ' + str(my_line_num) + ' of my lines and ' + str(good_line_num) + ' of their lines!')
def main():
    good_file = os.path.join(os.path.expanduser('~'), 'dev/cpu_log.txt')
    my_file = os.path.join(os.path.expanduser('~'), 'dev/r3000.txt')
    compare_files(good_file, my_file)

if __name__ == '__main__':
    main()