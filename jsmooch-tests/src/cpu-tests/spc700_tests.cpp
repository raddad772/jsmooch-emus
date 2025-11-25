//
// Created by . on 4/19/25.
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
#include "spc700_tests.h"
#include "component/cpu/spc700/spc700.h"


static u64 cycle_ptr = 0;

struct test_cpu_regs {
    u32 A, X, Y, PC, SP, P;
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
    u32 rw;
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

struct spc700_test_result
{
    u32 passed;
    u32 mycycles;
    test_cycle cycles[500];
    char msg[5000];
    u32 addr_io_mismatches;
    u32 length_mismatches;
    jsontest *failed_test_struct;
};


static void construct_path(char *out, u32 ins, size_t sz)
{
    char test_path[500] = {};
    const char* homeDir = get_user_dir();
    char *tp = out;
    tp += snprintf(tp, sz - (tp - out), "%s", homeDir);
    tp += snprintf(tp, sz - (tp - out), "/dev/spc700/v1");

    tp += snprintf(tp, sz - (tp - out), "%s/", test_path);
    tp += snprintf(tp, sz - (tp - out), "%02x.json", ins);
}

static jsontest tests[10000];


#define SKIPTESTLEN 2
// STP and WAI
static constexpr int skip_tests[SKIPTESTLEN] = {0xEF, 0xFF};

static int skip_test(int n)
{
    for (int skip_test : skip_tests) {
        if (n == skip_test) return 1;
    }
    return 0;
}

static void parse_state(json_object_s *object, test_state *state)
{
    json_object_element_s *el = object->start;
    state->num_ram_entry = 0;
    for (u32 i = 0; i < object->length; i++) {
        auto *str = static_cast<json_string_s *>(el->value->payload);
        u32 *dest = nullptr;
        if (strcmp(el->name->string, "pc") == 0) {
            dest = &state->regs.PC;
        }
        else if (strcmp(el->name->string, "a") == 0) {
            dest = &state->regs.A;
        }
        else if (strcmp(el->name->string, "x") == 0) {
            dest = &state->regs.X;
        }
        else if (strcmp(el->name->string, "y") == 0) {
            dest = &state->regs.Y;
        }
        else if (strcmp(el->name->string, "sp") == 0) {
            dest = &state->regs.SP;
        }
        else if (strcmp(el->name->string, "psw") == 0) {
            dest = &state->regs.P;
        }
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
            assert(el->value->type == json_type_number);
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
        el = el->next;
    }
}


static void parse_and_fill_out(read_file_buf *infile)
{
    json_value_s *root = json_parse(infile->buf.ptr, infile->buf.size);
    assert(root->type == json_type_array);

    auto* arr = static_cast<json_array_s *>(root->payload);
    json_array_element_s r{};
    assert(arr->length == 1000);
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

                    // number, number, string
                    auto *num = static_cast<json_number_s *>(arr2_el->value->payload);

                    if (num == nullptr) test->cycles[h].addr = -1;
                    else test->cycles[h].addr = atoi(num->number);

                    arr2_el = arr2_el->next;
                    num = static_cast<json_number_s *>(arr2_el->value->payload);

                    if (num == nullptr) test->cycles[h].data = -1;
                    else test->cycles[h].data = atoi(num->number);
                    arr2_el = arr2_el->next;

                    auto* st = static_cast<json_string_s *>(arr2_el->value->payload);
                    assert(st->string_size >= 4);
                    test->cycles[h].rw = st->string[0] == 'w';

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

static void copy_state_to_cpu(test_state *state, core *cpu)
{
    // u32 A, X, Y, P, SP, PC
    cpu->regs.A = state->regs.A;
    cpu->regs.X = state->regs.X;
    cpu->regs.Y = state->regs.Y;
    SPC700_P_setbyte(&cpu->regs.P, state->regs.P);
    cpu->regs.SP = state->regs.SP;
    cpu->regs.PC = state->regs.PC;

}

static void pprint_regs(SPC700_regs *cpu_regs, test_cpu_regs *test_regs, u32 last_pc, u32 only_print_diff) {
    printf("\nREG  CPU    TEST");
    printf("\n----------------");
#define TREG4(cpuname, testname, strname) if ((only_print_diff && (cpu_regs->cpuname != test_regs->testname)) || (!only_print_diff))\
        printf("\n" strname "  %04x   %04x", cpu_regs->cpuname, test_regs->testname);
    TREG4(A, A, "Acc.");
    TREG4(X, X, "X   ");
    TREG4(Y, Y, "Y   ");
    TREG4(PC, PC, "PC  ");
    printf("\nlast_pc: %04x", last_pc);
    TREG4(SP, SP, "SP  ");
#undef TREG4
#define TREG4(cpuname, testname, strname) if ((only_print_diff && ((cpu_regs->cpuname & 0xFF) != test_regs->testname)) || (!only_print_diff))\
        printf("\n" strname "  %04x   %04x", cpu_regs->cpuname & 0xFF, test_regs->testname);
    TREG4(P.v, P, "P    ");
#undef TREG4
}

static u32 testregs(core *cpu, test_state *final, u32 last_pc) {
    u32 passed = 1;
    // u32 A X Y P SP PC
    passed &= ((cpu->regs.PC & 0xFFFF) == final->regs.PC) || (last_pc == final->regs.PC);
    passed &= cpu->regs.A == final->regs.A;
    passed &= cpu->regs.X == final->regs.X;
    passed &= cpu->regs.Y == final->regs.Y;
    passed &= (cpu->regs.P.v & 0xFF) == final->regs.P;
    passed &= cpu->regs.SP == final->regs.SP;
    if (!passed) {
        pprint_regs(&cpu->regs, &final->regs, last_pc, 1);
    }
    return passed;
}

static void pprint_P(u32 r) {
    // C Z I H B P V N
#define F(v, l1, l2) (r & v) ? l1 : l2
    printf("%c%c%c%c%c%c%c%c",
           F(0x1, 'C', 'c'),
           F(0x2, 'Z', 'z'),
           F(0x4, 'I', 'i'),
           F(0x8, 'H', 'h'),
           F(0x10, 'B', 'b'),
           F(0x20, 'P', 'p'),
           F(0x40, 'V', 'v'),
           F(0x80, 'N', 'n')
    );
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4700) // warning C4700: uninitialized local variable 'last_pc' used
#endif

static int test_spc700_automated(spc700_test_result *out, core *cpu, u32 opc) {
    out->passed = 0;
    out->mycycles = 0;
    snprintf(out->msg, sizeof(out->msg), "");
    char *msgptr = out->msg;
    out->addr_io_mismatches = 0;
    out->length_mismatches = 0;
    out->failed_test_struct = nullptr;
    u32 last_pc=0;
    for (u32 i = 0; i < 1000; i++) {
        u32 passed = 1;
        printf("\n\nTest #%d/10000", i);
        out->failed_test_struct = &tests[i];
        jsontest *test = &tests[i];
        test_state *initial = &tests[i].initial;
        test_state *final = &tests[i].final;

        copy_state_to_cpu(initial, cpu);
        u32 test_pc = initial->regs.PC;

        test->num_ram_entry = initial->num_ram_entry;
        for (u32 j = 0; j < initial->num_ram_entry; j++) {
            test->ram[j].addr = initial->ram[j].addr;
            test->ram[j].val = initial->ram[j].val;
            cpu->RAM[test->ram[j].addr & 0xFFFF] = test->ram[j].val;
        }
        cpu->cycles = 0;
        cpu->regs.opc_cycles = 0;
        cpu->regs.IR = opc;
        cycle_ptr = 0;
        cpu->cycle(1);
        if (cycle_ptr != test->num_cycles) {
            printf("\nCYCLE MISMATCH! ME:%lld  TEST:%d", cycle_ptr, test->num_cycles);
        }

        passed &= testregs(cpu, final, last_pc); // warning C4700: uninitialized local variable 'last_pc' used

        for (u32 j = 0; j < final->num_ram_entry; j++) {
            u8 v = cpu->RAM[final->ram[j].addr & 0xFFFF];
            if (v != final->ram[j].val) {
                passed = 0;
                printf("\nRAM MISMATCH! ADDR:%04x ME:%02x TEST:%02x", final->ram[j].addr, v, final->ram[j].val);
            }
        }
        if (!passed) {
            printf("\n\nINITIAL P:%02x ", initial->regs.P);
            pprint_P(initial->regs.P);
            printf("\nFINAL   P:%02x ", final->regs.P);
            pprint_P(final->regs.P);
            printf("\nMY      P:%02x ", cpu->regs.P.v & 0xFF);
            pprint_P(cpu->regs.P.v);
            cpu->cycles = 0;
            cpu->cycle(1);
            out->passed = 0;
            return 0;
        }


    }
    out->passed = 1;
    return 1;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static u32 test_spc700_ins(core *cpu, u32 ins)
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

    spc700_test_result result{};

    test_spc700_automated(&result, cpu, ins);
    if (!result.passed) {
        printf("\nFAILED INSTRUCTION!");
    }

    return result.passed;
}


void test_spc700()
{
    SPC700 cpu(&cycle_ptr);

    u32 total_fail = 0;
    u32 start_test = 0;
    for (u32 i = start_test; i < 0x100; i++) {
        if (skip_test(i)) continue;
        u32 result = test_spc700_ins(&cpu, i);
        if (!result) {
            total_fail = 1;
            break;
        }
        printf("\nTest for %02x passed.", i);
    }

}