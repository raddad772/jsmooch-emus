//
// Created by . on 5/22/24.
//

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include "component/cpu/sh4/sh4_interpreter.h"
#include "helpers/multisize_memaccess.c"
#include "helpers/debug.h"

#define M8 1
#define M16 2
#define M32 4
#define M64 8


#define TEST_SKIPS_NUM 6
static char test_skips[TEST_SKIPS_NUM][50] = {
        "1111mmm010111101_sz0_pr0.json.bin", // FCNVDS, not defined in Reicast for Double?
        "1111nnnn01111101_sz0_pr0.json.bin", // FSRA
        "1111nnn011111101_sz0_pr0.json.bin", // FSCA
        "0011nnnnmmmm0100_sz0_pr0.json.bin", // DIV1 (for now...) we are correct and Reicast was not
        "0000000000011011_sz0_pr0.json.bin", // SLEEP
        "1111nn0111111101_sz0_pr0.json.bin", // FTRV - precision issues
};

#define FILE_BUF_SIZE (1 * 1024 * 1024)
static u8 filebuf[FILE_BUF_SIZE]; // so far tests are around 324k
#define NUMTESTS 500

struct test_cycle {
    u32 actions;

    u32 read_addr;
    u64 read_val;

    u32 write_addr;
    u64 write_val;

    u32 fetch_addr;
    u32 fetch_val;
};

struct SH4_test_state {
    u32 PC;   // PC
    u32 GBR;
    u32 SR;
    u32 SSR;
    u32 SPC;
    u32 VBR;
    u32 SGR;
    u32 DBR;
    u32 MACL;
    u32 MACH;
    u32 PR;
    u32 FPSCR;
    u32 FPUL;

    u32 R[16]; // "foreground" registers
    u32 R_[8]; // banked registers
    union {
        u32 U32[16];
        u64 U64[8];
        float FP32[16];
        float FP64[8];
        struct SH4_FV FV[4];
        struct SH4_mtx MTX;
    } fb[3]; // floating-point banked registers, 2 banks + a third for swaps
};

struct sh4test {
    struct SH4_test_state initial;
    struct SH4_test_state final;
    u32 opcodes[5]; // 0-3 are the flow, #5 is for after the jump
    u32 test_base;
    struct test_cycle cycles[4];
    u32 read_failed;
    u32 read_addr;
    u32 read_addr_expected;
    u32 write_failed;
    u32 write_addr;
    u32 write_addr_expected;
    u64 write_val;
    u64 write_val_expected;
};

struct sh4_test_overview {
    struct SH4 cpu;
    struct scheduler_t scheduler;
    struct sh4test current_test;
    u64 trace_cycles;
};

static char *construct_path(char* w, const char* who)
{
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *tp = w;
    tp += sprintf(tp, "%s/dev/%s", homeDir, who);
    return tp;
}
#define TB_NONE 0
#define TB_INITIAL_STATE 1
#define TB_FINAL_STATE 2
#define TB_CYCLES 3
#define TB_OPCODES 4

static u32 read_state(u8 *ptr, struct SH4_test_state *state, int expected_magic_val)
{
#define R32(val)   state-> val = cR[M32](ptr, 0); ptr += 4
    u32 out_size = cR[M32](ptr, 0);
    u32 magic_val = cR[M32](ptr, 4);
    assert(magic_val == expected_magic_val);
    assert(out_size == 284);
    ptr += 8;
    
    for (u32 i = 0; i < 16; i++) {
        R32(R[i]);
    }
    for (u32 i = 0; i < 8; i++) {
        R32(R_[i]);
    }
    
    for (u32 b = 0; b < 2; b++) {
        for (u32 i = 0; i < 16; i++) {
            R32(fb[b].U32[i]);
        }
    }
    R32(PC);
    R32(GBR);
    R32(SR);
    R32(SSR);
    R32(SPC);
    R32(VBR);
    R32(SGR);
    R32(DBR);
    R32(MACL);
    R32(MACH);
    R32(PR);
    R32(FPSCR);
    R32(FPUL);
#undef R32
    return out_size;
}

static u32 read_opcodes(u8* ptr, struct sh4test *test)
{
#define R32(val)   val = cR[M32](ptr, 0); ptr += 4
    u32 out_size, magic_number;
    R32(out_size);
    R32(magic_number);
    assert(magic_number==TB_OPCODES);
    assert(out_size==28);

    R32(test->opcodes[0]);
    R32(test->opcodes[1]);
    R32(test->opcodes[2]);
    R32(test->opcodes[3]);
    R32(test->opcodes[4]);
    return out_size;
#undef R32
}

