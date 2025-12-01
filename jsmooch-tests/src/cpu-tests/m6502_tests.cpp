//
// Created by . on 9/18/24.
//

#include <cassert>
#include <cstdlib>
#include <cstdio>

#include "m6502_tests.h"
#include "helpers/int.h"
#include "helpers/user.h"
#include "rfb.h"
#include "component/cpu/m6502/nesm6502_opcodes.h"
#include "component/cpu/m6502/m6502_opcodes.h"
#include "component/cpu/m6502/m6502.h"

#include "../json.h"

#define NTESTS 10000

struct test_cpu_regs {
    u32 a, x, y, pc, s, p;
};

struct ram_entry {
    u32 addr, val;
};

#define MAX_RAM_ENTRY 50

struct test_state {
    test_cpu_regs regs;
    u32 num_ram_entry;
    ram_entry ram[MAX_RAM_ENTRY];
};

struct test_cycle {
    i32 addr, data, rw;
};

static u32 test_RAM[65536];

#define MAX_CYCLES 20
struct jsontest {
    char name[50];
    test_state initial;
    test_state final;
    test_cycle cycles[MAX_CYCLES];
    u32 num_cycles;

    ram_entry opcodes[10];
};

struct m6502_test_result {
    u32 passed;
    test_cycle cycles[100];
    char msg[5000];
    jsontest *failed_test_struct;
};

static u32 skip_tests(u32 ins) {
    switch(ins) {
        case 0x02: // STP
        case 0x12: // STP
        case 0x22: // STP
        case 0x32: // STP
        case 0x42: // STP
        case 0x52: // STP
        case 0x62: // STP
        case 0x72: // STP
        case 0x92: // STP
        case 0xB2: // STP
        case 0xD2: // STP
        case 0xF2: // STP
            return 1;
        //case 0x58: // CLI
            //return 2;
        default:
            return 0;
    }
}

static void construct_path(char *out, u32 ins, bool nes, size_t sz)
{
    char test_path[500];

    const char *homeDir = get_user_dir();
    char *tp = out;
    if (nes) snprintf(tp, sz, "%s/dev/external/65x02/nes6502/v1/%02x.json", homeDir, ins);
    else snprintf(tp, sz, "%s/dev/external/65x02/6502/v1/%02x.json", homeDir, ins);
}

static void parse_state(json_object_s *object, test_state *state)
{
    json_object_element_s *el = object->start;
    state->num_ram_entry = 0;
    for (u32 i = 0; i < object->length; i++) {
        u32 p = 0;
        json_string_s *str = (json_string_s *)el->value->payload;
        u32 *dest = 0;
        if (strcmp(el->name->string, "a") == 0) {
            dest = &state->regs.a;
        }
        else if (strcmp(el->name->string, "x") == 0) {
            dest = &state->regs.x;
        }
        else if (strcmp(el->name->string, "y") == 0) {
            dest = &state->regs.y;
        }
        else if (strcmp(el->name->string, "s") == 0) {
            dest = &state->regs.s;
        }
        else if (strcmp(el->name->string, "p") == 0) {
            dest = &state->regs.p;
        }
        else if (strcmp(el->name->string, "pc") == 0) {
            dest = &state->regs.pc;
        }
        if (strcmp(el->name->string, "ram") == 0) {
            json_array_s *arr1 = (json_array_s *)el->value->payload;
            state->num_ram_entry = (u32)arr1->length;
            json_array_element_s *arr_el = arr1->start;
            for (u32 arr1_i = 0; arr1_i < arr1->length; arr1_i++) {
                assert(arr_el->value->type == json_type_array);
                json_array_s *arr2 = (json_array_s*) arr_el->value->payload;
                assert(arr2->length == 2);
                json_array_element_s *arr2_el = arr2->start;
                // Address
                json_number_s* nr = (json_number_s*)arr2_el->value->payload;
                state->ram[arr1_i].addr = atoi(nr->number);
                // Data
                arr2_el = arr2_el->next;
                nr = (json_number_s*)arr2_el->value->payload;
                state->ram[arr1_i].val = atoi(nr->number);

                arr_el = arr_el->next;
            }
        }
        else {
            // Grab number
            assert(el->value->type == json_type_number);
            json_number_s *num = (json_number_s *) el->value->payload;
            char *yo;
            u32 a = strtol(num->number, &yo, 10);
            assert(yo != num->number);
            if (dest == nullptr) {
                //printf("\nUHOH! %s \n", el->name->string);
            }
            else {
                *dest = a;
            }
        }
        el = el->next;
    }
}

