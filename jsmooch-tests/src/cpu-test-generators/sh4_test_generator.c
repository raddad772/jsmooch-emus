//
// Created by RadDad772 on 4/9/24.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <assert.h>
#include "myrandom.h"
#include "component/cpu/sh4/sh4_interpreter.h"
#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "helpers/cvec.h"
#include "helpers/int.h"
#include "helpers/multisize_memaccess.c"

static void cpSH4(u32 dest, u32 src) {
    /*memcpy(&SH4_decoded[dest], &SH4_decoded[src], 65536*sizeof(struct SH4_ins_t));
    memcpy(&SH4_disassembled[dest][0], &SH4_disassembled[src][0], 65536*30);
    memcpy(&SH4_mnemonic[dest][0], &SH4_mnemonic[src][0], 65535*30);*/
}

struct mem_pair {
    u32 addr;
    u32 val;
    u32 rw;
};

struct SH4_tester{
    struct SH4 cpu;
} ;

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

#define NUMTESTS 10

#define TCA_READ 1
#define TCA_WRITE 2
#define TCA_FETCHINS 4

struct test_cycle {
    u32 actions;

    i64 read_addr;
    u64 read_val;

    i64 write_addr;
    u64 write_val;

    i64 fetch_addr;
    u64 fetch_val;
};

static void clear_test_cycle(struct test_cycle* this)
{
    this->actions = 0;
    this->read_addr = this->write_addr = this->fetch_addr = -1;
    this->read_val = this->write_val = this->fetch_val = 0;
}

struct sh4test {
    struct SH4_test_state initial;
    struct SH4_test_state final;
    u16 opcodes[5]; // 0-3 are the flow, #5 is for after the jump
    u32 test_base;
    struct test_cycle cycles[4];
};

struct sh4test_array {
    struct SH4_ins_t *ins;
    char fname[40];
    u32 sz, pr;
    char encoding[17];
    struct sh4test tests[NUMTESTS];
};

// 237 encodings with SZ=0 PR=0
//  15 encodings with PR=1 SZ=1
//  10 encodings with PR=1 SZ=0, which are copied into PR=1 SZ=1 also
//=262 encodings
static struct sh4test_array sh4tests[262];

static u32 encoding_to_mask(const char* encoding_str, u32 m, u32 n, u32 d, u32 i)
{
    struct sh4_str_ret r;
    process_SH4_instruct(&r, encoding_str);
    m = (m & r.m_mask) << r.m_shift;
    n = (n & r.n_mask) << r.n_shift;
    d = (d & r.d_mask) << r.d_shift;
    i = (i & r.i_mask) << r.i_shift;

    u32 bit = 1 << 15;
    u32 out = 0;
    for (u32 l = 0; i < 16; i++) {
        if (encoding_str[l] == '1')
            out |= bit;
        bit >>= 1;
    }

    if (r.m_max) out |= (m << r.m_shift);

    out |= m | n | d | i;

    return out;
}

static u32 valid_rnd_SR(struct sfc32_state *rstate)
{
    u32 SR = sfc32(rstate) & 0b1110000000000001000001111110011;
    return SR;
}

static u32 valid_rnd_FPSCR(struct sfc32_state *rstate, u32 sz, u32 pr)
{
    u32 FPSCR = sfc32(rstate) & 0b1001000000000000000011;
    FPSCR |= (sz << 20) | (pr << 19);
    return FPSCR;
}

static void random_initial(struct SH4_test_state *ts, struct sfc32_state *rstate, u32 sz, u32 pr)
{
    for (u32 i = 0; i < 16; i++) {
        ts->R[i] = sfc32(rstate);
        if (i < 8) ts->R[i] = sfc32(rstate);
        ts->fb[0].U32[i] = sfc32(rstate);
        ts->fb[1].U32[i] = sfc32(rstate);
    }
#define sR(w) ts-> w = sfc32(rstate)
    sR(PC);
    sR(GBR);
    sR(SR);
    sR(SSR);
    sR(SPC);
    sR(VBR);
    sR(SGR);
    sR(DBR);
    sR(MACL);
    sR(MACH);
    sR(PR);
    ts->SR = valid_rnd_SR(rstate);
    ts->FPSCR = valid_rnd_FPSCR(rstate, sz, pr);
#undef sR
}

