#!/usr/bin/env python3
import os

def get_myline(my_file, skip_lines):
    while True:
        m = my_file.readline()
        if skip_lines > 0:
            skip_lines -= 1
            continue
        while len(m) < 2:
            if m is None:
                return None
            if len(m) < 1:
                return None
            m = my_file.readline()
        return m

def compare_files(good_file, my_file):
    num_lines = 0
    skip_lines = 45
    skip_my_lines = 7
    good_lines = []
    my_lines = []
    bad = False

    NUML = 10

    for line in good_file:
        if skip_lines > 0:
            print('SKIP THEIR LINE ' + line)
            skip_lines -= 1
            continue
        num_lines += 1
        if len(line) < 1:
            continue
        line = line.strip()
        if len(good_lines) > NUML:
            good_lines = good_lines[1:]
        good_lines.append(line)
        goodl = line
        myl = get_myline(my_file, skip_my_lines)
        skip_my_lines = 0
        if myl is None:
            print('OH NO ITS NONE AFTER ' + str(num_lines) + 'lines!')
            return
        myl = myl.strip()
        if len(my_lines) > NUML:
            my_lines = my_lines[1:]
        my_lines.append(myl)

        if goodl != myl:
            bad = True
            break
    if bad:
        print('BAD! ' + str(num_lines))

        print("\nGOOD:")
        for r in good_lines:
            print(r)
        print("\nBAD:")
        for r in my_lines:
            print(r)



def main():
    good_fil = os.path.join(os.path.expanduser('~'), 'dev/mesen_neutopia.txt')
    my_fil = os.path.join(os.path.expanduser('~'), 'dev/jsmooch_neutopia.txt')
    with open(good_fil, 'r') as gf:
        with open(my_fil, 'r') as mf:
            compare_files(gf, mf)

if __name__ == '__main__':
    main()