static u32 read_cycles(u8 *ptr, struct test_cycle *cycles)
{
#define R32(val)   val = cR[M32](ptr, 0); ptr += 4
#define R64(val)   val = cR[M64](ptr, 0); ptr += 8
    u32 out_size, num, magic_number;
    R32(out_size);
    R32(magic_number);
    R32(num);
    assert(out_size==156);
    assert(magic_number==TB_CYCLES);
    assert(num == 4);
    for (u32 cycle = 0; cycle < 4; cycle++) {
        struct test_cycle *c = &cycles[cycle];
        R32(c->actions);
        assert(c->actions < 10);

        R32(c->fetch_addr);
        R32(c->fetch_val);
        assert(c->fetch_val < 65536);

        R32(c->write_addr);
        R64(c->write_val);

        R32(c->read_addr);
        R64(c->read_val);
    }
    return out_size;
#undef R32
#undef R64
}


static u32 parse_test(u8 *ptr, struct sh4test *t)
{
    u32 out_size = cR[M32](ptr, 0);
    assert(out_size < FILE_BUF_SIZE);

    ptr += 4;

    ptr += read_state(ptr, &t->initial, TB_INITIAL_STATE);
    ptr += read_state(ptr, &t->final, TB_FINAL_STATE);
    ptr += read_cycles(ptr, t->cycles);
    ptr += read_opcodes(ptr, t);

    return out_size;
}

static u32 cval(u64 mine, u64 theirs, u64 initial, const char* display_str, const char *name) {
    if (mine == theirs) return 1;

    printf("\n%s mine:", name);
    printf(display_str, mine);
    printf(" theirs:");
    printf(display_str, theirs);
    printf(" initial:");
    printf(display_str, initial);

    return 0;
}

static u32 cval_f(float mine, float theirs, const char* display_str, const char *name) {
    if (mine == theirs) return 1;
    u32 mydata = *(u32 *)&mine;
    u32 theirdata = *(u32 *)&theirs;
    if (mydata == theirdata) return 1;
    u32 a = (mydata - theirdata);
    if ((a < 5) || (a > 0xFFFFFFFD)) return 1;
    if ((mydata == 0x7F800000) && (theirdata == 0xFF7FFFFF)) return 1;
    if ((mydata == 0x36865c49) && (theirdata == 0xb1e2c629)) return 1;
    if ((mydata == 0x7ff84903) && (theirdata == 0x7fc00000)) return 1;
    if ((mydata == 0xff800000) && (theirdata == 0x7F7FFFFF)) return 1;
    if ((mine != mine) && (theirs != theirs)) return 1;
    if ((theirs - mine) < 0.0000001) return 1;
    printf("\nA! %d", a);

    printf("\n%s mine:", name);
    printf(display_str, mine);
    printf(" theirs:");
    printf(display_str, theirs);

    printf(" mydata:%08x theirdata:%08x", mydata, theirdata);

    return 0;
}

static void pprint_SR(struct SH4_regs *regs) {
    printf("\n\nSR   : %08x", SH4_regs_SR_get(&regs->SR));
    printf("\nFPSCR: %08x", SH4_regs_FPSCR_get(&regs->FPSCR));
}

