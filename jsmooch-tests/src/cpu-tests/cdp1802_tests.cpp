//
// Created by . on 11/20/25.
//
#if defined(_MSC_VER)
#else
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#endif
#include <cassert>
#include "../json.h"

#define MAX_RAM_ENTRIES 50

#include "helpers/int.h"
#include "helpers/user.h"
#include "component/cpu/cdp1802/cdp1802.h"
#include "cdp1802_tests.h"

// IDLE
static constexpr int skip_tests[] = {
    0x00,
    0x34, // in original version, EF not included
    0x35, // in original version, EF not included
    0x36, // in original version, EF not included
    0x37, // in original version, EF not included
    0x3C, // in original version, EF not included
    0x3D, // in original version, EF not included
    0x3E, // in original version, EF not included
    0x3F, // in original version, EF not included
    0x68, // no file for this
};

struct test_cpu_regs {
    u32 D, X, N, P, R[16], DF, Q, T, IE, I;
};

struct ram_entry {
    u32 addr, val;
};

struct test_state {
    test_cpu_regs regs;
    u32 num_ram_entry;
    ram_entry ram[MAX_RAM_ENTRIES];
};

struct test_cycle {
    u32 mrd, mwr;
    i32 addr, data;
};

struct jsontest {
    char name[50];
    test_state initial;
    test_state final;
    test_cycle cycles[500];

    u32 num_ram_entry;
    ram_entry ram[MAX_RAM_ENTRIES];
    u32 num_cycles;
};

struct cdp1802_test_result
{
    u32 passed;
    u32 mycycles;
    test_cycle cycles[500];
    char msg[5000];
    u32 addr_io_mismatches;
    u32 length_mismatches;
    jsontest *failed_test_struct;
};

static jsontest tests[10000];

static int skip_test(int n)
{
    for (int skip_test : skip_tests) {
        if (n == skip_test) return 1;
    }
    return 0;
}

static void construct_path(char *out, u32 ins, size_t sz)
{
    char test_path[500] = {};
    const char* homeDir = get_user_dir();
    char *tp = out;
    tp += snprintf(tp, sz - (tp - out), "%s", homeDir);
    tp += snprintf(tp, sz - (tp - out), "/dev/external/cdp1802/v1");

    tp += snprintf(tp, sz - (tp - out), "%s/", test_path);
    tp += snprintf(tp, sz - (tp - out), "%02x.json", ins);
}

static void parse_state(json_object_s *object, test_state *state)
{
    json_object_element_s *el = object->start;
    state->num_ram_entry = 0;
    for (u32 i = 0; i < object->length; i++) {
        auto *str = static_cast<json_string_s *>(el->value->payload);
        u32 *dest = nullptr;
        if (strcmp(el->name->string, "r") == 0) {
            auto *arr1 = static_cast<json_array_s *>(el->value->payload);
            state->num_ram_entry = static_cast<u32>(arr1->length);
            json_array_element_s *arr_el = arr1->start;
            for (u32 arr1_i = 0; arr1_i < arr1->length; arr1_i++) {
                assert(arr_el->value->type == json_type_number);
                auto *num = static_cast<json_number_s *>(arr_el->value->payload);
                char *yo;
                u32 a = strtol(num->number, &yo, 10);
                assert(yo != num->number);
                state->regs.R[arr1_i] = a;
                arr_el = arr_el->next;
            }
        }
#define EIS(jsl, stl) else if (strcmp(el->name->string, jsl) == 0) dest = &state->regs. stl;
        EIS("p", P)
        EIS("x", X)
        EIS("n", N)
        EIS("i", I)
        EIS("t", T)
        EIS("d", D)
        EIS("df", DF)
        EIS("ie", IE)
        EIS("q", Q)
        if (strcmp(el->name->string, "ram") == 0) {
            auto *arr1 = static_cast<json_array_s *>(el->value->payload);
            state->num_ram_entry = static_cast<u32>(arr1->length);
            json_array_element_s *arr_el = arr1->start;
            for (u32 arr1_i = 0; arr1_i < arr1->length; arr1_i++) {
                assert(arr_el->value->type == json_type_array);
                auto *arr2 = static_cast<json_array_s *>(arr_el->value->payload);
                assert(arr2->length == 2);
                json_array_element_s *arr2_el = arr2->start;
                // Address
                auto* nr = static_cast<json_number_s *>(arr2_el->value->payload);
                state->ram[arr1_i].addr = atoi(nr->number);
                // Data
                arr2_el = arr2_el->next;
                nr = static_cast<json_number_s *>(arr2_el->value->payload);
                state->ram[arr1_i].val = atoi(nr->number);

                arr_el = arr_el->next;
            }
        }
        else {
            // Grab number
            if (strcmp(el->name->string, "r") != 0) {
                if (el->value->type == json_type_number) {
                    auto *num = static_cast<json_number_s *>(el->value->payload);
                    char *yo;
                    u32 a = strtol(num->number, &yo, 10);
                    assert(yo != num->number);
                    if (dest == nullptr) {
                        printf("\nUHOH! %s \n", el->name->string);
                    }
                    else {
                        *dest = a;
                    }
                }
                else if (el->value->type == json_type_true) {
                    *dest = 1;
                }
                else if (el->value->type == json_type_false) {
                    *dest = 0;
                }
                else {
                    assert(1==2);
                }
            }
        }
        el = el->next;
    }
}


