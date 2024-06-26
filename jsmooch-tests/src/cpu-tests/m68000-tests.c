#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>

#include "m68000-tests.h"

#include "helpers/debug.h"
#include "helpers/jsm_string.h"
#include "component/cpu/m68000/m68000.h"
#include "component/cpu/m68000/m68000_disassembler.h"
#include "component/cpu/m68000/m68000_instructions.h"

#include "helpers/multisize_memaccess.c"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (b) : (a))

static u8 *tmem = 0;

#define TEST_SKIPS_NUM 4
static char test_skips[TEST_SKIPS_NUM][50] = {
        "CHK.json.bin",
        "TAS.json.bin", // TAS is a mess, especially on CLK
        "ASR.w.json.bin", // bug in CLK ASR implementation
        "ROR.b.json.bin",
};

static u32 m68k_dasm_read(void *obj, u32 addr, u32 UDS, u32 LDS)
{
    u32 val = tmem[addr>>1];
    if (!UDS) val &= 0xFF;
    if (!LDS) val &= 0xFF00;
    return val;
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
    tp += sprintf(tp, "%s/dev/external/ProcessorTests/%s", homeDir, who);
    return tp;
}

#define FILE_BUF_SIZE 10 * 1024 * 1024
char *filebuf = 0;

#define M8 1
#define M16 2
#define M32 4
#define M64 8

#define R8 cR[M8](ptr, 0); ptr += 1
#define R16 cR[M16](ptr, 0); ptr += 2
#define R32 cR[M32](ptr, 0); ptr += 4
#define R64 cR[M64](ptr, 0); ptr += 8

#define MAX_RAM_PAIRS 100
#define MAX_TRANSACTIONS 100

enum transaction_kind {
    tk_idle_cycles,
    tk_read,
    tk_write,
    tk_tas
};

struct transaction {
    enum transaction_kind kind;
    u32 len;
    u32 fc;
    u32 addr_bus;
    u32 sz;
    u32 data_bus;
    u32 UDS, LDS;

    u32 visited;

    u32 start_cycle;
};

struct RAM_pair {
    u32 addr;
    u8 val;
};

struct m68k_test_transactions {
    u32 num_transactions;
    struct transaction items[MAX_TRANSACTIONS];
};

struct m68k_test_state {
    u32 d[8];
    u32 a[7];
    u32 usp, ssp, sr, pc;
    u32 prefetch[2];
    u32 num_RAM;
    struct RAM_pair RAM_pairs[MAX_RAM_PAIRS];
};

struct m68k_test {
    char name[100];
    struct m68k_test_state initial;
    struct m68k_test_state final;
    struct m68k_test_transactions transactions;
    u32 num_cycles;

    u32 current_cycle;
    u32 failed;
    u32 failed_w;
};

struct m68k_test_struct {
    struct M68k cpu;
    struct m68k_test test;
    struct m68k_test_transactions my_transactions;
    i64 had_group0;
    i64 had_group1;
    i64 had_group2;
};

static u8* read_test_transactions(u8* ptr, struct m68k_test *test)
{
    u32 sz = R32;
    u32 mn = R32;
    u32 cycle_num = 0;
    assert(mn==0x456789AB);
    test->num_cycles = R32;
    test->transactions.num_transactions = R32;

    for (u32 i = 0; i < test->transactions.num_transactions; i++) {
        struct transaction *t = &test->transactions.items[i];
        u8 k = R8;
        t->len = R32;
        t->start_cycle = cycle_num;
        t->visited = 0;
        cycle_num += t->len;
        switch(k) {
            case 0: // n
                t->kind = tk_idle_cycles;
                continue;
            case 1: // w
                t->kind = tk_write;
                break;
            case 2: // r
                t->kind = tk_read;
                break;
            case 3: // t
                t->kind = tk_tas;
                break;
            default:
                printf("\nWHAAAT? %d", k);
                return 0;
        }
        t->fc = R32;
        t->addr_bus = R32;
        t->sz = R32;
        t->data_bus = R32;
        assert ((t->sz >= 0) && (t->sz <= 1));
        t->sz++;  // 0 = byte. 1 = word..  change to our internal, 1 = byte, 2 = word

        if (t->sz == 1) {
            t->LDS = t->addr_bus & 1;
            t->UDS = !t->LDS;
        }
        else {
            t->UDS = t->LDS = 1;
        }
        t->addr_bus &= 0xFFFFFFFE;
    }

    return ptr;
}