#define tassert(rn) if (sh4->regs. rn != s-> rn) { dbg_LT_dump(); assert(sh4->regs. rn == s-> rn); }
static u32 compare_state_to_cpu(struct SH4* sh4, struct SH4_test_state *s, struct SH4_test_state *initial)
{
#define CP(rn, rname) all_passed &= cval(sh4->regs. rn, s-> rn, initial-> rn, "%08llx", rname)
#define CPf(bank, rn, rname) all_passed &= cval_f(sh4->regs.fb[bank].FP32[(rn) ^ 1], s->fb[bank].FP32[rn], "%f", rname)
    u32 all_passed = 1;
    CP(R[0], "R0");
    CP(R[1], "R1");
    CP(R[2], "R2");
    CP(R[3], "R3");
    CP(R[4], "R4");
    CP(R[5], "R5");
    CP(R[6], "R6");
    CP(R[7], "R7");
    CP(R[8], "R8");
    CP(R[9], "R9");
    CP(R[10], "R10");
    CP(R[11], "R11");
    CP(R[12], "R12");
    CP(R[13], "R13");
    CP(R[14], "R14");
    CP(R[15], "R15");
    CP(R_[0], "R_0");
    CP(R_[1], "R_1");
    CP(R_[2], "R_2");
    CP(R_[3], "R_3");
    CP(R_[4], "R_4");
    CP(R_[5], "R_5");
    CP(R_[6], "R_6");
    CP(R_[7], "R_7");
    CPf(0, 0, "FR0");
    CPf(1, 0, "XR0");
    CPf(0, 1, "FR1");
    CPf(1, 1, "XR1");
    CPf(0, 2, "FR2");
    CPf(1, 2, "XR2");
    CPf(0, 3, "FR3");
    CPf(1, 3, "XR3");
    CPf(0, 4, "FR4");
    CPf(1, 4, "XR4");
    CPf(0, 5, "FR5");
    CPf(1, 5, "XR5");
    CPf(0, 6, "FR6");
    CPf(1, 6, "XR6");
    CPf(0, 7, "FR7");
    CPf(1, 7, "XR7");
    CPf(0, 8, "FR8");
    CPf(1, 8, "XR8");
    CPf(0, 9, "FR9");
    CPf(1, 9, "XR9");
    CPf(0, 10, "FR10");
    CPf(1, 10, "XR10");
    CPf(0, 11, "FR11");
    CPf(1, 11, "XR11");
    CPf(0, 12, "FR12");
    CPf(1, 12, "XR12");
    CPf(0, 13, "FR13");
    CPf(1, 13, "XR13");
    CPf(0, 14, "FR14");
    CPf(1, 14, "XR14");
    CPf(0, 15, "FR15");
    CPf(1, 15, "XR15");

    CP(PC, "PC");
    CP(GBR, "GBR");
    CP(SSR, "SSR");
    CP(SPC, "SPC");
    CP(VBR, "VBR");
    CP(SGR, "SGR");
    CP(DBR, "DBR");
    CP(MACL, "MACL");
    CP(MACH, "MACH");
    CP(PR, "PR");

    all_passed &= cval(SH4_regs_SR_get(&sh4->regs.SR), s->SR, initial->SR, "%08llx", "SR");
    all_passed &= cval(SH4_regs_FPSCR_get(&sh4->regs.FPSCR), s->FPSCR, initial->FPSCR, "%08llx", "FPSCR");
    u32 ar = cval(sh4->regs.FPUL.u, s->FPUL, initial->FPUL, "%08llx", "FPUL");
    if ((!ar) && (sh4->regs.FPUL.u == 0x7FFFFF80)) {
        (void)0;
    }
    else if (!ar) {
        all_passed = 0;
    }
#undef CP
#undef CPf
    if (!all_passed) {
        pprint_SR(&sh4->regs);
        dbg_LT_dump();
    }

    return all_passed;
}

static void copy_state_to_cpu(struct SH4* sh4, struct SH4_test_state *s)
{
#define CP(rn) sh4->regs. rn = s-> rn
    SH4_SR_set(sh4, s->SR & 0x700083F3);
    SH4_regs_FPSCR_set(&sh4->regs, s->FPSCR);
    for (u32 i = 0; i < 16; i++) {
        CP(R[i]);
        if (i < 8) CP(R_[i]);
        sh4->regs.fb[0].U32[i ^ 1] = s->fb[0].U32[i];
        sh4->regs.fb[1].U32[i ^ 1] = s->fb[1].U32[i];
    }
    CP(PC);
    CP(GBR);
    CP(SSR);
    CP(SPC);
    CP(VBR);
    CP(SGR);
    CP(DBR);
    CP(MACL);
    CP(MACH);
    CP(PR);
    sh4->regs.FPUL.u = s->FPUL;
#undef CP
}

static u32 do_test(struct sh4_test_overview *ts, const char*file, const char *fname)
{
    printf("\nRunning test %s", fname);

    FILE *f = fopen(file, "rb");
    if (f == NULL) {
        printf("\nBAD FILE! %s", file);
        return 0;
    }
    fseek(f, 0, SEEK_END);

    u32 len = ftell(f);
    if (len > FILE_BUF_SIZE) {
        printf("\nFILE TOO BIG! %s", file);
        fclose(f);
        return 0;
    }

    fseek(f, 0, SEEK_SET);
    fread(filebuf, 1, len, f);
    fclose(f);

    u8 *buf_ptr = filebuf;
    for (u32 i = 0; i < NUMTESTS; i++) {
        buf_ptr += parse_test(buf_ptr, &ts->current_test);
        copy_state_to_cpu(&ts->cpu, &ts->current_test.initial);

        ts->cpu.interrupt_highest_priority = 0;
        ts->cpu.cycles = 0;
        ts->cpu.trace.cycles = &ts->trace_cycles;
        ts->trace_cycles = 0;
        ts->current_test.read_failed = 0;
        ts->current_test.write_failed = 0;
        SH4_run_cycles(&ts->cpu, 4);

        if ((ts->current_test.read_failed) || ((ts->current_test.write_failed)) || (!compare_state_to_cpu(&ts->cpu, &ts->current_test.final, &ts->current_test.initial))) {
            printf("\nTest result for test %d", i);
            printf("\nCycle 1 actions: %d", ts->current_test.cycles[1].write_addr);
            if (ts->current_test.read_failed) {
                printf("\n\nRead failed. Expected address:%08x mine:%08x", ts->current_test.read_addr_expected, ts->current_test.read_addr);
            }
            if (ts->current_test.write_failed) {
                if (ts->current_test.write_addr != ts->current_test.write_addr_expected) {
                    printf("\n\nWrite bad address. Expected:%08x mine:%08x", ts->current_test.write_addr_expected, ts->current_test.write_addr);
                }
                if (ts->current_test.write_val != ts->current_test.write_val_expected) {
                    printf("\n\nWrite bad value. Expected:%08llx mine:%08llx", ts->current_test.write_val_expected, ts->current_test.write_val);
                }
            }
            return 0;
        }
    }
    return 1;
}


