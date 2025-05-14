#!/usr/bin/env python3
import os

class cpu_line:
    def __init__(self):
        self.cycle = 0
        self.ln = ''
        self.C = '0000'
        self.DBR = '00'
        self.D = '0000'
        self.PC = '00|0000'
        self.S = '0000'
        self.X = '0000'
        self.Y = '0000'
        self.P = '00'
        self.E = False
        self.ins = '00'

def parse_myline(m):
    r = cpu_line()
    ls = m.strip().split(' ')
    try:
        r.cycle = int(ls[0])
    except:
        print('?', ls)
        raise
    r.C = ls[2].split(':')[1]
    r.DBR = ls[3].split(':')[1]
    r.D = ls[4].split(':')[1]
    r.PC = ls[5].split(':')[1]
    r.S = ls[6].split(':')[1]
    r.X = ls[7].split(':')[1]
    r.Y = ls[8].split(':')[1]
    r.P = ls[9].split(':')[1]
    r.E = int(ls[10].split(':')[1])
    r.ins = ls[12].split(':')[1].strip()
    r.ln = m
    return r

def get_myline(my_file):
    while True:
        m = my_file.readline()
        while len(m) < 2:
            m = my_file.readline()
        try:
            r = parse_myline(m)
        except:
            continue
        if r is None:
            return None
        return r

    r = cpu_line()
    ls = m.strip().split(' ')
    try:
        r.cycle = int(ls[0])
    except:
        print('?', ls)
        raise
    r.C = ls[2].split(':')[1]
    r.DBR = ls[3].split(':')[1]
    r.D = ls[4].split(':')[1]
    r.PC = ls[5].split(':')[1]
    r.S = ls[6].split(':')[1]
    r.X = ls[7].split(':')[1]
    r.Y = ls[8].split(':')[1]
    r.P = ls[9].split(':')[1]
    r.E = int(ls[10].split(':')[1])
    r.ins = ls[12].split(':')[1].strip()
    bad_fields = []


def compare_files(good_file, my_file):
    eo = 0
    num_lines = 0
    skip_lines = 0
    skip_my_lines = 0
    goodl = None
    seek_PC = None
    last_good_line = ''
    last_my_line = ''
    for line in good_file:
        if eo == 0:
            if skip_lines > 0:
                print('SKIP THEIR LINE ' + line)
                skip_lines -= 1
                continue
            last_good_line = line
            ls = line.split(' ')
            if len(ls) < 11:
                continue
            r = cpu_line()
            r.ln = line
            #print(line.split(' '))
            try:
                r.cycle = int(ls[0])
            except:
                if 'Division' in ls:
                    continue
                if 'WARNING!' in ls:
                    continue
                print('?', len(ls), ls)
                raise
            if seek_PC is not None:
                e = ls[5].split(':')[1]
                if e != seek_PC:
                    continue
                seek_PC = None
                continue
            r.C = ls[2].split(':')[1]
            r.DBR = ls[3].split(':')[1]
            r.D = ls[4].split(':')[1]
            r.PC = ls[5].split(':')[1]
            r.S = ls[6].split(':')[1]
            r.X = ls[7].split(':')[1]
            r.Y = ls[8].split(':')[1]
            r.P = ls[9].split(':')[1]
            r.E = ls[11].split(':')[1]
            if 'true' in r.E:
                r.E = True
            else:
                r.E = False

            goodl = r
            eo = 1
        else:
            eo = 0
            num_lines += 1
            goodl.ins = line.split('[')[1].split(']')[0]
            if ',' in goodl.ins:
                goodl.ins = goodl.ins.split(',')[0]

            r = get_myline(my_file)
            while skip_my_lines > 0:
                skip_my_lines -= 1
                print('SKIP MY LINE ' + r.ln)
                r = get_myline(my_file)
            #if goodl.C != r.C:
            #    if (goodl.)
            #    bad_fields.append('C')
            last_my_line = r.ln
            bad_fields = []
            if goodl.PC != r.PC:
                if r.PC == '00|9583':
                    seek_PC = (r.PC)
                    continue
                if goodl.PC == '80|8346':
                    skip_lines = 1
                    continue
                if goodl.PC == '80|8343':
                    skip_my_lines = 1
                    continue
                bad_fields.append('PC')
            if goodl.DBR != r.DBR:
                bad_fields.append('DBR')
            if goodl.D != r.D:
                bad_fields.append('D')
                bad_fields.append('PC')
            if goodl.S != r.S:
                bad_fields.append('S')
            if goodl.X != r.X:
                print('MISMATCH ON X DURING ' + goodl.ln + ' / ' + line)
                #bad_fields.append('X')
            if goodl.Y != r.Y:
                bad_fields.append('Y')
            if goodl.P != r.P:
                bad_fields.append('P')
            if goodl.E != r.E:
                bad_fields.append('E')
                print(goodl.E)
                print(r.E)
            if goodl.ins != r.ins:
                bad_fields.append('ins')
                print(goodl.ins)
                print(r.ins)


            if len(bad_fields) > 0:
                print("\nTheirs: " + goodl.ln.strip() + '/' + line.strip())
                print("Mine  : " + r.ln.strip())
                print("Mismatches: " + str(bad_fields))
                return
    n = ' '
    num = 0
    while n != '':
        n = my_file.readline()
        num += 1
    print("Short " + str(num) + "lines!")
    print("THEIR LAST: " + last_good_line)
    print("MY LAST   : " + last_my_line)



def main():
    good_fil = os.path.join(os.path.expanduser('~'), 'dev/siena_cpu.txt')
    my_fil = os.path.join(os.path.expanduser('~'), 'dev/my_cpu.txt')
    with open(good_fil, 'r') as gf:
        with open(my_fil, 'r') as mf:
            compare_files(gf, mf)

if __name__ == '__main__':
    main()