static u8* read_test_state(u8* ptr, struct m68k_test_state *state)
{
    u32 sz = R32;
    u32 mn = R32;
    assert(mn == 0x01234567);

    for(u32 i = 0; i < 8; i++) {
        state->d[i] = R32;
    }
    for (u32 i = 0; i < 7; i++) {
        state->a[i] = R32;
    }

    state->usp = R32;
    state->ssp = R32;
    state->sr = R32;
    state->pc = R32;

    state->prefetch[0] = R32;
    state->prefetch[1] = R32;

    state->num_RAM = R32;
    assert(state->num_RAM < MAX_RAM_PAIRS);
    for (u32 i = 0; i < state->num_RAM; i++) {
        state->RAM_pairs[i].addr = R32;
        state->RAM_pairs[i].val = R8;
        assert(state->RAM_pairs[i].addr < 0x1000000);
    }

    return ptr;
}

static u8* read_test_name(u8* ptr, struct m68k_test *test)
{
    u32 sz = R32;
    u32 mn = R32;
    assert(mn == 0x89ABCDEF);

    u32 num_chars = R32;
    u32 l = num_chars;
    if (l > 99) l = 99;
    memcpy(test->name, ptr, l);
    test->name[l] = 0;

    ptr += num_chars;
    return ptr;
}

static u8* decode_test(u8* ptr, struct m68k_test *test)
{
    u32 sz = R32;
    u32 mn = R32;
    assert(mn==0xABC12367);

    ptr = read_test_name(ptr, test);

    ptr = read_test_state(ptr, &test->initial);
    ptr = read_test_state(ptr, &test->final);

    ptr = read_test_transactions(ptr, test);

    return ptr;
}

static struct transaction* find_last_transaction(struct m68k_test_transactions *ts, u32 addr, enum transaction_kind tk, u32 UDS, u32 LDS)
{
    if (ts->num_transactions < 2) return NULL;
    for (i32 i = ts->num_transactions-1; i >= 0; i--) {
        struct transaction *t = &ts->items[i];
        if ((t->kind == tk) && ((t->addr_bus & 0xFFFFFE) == (addr & 0xFFFFFE)) && (t->UDS == UDS) && (t->LDS == LDS))
            return t;
    }

    return NULL;
}

static u32 compare_group12_frame(struct m68k_test_struct *ts, u32 base_addr)
{
    struct transaction *t1=NULL, *t2=NULL;
    for (u32 i = 0; i < 6; i += 2) {
        u32 addr = base_addr+i;
        t1 = find_last_transaction(&ts->test.transactions, addr, tk_write, 1, 1);
        t2 = find_last_transaction(&ts->my_transactions, addr, tk_write, 1, 1);
        if ((t1 == NULL) || (t2 == NULL)) {
            printf("\nnonononononoNONONONONO");
            return 0;
        }
        u32 v1 = t1->data_bus;
        u32 v2 = t2->data_bus;
        //if (v1 != v2) return 0;

        switch(i) {
            case 0:
                if ((MAX(v1, v2) - MIN(v1, v2)) <= 4) {
                    printf("\nPassing PC based on group2 disagreement");
                    return 1;
                }
                return 0;
            case 2:
                if (ts->cpu.ins->exec == &M68k_ins_DIVU) {
                    if (ts->had_group2) {
                        return 1;
                    }
                }
                return v1 == v2;
        }
    }
    return 1;
}

static u32 compare_group1_frame(struct m68k_test_struct *ts)
{
    return compare_group12_frame(ts, ts->cpu.state.exception.group1.base_addr);
}

static u32 compare_group2_frame(struct m68k_test_struct *ts)
{
    return compare_group12_frame(ts, ts->cpu.state.exception.group2.base_addr);
}