static void parse_and_fill_out(read_file_buf *infile)
{
    json_value_s *root = json_parse(infile->buf.ptr, infile->buf.size);
    assert(root->type == json_type_array);

    auto* arr = static_cast<json_array_s *>(root->payload);
    json_array_element_s r{};
    assert(arr->length == 10000);
    json_array_element_s* cur_node = arr->start;
    for (u32 i = 0; i < arr->length; i++) {
        jsontest *test = tests + i;
        test->num_cycles = 0;
        snprintf(test->name, sizeof(test->name), "unk");
        assert(cur_node->value->type == json_type_object);
        auto *tst = static_cast<json_object_s *>(cur_node->value->payload);
        auto *s = tst->start;
        for (u32 j = 0; j < tst->length; j++) {
            if (strcmp(s->name->string, "name") == 0) {
                assert(s->value->type == json_type_string);
                auto *str = static_cast<json_string_s *>(s->value->payload);
                snprintf(test->name, sizeof(test->name), "%s", str->string);
            }
            else if (strcmp(s->name->string, "initial") == 0) {
                assert(s->value->type == json_type_object);
                auto *state = static_cast<json_object_s *>(s->value->payload);
                parse_state(state, &test->initial);
            }
            else if (strcmp(s->name->string, "final") == 0) {
                assert(s->value->type == json_type_object);
                auto *state = static_cast<json_object_s *>(s->value->payload);
                parse_state(state, &test->final);
            }
            else if (strcmp(s->name->string, "cycles") == 0) {
                assert(s->value->type == json_type_array);
                auto *arr1 = static_cast<json_array_s *>(s->value->payload);
                test->num_cycles = static_cast<u32>(arr1->length);
                json_array_element_s* arr1_el = arr1->start;
                for (u32 h = 0; h < arr1->length; h++) {
                    assert(arr1_el != nullptr);
                    assert(arr1->length < 500);
                    auto* arr2 = static_cast<json_array_s *>(arr1_el->value->payload);
                    auto* arr2_el = arr2->start;

                    // string "r/w/n"
                    // // then if r or w: number, number
                    auto* st = static_cast<json_string_s *>(arr2_el->value->payload);
                    assert(st->string_size >= 1);
                    test->cycles[h].mrd = st->string[0] == 'r';
                    test->cycles[h].mwr = st->string[0] == 'w';
                    test->cycles[h].addr = -1;
                    if (test->cycles[h].mrd || test->cycles[h].mwr) {
                        arr2_el = arr2_el->next;
                        auto *num = static_cast<json_number_s *>(arr2_el->value->payload);

                        if (num == nullptr) test->cycles[h].addr = -1;
                        else test->cycles[h].addr = atoi(num->number);

                        arr2_el = arr2_el->next;
                        num = static_cast<json_number_s *>(arr2_el->value->payload);

                        if (num == nullptr) test->cycles[h].data = -1;
                        else test->cycles[h].data = atoi(num->number);
                        arr2_el = arr2_el->next;
                    }

                    arr1_el = arr1_el->next;
                }
            }
            else {
                printf("\nno done! %s", s->name->string);
            }

            s = s->next;
        }

        cur_node = cur_node->next;

    }
    free(root);
}

