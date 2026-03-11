//
// Created by . on 3/11/26.
//
#include <dirent.h>
#include <cstdio>
#include <cassert>

#include "component/cpu/r3000/r3000.h"
#include "helpers/better_irq_multiplexer.h"
#include "helpers/scheduler.h"
#include "cpu-test-helpers.h"
#include "helpers/multisize_memaccess.cpp"

#define M8 1
#define M16 2
#define M32 4
#define M64 8

#define R8 cR[M8](ptr, 0); ptr += 1
#define R16 cR[M16](ptr, 0); ptr += 2
#define R32 cR[M32](ptr, 0); ptr += 4
#define R64 cR[M64](ptr, 0); ptr += 8

struct test_cpu_state {
    u32 R[32]{};
    u32 hi{}, lo{}, EPC{}, PC{};
    struct {
        struct {
            i32 target;
            u32 val;
        } load;

        struct {
            u32 slot{}; // Is a delay slot?
            u32 take{}; // Are we taking it?
            u32 target{}; // What is the target?
        } branch;
    } delay;
};

enum : u64 { NOACTION = 0, READ = 1, WRITE = 2, FETCH = 4};

struct test_cycle {
    i64 val{}; // 1
    u32 actions{}; // 2
    i64 addr{}; // 3
    u32 sz{}; // 4

    bool visited{};
    u32 my_actions{};
};

struct test {
    test_cpu_state initial{}, final{};
    u32 num_cycles{};
    test_cycle cycles[50]{};
    u32 opcode{}, opcode_addr{};
};

struct gstate_t {
    u64 waitstates{};
    u64 clock{};
    scheduler_t scheduler{&clock};
    IRQ_multiplexer_b irq_multiplexer{0};
    R3000::core cpu {&clock, &waitstates, &scheduler, &irq_multiplexer};
    test test{};
    bool failed{};
} gstate{};

u32 do_read(void *ptr, u32 addr, u8 sz) {
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];
        if (c.addr == addr) {
            c.visited = true;
            c.my_actions |= READ;
            if (c.sz != sz) {
                gstate.failed = true;
                printf("\nFAIL WRONG READ SIZE");
            }
            return c.val;
        }
    }
    gstate.failed = true;
    printf("\nFAIL WRONG READ ADDR %08x", addr);
    return 0;
}

void do_write(void *ptr, u32 addr, u8 sz, u32 val) {
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];
        if (c.addr == addr) {
            c.visited = true;
            c.my_actions |= WRITE;
            if (c.sz != sz) {
                gstate.failed = true;
                printf("\nFAIL WRONG WRITE SIZE");
            }
            if (c.val != val) {
                gstate.failed = true;
                printf("\nFAIL WRONG WRITE VALUE");
            }
            return;
        }
    }
    printf("\nFAIL BAD WRITE ADDRESS %08x", addr);
}

u32 do_fetch_ins(void *ptr, u32 addr) {
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];
        if (c.addr == addr) {
            c.visited = true;
            c.my_actions |= FETCH;
            return c.val;
        }
    }
    gstate.failed = true;
    printf("\nFAIL WRONG FETCH ADDR %08x", addr);
    return 0;
}

#define FILE_BUF_SIZE 512*1024
static char *filebuf{};

static u8* load_state(test_cpu_state &state, u8* ptr) {
    for (u32 i = 0; i < 32; i++) {
        state.R[i] = R32;
    }
    state.hi = R32;
    state.lo = R32;
    state.EPC = R32;
    state.PC = R32;
    state.delay.branch.target = R32;
    state.delay.branch.slot = R32;
    state.delay.branch.take = R32;
    state.delay.load.target = R32;
    state.delay.load.val = R32;
    return ptr;
}

static u8* decode_test(u8* ptr) {
    // Empty outcycles
    for (auto & c : gstate.test.cycles) {
        c.actions = NOACTION;
        c.addr = -1;
        c.my_actions = NOACTION;
        c.visited = false;
        c.sz = 0;
        c.val = -1;
    }
    gstate.test.opcode = R32;
    gstate.test.opcode_addr = R32;
    ptr = load_state(gstate.test.initial, ptr);
    ptr = load_state(gstate.test.final, ptr);
    gstate.test.num_cycles = R32;
    for (u32 i = 0; i < gstate.test.num_cycles; i++) {
        auto &c = gstate.test.cycles[i];;
        c.val = R64;
        c.actions = R32;
        c.addr = R64;
        c.sz = R32;
    }
    return ptr;
}

static bool do_test(const char *file, const char *fname) {
    FILE *f = fopen(file, "rb");
    if (f == nullptr) {
        printf("\nBAD FILE! %s", file);
        return false;
    }
    if (!filebuf) filebuf = static_cast<char *>(malloc(FILE_BUF_SIZE));
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
    auto *ptr = reinterpret_cast<u8 *>(filebuf);
    u32 num_tests = R32;
    printf("\n%d tests in file!", num_tests);

    for (u32 i = 0; i < num_tests; i++) {
        ptr = decode_test(ptr);
        copy_state_to_cpu();
        gstate.waitstates = gstate.clock = 0;

        gstate.cpu.instruction();

        if (gstate.clock != gstate.test.num_cycles) {
            // FAIL for num cycles
        }
        if (!compare_state_to_cpu()) {
            // FAIL for this
        }
        if (!compare_cycles()) {
            // FAIL for this
        }
        if (gstate.failed) return false;
    }

    return true;
}

void test_r3000() {
    gstate.cpu.read = &do_read;
    gstate.cpu.write = &do_write;
    gstate.cpu.fetch_ins = &do_fetch_ins;

    dbg_init();
    dbg_disable_trace();

    char PATH[500];
    construct_cpu_test_path(PATH, "r3000", "", sizeof(PATH));

    char mfp[500][500];
    char mfn[500][500];
    int num_files = 0;
    DIR *dp;
    dirent *ep;
    dp = opendir (PATH);

    if (dp != nullptr)
    {
        while ((ep = readdir (dp)) != nullptr) {
            if (strstr(ep->d_name, ".json.bin") != nullptr) {
                snprintf(mfp[num_files], 500, "%s/%s", PATH, ep->d_name);
                snprintf(mfn[num_files], 500, "%s", ep->d_name);
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
        printf("\nDoing test %s", mfn[i]);
        if (!do_test(mfp[i], mfn[i])) break;
        completed_tests++;
    }
    dbg_flush();
    printf("\n\nCompleted %d of %d tests succesfully!", completed_tests, num_files);
}