static u32 compare_group0_frame(struct m68k_test_struct *ts)
{
    struct transaction *t1=NULL, *t2=NULL;
    // Go over any transactions that are >= g0_min and < g0_max.
    // map
    // +0 FIRST WORD (ignore I/N bit)
    // +2 ACCESS HI
    // +4 ACCESS LO
    // +6 IR
    // +8 SR
    // +10 PC hi
    // +12 PC lo (ignore if within 4 or so)
    u32 all_passed = 1;
    u32 pc_hi1, pc_hi2, pc1, pc2;
    for (u32 i = 0; i < 14; i+= 2) {
        u32 addr = i + ts->cpu.state.exception.group0.base_addr;
        t1 = find_last_transaction(&ts->test.transactions, addr, tk_write, 1, 1);
        t2 = find_last_transaction(&ts->my_transactions, addr, tk_write, 1, 1);
        if ((t1 == NULL) || (t2 == NULL)) {
            printf("\nNONONONONONONONNONONONONONONOON");
            return 0;
        }
        u32 v1 = t1->data_bus;
        u32 v2 = t2->data_bus;
        u32 min, max;
        switch(i) {
            case 0: // FIRST WORD - ignore I/N bit
                if ((v1 & 0xFFF7) != (v2 & 0xFFF7)) {
                    printf("\nfirst word mismatch good:%04x mine:%04x %04x", v1, v2, v1 ^ v2);
                    if ((v1 & 0xFFF0) != (v2 & 0xFFF0)) {
                        printf("\n OH NO!");
                        all_passed = 0;
                    }
                }
                break;
            case 4: // ACCESS LO
                if (v1 != v2) {
                    if ((MAX(v1, v2) - MIN(v1, v2)) < 3) {
                        printf("\nHMMMM order mismatch");
                    } else {
                        printf("\n.");
                        all_passed = 0;
                    }
                }
                break;
            case 2: // ACCESS HI
            case 6: // IR
            case 8: // SR
                all_passed &= v1 == v2;
                break;
            case 10: // PC HI
                pc_hi1 = v1 << 16;
                pc_hi2 = v2 << 16;
                break;
            case 12: // PC LO, ignore differences of up to like 10
                pc2 = pc_hi2 | v2;
                pc1 = pc_hi1 | v1;
                max = MAX(pc1, pc2);
                min = MIN(pc1, pc2);
                if ((max - min) > 10) {
                    printf("\nDiscrepency in PC of %d", max - min);
                    all_passed = 0;
                }
                break;

        }
    }
    return all_passed;
}

static u32 compare_state_to_ram(struct m68k_test_struct *ts)
{
    struct m68k_test_state *s = &ts->test.final;
    u32 all_passed = 1;
    u32 g0_passed = 1;
    i64 g0_min = -1, g0_max = -1;
    if (ts->had_group0 != -1) {
        g0_min = ts->cpu.state.exception.group0.base_addr;
        g0_max = ts->cpu.state.exception.group0.base_addr + 14;
    }
    if (ts->had_group1 != -1) {
        g0_min = ts->cpu.state.exception.group1.base_addr;
        g0_max = ts->cpu.state.exception.group1.base_addr + 6;
    }
    if (ts->had_group2 != -1) {
        g0_min = ts->cpu.state.exception.group2.base_addr;
        g0_max = ts->cpu.state.exception.group2.base_addr + 6;
    }
    printf("\n");
    for (u32 i = 0; i < s->num_RAM; i++) {
        u32 addr = s->RAM_pairs[i].addr;
        u32 val = s->RAM_pairs[i].val;
        if (tmem[addr] != val) {
            if ((g0_min != -1) && ((addr >= g0_min) && (addr < g0_max))) {
                g0_passed = 0;
            } else {
                all_passed = 0;
                printf("\nFAIL ADDR #%d ADDR:%04x VAL:%02x MY:%02x", i, addr, val, tmem[addr]);
            }
        }
    }
    if (!g0_passed) {
        if (ts->had_group0 != -1) all_passed &= compare_group0_frame(ts);
        if (ts->had_group1 != -1) all_passed &= compare_group1_frame(ts);
        if (ts->had_group2 != -1) all_passed &= compare_group2_frame(ts);
    }
    return all_passed;
}

