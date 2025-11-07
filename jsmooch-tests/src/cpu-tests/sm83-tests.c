//
// Created by Dave on 1/25/2024.
//

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif
#include "sm83-tests.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "string.h"

#include "helpers_c/int.h"
#include "helpers_c/user.h"
#include "rfb.h"
#include "component/cpu/sm83/sm83.h"
#include "../json.h"

struct test_cpu_regs {
    u32 pc; u32 sp;
    u32 a; u32 b; u32 c; u32 d;
    u32 e; u32 f; u32 h; u32 l;
    u32 ime; u32 ie;
};

struct ram_entry {
    u32 addr;
    u32 val;
};

struct test_state {
    struct test_cpu_regs regs;
    u32 num_ram_entry;
    struct ram_entry ram[50];
};

struct test_cycle {
    i32 addr;
    i32 data;
    u32 r;
    u32 w;
    u32 m; // memory request
};

static u8 test_RAM[65536];

struct jsontest {
    char name[50];
    struct test_state initial;
    struct test_state final;
    struct test_cycle cycles[20];
    u32 num_cycles;

    struct ram_entry opcodes[5];
};

u32 skip_test(u32 prefix, u32 ins) {
    switch(prefix) {
        case 0:
            switch(ins) {
                case 0x10:
                case 0xCB:
                    return 1;
                case 0x76: // HALT
                    return 1;
                case 0xD3: // invalid opcodes
                case 0xDB:
                case 0xDD:
                case 0xE3:
                case 0xE4:
                case 0xEB:
                case 0xEC:
                case 0xED:
                case 0xF4:
                case 0xFC:
                case 0xFD:
                    return 1;
                default:
                    return 0;
            }
        default:
            return 0;
    }
}
u32 is_call(u32 prefix, u32 ins) {
    switch(prefix) {
        case 0:
            switch(ins) {
                case 0x18:
                case 0x20:
                case 0x28:
                case 0x30:
                case 0x38:
                case 0xC0:
                case 0xC2:
                case 0xC3:
                case 0xC4:
                case 0xC5:
                case 0xC8:
                case 0xC9:
                case 0xCA:
                case 0xCC:
                case 0xCD:
                case 0xD0:
                case 0xD2:
                case 0xD4:
                case 0xD8:
                case 0xD9:
                case 0xDA:
                case 0xDC:
                case 0xE9:
                    return 1;
                default:
                    return 0;
            }
        case 0xCB:
            return 0;
        default:
            return 0;
    }
}

static void pprint_regs(struct SM83_regs *cpu_regs, struct test_cpu_regs *test_regs, u32 last_pc, u32 only_print_diff)
{
    printf("\nREG CPU    TEST");
    printf("\n----------------");
    if ((only_print_diff && (last_pc != test_regs->pc)) || (!only_print_diff))
        printf("\nPC  %04x   %04x", last_pc, test_regs->pc);
    if ((only_print_diff && (cpu_regs->SP != test_regs->sp)) || (!only_print_diff))
        printf("\nSP  %04x   %04x", cpu_regs->SP, test_regs->sp);
    if ((only_print_diff && (cpu_regs->A != test_regs->a)) || (!only_print_diff))
        printf("\nA   %02x     %02x", cpu_regs->A, test_regs->a);
    if ((only_print_diff && (cpu_regs->B != test_regs->b)) || (!only_print_diff))
        printf("\nB   %02x     %02x", cpu_regs->B, test_regs->b);
    if ((only_print_diff && (cpu_regs->C != test_regs->c)) || (!only_print_diff))
        printf("\nC   %02x     %02x", cpu_regs->C, test_regs->c);
    if ((only_print_diff && (cpu_regs->D != test_regs->d)) || (!only_print_diff))
        printf("\nD   %02x     %02x", cpu_regs->D, test_regs->d);
    if ((only_print_diff && (cpu_regs->E != test_regs->e)) || (!only_print_diff))
        printf("\nE   %02x     %02x", cpu_regs->E, test_regs->e);
    if ((only_print_diff && (cpu_regs->H != test_regs->h)) || (!only_print_diff))
        printf("\nH   %02x     %02x", cpu_regs->H, test_regs->h);
    if ((only_print_diff && (cpu_regs->L != test_regs->l)) || (!only_print_diff))
        printf("\nL   %02x     %02x", cpu_regs->L, test_regs->l);
    if ((only_print_diff && (SM83_regs_F_getbyte(&cpu_regs->F) != test_regs->f)) || (!only_print_diff))
        printf("\nF   %02x     %02x", SM83_regs_F_getbyte(&cpu_regs->F), test_regs->f);
}

