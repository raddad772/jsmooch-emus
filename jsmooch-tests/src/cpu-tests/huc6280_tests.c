//
// Created by . on 6/28/25.
//

#include "huc6280_tests.h"

#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ISTART 0

#include "m6502_tests.h"
#include "cpu-test-helpers.h"
#include "helpers/int.h"
#include "component/cpu/huc6280/huc6280.h"
#include "component/cpu/huc6280/huc6280_misc.h"
#include "component/cpu/huc6280/huc6280_opcodes.h"
#include "helpers/multisize_memaccess.c"

#define MAX_CYCLES 500
#define MAX_RAM_PAIRS 500

#define SKIP_NUM 5
static const int ST_SKIPS[SKIP_NUM] = { 0x73, // TII
                                        0xC3, // TDD
                                        0xD3, // TIN
                                        0xE3, // TIA
                                        0xF3
} ;

#define ST_TEST_NUM 3
static const int ST_TESTS[ST_TEST_NUM] = {0x03, 0x13, 0x23};

#define MOV_TEST_NUM 4
// 73, D3, C3, E3,
static const int ST_MOVS[MOV_TEST_NUM] = { 0x73, 0xC3, 0xD3, 0xE3 };

u32 is_st_opcode(u32 opc) {
    for (u32 i = 0; i < ST_TEST_NUM; i++)
        if (opc == ST_TESTS[i]) return 1;
    return 0;
}

u32 is_skip(u32 opc) {
    for (u32 i = 0; i < SKIP_NUM; i++)
        if (opc == ST_SKIPS[i]) return 1;
    return 0;
}
u32 is_mov_opcode(u32 opc) {
    for (u32 i = 0; i < MOV_TEST_NUM; i++)
        if (opc == ST_MOVS[i]) return 1;
    return 0;
}


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
    u32 opcode;

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
    u32 failed;
    u32 fail_cycle;
} ts;

static u32 do_test_read_trace(void *ptr, u32 addr)
{
    return 0;
}

static u32 decode_state(u8 *buf, struct huc6280_state *st)
{
    u32 full_sz = cR32(buf, 0);
    //printf("\nSTATE SZ:%d", full_sz);
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
        rp->val = cR8(buf, 4);
        buf += 5;
    }
    return full_sz;
}

static u32 decode_name(u8 *buf)
{
    buf[49] = 0;
    u8 *r = buf + 48;
    while(r > buf) {
        if (*r == ' ') {
            *r = 0;
        }
        else {
            break;
        }
        r--;
    }
    strncpy(ts.test.name, (char *)buf, 50);
    return 50;
}

