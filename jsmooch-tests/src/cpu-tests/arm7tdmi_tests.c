//
// Created by . on 12/4/24.
//
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "arm7tdmi_tests.h"
#include "helpers/multisize_memaccess.c"
#include "component/cpu/arm7tdmi/arm7tdmi.h"
#include "component/cpu/arm7tdmi/arm7tdmi_instructions.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (b) : (a))

static char *construct_path(char* w, const char* who)
{
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *tp = w;
    tp += sprintf(tp, "%s/dev/ARM7TDMI/v1/%s", homeDir, who);
    return tp;
}

#define FILE_BUF_SIZE 12 * 1024 * 1024
static char *filebuf = 0;
#define M8 1
#define M16 2
#define M32 4
#define M64 8
#define R8 cR[M8](ptr, 0); ptr += 1
#define R16 cR[M16](ptr, 0); ptr += 2
#define R32 cR[M32](ptr, 0); ptr += 4
#define R64 cR[M64](ptr, 0); ptr += 8

#define MAX_TRANSACTIONS 30

#define TEST_SKIPS_NUM 0

enum transaction_kind {
    IK_read_ins,
    IK_read,
    IK_write
};

struct transaction {
    enum transaction_kind kind;
    u32 size;
    u32 addr;
    u32 data;
    u32 cycle;
    u32 access;

    u32 visited;
};

struct arm7_test_transactions {
    u32 num;
    struct transaction items[MAX_TRANSACTIONS];
};

struct arm7_test_state {
    // R0-15
    u32 R[16];
    u32 R_fiq[7];
    u32 R_svc[2];
    u32 R_abt[2];
    u32 R_irq[2];
    u32 R_und[2];

    u32 SPSR_fiq, SPSR_svc, SPSR_abt, SPSR_irq, SPSR_und;
    u32 CPSR;
    u32 pipeline[2];
};

struct arm7_test {
    char name[100];
    struct arm7_test_state initial;
    struct arm7_test_state final;
    struct arm7_test_transactions transactions;
    u32 num_cycles;
    u32 opcodes[5];
    u32 base_addr;

    u32 failed;
    u32 current_cycle;
};

struct arm7_test_struct {
    struct ARM7TDMI cpu;
    struct arm7_test test;
    struct arm7_test_transactions my_transactions;

    u64 trace_cycles;
};

static u8* load_state(struct arm7_test_state *state, u8 *ptr, u32 initial)
{
    u8 *start = ptr;
    u32 state_sz = R32;
    u32 kind = R32;
    if (initial) {
        assert(kind == 1);
    }
    else {
        assert(kind == 2);
    }
    u32 i;
    for (i = 0; i < 16; i++) {
        state->R[i] = R32;
    }

    for (i = 0; i < 7; i++) {
        state->R_fiq[i] = R32;
    }
    for (i = 0; i < 2; i++) {
        state->R_svc[i] = R32;
    }
    for (i = 0; i < 2; i++) {
        state->R_abt[i] = R32;
    }
    for (i = 0; i < 2; i++) {
        state->R_irq[i] = R32;
    }
    for (i = 0; i < 2; i++) {
        state->R_und[i] = R32;
    }

    state->CPSR = R32;
    //  fiq, svc, abt, irq, und
    state->SPSR_fiq = R32;
    state->SPSR_svc = R32;
    state->SPSR_abt = R32;
    state->SPSR_irq = R32;
    state->SPSR_und = R32;
    state->pipeline[0] = R32;
    state->pipeline[1] = R32;
    assert((start+state_sz)==ptr);
    return ptr;
}

static u8* load_transactions(struct arm7_test_transactions *trs, u8 *ptr)
{
    u8 *start = ptr;
    u32 full_sz = R32;
    u32 mn = R32;
    u32 num_transactions = R32;
    assert(mn == 3);
    trs->num = 0;
    for(u32 i = 0; i < num_transactions; i++) {
        struct transaction *t = &trs->items[trs->num];
        trs->num++;
        t->kind = R32;
        assert(t->kind < 4);
        t->size = R32;
        t->addr = R32;
        t->data = R32;
        t->cycle = R32;
        t->access = R32;
        t->visited = 0;
    }
    assert((start+full_sz)==ptr);
    return ptr;
}

