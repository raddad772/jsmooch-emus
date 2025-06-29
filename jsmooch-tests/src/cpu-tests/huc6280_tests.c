//
// Created by . on 6/28/25.
//

#include "huc6280_tests.h"

#include <assert.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "m6502_tests.h"
#include "helpers/int.h"
#include "component/cpu/huc6280/huc6280.h"
#include "component/cpu/huc6280/huc6280_misc.h"
#include "component/cpu/huc6280/huc6280_opcodes.h"
#include "helpers/multisize_memaccess.c"

#define MAX_CYCLES 500
#define MAX_RAM_PAIRS 500

struct RAM_pair {
    u32 addr, val;
};

struct huc6280_state {
    u32 A, X, Y, S, P, PC;
    u32 MPR[8];
    u32 num_RAM;
    struct RAM_pair RAM[MAX_RAM_PAIRS];
};

struct cycle {
    u32 r, w, D, Addr, dummy;
};

struct huc6280_test {
    struct huc6280_state initial, final;
    char name[51];

    u32 total_cycles;
    u32 recorded_cycles;
    struct cycle cycles[MAX_CYCLES];
};

struct huc6280_test_struct {
    struct HUC6280 cpu;
    struct huc6280_test test;
    u32 num_cycle;
    struct cycle my_cycles[MAX_CYCLES];
    u8 *RAM;
} ts;

static u32 do_test_read_trace(void *ptr, u32 addr)
{
    return 0;
}

static char *construct_path(char* w, const char* who)
{
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *tp = w;
    tp += sprintf(tp, "%s/dev/huc6280/v1/%s", homeDir, who);
    return tp;
}

static u32 decode_state(u8 *buf, struct huc6280_state *st)
{
    u32 full_sz = cR32(buf, 0);
    buf += 4;
    st->A = cR8(buf, 0);
    st->X = cR8(buf, 1);
    st->Y = cR8(buf, 2);
    st->S = cR8(buf, 3);
    st->P = cR8(buf, 4);
    st->PC = cR16(buf, 5);
    buf += 7;
    for (u32 i = 0; i < 8; i++) {
        st->MPR[i] = cR8(buf, i);
    }
    buf += 8;
    st->num_RAM = cR32(buf, 0);
    buf += 4;
    for (u32 i = 0; i < st->num_RAM; i++) {
        struct RAM_pair *rp = &st->RAM[i];
        rp->addr = cR32(buf, 0);
        rp->val = cR32(buf, 4);
        buf += 5;
    }
    return full_sz;
}

static u32 decode_name(u8 *buf)
{
    buf[49] = 0;
    u8 *r = buf + 48;
    while(r > buf) {
        if (*r != ' ') {
            *(r + 1) = 0;
        }
    }
    strncpy(ts.test.name, (char *)buf, 50);
    return 50;
}

static u32 decode_cycles(u8 *buf)
{
    u32 full_sz = cR32(buf, 0);
    buf += 4;

    ts.test.total_cycles = cR32(buf, 0);
    buf += 4;
    // 4 bytes num actually recorded cycles
    ts.test.recorded_cycles = cR32(buf, 0);
    buf += 4;
    for (u32 i = 0; i < ts.test.recorded_cycles; i++) {
        u32 cdata = cR32(buf, 0);
        struct cycle *c = &ts.test.cycles[i];
        buf += 4;

        c->r = cdata & 1;
        c->w = (cdata >> 1) & 1;
        if (!(cdata & 4)) {
            c->Addr = (cdata >> 3) & 0x1FFFFF;
            c->D = (cdata >> 24) & 0xFF;
        }
        else {
            c->Addr = 0;
            c->D = 0;
        }
    }

    return full_sz;
}

static void fill_test(FILE *f)
{
    u32 sz;
    fread(&sz, sizeof(sz), 1, f);
    sz -= 4;
    u8 *buf = malloc(sz);
    fread(buf, 1, sz, f);
    // Read 50 bytes and right-strip
    buf += decode_name(buf);
    // Skip opcode #
    buf += 4;
    // load initial state
    buf += decode_state(buf, &ts.test.initial);
    // load final state
    buf += decode_state(buf, &ts.test.final);
    // 4 butes num cycles
    buf += decode_cycles(buf);
    free(buf);

}

static void copy_state_to_cpu(struct huc6280_state *st)
{
    ts.cpu.regs.X = st->X;
    ts.cpu.regs.Y = st->Y;
    ts.cpu.regs.A = st->A;
    ts.cpu.regs.S = st->S;
    ts.cpu.regs.P.u = st->P;
    ts.cpu.regs.PC = st->PC;
    for (u32 i = 0; i < 8; i++) {
        ts.cpu.regs.MPR[i] = st->MPR[i] << 13;
    }
    for (u32 i = 0; i < st->num_RAM; i++) {
        struct RAM_pair *rp = &st->RAM[i];
        ts.RAM[rp->addr] = rp->val;
    }
}

static u32 get_long_addr(u32 addr)
{
    u32 mpr = addr >> 13;
    u32 addr_lo = addr & 0x1FFF;
    u32 upper13 = ts.cpu.regs.MPR[mpr];
    addr = upper13 | addr_lo;
    return addr;
}

static u32 do_my_read(u32 addr)
{
    //TODO: read from test! compare cycle!
}

void do_test(char *fname)
{
    FILE *f = fopen(fname, "rb");
    u32 NUMTEST;
    fread(&NUMTEST, sizeof(NUMTEST), 1, f);
    printf("\nOpening test %s", fname);
    for (u32 testnum = 0; testnum < NUMTEST; testnum++) {
        fill_test(f);

        copy_state_to_cpu(&ts.test.initial);

        // We must "pump" the first cycle by setting the pins ourselves
        u32 iaddr = get_long_addr(ts.test.initial.PC);
        u8 v = do_my_read(ts.cpu.regs.PC);
        ts.cpu.regs.PC = (ts.cpu.regs.PC+1) & 0xFFFF;
        ts.cpu.pins.Addr = iaddr;
        ts.cpu.pins.D = v;
        ts.cpu.pins.BM = 0;
        ts.cpu.pins.RD = 1;
        ts.cpu.pins.WR = 0;
        ts.num_cycle = 1;

    }

    fclose(f);
}

void test_huc6280()
{
    struct jsm_string yo;
    jsm_string_init(&yo, 50);
    struct jsm_debug_read_trace rt;
    rt.read_trace = &do_test_read_trace;
    rt.ptr = (void *)&ts;
    ts.RAM = malloc(0x200000);

    struct scheduler_t fakescheduler;
    u64 clock;
    u64 waitstates;
    scheduler_init(&fakescheduler, &clock, &waitstates);
    HUC6280_init(&ts.cpu, &fakescheduler, 10000);
    HUC6280_setup_tracing(&ts.cpu, &rt);

    for (u32 i = 0; i < 0x100; i++) {
        char PATH[500];
        char yo[50];
        snprintf(yo, sizeof(yo), "%02x.json.bin", i);
        construct_path(PATH, yo);
        do_test(PATH);
    }


    HUC6280_delete(&ts.cpu);
    jsm_string_delete(&yo);
    free(ts.RAM);
}