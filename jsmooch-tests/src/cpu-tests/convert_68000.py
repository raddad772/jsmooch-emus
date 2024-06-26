#!/usr/bin/python3

import os
import glob
import json
import struct

M68k_JSON_PATH = os.path.expanduser('~') + '/dev/external/ProcessorTests/680x0/68000/v1/'
buffer = bytearray(10000 * 1024)  # 1k per test assumption

REG_ORDER = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7',
             'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'usp',
             'ssp', 'sr', 'pc'
             ]


def write_state(state, ptr):
    state_start = ptr
    # Make space for state size and magic number 0x12345678
    ptr += 8

    # Registers. Pack as 32 bits BECAUSE
    for a in REG_ORDER:
        struct.pack_into("<I", buffer, ptr, state[a])
        ptr += 4

    # Prefetch
    struct.pack_into("<II", buffer, ptr, state['prefetch'][0], state['prefetch'][1])
    ptr += 8

    # RAM 5-byte values
    # address, value   addr=32bit value=8bit
    struct.pack_into("<I", buffer, ptr, len(state['ram']))
    ptr += 4
    done_addrs = set()
    for addr, val in state['ram']:
        if addr >= 0x1000000:
            print('BAD ADDR', addr)
        if addr in done_addrs:
            print('DUPLICATE ADDR', addr)
        done_addrs.add(addr)
        struct.pack_into("<IB", buffer, ptr, addr, val)
        ptr += 5

    struct.pack_into("<II", buffer, state_start, ptr - state_start, 0x01234567)
    return ptr


def write_name(name, ptr):
    name_start = ptr
    ptr += 8

    a = len(name)
    if a > 64: a = 64
    struct.pack_into("<I", buffer, ptr, len(name))
    ptr += 4
    for i in range(0, a):
        try:
            struct.pack_into("<B", buffer, ptr, ord(name[i]))
            ptr += 1
        except IndexError:
            print('WAIT WHAT?')
            raise

    struct.pack_into("<II", buffer, name_start, ptr - name_start, 0x89ABCDEF)

    return ptr


def write_transactions(length, transactions, ptr):
    # Idle for x cycles, or... (len 2)
    # read, write, tas... with
    # r/w/t, len, FC, addr_bus, .b/.w, data_bus

    # So each transaction entry has:
    #   1) byte indicating kind (idle, r, w, t)
    # then splits according to kind
    transactions_start = ptr

    ptr += 8
    struct.pack_into("<II", buffer, ptr, length, len(transactions))
    ptr += 8
    for t in transactions:
        f = t[0]
        if f == 'n':
            tw = 0
        elif f == 'w':
            tw = 1
        elif f == 'r':
            tw = 2
        elif f == 't':
            tw = 3
        else:
            print('UNKNOWN KIND?', f)
            continue
        struct.pack_into("<BI", buffer, ptr, tw, t[1])
        '''print(type(t[1]))
        if t[1] > 200:
            print('WHAT? ' + str(t[1]))'''
        ptr += 5
        if tw == 0: continue
        # r/w/t, len, FC, addr_bus, .b/.w, data_bus
        struct.pack_into("<IIII", buffer, ptr, t[2], t[3], 0 if t[4] == '.b' else 1, t[5])
        ptr += 16

    struct.pack_into("<II", buffer, transactions_start, ptr - transactions_start, 0x456789AB)
    return ptr


def decode_file(fname):
    ptr = 0
    print(fname)
    with open(fname, 'r') as infile:
        obj = json.load(infile)

    num_tests = len(obj)

    struct.pack_into("<ii", buffer, ptr, 0x1A3F5D71, num_tests)
    ptr += 8

    for test in obj:
        # Make space for INT32(0) and test size
        test_start_ptr = ptr
        ptr += 8

        ptr = write_name(test['name'], ptr)

        # intitial/ram, final/ram
        ptr = write_state(test['initial'], ptr)
        ptr = write_state(test['final'], ptr)

        ptr = write_transactions(test['length'], test['transactions'], ptr)
        struct.pack_into("<II", buffer, test_start_ptr, ptr - test_start_ptr, 0xABC12367)

    # for item in obj:
    with open(fname + '.bin', 'wb') as outfile:
        outfile.write(bytes(buffer[:ptr]))


def main(where):
    fs = glob.glob(where + '**.json')
    for fname in fs:
        decode_file(fname)


if __name__ == '__main__':
    main(M68k_JSON_PATH)