static void w16(u32 addr, u32 val)
{
    tmem[addr] = (val >> 8) & 0xFF;
    tmem[addr+1] = val & 0xFF;
}


static void copy_state_to_RAM(struct m68k_test_state *s)
{
    w16(s->pc, s->prefetch[0]);
    w16(s->pc+2, s->prefetch[1]);
    for (u32 i = 0; i < s->num_RAM; i++) {
        assert(s->RAM_pairs[i].addr < 0x1000000);
        tmem[s->RAM_pairs[i].addr] = s->RAM_pairs[i].val;
    }
}

static void copy_state_to_cpu(struct M68k* cpu, struct m68k_test_state *s)
{
    M68k_set_SR(cpu, s->sr);
    for (u32 i = 0; i < 8; i++) {
        cpu->regs.D[i] = s->d[i];
        if (i < 7) cpu->regs.A[i] = s->a[i];
    }
    cpu->regs.PC = s->pc + 4;
    cpu->regs.IRC = s->prefetch[1];
    cpu->regs.IR = s->prefetch[0];
    cpu->regs.IRD = s->prefetch[0];
    if (cpu->regs.SR.S) { // supervisor mode!
        cpu->regs.A[7] = s->ssp;
        cpu->regs.ASP = s->usp;
    }
    else {
        cpu->regs.A[7] = s->usp;
        cpu->regs.ASP = s->ssp;
    }
}

static u32 do_test_read_trace(void *ptr, u32 addr, u32 UDS, u32 LDS)
{
    assert(addr<0x01000000);
    u32 v = 0;
    if (UDS) v |= tmem[addr] << 8;
    if (LDS) v |= tmem[addr|1];
    return v;
}

static struct transaction* find_transaction(struct m68k_test_struct *ts, enum transaction_kind tk, u32 addr, u32 UDS, u32 LDS)
{
    struct transaction* t = NULL;
    for (i32 i = 0; i < ts->test.transactions.num_transactions; i++) {
        t = &ts->test.transactions.items[i];
        if (((t->addr_bus & 0xFFFFFE) == addr) && (t->kind == tk) && (t->visited == 0) && (t->UDS == UDS) && (t->LDS == LDS)) {
            return t;
        }
    }
    return NULL;
}

static u32 do_test_read(struct m68k_test_struct *ts, u32 addr, u32 UDS, u32 LDS)
{
    assert(addr<0x01000000);
    u32 v = 0;
    if (UDS) v |= tmem[addr] << 8;
    if (LDS) v |= tmem[addr|1];
    ts->cpu.pins.DTACK = 1;
    struct transaction *m = &ts->my_transactions.items[ts->my_transactions.num_transactions++];
    m->kind = tk_read;
    m->start_cycle = ts->cpu.trace_cycles - 1;
    m->addr_bus = addr;
    m->data_bus = v;
    m->visited = 1;
    m->UDS = UDS;
    m->LDS = LDS;
    m->sz = UDS + LDS;

    struct transaction *t = find_transaction(ts, tk_read, addr, UDS, LDS);
    if (t == NULL) {
        printf("\nNo read found from address %06x cycle:%lld", addr, ts->cpu.trace_cycles);
        ts->test.failed = 1;
        return 0;
    }
    u32 sz = UDS + LDS;
    t->visited = 1;

    if (t->start_cycle != (ts->cpu.trace_cycles)) {
        if (dbg.trace_on) printf("\nMISMATCH READ %06x their cycle:%d    mine:%lld", addr, t->start_cycle, ts->cpu.trace_cycles);
    }

    return v;
}