static u8* load_opcodes(struct arm7_test *ts, u8 *ptr)
{
    u8 *start = ptr;
    u32 full_sz = R32;

    u32 mn = R32;
    assert(mn == 4);

    for (u32 i = 0; i < 5; i++) {
        ts->opcodes[i] = R32;
    }

    ts->base_addr = R32;
    assert((start+full_sz)==ptr);
    return ptr;
}

static u8* decode_test(struct arm7_test *test, u8 *ptr)
{
    u8 *start = ptr;
    u32 test_size = R32;
    ptr = load_state(&test->initial, ptr, 1);
    ptr = load_state(&test->final, ptr, 0);
    ptr = load_transactions(&test->transactions, ptr);
    ptr = load_opcodes(test, ptr);

    assert((start+test_size)==ptr);
    return ptr;
}


static u32 fetchins_test_cpu(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct arm7_test_struct *ts = (struct arm7_test_struct *)ptr;
    i64 v = -1;
    if (addr >= ts->test.base_addr) {
        if ((addr - ts->test.base_addr) <= 12) {
            u32 diff =  addr - ts->test.base_addr;
            if (sz == 4)
                v = ts->test.opcodes[diff >> 2];
            else
                v = ts->test.opcodes[diff >> 1];
        }
    }
    if (v == -1) v = ts->test.opcodes[3];

    struct transaction *myt = &ts->my_transactions.items[ts->my_transactions.num++];
    myt->kind = 0;
    myt->cycle = ts->trace_cycles;
    myt->addr = addr;
    myt->data = (u32)v;
    myt->size = sz;
    myt->access = access;
    myt->visited = 0;

    struct transaction *theirt = NULL;
    for (u32 i = 0; i < ts->test.transactions.num; i++) {
        struct transaction *t = &ts->test.transactions.items[i];
        if (t->addr == addr) {
            theirt = t;
        }
    }
    /*if (theirt == NULL) {
        ts->test.failed = 1;
        printf("\nUH OH! CANT FIND TRANSACTION TO READ FROM!");
        return 0;
    }*/
    if (theirt) theirt->visited = 1;

    return (u32)v;
}

static u32 read_test_cpu(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct transaction *theirt = NULL;

    if (has_effect) {
        struct arm7_test_struct *ts = (struct arm7_test_struct *) ptr;
        struct transaction *myt = &ts->my_transactions.items[ts->my_transactions.num++];
        myt->kind = 1;
        myt->cycle = ts->trace_cycles;
        myt->addr = addr;
        myt->size = sz;
        myt->access = access;
        myt->visited = 0;

        for (u32 i = 0; i < ts->test.transactions.num; i++) {
            struct transaction *t = &ts->test.transactions.items[i];
            if (t->addr == addr) {
                if (theirt) printf("\nOOPS2 MULTIPLE TRANSACTIONS FOUND!");
                theirt = t;
            }
        }
        if (theirt == NULL) {
            ts->test.failed = 1;
            printf("\nUH OH! CANT FIND TRANSACTION TO READ FROM!");
            return 0;
        }
        myt->data = theirt->data;
        theirt->visited = 1;
    }
    if (theirt != NULL) {
        return theirt->data;
    }
    printf("\nBAD READ NOT FOUND! %08x", addr);
    return 0;
}

static void write_test_cpu(void *ptr, u32 addr, u32 sz, u32 val, u32 access)
{
    struct arm7_test_struct *ts = (struct arm7_test_struct *)ptr;
    struct transaction *myt = &ts->my_transactions.items[ts->my_transactions.num++];
    printf("\nWRITE ADDR:%08x VAL:%08x", addr, val);
    myt->kind = 2;
    myt->cycle = ts->trace_cycles;
    myt->addr = addr;
    myt->data = val;
    myt->size = sz;
    myt->access = access;
    myt->visited = 0;
}

