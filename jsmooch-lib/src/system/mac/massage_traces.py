#!/usr/bin/python3

import os
import json


def main():
    infilename = os.path.join(os.path.expanduser('~'), 'Downloads/trace.log')
    outfilename = os.path.join(os.path.expanduser('~'), 'dev/trace-512k.txt')
    lines = 0
    last_good_line = ''
    if os.path.exists(outfilename):
        os.unlink(outfilename)
    with open(outfilename, 'w') as outfile:
        with open(infilename, "r") as infile:
            line = infile.readline()
            lines = 1
            while line is not None:
                # 56 00400030 4CF9 A: [FFFFFFFF, 0, 0, 0, 0, 0, 0] D: [FFFFFFFF, 0, 0, 0, 0, 0, 0, 0] USP: 000000 SSP: 28BA4E50 PC: 400030 SR: RegisterSR { 0: 2700, sr: 2700, ccr: 0, c: false, v: false, z: false, n: false, x: false, int_prio_mask: 7, supervisor: true, trace: false }
                e = line.split(' ')
                cycles = e[0].replace(',','')
                try:
                    addr = e[1].replace(',','')
                except:
                    print('BAD LINE:', lines, line, type(line))
                    print('LAST GOOD LINE', last_good_line)
                    line = infile.readline()
                    lines += 1
                    continue
                opcode = e[2].replace(',','')
                a_array = [s.replace(',', '') for s in e[4:11]]
                d_array = [s.replace(',', '') for s in e[12:20]]
                try:
                    usp = e[21].replace(',','')
                except:
                    print('LINE?', lines, line)
                    print('LAST GOOD LINE', last_good_line)
                    line = infile.readline()
                    lines += 1
                    continue
                ssp = e[23].replace(',','')
                pc = e[25].replace(',','')
                if len(pc) < 8: pc = ('0' * (8-len(pc))) + pc
                sr = e[30].replace(',','')
                sr_int = int(sr, 16)
                if sr_int & 0x2000: # supervisor bit set!
                    a_array.append(ssp)
                    asp = usp
                else:
                    a_array.append(usp)
                    asp = ssp
                if len(asp) < 8: asp = ('0' * (8-len(asp))) + asp
                a_array[0] = a_array[0][1:]
                a_array[6] = a_array[6][0:-1]
                d_array[0] = d_array[0][1:]
                d_array[7] = d_array[7][0:-1]
                s = 'cyc:' + cycles + ' a:' + addr + ' opc:' + opcode + ' d:'
                for i in d_array:
                    r = i
                    if len(r) < 8:
                        r = ('0' * (8-len(r))) + i
                    s += r + ' '
                s += 'a:'
                for i in a_array:
                    r = i
                    if len(r) < 8:
                        r = ('0' * (8-len(r))) + i
                    s += r + ' '
                s += 'pc:' + pc
                s += ' sr:' + sr
                s += ' asp:' + asp
                outfile.write(s + '\n')
                last_good_line = line
                line = infile.readline()
                lines += 1
                if not line: break
    print('READ lines: ', lines)


if __name__ == '__main__':
    '''i = 'ae40'
    r = i
    if len(r) < 8:
        r = ('0' * (8-len(r))) + i
    print(r)'''


    main()