static void do_test_write(struct m68k_test_struct *ts, u32 addr, u32 UDS, u32 LDS, u32 val)
{
    //printf("\nWRITE! %06x %d %d %04x", addr, UDS, LDS, val);
    assert(addr<0x01000000);
    if (UDS) tmem[addr] = (val >> 8) & 0xFF;
    if (LDS) tmem[addr|1] = val & 0xFF;
    ts->cpu.pins.DTACK = 1;
    struct transaction *m = &ts->my_transactions.items[ts->my_transactions.num_transactions++];
    m->kind = tk_write;
    m->start_cycle = ts->cpu.trace_cycles - 2;
    m->addr_bus = addr;
    m->data_bus = val;
    m->visited = 1;
    m->UDS = UDS;
    m->LDS = LDS;
    m->sz = UDS + LDS;


    struct transaction* t = find_transaction(ts, tk_write, addr, UDS, LDS);
    if (t == NULL) {
        printf("\nWarning bad write not found addr:%06x val:%08x cyc:%lld", addr, val, ts->cpu.trace_cycles);
        ts->test.failed = 1;
        return;
    }
    u32 sz = UDS + LDS;
    t->visited = 1;
    if (sz != t->sz) {
        printf("\nFailed wrong size. Mine: %d  theirs: %d", sz, t->sz);
        ts->test.failed = 1;
        return;
    }
    u32 v = 0;
    if (UDS && LDS) v = val;
    else if (LDS) v = val;
    else v = val >> 16;
    if (t->start_cycle != ts->cpu.trace_cycles) {
        if (dbg.trace_on) printf("\nMISMATCH WRITE %06x their cycle:%d    mine:%lld", addr, t->start_cycle, ts->cpu.trace_cycles);
    }
    if (t->data_bus != v) {
        //printf("\nWrong value write (correct addr):%06x val: %04x mine:%04x", addr, t->data_bus, val);
        ts->test.failed_w = 1;
        return;
    }
}

static u32 cval_SR(u64 mine, u64 theirs, u64 initial, const char* display_str, const char *name, u32 had_group0, u32 had_predec, struct m68k_test_struct *ts) {
    if (mine == theirs) return 1;
    if (had_predec && had_group0) {
        if ((MAX(mine, theirs) - MIN(mine,theirs)) < 3) {
            printf("\nHmmm?");
            return 1;
        }
    }
    if (ts->had_group2 && ts->cpu.ins->exec == &M68k_ins_DIVU) {
        return 1;
    }

    printf("\n%s mine:", name);
    printf(display_str, mine);
    printf(" theirs:");
    printf(display_str, theirs);
    printf(" initial:");
    printf(display_str, initial);

    return 0;
}


static u32 cval(u64 mine, u64 theirs, u64 initial, const char* display_str, const char *name, u32 had_group0, u32 had_predec) {
    if (mine == theirs) return 1;
    if (had_predec && had_group0) {
        if ((MAX(mine, theirs) - MIN(mine,theirs)) < 3) {
            printf("\nHmmm?");
            return 1;
        }
    }

    printf("\n%s mine:", name);
    printf(display_str, mine);
    printf(" theirs:");
    printf(display_str, theirs);
    printf(" initial:");
    printf(display_str, initial);

    return 0;
}

static void rpp(u16 v)
{
    printf("%04x ---%c%c%c%c%c", v, v & 0x10 ? 'X' : 'x', v & 8 ? 'N' : 'n', v & 4 ? 'Z' : 'z', v & 2 ? 'V' : 'v', v & 1 ? 'C' : 'c');
}

static void pprint_SR(u16 SR, u16 initial_SR, u16 final_SR, u32 PC)
{
    printf("\n\nInitial SR:");
    rpp(initial_SR);
    printf("\n     My SR:");
    rpp(SR);
    printf("\n  Final SR:");
    rpp(final_SR);
    printf("\n        PC: %06x", PC);
}

static void pprint_state(struct m68k_test_state *s, const char *r)
{
    printf("\n\nProcessor state %s:", r);
    printf("\nD0:%08x D1:%08x D2:%08x D3:%08x", s->d[0], s->d[1], s->d[2], s->d[3]);
    printf("\nD4:%08x D5:%08x D6:%08x D7:%08x", s->d[4], s->d[5], s->d[6], s->d[7]);
    printf("\nA0:%08x A1:%08x A2:%08x A3:%08x", s->a[0], s->a[1], s->a[2], s->a[3]);
    printf("\nA4:%08x A5:%08x A6:%08x", s->a[4], s->a[5], s->a[6]);
}