static u32 do_test_read_trace(void *ptr, u32 addr, u32 sz) {
    return read_test_cpu(ptr, addr, sz, 0, 0);
}

#define NUMTESTS 20000

static void copy_state_to_cpu(struct ARM7TDMI* cpu, struct arm7_test_state *ts)
{
    for (u32 i = 0; i < 16; i++) {
        cpu->regs.R[i] = ts->R[i];
        if (i < 7) cpu->regs.R_fiq[i] = ts->R_fiq[i];
        if (i < 2) {
            cpu->regs.R_abt[i] = ts->R_abt[i];
            cpu->regs.R_irq[i] = ts->R_irq[i];
            cpu->regs.R_svc[i] = ts->R_svc[i];
            cpu->regs.R_und[i] = ts->R_und[i];
            cpu->pipeline.opcode[i] = ts->pipeline[i];
        }
        cpu->regs.SPSR_abt = ts->SPSR_abt;
        cpu->regs.SPSR_fiq = ts->SPSR_fiq;
        cpu->regs.SPSR_irq = ts->SPSR_irq;
        cpu->regs.SPSR_svc = ts->SPSR_svc;
        cpu->regs.SPSR_und = ts->SPSR_und;
        cpu->regs.CPSR.u = ts->CPSR;
    }
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

static u32 compare_state_to_cpu(struct arm7_test_struct *ts, struct arm7_test_state *final, struct arm7_test_state *initial)
{
    u32 all_passed = 1;
#define CP(rn, rn2, rname) all_passed &= cval(ts->cpu.regs. rn, final-> rn2, initial-> rn2, "%08llx", rname);
    CP(R[0], R[0], "r0");
    CP(R[1], R[1], "r1");
    CP(R[2], R[2], "r2");
    CP(R[3], R[3], "r3");
    CP(R[4], R[4], "r4");
    CP(R[5], R[5], "r5");
    CP(R[6], R[6], "r6");
    CP(R[7], R[7], "r7");
    CP(R[8], R[8], "r8");
    CP(R[9], R[9], "r9");
    CP(R[10], R[10], "r10");
    CP(R[11], R[11], "r11");
    CP(R[12], R[12], "r12");
    CP(R[13], R[13], "r13");
    CP(R[14], R[14], "r14");
    CP(R[15], R[15], "r15");
    CP(R_fiq[0], R_fiq[0], "r_fiq0");
    CP(R_fiq[1], R_fiq[1], "r_fiq1");
    CP(R_fiq[2], R_fiq[2], "r_fiq2");
    CP(R_fiq[3], R_fiq[3], "r_fiq3");
    CP(R_fiq[4], R_fiq[4], "r_fiq4");
    CP(R_fiq[5], R_fiq[5], "r_fiq5");
    CP(R_fiq[6], R_fiq[6], "r_fiq6");
    CP(R_svc[0], R_svc[0], "r_svc0");
    CP(R_svc[1], R_svc[1], "r_svc1");
    CP(R_abt[0], R_abt[0], "r_abt0");
    CP(R_abt[1], R_abt[1], "r_abt1");
    CP(R_irq[0], R_irq[0], "r_irq0");
    CP(R_irq[1], R_irq[1], "r_irq1");
    CP(R_und[0], R_und[0], "r_und0");
    CP(R_und[1], R_und[1], "r_und1");
    CP(CPSR.u, CPSR, "CPSR");
    CP(SPSR_fiq, SPSR_fiq, "SPSR_fiq");
    CP(SPSR_svc, SPSR_svc, "SPSR_svc");
    CP(SPSR_abt, SPSR_abt, "SPSR_abt");
    CP(SPSR_irq, SPSR_irq, "SPSR_irq");
    CP(SPSR_und, SPSR_und, "SPSR_und");
    all_passed &= cval(ts->cpu.pipeline.opcode[0], final->pipeline[0], initial->pipeline[0], "%08llx", "pipeline0");
    all_passed &= cval(ts->cpu.pipeline.opcode[1], final->pipeline[1], initial->pipeline[1], "%08llx", "pipeline1");
#undef CP
    return all_passed;
}

static u32 do_test(struct arm7_test_struct *ts, const char*file, const char *fname) {
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

    u8 *ptr = (u8*)filebuf;
    u32 mn, numtests;
    mn = R32;
    numtests = R32;
    assert(mn == 0xD33DBAE0);

    for (u32 i = 0; i < numtests; i++) {
        ptr = decode_test(&ts->test, ptr);

        copy_state_to_cpu(&ts->cpu, &ts->test.initial);
        printf("\n\nTEST NUM %d BASE ADDR:%08x", i, ts->test.base_addr);
        ts->test.failed = 0;
        ts->trace_cycles = 0;
        ts->cpu.testing = 1;
        ts->my_transactions.num = 0;
        if (dbg.trace_on) {
            printf("\nNUM CYCLES %d", ts->test.num_cycles);
            fflush(stdout);
        }

        ARM7TDMI_cycle(&ts->cpu, 1);

        for (u32 j = 0; j < ts->test.transactions.num; j++) {
            struct transaction *t = &ts->test.transactions.items[j];
            if ((t->visited == 0)) {
                printf("\nWARNING UNVISITED TRANSACTION kind:%c addr:%08x sz:%d val:%08x", t->kind == 0 ? 'i' : (t->kind == 1 ? 'r' : 'w'), t->addr, t->size, t->data);
                ts->test.failed = 4;
            }
        }

        if ((!compare_state_to_cpu(ts, &ts->test.final, &ts->test.initial))) {//|| (!compare_state_to_ram(ts)) || ts->test.failed) {
            printf("\nTest failed...");
            return 0;
        }
    }

    return 1;
}

void test_arm7tdmi()
{
    struct jsm_string yo;
    jsm_string_init(&yo, 50);
    struct jsm_debug_read_trace rt;
    rt.read_trace_arm = &do_test_read_trace;

    struct arm7_test_struct ts;
    rt.ptr = &ts;

    memset(&ts, 0, sizeof(ts));
    ARM7TDMI_init(&ts.cpu);
    ARM7TDMI_setup_tracing(&ts.cpu, &rt, &ts.trace_cycles);
    ts.cpu.read = &read_test_cpu;
    ts.cpu.write = &write_test_cpu;
    ts.cpu.fetch_ins = &fetchins_test_cpu;
    ts.cpu.read_ptr = ts.cpu.write_ptr = ts.cpu.fetch_ptr = &ts;
    dbg_init();
    dbg_disable_trace();

    char PATH[500];
    construct_path(PATH, "");

    DIR *dp;
    struct dirent *ep;
    dp = opendir(PATH);
    char mfp[500][500];
    char mfn[500][500];
    int num_files = 0;

    if (dp != NULL) {
        while ((ep = readdir (dp)) != NULL) {
            if (strstr(ep->d_name, ".json.bin") != NULL) {
                sprintf(mfp[num_files], "%s/%s", PATH, ep->d_name);
                sprintf(mfn[num_files], "%s", ep->d_name);
                printf("\n%s", ep->d_name);
                num_files++;
            }
        }
        closedir(dp);
    }
    else {
        printf("\nCouldn't open the directory");
        return;
    }
    printf("\nFound %d tests!", num_files);

    u32 completed_tests = 0;
    u32 nn = 20;
    u32 test_start = 15;
    filebuf = malloc(FILE_BUF_SIZE);
    for (u32 i = test_start; i < num_files; i++) {
        u32 skip = 0;
        printf("\nDoing test %s", mfn[i]);
        if (!do_test(&ts, mfp[i], mfn[i])) break;
        if (completed_tests > nn) break;
        if (completed_tests >= nn) dbg_enable_trace();
        completed_tests++;
    }
    dbg_flush();
    printf("\n\nCompleted %d out of %d test succesfully! %d skips", completed_tests, num_files - TEST_SKIPS_NUM, TEST_SKIPS_NUM);


   free(filebuf);
}