static u32 testregs(struct SM83* cpu, struct test_state* final, u32 last_pc, u32 is_call)
{
    u32 passed = 1;
    u32 rpc = cpu->regs.PC == last_pc;
    if (!rpc && (((cpu->regs.PC-1)&0xFFFF) == last_pc) && is_call)
        rpc = 1;
    passed &= rpc;
    passed &= cpu->regs.SP == final->regs.sp;
    passed &= cpu->regs.A == final->regs.a;
    passed &= cpu->regs.B == final->regs.b;
    passed &= cpu->regs.C == final->regs.c;
    passed &= cpu->regs.D == final->regs.d;
    passed &= cpu->regs.E == final->regs.e;
    passed &= SM83_regs_F_getbyte(&cpu->regs.F) == final->regs.f;
    passed &= cpu->regs.H == final->regs.h;
    passed &= cpu->regs.L == final->regs.l;
    if (passed == 0) {
        printf("\nFAILED ON REGS");
        pprint_regs(&cpu->regs, &final->regs, cpu->regs.PC, true);
    }

    return passed;

}

static void construct_path(char *out, u32 iclass, u32 ins)
{
    char test_path[500];

    const char *homeDir = get_user_dir();
    char *tp = out;
    tp += sprintf(tp, "%s", homeDir);
    tp += sprintf(tp, "/dev/jsmoo/misc/tests/GeneratedTests/sm83/v1");

    tp += sprintf(tp, "%s/", test_path);
    if (iclass != 0)
        tp += sprintf(tp, "cb ");
    tp += sprintf(tp, "%02x.json", ins);
}

static void parse_state(struct json_object_s *object, struct test_state *state)
{
    struct json_object_element_s *el = object->start;
    state->num_ram_entry = 0;
    for (u32 i = 0; i < object->length; i++) {
        struct json_string_s *str = (struct json_string_s *)el->value->payload;
        u32 *dest = 0;
        if (strcmp(el->name->string, "a") == 0) {
            dest = &state->regs.a;
        }
        else if (strcmp(el->name->string, "b") == 0) {
            dest = &state->regs.b;
        }
        else if (strcmp(el->name->string, "c") == 0) {
            dest = &state->regs.c;
        }
        else if (strcmp(el->name->string, "d") == 0) {
            dest = &state->regs.d;
        }
        else if (strcmp(el->name->string, "e") == 0) {
            dest = &state->regs.e;
        }
        else if (strcmp(el->name->string, "f") == 0) {
            dest = &state->regs.f;
        }
        else if (strcmp(el->name->string, "h") == 0) {
            dest = &state->regs.h;
        }
        else if (strcmp(el->name->string, "l") == 0) {
            dest = &state->regs.l;
        }
        else if (strcmp(el->name->string, "pc") == 0) {
            dest = &state->regs.pc;
        }
        else if (strcmp(el->name->string, "sp") == 0) {
            dest = &state->regs.sp;
        }
        else if (strcmp(el->name->string, "ime") == 0) {
            dest = &state->regs.ime;
        }
        else if (strcmp(el->name->string, "ie") == 0) {
            dest = &state->regs.ie;
        }
        else if (strcmp(el->name->string, "ei") == 0) {
            dest = &state->regs.ie;
        }
        if (strcmp(el->name->string, "ram") == 0) {
            struct json_array_s *arr1 = (struct json_array_s *)el->value->payload;
            state->num_ram_entry = (u32)arr1->length;
            struct json_array_element_s *arr_el = arr1->start;
            for (u32 arr1_i = 0; arr1_i < arr1->length; arr1_i++) {
                assert(arr_el->value->type == json_type_array);
                struct json_array_s *arr2 = (struct json_array_s*) arr_el->value->payload;
                assert(arr2->length == 2);
                struct json_array_element_s *arr2_el = arr2->start;
                // Address
                struct json_number_s* nr = (struct json_number_s*)arr2_el->value->payload;
                state->ram[arr1_i].addr = atoi(nr->number);
                // Data
                arr2_el = arr2_el->next;
                nr = (struct json_number_s*)arr2_el->value->payload;
                state->ram[arr1_i].val = atoi(nr->number);

                arr_el = arr_el->next;
            }
        }
        else {
            // Grab number
            assert(el->value->type == json_type_number);
            struct json_number_s *num = (struct json_number_s *) el->value->payload;
            char *yo;
            u32 a = strtol(num->number, &yo, 10);
            assert(yo != num->number);
            if (dest == NULL) {
                printf("\nUHOH! %s \n", el->name->string);
            }
            else {
                *dest = a;
            }
        }
        el = el->next;
    }
}