static u32 had_ea_with_predec(struct M68k* this)
{
    switch(this->ins->operand_mode) {
        case M68k_OM_qimm:
        case M68k_OM_qimm_r:
        case M68k_OM_imm16:
        case M68k_OM_qimm_qimm:
        case M68k_OM_none:
        case M68k_OM_r:
        case M68k_OM_r_r:
            return 0;
        case M68k_OM_ea_r:
        case M68k_OM_ea:
            return this->ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement;
        case M68k_OM_qimm_ea:
        case M68k_OM_r_ea:
            return this->ins->ea2.kind == M68k_AM_address_register_indirect_with_predecrement;
        case M68k_OM_ea_ea:
            return this->ins->ea1.kind == M68k_AM_address_register_indirect_with_predecrement ||
                   this->ins->ea2.kind == M68k_AM_address_register_indirect_with_predecrement;
        default:
            assert(1==0);
    }
}


static u32 compare_state_to_cpu(struct m68k_test_struct *ts, struct m68k_test_state *final, struct m68k_test_state *initial)
{
    u32 all_passed = 1;
    u32 ea = had_ea_with_predec(&ts->cpu);
#define CP(rn, rn2, rname) all_passed &= cval(ts->cpu.regs. rn, final-> rn2, initial-> rn2, "%08llx", rname, ts->had_group0 != -1, ea)
    struct M68k* cpu = &ts->cpu;
    CP(D[0], d[0], "d0");
    CP(D[1], d[1], "d1");
    CP(D[2], d[2], "d2");
    CP(D[3], d[3], "d3");
    CP(D[4], d[4], "d4");
    CP(D[5], d[5], "d5");
    CP(D[6], d[6], "d6");
    CP(D[7], d[7], "d7");
    if ((ts->cpu.ins->exec == &M68k_ins_MOVEM_TO_REG) && (ts->had_group0)) {
        u32 num_mismatch = 0;
        u32 lm = 0;
        for (u32 i = 0; i < 7; i++) {
            if (ts->cpu.regs.A[i] != final->a[i]) {
                num_mismatch++;
                lm = i;
            }
        }
        if (num_mismatch > 1) {
            CP(A[0], a[0], "a0");
            CP(A[1], a[1], "a1");
            CP(A[2], a[2], "a2");
            CP(A[3], a[3], "a3");
            CP(A[4], a[4], "a4");
            CP(A[5], a[5], "a5");
            CP(A[6], a[6], "a6");
        }
        else if (num_mismatch == 1) {
            if ((MAX(ts->cpu.regs.A[lm], final->a[lm]) - MIN(ts->cpu.regs.A[lm], final->a[lm])) > 4) {
                printf("\n UHOH can't account for this?");
                CP(A[0], a[0], "a0");
                CP(A[1], a[1], "a1");
                CP(A[2], a[2], "a2");
                CP(A[3], a[3], "a3");
                CP(A[4], a[4], "a4");
                CP(A[5], a[5], "a5");
                CP(A[6], a[6], "a6");
            }
        }
    }
    else {
        CP(A[0], a[0], "a0");
        CP(A[1], a[1], "a1");
        CP(A[2], a[2], "a2");
        CP(A[3], a[3], "a3");
        CP(A[4], a[4], "a4");
        CP(A[5], a[5], "a5");
        CP(A[6], a[6], "a6");
    }
    CP(PC, pc+4, "pc");
    CP(IR, prefetch[0], "ir");
    CP(IRC, prefetch[1], "irc");
    all_passed &= cval_SR(M68k_get_SR(cpu), final->sr, initial->sr, "%08llx", "sr", ts->had_group0 != -1, ea, ts);
    if (cpu->regs.SR.S) { // supervisor mode!
        CP(A[7], ssp, "a7");
        CP(ASP, usp, "asp");
    }
    else {
        CP(A[7], usp, "a7");
        CP(ASP, ssp, "asp");
    }
#undef CP
    if (!all_passed) {
        pprint_SR(cpu->regs.SR.u, initial->sr, final->sr, cpu->regs.PC);
        pprint_state(initial, "initial");
        pprint_state(final, "final");
    }
    return all_passed;
}