static void copy_state_to_cpu(test_state *state, CDP1802::core *cpu)
{
#define set(c, s) cpu->regs. c = state->regs. s;
    set(D, D);
    set(X, X);
    set(N, N);
    set(P, P);
    set(DF, DF);
    set(T.u, T);
    set(IE, IE);
    set(I, I);
    cpu->pins.Q = state->regs.Q;
    //cpu->pins.EF = state->regs.
    for (u32 i = 0; i < 16; i++) {
        set(R[i].u, R[i]);
    }
#undef set
}

static u8 RAM[65536];

static int is_3cycle(u32 opc) {
    if ((opc >= 0xC0) && (opc < 0xD0)) return 1;
    return 0;
}

static void pprint_regs(CDP1802::core &cpu, test_cpu_regs *test_regs, u32 last_pc, u32 only_print_diff) {
    printf("\nREG  CPU    TEST");
    printf("\n----------------");

    for (u32 i = 0; i < 16; i++) {

        if ((only_print_diff && (cpu.regs.R[i].u != test_regs->R[i])) || (!only_print_diff)) {
            printf("\nR%d   ", i);
            if (i < 10) printf(" ");
            printf("%04x   %04x", cpu.regs.R[i].u, test_regs->R[i]);
        }

    }
#define TREG2(cpuname, testname, strname) if ((only_print_diff && (cpu.cpuname != test_regs->testname)) || (!only_print_diff))\
printf("\n" strname "  %02x     %02x", cpu.cpuname, test_regs->testname);
#define TREG1(cpuname, testname, strname) if ((only_print_diff && (cpu.cpuname != test_regs->testname)) || (!only_print_diff))\
printf("\n" strname "  %1x      %1x", cpu.cpuname, test_regs->testname);
#define DPR(cpuname, testname, strname) printf("\n" strname "  %02d     %02d", cpu.cpuname, test_regs->testname);


    TREG2(regs.D, D, "D   ");
    //TREG1(X, X, "X   ");
    //TREG1(N, N, "N   ");
    //TREG1(P, P, "P   ");
    DPR(regs.X, X, "X   ");
    DPR(regs.N, N, "N   ");
    DPR(regs.P, P, "P   ");
    DPR(pins.Q, Q, "Q   ");
    TREG1(regs.DF, DF, "DF  ");
    TREG1(regs.T.u, T, "T   ");
    TREG1(regs.IE, IE, "IE  ");
    TREG1(regs.I, I, "I   ");
#undef DPR
#undef TREG2
#undef TREG1
}


static u32 testregs(CDP1802::core &cpu, test_state *final, u32 last_pc, u32 testnum, char *nm, u32 opc) {
    u32 passed = 1;
    // u32 A X Y P SP PC
#define set(c, s) passed &= cpu.regs.c == final->regs. s;
    set(D, D);
    set(X, X);
    set(N, N);
    set(P, P);
    set(DF, DF);
    set(T.u, T);
    set(IE, IE);
    set(I, I);
    passed &= cpu.pins.Q == final->regs.Q;
    for (u32 i = 0; i < 16; i++) {
        if (cpu.regs.R[i].u != final->regs.R[i]) {
            if ((opc >= 0x30) && (opc < 0x40) && (cpu.regs.R[i].u == ((final->regs.R[i] + 0x100) & 0xFFFF)))
                continue;
        }
        set(R[i].u, R[i]);
    }
#undef set
    if (!passed) {
        printf("\nFAIL TEST #%d NAME:%s", testnum, nm);;
        pprint_regs(cpu, &final->regs, last_pc, 1);
    }
    return passed;
}