static void parse_and_fill_out(struct jsontest tests[1000], struct read_file_buf *infile)
{
    struct json_value_s *root = json_parse(infile->buf.ptr, infile->buf.size);
    assert(root->type == json_type_array);

    struct json_array_s* arr = (struct json_array_s*)root->payload;
    struct json_array_element_s r;
    assert(arr->length == 1000);
    struct json_array_element_s* cur_node = arr->start;
    for (u32 i = 0; i < 1000; i++) {
        struct jsontest *test = tests+i;
        test->num_cycles = 0;
        // TODO: add opcodes here

        sprintf(test->name, "Unknown");
        assert(cur_node->value->type == json_type_object);
        struct json_object_s *tst = (struct json_object_s *)cur_node->value->payload;
        struct json_object_element_s *s = (struct json_object_element_s *)tst->start;
        for (u32 j = 0; j < tst->length; j++) {
            if (strcmp(s->name->string, "name") == 0) {
                assert(s->value->type == json_type_string);
                struct json_string_s *str = (struct json_string_s *)s->value->payload;
                sprintf(test->name,"%s", str->string);
            }
            else if (strcmp(s->name->string, "initial") == 0) {
                assert(s->value->type == json_type_object);
                struct json_object_s* state = (struct json_object_s *)s->value->payload;
                parse_state(state, &test->initial);
            }
            else if (strcmp(s->name->string, "final") == 0) {
                assert(s->value->type == json_type_object);
                struct json_object_s* state = (struct json_object_s*)s->value->payload;
                parse_state(state, &test->final);
            }
            else if (strcmp(s->name->string, "cycles") == 0) {
                assert(s->value->type == json_type_array);
                struct json_array_s* arr1 = (struct json_array_s*)s->value->payload;
                test->num_cycles = (u32)arr1->length;
                struct json_array_element_s* arr1_el = arr1->start;
                for (u32 h = 0; h < arr1->length; h++) {
                    assert(arr1_el != NULL);
                    assert(arr1->length < 20);
                    struct json_array_s* arr2 = (struct json_array_s*)arr1_el->value->payload;
                    struct json_array_element_s* arr2_el = (struct json_array_element_s*)arr2->start;

                    // number, number, string
                    struct json_number_s *num = (struct json_number_s *)arr2_el->value->payload;

                    test->cycles[h].addr = atoi(num->number);

                    arr2_el = arr2_el->next;
                    num = (struct json_number_s *)arr2_el->value->payload;

                    test->cycles[h].data = atoi(num->number);
                    arr2_el = arr2_el->next;

                    struct json_string_s* st = (struct json_string_s *)arr2_el->value->payload;
                    test->cycles[h].r = st->string[0] == 'r' ? 1 : 0;
                    test->cycles[h].w = st->string[1] == 'w' ? 1 : 0;
                    test->cycles[h].m = st->string[2] == 'm' ? 1 : 0;

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

struct sm83_test_result
{
    u32 passed;
    u32 mycycles;
    struct test_cycle cycles[100];
    char msg[5000];
    u32 addr_io_mismatches;
    u32 length_mismatches;
    struct jsontest *failed_test_struct;
};

static void pprint_test(struct jsontest *test, struct test_cycle *cpucycles) {
    printf("\nCycles");
    for (u32 i = 0; i < test->num_cycles; i++) {
        printf("\n\nTEST cycle:%d  addr:%04x  data:%02x  rwm:%d%d%d", i, test->cycles[i].addr, test->cycles[i].data, test->cycles[i].r, test->cycles[i].w, test->cycles[i].m);
        printf("\nCPU  cycle:%d  addr:%04x  data:%02x  rwm:%d%d%d", i, cpucycles[i].addr, cpucycles[i].data, cpucycles[i].r, cpucycles[i].w, cpucycles[i].m);
    }
}

void test_sm83_automated(struct sm83_test_result *out, struct SM83* cpu, struct jsontest tests[1000], u32 is_call)
{
    out->passed = 0;
    out->mycycles = 0;
    sprintf(out->msg, "");
    char *msgptr = out->msg;
    out->addr_io_mismatches = 0;
    out->length_mismatches = 0;
    out->failed_test_struct = NULL;

    u32 last_pc;
    u32 ins;
    for (u32 i = 0; i < 1000; i++) {
        out->failed_test_struct = &tests[i];
        struct jsontest *test = &tests[i];
        struct test_state *initial = &tests[i].initial;
        struct test_state *final = &tests[i].final;
        cpu->regs.PC = initial->regs.pc;
        last_pc = cpu->regs.PC;
        cpu->regs.SP = initial->regs.sp;
        cpu->regs.A = initial->regs.a;
        cpu->regs.B = initial->regs.b;
        cpu->regs.C = initial->regs.c;
        cpu->regs.D = initial->regs.d;
        cpu->regs.E = initial->regs.e;
        SM83_regs_F_setbyte(&cpu->regs.F, initial->regs.f);
        cpu->regs.H = initial->regs.h;
        cpu->regs.L = initial->regs.l;

        for (u32 j = 0; j < initial->num_ram_entry; j++) {
            test_RAM[initial->ram[j].addr] = (u8)initial->ram[j].val;
        }

        cpu->regs.IR = SM83_S_DECODE;
        cpu->pins.Addr = cpu->regs.PC;
        //printf("SETTING ADDR %04x", cpu->pins.Addr);
        cpu->pins.D = test_RAM[cpu->regs.PC];
        cpu->pins.MRQ = 1;
        cpu->pins.RD = 1;
        cpu->regs.TCU = 0;
        cpu->regs.PC = (cpu->regs.PC + 1) & 0xFFFF;

        u32 addr;
        u32 passed = 1;
        for (u32 cyclei = 0; cyclei < test->num_cycles; cyclei++) {
            struct test_cycle *cycle = &test->cycles[cyclei];

            struct test_cycle *out_cycle = &out->cycles[cyclei];

            addr = cpu->pins.Addr;
            if (cpu->pins.RD && cpu->pins.MRQ) {
                cpu->pins.D = test_RAM[cpu->pins.Addr];
            }

            if (cycle->addr != addr) {
                printf("\nMISMATCH IN PIN ADDR ME:%04x  TEST:%04x  STEP:%d", addr, cycle->addr, cyclei);
                passed = 0;
            }
            if (cycle->data != cpu->pins.D) {
                printf("\nMISMATCH IN DATA PINS!");
                passed = 0;
            }
            if (cpu->pins.RD != cycle->r) {
                printf("\nMISMATCH IN R");
                passed = 0;
            }
            if (cpu->pins.WR != cycle->w) {
                printf("\nMISMATCH IN W. MINE:%d THEIRS:%d cycle:%d", cpu->pins.WR, cycle->w, cyclei);
                passed = 0;
            }
            if (cpu->pins.MRQ != cycle->m) {
                printf("\nMISMATCH IN M");
                passed = 0;
            }
            out_cycle->data = (i32)cpu->pins.D;
            out_cycle->addr = (i32)cpu->pins.Addr;
            out_cycle->r = cpu->pins.RD;
            out_cycle->w = cpu->pins.WR;
            out_cycle->m = cpu->pins.MRQ;

            SM83_cycle(cpu);

            last_pc = cpu->regs.PC;
            if (cyclei == (test->num_cycles-1)) {
                if (cpu->regs.IR != SM83_S_DECODE) {
                    printf("\nLENGTH MISMATCH! IR:%d cycle:%d", cpu->regs.IR, cyclei);
                    out->length_mismatches++;
                }
            }

            if (cpu->pins.WR && cpu->pins.MRQ) {
                test_RAM[cpu->pins.Addr] = (u8)cpu->pins.D;
            }
        }
        if (!passed) {
            printf("\nFAILED TEST! %d", i);
            SM83_cycle(cpu);
            out->passed = 0;
            return;
        }

        //SM83_cycle(cpu);
        passed &= testregs(cpu, final, cpu->regs.PC, is_call);
        for (u32 x = 0; x < final->num_ram_entry; x++) {
            if (test_RAM[final->ram[x].addr] != final->ram[x].val) {
                passed = 0;
                printf("\nRAM failed! addr:%04x mine:%02x test:%02x", final->ram[x].addr, test_RAM[final->ram[x].addr], final->ram[x].val);
            }
        }

        if (passed == 0) {
            printf("\nFAILED AT END!");
            SM83_cycle(cpu);
            out->passed = 0;
            return;
        }
    }
    out->passed = 1;
    out->failed_test_struct = NULL;
}

u32 test_sm83_ins(struct SM83 *cpu, u32 iclass, u32 ins, u32 is_call)
{
    printf("\nTesting instruction %02x %02x", iclass ? 0xCB : 0, ins);
    char path[200];
    construct_path(path, iclass, ins);
    struct read_file_buf infile;
    struct jsontest tests[1000];

    /*open_and_read(path, &infile);
    if (infile.success == 0) {
        printf("\n\nCouldn't open file! %s", path);
        return 0;
    }

    parse_and_fill_out(tests, &infile);

    struct sm83_test_result result;
    test_sm83_automated(&result, cpu, tests, is_call);
    if (!result.passed) {
        printf("\nFAILED INSTRUCTION: %02x %02x", iclass ? 0xCB : 0, ins);
        pprint_test(result.failed_test_struct, result.cycles);
    }
    rfb_cleanup(&infile);*/
    assert(1==2);

    //return result.passed;
    return 0;
}

void test_sm83()
{
    struct SM83 cpu;
    SM83_init(&cpu);
    u32 test_classes[2] = { 0, 0xCB };
    u32 total_fail = 0;
    u32 start_test = 0;
    for (u32 tci = 0; tci < 2; tci++) {
        u32 iclass = test_classes[tci];
        for (u32 i = start_test; i < 256; i++) {
            if (skip_test(iclass, i)) {
                printf("\nSkip test %02x", i);
                continue;
            }
            u32 result = test_sm83_ins(&cpu, iclass, i, is_call(iclass, i));
            if (!result) {
                total_fail = 1;
                break;
            }
            if (total_fail) break;
            printf("\nTest for %02x %02x passed.", iclass, i);
        }
        if (total_fail) break;
    }
}