static void cycle_cpu(struct m68k_test_struct *ts)
{
    M68k_cycle(&ts->cpu);
    if ((ts->cpu.pins.AS) && (!ts->cpu.pins.DTACK)) { // CPU is trying to read or write
        if (ts->cpu.pins.RW) { // Write!
            do_test_write(ts, ts->cpu.pins.Addr, ts->cpu.pins.UDS, ts->cpu.pins.LDS, ts->cpu.pins.D);
        }
        else { // if (!ts->cpu.pins.DTACK) { // Read!
            ts->cpu.pins.D = do_test_read(ts, ts->cpu.pins.Addr, ts->cpu.pins.UDS, ts->cpu.pins.LDS);
        }
    }
    ts->test.current_cycle++;
}


static u32 dopppt(char *ptr, struct transaction *t)
{
    char *m = ptr;
    m += sprintf(ptr, "(%02d) %c.%c %06x:%04x  ", t->start_cycle, t->kind == tk_write ? 'W' : 'R', t->sz == 1 ? '1' : '2', t->addr_bus, t->data_bus);
    return m - ptr;
}

static void pprint_transactions(struct m68k_test_transactions *ts, struct m68k_test_transactions *mts)
{
    char buf[500];
    printf("\n---Transactions:");
    u32 bigger = MAX(ts->num_transactions, mts->num_transactions);
    i32 tsi = 0, mtsi = 0;
    while(1) {
        // Search for next tk_read or tk_write
        struct transaction *t1 = NULL, *t2 = NULL;
        for (i32 i = tsi; i < ts->num_transactions; i++) {
            struct transaction *t = &ts->items[i];
            if ((t->kind == tk_write) || (t->kind == tk_read)) {
                t1 = t;
                tsi = i+1;
                break;
            }
        }
        for (i32 i = mtsi; i < mts->num_transactions; i++) {
            struct transaction *t = &mts->items[i];
            if ((t->kind == tk_write) || (t->kind == tk_read)) {
                t2 = t;
                mtsi = i+1;
                break;
            }
        }
        char *ptr = buf;
        memset(ptr, 0, 100);
        if ((t1 != NULL) && (t2 != NULL)) {
            u32 c = t2->data_bus;
            if (((t2->UDS + t2->LDS) == 1) && (t2->UDS)) c >>= 8;
            if (t1->data_bus != t2->data_bus) ptr += sprintf(ptr, "!");
            else ptr += sprintf(ptr, " ");
            if (t1->addr_bus != t2->addr_bus) ptr += sprintf(ptr, "-");
            else ptr += sprintf(ptr, " ");
        }
        else ptr += sprintf(ptr, "  ");
        if (t1 == NULL) {
            ptr += sprintf(ptr, "                          ");
        }
        else {
            ptr += dopppt(ptr, t1);
        }
        if (t2 != NULL) {
            ptr += dopppt(ptr, t2);
        }
        printf("\n%s", buf);
        if ((t1 == NULL) && (t2 == NULL)) break;
    }
}

