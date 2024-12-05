#!/usr/bin/python3
import os
import glob
import json
from struct import unpack_from
from typing import Dict, Any

NUMTESTS = 20000

def load_state(buf, ptr) -> (int, Any):
    full_sz = unpack_from('i', buf, ptr)[0]

    state = {'R': [], 'R_fiq': [], 'R_svc': [], 'R_abt': [], 'R_irq': [], 'R_und': [], 'CPSR': None, 'SPSR': [], 'pipeline': []}
    ptr += 8
    values = unpack_from('I' * 40, buf, ptr)

    for i in range(0, 16):
        state['R'].append(values[i])

    for i in range(0, 7):
        state['R_fiq'].append(values[i+16])

    for i in range(0, 2):
        state['R_svc'].append(values[i+23])

    for i in range(0, 2):
        state['R_abt'].append(values[i+25])

    for i in range(0, 2):
        state['R_irq'].append(values[i+27])

    for i in range(0, 2):
        state['R_und'].append(values[i+29])

    state['CPSR'] = values[31]
    state['SPSR'] = [ values[32], values[33], values[34], values[35], values[36], values[37] ]
    state['pipeline'] = [ values[38], values[39] ]
    return full_sz, state

def load_transactions(buf, ptr) -> (int, Dict):
    transactions = []
    full_sz, mn, num_transactions = unpack_from('iii', buf, ptr)
    ptr += 12
    if mn != 3: print('!!!!', mn)
    for i in range(0, num_transactions):
        values = unpack_from('<IIIII', buf, ptr)
        ptr += (5 * 4)
        transaction = {
            'kind': values[0],
            'size': values[1],
            'addr': values[2],
            'data': values[3],
            'cycle': values[4]
        }
        if values[0] > 3:
            print('KINDS?', values[0])
        transactions.append(transaction)

    return full_sz, transactions

def load_opcodes(buf, ptr):
    full_sz = unpack_from('i', buf, ptr)[0]
    opcodes = []
    ptr += 8
    values = unpack_from('IIIII', buf, ptr)
    ptr += (5 * 4)
    base_addr = unpack_from('I', buf, ptr)
    return full_sz, list(values), base_addr

def load_base_addr(buf, ptr):
    return unpack_from('I', buf, ptr)[0]

def decode_test(buf, ptr) -> (int, Dict):
    full_sz = unpack_from('i', buf, ptr)[0]
    ptr += 4
    test = {}
    sz, test['initial'] = load_state(buf, ptr)
    ptr += sz
    sz, test['final'] = load_state(buf, ptr)
    ptr += sz
    sz, test['transactions'] = load_transactions(buf, ptr)
    ptr += sz
    sz, test['opcodes'], test['base_addr'] = load_opcodes(buf, ptr)
    ptr += sz

    return full_sz, test



def decode_file(infilename, outfilename):
    print('Decoding ' + infilename)
    with open(infilename, 'rb') as infile:
        content = infile.read()
    ptr = 0
    tests = []
    for i in range(0, NUMTESTS):
        sz, test = decode_test(content, ptr)
        ptr += sz
        tests.append(test)
    if os.path.exists(outfilename):
        os.unlink(outfilename)
    with open(outfilename, 'w') as outfile:
        outfile.write(json.dumps(tests, indent=2))

def do_path(where):
    print("Doing path...", where)
    fs = glob.glob(where + '**.json.bin')
    for fname in fs:
        decode_file(fname, fname[:-4])

def main():
    do_path('')

if __name__ == '__main__':
    main()


'''
    for (u32 i = 0; i < 16; i++) {
        W32(R[i]);
    }
    // R_0-R_7
    for (u32 i = 0; i < 8; i++) {
        W32(R_[i]);
    }
    // FP bank 0 0-15, and bank 1 0-15
    for (u32 b = 0; b < 2; b++) {
        for (u32 i = 0; i < 16; i++) {
            W32(fb[b].U32[i]);
        }
    }
    W32(PC);
    W32(GBR);
    W32(SR);
    W32(SSR);
    W32(SPC);
    W32(VBR);
    W32(SGR);
    W32(DBR);
    W32(MACL);
    W32(MACH);
    W32(PR);
    W32(FPSCR);
'''