static void parse_and_fill_out(jsontest tests[NTESTS], read_file_buf *infile)
{
    json_value_s *root = json_parse(infile->buf.ptr, infile->buf.size);
    assert(root->type == json_type_array);

    json_array_s* arr = (json_array_s*)root->payload;
    json_array_element_s r;
    assert(arr->length == NTESTS);
    json_array_element_s* cur_node = arr->start;
    for (u32 i = 0; i < NTESTS; i++) {
        jsontest *test = tests+i;
        test->num_cycles = 0;

        snprintf(test->name, sizeof(test->name), "Unknown");
        assert(cur_node->value->type == json_type_object);
        json_object_s *tst = (json_object_s *)cur_node->value->payload;
        json_object_element_s *s = (json_object_element_s *)tst->start;
        for (u32 j = 0; j < tst->length; j++) {
            if (strcmp(s->name->string, "name") == 0) {
                assert(s->value->type == json_type_string);
                json_string_s *str = (json_string_s *)s->value->payload;
                snprintf(test->name, sizeof(test->name),"%s", str->string);
            }
            else if (strcmp(s->name->string, "initial") == 0) {
                assert(s->value->type == json_type_object);
                json_object_s* state = (json_object_s *)s->value->payload;
                parse_state(state, &test->initial);
            }
            else if (strcmp(s->name->string, "final") == 0) {
                assert(s->value->type == json_type_object);
                json_object_s* state = (json_object_s*)s->value->payload;
                parse_state(state, &test->final);
            }
            else if (strcmp(s->name->string, "cycles") == 0) {
                assert(s->value->type == json_type_array);
                json_array_s* arr1 = (json_array_s*)s->value->payload;
                test->num_cycles = (u32)arr1->length;
                json_array_element_s* arr1_el = arr1->start;
                for (u32 h = 0; h < arr1->length; h++) {
                    assert(arr1_el != nullptr);
                    assert(arr1->length < 20);
                    json_array_s* arr2 = (json_array_s*)arr1_el->value->payload;
                    json_array_element_s* arr2_el = (json_array_element_s*)arr2->start;

                    // number, number, string
                    json_number_s *num = (json_number_s *)arr2_el->value->payload;

                    test->cycles[h].addr = atoi(num->number);

                    arr2_el = arr2_el->next;
                    num = (json_number_s *)arr2_el->value->payload;

                    if (num == nullptr) {
                        test->cycles[h].data = -1;
                    }
                    else
                        test->cycles[h].data = atoi(num->number);

                    arr2_el = arr2_el->next;

                    json_string_s* st = (json_string_s *)arr2_el->value->payload;
                    // rwmi
                    test->cycles[h].rw = st->string[0] == 'r' ? 0 : 1;

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

static void prettypp(u32 v)
{
    printf("%c%c%c%c%c%c%c%c",
           v & 0x01 ? 'C' : 'c',
           v & 0x02 ? 'Z' : 'z',
           v & 0x04 ? 'I' : 'i',
           v & 0x08 ? 'D' : 'd',
           v & 0x10 ? 'B' : 'b',
           v & 0x20 ? '1' : '0',
           v & 0x40 ? 'V' : 'v',
           v & 0x80 ? 'N' : 'n'
    );
}

static void pprint_P(u32 mine, u32 final, u32 initial)
{

    for (u32 i = 0; i < 3; i++) {
        u32 v = 0;
        if (i == 0) {
            printf("\nP ");
            v = mine;
        } else {
            printf("  ");
            if (i == 1) v = final;
            else v = initial;
        }

// return this->C | (this->Z << 1) | (this->I << 2) | (this->D << 3) | (this->B << 4) | 0x20 | (this->V << 6) | (this->N << 7);
        prettypp(v);
    }
}


static void pprint_regs(M6502::regs *cpu_regs, test_cpu_regs *test_regs, test_cpu_regs *initial_regs, u32 last_pc, u32 only_print_diff)
{
    printf("\nREG CPU       TESTEND     TESTSTART");
    printf("\n------------------------------------");
    if ((only_print_diff && (last_pc != test_regs->pc)) || (!only_print_diff))
        printf("\nPC  %04x      %04x     %04x", last_pc, test_regs->pc, initial_regs->pc);
    if ((only_print_diff && (cpu_regs->S  != test_regs->s)) || (!only_print_diff))
        printf("\nS   %02x        %02x        %02x", cpu_regs->S, test_regs->s, initial_regs->s);
    if ((only_print_diff && (cpu_regs->A != test_regs->a)) || (!only_print_diff))
        printf("\nA   %02x        %02x        %02x", cpu_regs->A, test_regs->a, initial_regs->a);
    if ((only_print_diff && (cpu_regs->X != test_regs->x)) || (!only_print_diff))
        printf("\nX   %02x        %02x        %02x", cpu_regs->X, test_regs->x, initial_regs->x);
    if ((only_print_diff && (cpu_regs->Y != test_regs->y)) || (!only_print_diff))
        printf("\nY   %02x        %02x        %02x", cpu_regs->Y, test_regs->y, initial_regs->y);
    if ((only_print_diff && ((cpu_regs->P.getbyte() & 0xEF) != (test_regs->p & 0xEF))) || (!only_print_diff))
        pprint_P(cpu_regs->P.getbyte(), test_regs->p, initial_regs->p);
}

static void pprint_test(jsontest *test, test_cycle *cpucycles) {
    printf("\nCycles");
    for (u32 i = 0; i < test->num_cycles; i++) {
#define DC(x) (test->cycles[i]. x == cpucycles[i]. x)
        u32 same = DC(addr) && DC(data) && DC(rw);
        printf("\n\n%cTEST cycle:%d  addr:%04x  data:%02x  rw:%d", same ? ' ' : '*', i, test->cycles[i].addr, test->cycles[i].data, test->cycles[i].rw);
        printf("\n MINE cycle:%d  addr:%04x  data:%02x  rw:%d", i, cpucycles[i].addr, cpucycles[i].data, cpucycles[i].rw);
        //pprint_P(cpucycles[i].data, test->cycles[i].data, 0);
    }
}

//#define PP(a, b) passed &= cpu->regs.a == final->regs.b; printf("\n%s %04x %04x %d", #a, cpu->regs.a, final->regs.b, passed)
#define PP(a, b) passed &= cpu->regs.a == final->regs.b

static u32 testregs(M6502::core* cpu, test_state* final, test_state* initial, u32 last_pc, u32 is_call, u32 tn)
{
    u32 passed = 1;
    u32 rpc = last_pc == final->regs.pc;
    if ((!rpc) && is_call && (((cpu->regs.PC - 1) & 0xFFFF) == final->regs.pc))
        rpc = 1;
    //printf("\nPC %04x %04x %d", last)
    passed &= rpc;
    PP(S, s);
    PP(A, a);
    PP(X, x);
    PP(Y, y);
    passed &= (cpu->regs.P.getbyte() & 0xEF) == (final->regs.p & 0xEF);
    //passed &= cpu->regs.Q == final->regs.q;
    if (passed == 0) {
        printf("\nFAILED ON REGS %d", tn);
        pprint_regs(&cpu->regs, &final->regs, &initial->regs, last_pc, true);
    }

    return passed;
}

static void test_m6502_automated(m6502_test_result *out, M6502::core* cpu, jsontest tests[NTESTS], u32 is_call)
{
    out->passed = 0;
    snprintf(out->msg, sizeof(out->msg), "");
    char *msgptr = out->msg;
    //out->length_mismatches = 0;
    out->failed_test_struct= nullptr;

    u32 last_pc;
    u32 ins;
    *cpu->trace.cycles = 1;
    for (u32 i = 0; i < NTESTS; i++) {
        out->failed_test_struct= &tests[i];
        jsontest *test = &tests[i];
        test_state *initial = &tests[i].initial;
        test_state *final = &tests[i].final;

        cpu->regs.PC = initial->regs.pc;
        cpu->regs.S = initial->regs.s;
        cpu->regs.A = initial->regs.a;
        cpu->regs.X = initial->regs.x;
        cpu->regs.Y = initial->regs.y;
        cpu->regs.P.setbyte(initial->regs.p);

        for (u32 j = 0; j < initial->num_ram_entry; j++) {
            test_RAM[initial->ram[j].addr] = static_cast<u8>(initial->ram[j].val);
        }

        cpu->pins.Addr = cpu->regs.PC;
        cpu->pins.RW = 0;
        cpu->regs.TCU = 0;
        cpu->pins.D = test_RAM[initial->regs.pc];
        cpu->regs.PC = (cpu->regs.PC + 1) & 0xFFFF;
        cpu->pins.RW = 0;
        test_cycle *out_cycle = &out->cycles[0];
        out_cycle->data = cpu->pins.D;
        out_cycle->rw = 0;
        out_cycle->addr = initial->regs.pc;

        u32 addr;
        u32 passed = 1;
        for (u32 cyclei = 1; cyclei < test->num_cycles; cyclei++) {
            test_cycle *cycle = &test->cycles[cyclei];

            out_cycle = &out->cycles[cyclei];

            addr = cpu->pins.Addr;

            cpu->cycle();
            if (cpu->pins.RW == 0) {
                if (test_RAM[cpu->pins.Addr] != 0xFFFFFFFF)
                    cpu->pins.D = test_RAM[cpu->pins.Addr];
            }
            addr = cpu->pins.Addr;

            if (cycle->addr != addr) {
                printf("\nMISMATCH IN PIN ADDR ME:%04x  TEST:%04x  STEP:%d", addr, cycle->addr, cyclei);
                passed = 0;
            }
            if ((cycle->data != cpu->pins.D) && (cycle->data != 0xFFFFFFFF)) {
                printf("\nMISMATCH IN DATA PINS!");
                passed = 0;
            }
            if (cpu->pins.RW != cycle->rw) {
                printf("\nMISMATCH IN RW. MINE:%d THEIRS:%d cycle:%d", cpu->pins.RW, cycle->rw, cyclei);
                passed = 0;
            }
            out_cycle->data = (i32)cpu->pins.D;
            out_cycle->addr = (i32)cpu->pins.Addr;
            out_cycle->rw = cpu->pins.RW;
            last_pc = cpu->regs.PC;

            if (cpu->pins.RW) {
                test_RAM[cpu->pins.Addr] = (u8)cpu->pins.D;
            }
        }
        if (!passed) {
            printf("\nFAILED TEST! %d", i);
            pprint_regs(&cpu->regs, &final->regs, &initial->regs, last_pc, 0);
            out->passed = 0;
            return;
        }

        cpu->cycle();
        if (cpu->regs.TCU != 0) {
            printf("\nLEN MISMATCH!? %d", cpu->regs.TCU);
            passed = 0;
        }
        last_pc = (cpu->regs.PC - 1) & 0xFFFF;
        passed &= testregs(cpu, final, initial, last_pc, is_call, i);
        for (u32 x = 0; x < final->num_ram_entry; x++) {
            if (test_RAM[final->ram[x].addr] != final->ram[x].val) {
                passed = 0;
                printf("\nRAM failed! addr:%04x mine:%02x test:%02x", final->ram[x].addr, test_RAM[final->ram[x].addr], final->ram[x].val);
            }
        }

        if (passed == 0) {
            printf("\nFAILED AT END!");
            out->passed = 0;
            pprint_regs(&cpu->regs, &final->regs, &initial->regs, last_pc, 0);
            return;
        }
    }
    out->passed = 1;
    out->failed_test_struct= nullptr;
}

jsontest tests[NTESTS];

u32 test_m6502_ins(M6502::core* cpu, u32 ins, u32 is_call, bool is_nes)
{
    char path[400];
    construct_path(path, ins, is_nes, sizeof(path));

    read_file_buf infile;
    if (!infile.read(nullptr, path)) {
        printf("\n\nCouldn't open file! %s", path);
        return 0;
    }
    parse_and_fill_out(tests, &infile);

    m6502_test_result result{};
    test_m6502_automated(&result, cpu, tests, is_call);
    if (!result.passed) {
        printf("\nFAILED INSTRUCTION: %02x", ins);
        pprint_test(result.failed_test_struct, result.cycles);
    }
    return result.passed;
}

static u32 read_trace_m6502(void *ptr, u32 addr)
{
    return test_RAM[addr];
}

void test_6502(M6502::ins_func *funcs, bool is_nes)
{
    dbg_init();
    M6502::core cpu{funcs};
    u64 trace_cycles = 0;
    jsm_debug_read_trace rd;
    u32 total_fail = 0;
    rd.ptr = nullptr;
    rd.read_trace = &read_trace_m6502;
    cpu.setup_tracing(&rd, &trace_cycles);
    u32 start_test = 0;
    //TODO POLL IRQ AT PROPER TIME

    for (u32 i = start_test; i < 256; i++) {
        if (skip_tests(i)) {
            printf("\nTest for %02x skipped.", i);
            continue;
        }
        printf("\nTesting %02x", i);
        u32 result = test_m6502_ins(&cpu, i, 0, is_nes);
        if (!result) {
            printf("\nFAIL!");
            total_fail = 1;
            break;
        }
    }
    if (!total_fail) {
        printf("\nCongrats!");
    }
}

void test_nesm6502()
{
    printf("\nTesting NES m6502...");
    test_6502(M6502::nesdecoded_opcodes, true);
}

void test_regular6502()
{
    printf("\nTesting regular m6502...");
    test_6502(M6502::decoded_opcodes, false);
}