static void service_rw(CDP1802::core *cpu) {
    if (cpu->pins.MRD) {
        cpu->pins.D = RAM[cpu->pins.Addr];
        //printf("\nREAD %02x FROM %04x", cpu->pins.D, cpu->pins.Addr);
    }
    if (cpu->pins.MWR) {
        //printf("\nWRITE %02x TO %04x", cpu->pins.D, cpu->pins.Addr);
        RAM[cpu->pins.Addr] = cpu->pins.D;
    }
}


static int test_cdp1802_automated(cdp1802_test_result *out, CDP1802::core *cpu, u32 opc) {
    out->passed = 0;
    out->mycycles = 0;
    snprintf(out->msg, sizeof(out->msg), "");
    char *msgptr = out->msg;
    out->addr_io_mismatches = 0;
    out->length_mismatches = 0;
    out->failed_test_struct = nullptr;
    u32 last_pc=0;
    for (auto & i : RAM) {
        i = 0;
    }
    for (u32 i = 0; i < 10000; i++) {
        u32 passed = 1;
        //printf("\n\nTest #%d/10000", i);
        out->failed_test_struct = &tests[i];
        jsontest *test = &tests[i];
        test_state *initial = &tests[i].initial;
        test_state *final = &tests[i].final;

        copy_state_to_cpu(initial, cpu);
        u32 test_pc = initial->regs.R[initial->regs.P];

        test->num_ram_entry = initial->num_ram_entry;
        for (u32 j = 0; j < initial->num_ram_entry; j++) {
            test->ram[j].addr = initial->ram[j].addr;
            test->ram[j].val = initial->ram[j].val;
            RAM[test->ram[j].addr & 0xFFFF] = test->ram[j].val;
        }
        //cpu->num_fetches = 0;
        cpu->prepare_fetch();
        service_rw(cpu);
        cpu->cycle();
        service_rw(cpu);
        last_pc = cpu->regs.R[cpu->regs.P].u;
        cpu->cycle();
        service_rw(cpu);
        if (is_3cycle(opc)) {
            cpu->cycle();
            service_rw(cpu);
        }

        cpu->regs.R[cpu->regs.P].u--;

        passed &= testregs(*cpu, final, last_pc, i, test->name, opc); // warning C4700: uninitialized local variable 'last_pc' used

        for (u32 j = 0; j < final->num_ram_entry; j++) {
            u8 v = RAM[final->ram[j].addr & 0xFFFF];
            if (v != final->ram[j].val) {
                passed = 0;
                printf("\nRAM MISMATCH! ADDR:%04x ME:%02x TEST:%02x", final->ram[j].addr, v, final->ram[j].val);
            }
        }
        if (!passed) {
            out->passed = 0;
            return 0;
        }
    }
    out->passed = 1;
    return 1;
}


static u32 test_cdp1802_ins(CDP1802::core &cpu, u32 ins)
{
    printf("\n\nTesting instruction %02x", ins);
    char path[500];
    construct_path(path, ins, 500);
    read_file_buf infile{};
    infile.read(nullptr, path);
    if (infile.buf.size < 10) {
        printf("\nBAD TEST! %s", path);
        return 0;
    }

    parse_and_fill_out(&infile);

    cdp1802_test_result result{};

    test_cdp1802_automated(&result, &cpu, ins);
    if (!result.passed) {
        printf("\nFAILED INSTRUCTION!");
    }

    return result.passed;
}


void test_cdp1802()
{
    u64 blah;
    CDP1802::core cpu(&blah);
    cpu.reset();
    cpu.pins.clear_wait = CDP1802::pins::RUN;

    u32 total_fail = 0;
    u32 start_test = 0;
    for (u32 i = start_test; i < 0x100; i++) {
        if (skip_test(i)) continue;
        u32 result = test_cdp1802_ins(cpu, i);
        if (!result) {
            total_fail = 1;
            break;
        }
        printf("\nTest for %02x passed.", i);
    }

}