static u32 randomize_opcode(const char* encoding_str, struct sfc32_state *rstate)
{
    u32 m = sfc32(rstate), n = sfc32(rstate), d = sfc32(rstate), i = sfc32(rstate);
    return encoding_to_mask(encoding_str, m, n, d, i);
}

static void copy_state_to_cpu(struct SH4_test_state *st, struct SH4 *sh4)
{
    SH4_SR_set(sh4, st->SR);
    SH4_regs_FPSCR_set(&sh4->regs, st->FPSCR);

#define CP(rn) sh4->regs. rn = st->rn
    for (u32 i = 0; i < 16; i++) {
        CP(R[i]);
        if (i < 8) CP(R_[i]);
        CP(fb[0].U32[i]);
        CP(fb[1].U32[i]);
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
#undef CP
}

static void copy_state_from_cpu(struct SH4_test_state *st, struct SH4 *sh4)
{
    st->SR = SH4_regs_SR_get(sh4);
    st->FPSCR = SH4_regs_FPSCR_get(&sh4->regs.FPSCR);

#define CP(rn) st-> rn = sh4->regs. rn
    for (u32 i = 0; i < 16; i++) {
        CP(R[i]);
        if (i < 8) CP(R_[i]);
        CP(fb[0].U32[i]);
        CP(fb[1].U32[i]);
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
#undef CP
}

struct generating_test_struct {
    struct SH4_tester *tester;
    struct sh4test *test;
    struct sfc32_state *rstate;

    i64 read_addrs[7];
    u64 read_values[7];
    i64 read_sizes[7];
    u64 read_cycles[7];

    i64 write_addr;
    u64 write_value;
    i64 write_size;
    u64 write_cycle;

    i64 ifetch_addr;
    u32 ifetch_data;

    u32 read_num;
};

static u32 test_fetch_ins(void *ptr, u32 addr)
{
    struct generating_test_struct *this = (struct generating_test_struct*)ptr;
    this->ifetch_addr = addr;
    i64 num = (i64)addr - this->test->test_base;
    u32 v;
    if ((num >= 0) && (num < 4)) v = this->test->opcodes[num];
    else v = this->test->opcodes[4];
    this->ifetch_data = v;
    return v;
}

static u64 test_read(void *ptr, u32 addr, u32 sz)
{
    struct generating_test_struct *this = (struct generating_test_struct*)ptr;
    assert(this->read_num < 7);
    u64 v = (u64)sfc32(this->rstate) | ((u64)sfc32(this->rstate) << 32);

    switch(sz) {
        case 1:
            v &= 0xFF;
            break;
        case 2:
            v &= 0xFFFF;
            break;
        case 4:
            v &= 0xFFFFFFFF;
            break;
        case 8:
            break;
        default:
            assert(1==0);
            return 0;
    }
    this->read_addrs[this->read_num] = addr;
    this->read_sizes[this->read_num] = sz;
    this->read_values[this->read_num] = v;
    this->read_cycles[this->read_num] = this->tester->cpu.clock.trace_cycles;
    this->read_num++;
    return v;
}

static void test_write(void *ptr, u32 addr, u64 val, u32 sz)
{
    struct generating_test_struct *this = (struct generating_test_struct*)ptr;
    assert(this->write_addr==-1);
    this->write_addr = addr;
    this->write_value = val;
    this->write_size = sz;
    this->write_cycle = this->tester->cpu.clock.trace_cycles;
}

void construct_path(char* w, const char* who)
{
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *tp = w;
    tp += sprintf(tp, "%s/dev/%s", homeDir, who);
}

#define TB_NONE 0
#define TB_INITIAL_STATE 1
#define TB_FINAL_STATE 2
#define TB_CYCLES 3
#define TB_OPCODES 4

#define M8 1
#define M16 2
#define M32 4
#define M64 8

static u32 write_cycles(u8 *where, const struct sh4test *test)
{
    cW[M32](where, 4, TB_CYCLES);
    cW[M32](where, 8, 4);
    u32 r = 12;
#define W32(v) cW[M32](where, r, v); r += 4
#define W64(v) cW[M32](where, r, (u64)v); r += 8
    for (u32 cycle = 0; cycle < 4; cycle++) {
        const struct test_cycle *c = &test->cycles[cycle];
        W32(c->actions);

        W64(c->fetch_addr);
        W64(c->fetch_val);

        W64(c->write_addr);
        W64(c->write_val);

        W64(c->read_addr);
        W64(c->read_val);
    }
    cW[M32](where, 0, r);
#undef W32
#undef W64
    return r;
}

static u32 write_state(u8* where, struct SH4_test_state *state, u32 is_final)
{
    cW[M32](where, 4, is_final ? TB_FINAL_STATE : TB_INITIAL_STATE);

    u32 r = 8;
    // R0-R15 offset 8
#define W32(val) cW[M32](where, r, state-> val); r += 4
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
#undef W32
    // Write size of block
    cW[M32](where, 0, r);
    return r;
}

static u32 write_opcodes(u8* where, struct sh4test *test)
{
    cW[M32](where, 4, TB_OPCODES);
    u32 r = 8;
#define W32(val) cW[M32](where, r, val); r += 4
    W32((u32)test->opcodes[0]);
    W32((u32)test->opcodes[1]);
    W32((u32)test->opcodes[2]);
    W32((u32)test->opcodes[3]);
    W32((u32)test->opcodes[4]);
#undef W32
    cW[M32](where, 0, r);
    return r;
}

static u8 outbuf[64 * 1024];

static void write_tests(struct sh4test_array *ta)
{
    char fpath[250];
    char rp[250];
    sprintf(rp, "sh4_json/%s", ta->fname);
    construct_path(fpath, rp);
    remove(fpath);

    FILE *f = fopen(fpath, "wb");

    u32 outbuf_idx = 0;
    /*
     u32 size
       u32 size
       u32 kind
       ...,

       u32 size
       u32 kind
       ...,
     */

    for (u32 tnum = 0; tnum < NUMTESTS; tnum++) {
        struct sh4test *t = &ta->tests[tnum];
        // Write out initial state, final state
        u32 outbuf_start = outbuf_idx;
        outbuf_idx += 4;
        outbuf_idx += write_state(&outbuf[outbuf_idx], &t->initial, 0); // 272 bytes
        outbuf_idx += write_state(&outbuf[outbuf_idx], &t->final, 1); // 272 bytes
        outbuf_idx += write_cycles(&outbuf[outbuf_idx], t);
        outbuf_idx += write_opcodes(&outbuf[outbuf_idx], t);
        cW[M32](outbuf, outbuf_start, outbuf_idx - outbuf_start);
    }

    fclose(f);
}

static void generate_test_struct(const char* encoding_str, const char* mnemonic, u32 sz, u32 pr, u32 num, struct SH4_tester *t)
{
    struct sh4test_array *ta = &sh4tests[num];
    snprintf(ta->fname, sizeof(ta->fname), "%s_sz%d_pr%d.json", encoding_str, sz, pr);
    snprintf(ta->encoding, sizeof(ta->encoding), "%s", encoding_str);

    ta->sz = sz;
    ta->pr = pr;
    struct sfc32_state rstate;
    sfc32_seed(ta->fname, &rstate);

    struct generating_test_struct test_struct;

    test_struct.rstate = &rstate;
    test_struct.tester = t;

    t->cpu.fetch_ins = &test_fetch_ins;
    t->cpu.read = &test_read;
    t->cpu.write = &test_write;
    t->cpu.mptr = &test_struct;

    for (u32 i = 0; i < NUMTESTS; i++) {
        struct sh4test *tst = &ta->tests[i];
        random_initial(&tst->initial, &rstate, sz, pr);
        tst->test_base = tst->initial.PC;
        tst->opcodes[0] = 9; // NOP
        tst->opcodes[1] = randomize_opcode(encoding_str, &rstate); // opcode we're testing
        tst->opcodes[2] = 0b0110000100010011; // add R1, R1. So R1 += R1
        tst->opcodes[3] = 9; // NOP again
        tst->opcodes[4] = 0b0110001000100011; // add R2, R2. so R2 += R2. for after a jump
        copy_state_to_cpu(&tst->initial, &t->cpu);

        // Make sure our CPU isn't interrupted...
        t->cpu.interrupt_highest_priority = 0;
        t->cpu.cycles = 0;
        t->cpu.clock.trace_cycles = 0;

        // Zero our test struct
        test_struct.test = tst;
        test_struct.read_num = 0;
        test_struct.write_addr = test_struct.write_size = -1;
        test_struct.write_value = -1;
        test_struct.write_cycle = 0;
        test_struct.ifetch_addr = -1;
        test_struct.ifetch_data = 65536;
        for (u32 j = 0; j < 7; j++) {
            test_struct.read_addrs[j] = -1;
            test_struct.read_sizes[j] = -1;
            test_struct.read_values[j] = 0;
            test_struct.read_cycles[j] = 0;
        }

        // Run CPU 4 cycles
        // Our amazeballs CPU can run 2-for-1 so do it special!
        // ONLY opcode 1 could have a delay slot so all should finish
        SH4_run_cycles(&t->cpu, 4);

        // Now fill out rest of test
        copy_state_from_cpu(&tst->final, &t->cpu);

        for (u32 cycle = 0; cycle < 4; cycle++) {
            struct test_cycle *c = &tst->cycles[cycle];
            clear_test_cycle(c);
            if ((test_struct.write_addr != -1) && (test_struct.write_cycle == cycle)) {
                c->actions |= TCA_WRITE;
                c->write_addr = test_struct.write_addr;
                c->write_val = test_struct.write_value;
            }
            for (u32 j = 0; j < 7; j++) {
                if ((test_struct.read_addrs[j] != -1) && (test_struct.read_cycles[j] == cycle)) {
                    assert((c->actions & TCA_READ) == 0);
                    c->actions |= TCA_READ;
                    c->read_addr = test_struct.read_addrs[j];
                    c->read_val = test_struct.read_values[j];
                }
            }
            c->actions |= TCA_FETCHINS;
        }

        // Now dump to buffer/disk
    }
    write_tests(ta);
}

static void fill_sh4_encodings(void)
{
    for (u32 szpr = 0; szpr < 4; szpr++) {
        for (u32 i = 0; i < 65536; i++) {
            SH4_decoded[szpr][i] = (struct SH4_ins_t) {
                    .opcode = i,
                    .Rn = -1,
                    .Rm = -1,
                    .imm = 0,
                    .disp = 0,
                    .exec = NULL,
                    .decoded = 0
            };
        }
    }

    u32 r = 0;
#define OE(opcstr, func, mn) generate_test_struct(opcstr, mn, 0, 0, r, &t); r++
#define OEo(opcstr, func, mn, sz, pr) (void)0/*generate_test_struct(opcstr, func, mn, 1, ((sz<<1) | pr))*/

#include "component/cpu/sh4/sh4_decodings.c"

#undef OE
#undef OEo
}

void test_sh4()
{
    struct SH4_tester t;
    SH4_init(&t.cpu, NULL);

    fill_sh4_encodings();

    /* Gotta fill these out
    void *mptr;
    u32 (*fetch_ins)(void*,u32);
    u64 (*read)(void*,u32,u32);
    void (*write)(void*,u32,u64,u32);
    */

}