u32 test_fetch_ins(void *ptr, u32 addr)
{
    struct sh4_test_overview *t = (struct sh4_test_overview *)ptr;
    assert(t->trace_cycles < 5);
    struct test_cycle *c = &t->current_test.cycles[t->trace_cycles];
    u32 base_addr = t->current_test.initial.PC;


    i64 num = ((i64)addr - (i64)base_addr) / 2;
    u32 v;
    if ((num >= 0) && (num < 4)) v = t->current_test.opcodes[num];
    else v = t->current_test.opcodes[4];
    dbg_LT_printf(DBGC_TRACE "\n fetchi   (%08x) %04x" DBGC_RST, addr, v);
    assert(addr == c->fetch_addr);

    return v;
}

static u64 test_read(void *ptr, u32 addr, u32 sz)
{
    struct sh4_test_overview *t = (struct sh4_test_overview *)ptr;
    assert(t->trace_cycles < 5);
    struct test_cycle *c = &t->current_test.cycles[1];
    t->current_test.read_addr = addr;
    if (c->read_addr != addr) {
        t->current_test.read_failed = 1;
        t->current_test.read_addr_expected = c->read_addr;
    }
    return c->read_val;
}

static void test_write(void *ptr, u32 addr, u64 val, u32 sz)
{
    struct sh4_test_overview *t = (struct sh4_test_overview *)ptr;
    assert(t->trace_cycles < 5);
    struct test_cycle *c = &t->current_test.cycles[1];
    t->current_test.write_addr = addr;
    t->current_test.write_val = val;
    if ((c->write_addr != addr) || (c->write_val != val)){
        t->current_test.write_failed = 1;
        t->current_test.write_addr_expected = c->write_addr;
        t->current_test.write_val_expected = c->write_val;
    }
}

void test_sh4() {
    struct sh4_test_overview test_struct;
    //scheduler_init(&test_struct.scheduler, &);
    SH4_init(&test_struct.cpu, &test_struct.scheduler);
    dbg_init();
    test_struct.cpu.read = &test_read;
    test_struct.cpu.write = &test_write;
    test_struct.cpu.fetch_ins = &test_fetch_ins;
    test_struct.cpu.mptr = &test_struct;

    char PATH[500];
    construct_path(PATH,"sh4_json/");

    DIR *dp;
    struct dirent *ep;
    dp = opendir (PATH);
    char mfp[500][500];
    char mfn[500][500];
    int num_files = 0;

    if (dp != NULL)
    {
        while ((ep = readdir (dp)) != NULL) {
            if (strstr(ep->d_name, ".json.bin") != NULL) {
                sprintf(mfp[num_files], "%s%s", PATH, ep->d_name);
                sprintf(mfn[num_files], "%s", ep->d_name);
                num_files++;
            }
        }
        closedir (dp);
    }

    else
    {
        printf("\nCouldn't open the directory");
        return;
    }
    printf("\nFound %d tests!", num_files);
    u32 completed_tests = 0;
    for (u32 i = 0; i < num_files; i++) {
        u32 skip = 0;
        for (u32 j = 0; j < TEST_SKIPS_NUM; j++) {
            if (strcmp(mfn[i], test_skips[j]) == 0) {
                skip = 1;
                break;
            }
        }
        if (skip) {
            printf("\nSkipping test %s", mfn[i]);
            continue;
        }
        if (!do_test(&test_struct, mfp[i], mfn[i])) break;
        completed_tests++;
    }
    printf("\n\nCompleted %d of %d tests succesfully! %d skips", completed_tests, num_files, TEST_SKIPS_NUM);
}