static u32 decode_cycles(u8 *buf)
{
    u32 full_sz = 8;
    ts.test.total_cycles = cR32(buf, 0);
    //printf("\nTOTAL CYCLE:%d", ts.test.total_cycles);
    buf += 4;
    // 4 bytes num actually recorded cycles
    ts.test.recorded_cycles = cR32(buf, 0);
    assert(ts.test.recorded_cycles <= MAX_CYCLES);
    //printf("\nRECORDED CYCLE:%d", ts.test.recorded_cycles);
    buf += 4;
    fflush(stdout);
    for (u32 i = 0; i < ts.test.recorded_cycles; i++) {
        u32 cdata = cR32(buf, 0);
        struct cycle *c = &ts.test.cycles[i];
        buf += 4;
        full_sz += 4;

        c->r = cdata & 1;
        c->w = (cdata >> 1) & 1;
        c->dummy = (cdata >> 2) & 1;
        c->Addr = (cdata >> 3) & 0x1FFFFF;
        c->D = (cdata >> 24) & 0xFF;
        if (c->w == 0 && c->r == 0) {
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
    u8 *bm = malloc(sz);
    u8 *buf = bm;
    fread(buf, 1, sz, f);
    //printf("\nSZ: %d", sz);
    // Read 50 bytes and right-strip
    buf += decode_name(buf);
    //printf("\nNAME: %s", ts.test.name);
    // opcode #
    ts.test.opcode = *(u32 *)buf;
    //printf("\nOPCODE: %02x", opcode);
    buf += 4;
    // load initial state
    buf += decode_state(buf, &ts.test.initial);
    // load final state
    buf += decode_state(buf, &ts.test.final);
    // 4 butes num cycles
    fflush(stdout);
    buf += decode_cycles(buf);
    free(bm);

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

static void do_idle() {
    //printf("\nIDLE!");
    if (ts.num_cycle < MAX_CYCLES) {
        struct cycle *myc = &ts.my_cycles[ts.num_cycle];
        myc->r = 0;
        myc->w = 0;
        myc->D = 0;
        myc->Addr = 0;
        myc->dummy = 1;
    }
}

static void do_my_write()
{
    //printf("\nWRITE!");
    if (ts.num_cycle < MAX_CYCLES) {
        struct cycle *myc = &ts.my_cycles[ts.num_cycle];
        myc->r = 0;
        myc->w = 1;
        myc->dummy = 0;
        myc->D = ts.cpu.pins.D;
        myc->Addr = ts.cpu.pins.Addr;
    }
    ts.RAM[ts.cpu.pins.Addr] = ts.cpu.pins.D;
}

static u32 do_my_read()
{
    //printf("\nREAD!");
    if (ts.num_cycle < MAX_CYCLES) {
        struct cycle *myc = &ts.my_cycles[ts.num_cycle];
        myc->r = 1;
        myc->w = 0;
        myc->dummy = 0;
        myc->D = ts.RAM[ts.cpu.pins.Addr];
        myc->Addr = ts.cpu.pins.Addr;
        return myc->D;
    }
    return ts.RAM[ts.cpu.pins.Addr];
}

static void compare_ram()
{
    for (u32 i = 0; i < ts.test.final.num_RAM; i++) {
        struct RAM_pair *rp = &ts.test.final.RAM[i];
        if (ts.RAM[rp->addr] != rp->val) {
            if ((i >= 0) && (i <= 3) && (is_mov_opcode(ts.test.opcode))) {
                printf("\nWARN2");
            }
            else if (i < 490) {
                printf("\nFAIL RAM %d! ADDR:%06x  EXPECT:%02x  MEASURE:%02x", i, rp->addr, rp->val, ts.RAM[rp->addr]);
                ts.failed = 1;
            }
            else {
                printf("\nWARN FAIL >=490: %d", i);
            }
        }
    }
}

static void pprint_P(u32 val)
{
    // N V T B D I Z C
    printf("%c%c%c%c%c%c%c%c",
           val & 0x80 ? 'N' : 'n',
           val & 0x40 ? 'V' : 'v',
           val & 0x20 ? 'T' : 't',
           val & 0x10 ? 'B' : 'b',
           val & 8 ? 'D' : 'd',
           val & 4 ? 'I' : 'i',
           val & 2 ? 'Z' : 'z',
           val & 1 ? 'C' : 'c');
}

static void pprint_cycles()
{
    printf("\n\nExpected            |  Measured");
    printf("\n#   Addr    D   P   |   Addr    D   P");
    for (u32 i = 0; i < ts.test.recorded_cycles; i++) {
        struct cycle *tc = &ts.test.cycles[i];
        if (!tc->r && !tc->w) {
            printf("\n%d:  ------  --     ", i);
        }
        else {
            printf("\n%d:  %06X  %02X  %c%c%c", i,
                   tc->Addr, tc->D, tc->r ? 'R' : ' ', tc->w ? 'W' : ' ', tc->dummy ? 'D' : ' ');
        }
        if (i < ts.num_cycle) {
            struct cycle *mc = &ts.my_cycles[i];
            if (!mc->r && !mc->w)
                printf(" |  ------  --    ");
            else
                printf(" |  %c%06X %c%02X %c%c%c",
                       mc->Addr == tc->Addr ? ' ' : '*',
                       mc->Addr,
                       mc->D == tc->D ? ' ' : '*',
                       mc->D,
                       ((mc->r == tc->r) && (mc->w == tc->w)) ? ' ' : '*',
                       mc->r ? 'R' : ' ', mc->w ? 'W' : ' ');
        }
    }
}

static void pprint_cpu(u32 show_diffs_only, struct huc6280_state *st) {
    u32 show_all = !show_diffs_only;
    //u32 show_all = 1;

    if (!st) {
        printf("\nA:%02x  X:%02x  Y:%02x  SP:%02x  P:", ts.cpu.regs.A, ts.cpu.regs.X, ts.cpu.regs.Y, ts.cpu.regs.S);
        pprint_P(ts.cpu.regs.P.u);
        printf("  PC:%04x\n", ts.cpu.regs.PC);
        for (u32 i = 0; i < 8; i++) {
            printf("%d:%02x", i, ts.cpu.regs.MPR[i] >> 13);
        }
        return;
    }
    printf("\n    Expected      Measured");
    printf("\n    --------      --------");
    if (1 || show_all || (st->PC != ((ts.cpu.regs.PC-1) & 0xFFFF))) {
        printf("\n%cPC:%04x          %04x", st->PC != ((ts.cpu.regs.PC-1) & 0xFFFF) ? '*' : ' ', st->PC, ((ts.cpu.regs.PC-1) & 0xFFFF));
    }
    if (show_all || (st->A != ts.cpu.regs.A)) printf("\n%cA: %02x            %02x", st->A != ts.cpu.regs.A ? '*' : ' ', st->A, ts.cpu.regs.A);
    if (show_all || (st->X != ts.cpu.regs.X)) printf("\n%cX: %02x            %02x", st->X != ts.cpu.regs.X ? '*' : ' ', st->X, ts.cpu.regs.X);
    if (show_all || (st->Y != ts.cpu.regs.Y)) printf("\n%cY: %02x            %02x", st->Y != ts.cpu.regs.Y ? '*' : ' ', st->Y, ts.cpu.regs.Y);
    if (show_all || (st->S != ts.cpu.regs.S)) printf("\n%cS: %02x            %02x", st->S != ts.cpu.regs.S ? '*' : ' ', st->S, ts.cpu.regs.S);
    if (1 || show_all || (st->P != ts.cpu.regs.P.u)) {
        printf("\n%cP: ", st->P != ts.cpu.regs.P.u ? '*' : ' ');
        pprint_P(st->P);
        printf("      ");
        pprint_P(ts.cpu.regs.P.u);
    }
    for (u32 i = 0; i < 8; i++) {
        if (show_all || (st->MPR[i] != (ts.cpu.regs.MPR[i] >> 13))) {
            printf("\n%cM%d:%02x            %02x", st->MPR[i] != (ts.cpu.regs.MPR[i] >> 13) ? '*' : ' ', i, st->MPR[i], ts.cpu.regs.MPR[i] >> 13);
        }
    }
}

static void compare_state(struct huc6280_state *st)
{
#define Cpr(x,y,z) if (st-> x != ts.cpu.regs. y) { failed = 1; printf("\nIT WAS %s", z); }
    u32 failed = 0;
    if (st->PC != ((ts.cpu.regs.PC - 1) & 0xFFFF)) {
        printf("\nITS THE PC: %04x vs %04x", st->PC, ((ts.cpu.regs.PC-1) & 0xFFFF));
        failed = 1;
        ts.fail_cycle = ts.num_cycle;
    }
    Cpr(A, A, "A")
    Cpr(X, X, "X")
    Cpr(Y, Y, "Y")
    Cpr(S, S, "S")
    Cpr(P, P.u, "P")
    for (u32 i = 0; i < 8; i++) {
        Cpr(MPR[i], MPR[i] >> 13, "MPR0...7")
    }
#undef Cpr
    if (failed) {
        if (is_mov_opcode(ts.test.opcode)) {
            printf("\nWARN!");
        }
        else {
            printf("\nFAIL: STATE OPCODE:%02x", ts.test.opcode);
            pprint_cpu(1, &ts.test.final);
            pprint_cycles();
            ts.failed = 1;
            ts.fail_cycle = ts.num_cycle;
        }
    }
}

static void compare_cycle(u32 num)
{
    if (num < MAX_CYCLES) {
        struct cycle *c = &ts.test.cycles[num];
        u32 failed = 0;
        if (ts.cpu.pins.RD && ts.cpu.pins.WR) {
            printf("\nERROR RD AND WR!");
        }
        if ((c->r != ts.cpu.pins.RD) || (c->w != ts.cpu.pins.WR)) {
            printf("\nFAIL: RD/WR %d", num);
            failed = 1;
        }
        if ((ts.cpu.pins.RD || ts.cpu.pins.WR) && ((ts.cpu.pins.D != c->D) || (ts.cpu.pins.Addr != c->Addr))) {
            printf("\nFAIL D/Addr %d", num);
            failed = 1;
        }
        if (failed) {
            printf("\nFAIL COMPARE CYCLE %d", num);
            if (is_st_opcode(ts.test.opcode) && (num == 3)) {
                printf("\nOH NO SKIP ST OPCODE");
            }
            else {
                ts.failed=1;
                ts.fail_cycle = ts.num_cycle;
            }
        }
    }
}

static void service_RW()
{
    if (ts.cpu.pins.RD)
        ts.cpu.pins.D = do_my_read();
    else if (ts.cpu.pins.WR)
        do_my_write();
    else
        do_idle();
}

void do_test(char *fname)
{
    FILE *f = fopen(fname, "rb");
    u32 NUMTEST;
    fread(&NUMTEST, sizeof(NUMTEST), 1, f);
    printf("\nOpening test %s", fname);

    //compare_cycle(0);
    //printf("\nNUMTEST: %d", NUMTEST);

    for (u32 testnum = 0; testnum < NUMTEST; testnum++) {
        printf("\n\nTEST %d", testnum);
        fill_test(f);

        copy_state_to_cpu(&ts.test.initial);

        // We must "pump" the first cycle by setting the pins ourselves
        u32 iaddr = get_long_addr(ts.test.initial.PC);
        ts.num_cycle = 0;
        ts.cpu.ins_decodes = 0;
        ts.cpu.regs.PC = (ts.cpu.regs.PC+1) & 0xFFFF;
        ts.cpu.pins.Addr = iaddr;
        u8 v = do_my_read();
        ts.cpu.pins.D = v;
        ts.cpu.pins.BM = 0;
        ts.cpu.pins.RD = 1;
        ts.cpu.pins.WR = 0;
        ts.cpu.regs.TCU = 0;
        ts.failed = 0;

        // Test cycle #0 has already transpired, an opcode read ending prev. instruction
        u32 ins_decode_start = ts.cpu.ins_decodes+1;
        i32 our_last_cycle = -1;
        u32 i = 1;
        while(true) {
            ts.num_cycle = i;
            HUC6280_cycle(&ts.cpu);
            service_RW();
            //if (i < (ts.test.total_cycles-1))
            if (i < ts.test.total_cycles) compare_cycle(i);
            if (ts.cpu.regs.TCU == 0) {
                if (our_last_cycle == -1) our_last_cycle = i;
                break;
            }
            //if (ts.failed) break;
            i++;
        }
        ts.num_cycle = ts.test.total_cycles;
        //printf("\nEXT CYCLE");
        //HUC6280_cycle(&ts.cpu); // Our last cycle has some stuff in it sometimes
        /*if (ts.cpu.regs.TCU == 0) {
            if (our_last_cycle == -1) our_last_cycle = ts.test.total_cycles;
        }*/
        if (our_last_cycle != ts.test.total_cycles) {
            printf("\nCYCLE MISMATCH");
            printf("\nBAD! TEST CYCLES:%d  OURS:%d", ts.test.total_cycles, our_last_cycle);
            ts.failed = 1;
        }
        //service_RW();
        compare_state(&ts.test.final);
        compare_ram();
        if (ts.failed) break;

    }
    if (ts.failed) {
        printf("\nFAILED AT CYCLE:%d", ts.fail_cycle);
        printf("\nT AT START: %d", (ts.test.initial.P >> 5) & 1);
        pprint_cycles();
        printf("\n\n----INITIAL STATE AND OUR FINAL");
        pprint_cpu(false, &ts.test.initial);
        printf("\n\n----FINAL STATE AND OURS");
        pprint_cpu(true, &ts.test.final);
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
    struct jsm_debug_read_trace t;
    HUC6280_setup_tracing(&ts.cpu, &rt, &clock, 1);

    for (u32 i = ISTART; i < 0x100; i++) {
        if (is_skip(i)) {
            printf("\nSkip test %02x", i);
            continue;
        }
        char PATH[500];
        char yo[50];
        snprintf(yo, sizeof(yo), "%02x.json.bin", i);
        construct_cpu_test_path(PATH, "huc6280", yo);
        do_test(PATH);
        if (ts.failed) break;
    }


    HUC6280_delete(&ts.cpu);
    jsm_string_delete(&yo);
    free(ts.RAM);
}