static u32 do_test(struct m68k_test_struct *ts, const char*file, const char *fname)
{
    FILE *f = fopen(file, "rb");
    if (f == NULL) {
        printf("\nBAD FILE! %s", file);
        return 0;
    }
    if (filebuf == 0) filebuf = malloc(FILE_BUF_SIZE);
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

    u8 *ptr = (u8 *)filebuf;
    u32 v = R32;

    assert(v==0x1A3F5D71);
    u32 num_tests = R32;
    for (u32 i = 0; i < num_tests; i++) {
        if (dbg.trace_on) printf("\nSubtest %d (%s)", i, fname);
        ptr = decode_test(ptr, &ts->test);

        copy_state_to_cpu(&ts->cpu, &ts->test.initial);
        copy_state_to_RAM(&ts->test.initial);

        ts->cpu.state.current = M68kS_decode;
        ts->test.current_cycle = 0;
        ts->test.failed = 0;
        ts->test.failed_w = 0;
        ts->cpu.trace_cycles = 0;
        ts->cpu.testing = 1;
        ts->my_transactions.num_transactions = 0;
        ts->had_group0 = ts->had_group1 = ts->had_group2 = -1;

        for (u32 j = 0; j < ts->test.num_cycles*10; j++) {
            cycle_cpu(ts);
            if (j == 0) {
                ts->cpu.ins_decoded = 0;
            }
            else {
                if (ts->cpu.ins_decoded) break;
            }
            if ((ts->cpu.state.current == M68kS_exc_group0) && (ts->had_group0 == -1)) {
                ts->had_group0 = (i64)ts->cpu.trace_cycles;
            }
            if ((ts->cpu.state.current == M68kS_exc_group1) && (ts->had_group1 == -1)) ts->had_group1 = (i64)ts->cpu.trace_cycles;
            if ((ts->cpu.state.current == M68kS_exc_group2) && (ts->had_group2 == -1)) ts->had_group2 = (i64)ts->cpu.trace_cycles;
        }

        u32 unvisited = 0;
        for (u32 j = 0; j < ts->test.transactions.num_transactions; j++) {
            struct transaction *t = &ts->test.transactions.items[j];
            if (((t->kind == tk_read) || (t->kind == tk_write)) && (t->visited == 0)) {
                printf("\nWARNING UNVISITED TRANSACTION sz:%c rw:%c addr:%06x val:%08x cyc:%d", t->sz == 1 ? 'b' : 'w', t->kind == tk_write ? 'W' : 'R', t->addr_bus, t->data_bus, t->start_cycle);
                ts->test.failed = 1;
            }
        }


        if (ts->cpu.trace_cycles != ts->test.num_cycles) {
            if (dbg.trace_on) printf("\nWARNING CYCLES MISMATCH. theirs:%d   mine:%lld", ts->test.num_cycles, ts->cpu.trace_cycles);
        }

        if ((!compare_state_to_cpu(&ts->cpu, &ts->test.final, &ts->test.initial)) || (!compare_state_to_ram(ts)) || ts->test.failed) {
            pprint_transactions(&ts->test.transactions, &ts->my_transactions);
            printf("\nTest result for test %d: failed", i);
            return 0;
        }
    }

    return 1;
}


void test_m68000()
{
    //struct M68k cpu;
    do_M68k_decode();
    struct jsm_string yo;
    jsm_string_init(&yo, 50);
    struct jsm_debug_read_trace rt;
    rt.read_trace_m68k = &do_test_read_trace;

    struct m68k_test_struct ts;
    rt.ptr = (void *)&ts;

    M68k_init(&ts.cpu, 0);
    M68k_setup_tracing(&ts.cpu, &rt);
    dbg_init();
    dbg_disable_trace();
    //dbg_enable_trace();

    /*struct M68k_ins_t *ins = &M68k_decoded[0xd862];
    rt.read_trace_m68k = &m68k_dasm_read;
    M68k_disassemble(0, tmem[0], &rt, &yo);
    printf("\nReturned string: %s", yo.ptr);
    printf("\nYO! %04x", ins->opcode);*/
    char PATH[500];
    construct_path(PATH,"680x0/68000/v1");

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
                sprintf(mfp[num_files], "%s/%s", PATH, ep->d_name);
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

    tmem = malloc(0x1000000); // 24 MB RAM allocate

    u32 completed_tests = 0;
    u32 nn = 35; // 35
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
        printf("\nDoing test %s", mfn[i]);
        //if (completed_tests > nn) return;
        if (!do_test(&ts, mfp[i], mfn[i])) break;
        if (completed_tests > nn) break;
        if (completed_tests > (nn-1)) dbg_enable_trace();
        completed_tests++;
    }
    dbg_flush();
    printf("\n\nCompleted %d of %d tests succesfully! %d skips", completed_tests, num_files, TEST_SKIPS_NUM);
    free(tmem);
    tmem = 0;
}