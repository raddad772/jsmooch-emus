//
// Created by . on 4/17/25.
//

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include "../json.h"

#define MAX_RAM_ENTRIES 50

#include "helpers/int.h"
#include "wdc65816_tests.h"

#include "component/cpu/wdc65816/wdc65816.h"

struct test_cpu_regs {
    u32 C, DBR, D, X, Y, P, PBR, PC, S, E;
};

struct ram_entry {
    u32 addr, val;
};

struct test_state {
    struct test_cpu_regs regs;
    u32 num_ram_entry;
    struct ram_entry ram[MAX_RAM_ENTRIES];
};

struct test_cycle {
    u32 VDA, VPA, VPB, RWB, E, M, X, MLB;
    u32 PDV;
    i32 addr, data;
};

struct jsontest {
    char name[50];
    struct test_state initial;
    struct test_state final;
    struct test_cycle cycles[500];

    u32 num_ram_entry;
    struct ram_entry ram[MAX_RAM_ENTRIES];
    u32 num_cycles;
};

struct wdc65816_test_result
{
    u32 passed;
    u32 mycycles;
    struct test_cycle cycles[500];
    char msg[5000];
    u32 addr_io_mismatches;
    u32 length_mismatches;
    struct jsontest *failed_test_struct;
};


static void construct_path(char *out, u32 iclass, u32 ins)
{
    char test_path[500];
    memset(test_path, 0, sizeof(test_path));
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char *tp = out;
    tp += sprintf(tp, "%s", homeDir);
    tp += sprintf(tp, "/dev/external/ProcessorTests/65816/v1");

    tp += sprintf(tp, "%s/", test_path);
    tp += sprintf(tp, "%02x.%c.json", ins, iclass ? 'e' : 'n');
}

static struct jsontest tests[10000];

static void parse_state(struct json_object_s *object, struct test_state *state)
{
    struct json_object_element_s *el = object->start;
    state->num_ram_entry = 0;
    for (u32 i = 0; i < object->length; i++) {
        struct json_string_s *str = (struct json_string_s *)el->value->payload;
        u32 *dest = 0;
        if (strcmp(el->name->string, "pc") == 0) {
            dest = &state->regs.PC;
        }
        else if (strcmp(el->name->string, "s") == 0) {
            dest = &state->regs.S;
        }
        else if (strcmp(el->name->string, "p") == 0) {
            dest = &state->regs.P;
        }
        else if (strcmp(el->name->string, "a") == 0) {
            dest = &state->regs.C;
        }
        else if (strcmp(el->name->string, "x") == 0) {
            dest = &state->regs.X;
        }
        else if (strcmp(el->name->string, "y") == 0) {
            dest = &state->regs.Y;
        }
        else if (strcmp(el->name->string, "dbr") == 0) {
            dest = &state->regs.DBR;
        }
        else if (strcmp(el->name->string, "d") == 0) {
            dest = &state->regs.D;
        }
        else if (strcmp(el->name->string, "pbr") == 0) {
            dest = &state->regs.PBR;
        }
        else if (strcmp(el->name->string, "e") == 0) {
            dest = &state->regs.E;
        }
        if (strcmp(el->name->string, "ram") == 0) {
            struct json_array_s *arr1 = (struct json_array_s *)el->value->payload;
            state->num_ram_entry = arr1->length;
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

static void parse_and_fill_out(struct read_file_buf *infile)
{
    struct json_value_s *root = json_parse(infile->buf.ptr, infile->buf.size);
    assert(root->type == json_type_array);

    struct json_array_s* arr = (struct json_array_s*)root->payload;
    struct json_array_element_s r;
    assert(arr->length == 10000);
    struct json_array_element_s* cur_node = arr->start;
    for (u32 i = 0; i < arr->length; i++) {
        struct jsontest *test = tests + i;
        test->num_cycles = 0;
        sprintf(test->name, "unk");
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
                test->num_cycles = arr1->length;
                struct json_array_element_s* arr1_el = arr1->start;
                for (u32 h = 0; h < arr1->length; h++) {
                    assert(arr1_el != NULL);
                    assert(arr1->length < 500);
                    struct json_array_s* arr2 = (struct json_array_s*)arr1_el->value->payload;
                    struct json_array_element_s* arr2_el = (struct json_array_element_s*)arr2->start;

                    // number, number, string
                    struct json_number_s *num = (struct json_number_s *)arr2_el->value->payload;

                    if (num == NULL) test->cycles[h].addr = -1;
                    else test->cycles[h].addr = atoi(num->number);

                    arr2_el = arr2_el->next;
                    num = (struct json_number_s *)arr2_el->value->payload;

                    if (num == NULL) test->cycles[h].data = -1;
                    else test->cycles[h].data = atoi(num->number);
                    arr2_el = arr2_el->next;

                    struct json_string_s* st = (struct json_string_s *)arr2_el->value->payload;
                    assert(st->string_size >= 8);
                    test->cycles[h].VDA = st->string[0] == 'd' ? 1 : 0;
                    test->cycles[h].VPA = st->string[1] == 'p' ? 1 : 0;
                    test->cycles[h].VPB = st->string[2] == 'v' ? 1 : 0;
                    test->cycles[h].RWB = st->string[3] == 'r' ? 1 : 0;
                    test->cycles[h].E = st->string[4] == 'e' ? 1 : 0;
                    test->cycles[h].M = st->string[5] == 'm' ? 1 : 0;
                    test->cycles[h].X = st->string[6] == 'x' ? 1 : 0;
                    test->cycles[h].MLB = st->string[7] == 'l' ? 1 : 0;

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

static void copy_state_to_cpu(struct test_state *state, struct WDC65816 *cpu)
{
    //    u32 C, DBR, D, X, Y, P, PBR, PC, S, E;
    cpu->regs.C = state->regs.C;
    cpu->regs.DBR = state->regs.DBR;
    cpu->regs.D = state->regs.D;
    cpu->regs.X = state->regs.X;
    cpu->regs.Y = state->regs.Y;
    cpu->regs.P.v = state->regs.P;
    cpu->regs.PBR = state->regs.PBR;
    cpu->regs.PC = (state->regs.PC+1)&0xFFFF;
    cpu->regs.S = state->regs.S;
    cpu->regs.E = state->regs.E;
}

static void pprint_regs(struct WDC65816_regs *cpu_regs, struct test_cpu_regs *test_regs, u32 last_pc, u32 only_print_diff)
{
    printf("\nREG  CPU    TEST");
    printf("\n----------------");
#define TREG4(cpuname, testname, strname) if ((only_print_diff && (cpu_regs->cpuname != test_regs->testname)) || (!only_print_diff))\
        printf("\n" strname "  %04x   %04x", cpu_regs->cpuname, test_regs->testname);
    TREG4(C, C, "Acc.");
    TREG4(X, X, "X   ");
    TREG4(Y, Y, "Y   ");
    TREG4(PC, PC, "PC  ");
    printf("\nlast_pc: %04x", last_pc);
    TREG4(D, D, "D   ");
    TREG4(DBR, DBR, "DBR ");
    TREG4(PBR, PBR, "PBR ");
    TREG4(S, S, "S ");
    TREG4(E, E, "E   ");
    TREG4(P.v, P, "P    ");
#undef TREG4
}
static u32 testregs(struct WDC65816 *cpu, struct test_state *final, u32 last_pc)
{
    u32 passed = 1;
    // u32 C, DBR, D, X, Y, P, PBR, PC, S, E;
    passed &= (((cpu->regs.PC-1)&0xFFFF) == final->regs.PC) || (last_pc == final->regs.PC);
    passed &= cpu->regs.C == final->regs.C;
    passed &= cpu->regs.DBR == final->regs.DBR;
    passed &= cpu->regs.D == final->regs.D;
    passed &= cpu->regs.X == final->regs.X;
    passed &= cpu->regs.Y == final->regs.Y;
    passed &= cpu->regs.P.v == final->regs.P;
    passed &= cpu->regs.PBR == final->regs.PBR;
    passed &= cpu->regs.S == final->regs.S;
    passed &= cpu->regs.E == final->regs.E;
    if (!passed) {
        pprint_regs(&cpu->regs, &final->regs, last_pc, 1);
    }
    return passed;
}

static int test_wdc65816_automated(struct wdc65816_test_result *out, struct WDC65816 *cpu) {
    out->passed = 0;
    out->mycycles = 0;
    sprintf(out->msg, "");
    char *msgptr = out->msg;
    out->addr_io_mismatches = 0;
    out->length_mismatches = 0;
    out->failed_test_struct = NULL;
    u32 last_pc;
    u32 ins;
    for (u32 i = 0; i < 10000; i++) {
        //printf("\n\nTest #%d/10000", i);
        out->failed_test_struct = &tests[i];
        struct jsontest *test = &tests[i];
        struct test_state *initial = &tests[i].initial;
        struct test_state *final = &tests[i].final;

        copy_state_to_cpu(initial, cpu);
        u32 test_pc = (initial->regs.PBR << 16) | initial->regs.PC;

        test->num_ram_entry = initial->num_ram_entry;
        i32 dataval = -1;
        for (u32 j = 0; j < initial->num_ram_entry; j++) {
            test->ram[j].addr = initial->ram[j].addr;
            test->ram[j].val = initial->ram[j].val;
            if (test->ram[j].addr == test_pc) dataval = test->ram[j].val;
        }

        cpu->regs.TCU = 0;
        cpu->pins.Addr = (cpu->regs.PC-1) & 0xFFFF;
        assert(dataval != -1);
        cpu->regs.IR = cpu->pins.D;
        cpu->pins.D = dataval;
        cpu->pins.PDV = 1;
        cpu->pins.RW = 0;
        cpu->pins.BA = cpu->regs.PBR;
        //cpu->regs.PC = (cpu->regs.PC + 1) & 0xFFFF;

        u32 addr, passed = 1;
        for (u32 cyclei = 0; cyclei < test->num_cycles; cyclei++) {
            //printf("\nCycle #%d/%d", cyclei, test->num_cycles-1);
            struct test_cycle *cycle = &test->cycles[cyclei];
            struct test_cycle *out_cycle = &out->cycles[cyclei];
            u32 iocycle = cycle->VDA || cycle->VPA;
            addr = cpu->pins.Addr | (cpu->pins.BA << 16);

            if (cpu->pins.PDV && !cpu->pins.RW) { // Read request!
                i32 outdata = -1;
                for (u32 j = 0; j < test->num_ram_entry; j++) {
                    struct ram_entry *re = &test->ram[j];
                    if (re->addr == addr) {
                        outdata = re->val;
                        break;
                    }
                }
                if (outdata == -1) {
                    printf("\nREAD NOT FOUND TO %06x", addr);
                    passed = 0;
                    outdata = 0;
                }
                cpu->pins.D = outdata;
            }

            if (iocycle && cycle->addr != addr) {
                printf("\nMISMATCH IN PIN ADDR ME:%06x   TEST:%06x   STEP:%d", addr, cycle->addr, cyclei);
                passed = 0;
            }
            if (iocycle && cycle->data != cpu->pins.D) {
                printf("\nMISMATCH IN DATA PINS!");
                passed = 0;
            }
            u32 test_PDV = cycle->VDA | cycle->VPA | cycle->VPB;
            if (test_PDV != cpu->pins.PDV) {
                printf("\nMISMATCH IN PDV!");
                passed = 0;
            }
            if (cpu->pins.RW != (cycle->RWB ^ 1)) {
                printf("\nMISMATCH IN RWB! TEST:%d  ME:%d", cycle->RWB ^ 1, cpu->pins.RW);
                passed = 0;
            }

            out_cycle->data = cpu->pins.D;
            out_cycle->addr = cpu->pins.Addr | (cpu->pins.BA << 16);
            out_cycle->PDV = cpu->pins.PDV;
            out_cycle->RWB = cpu->pins.RW;

            last_pc = cpu->regs.PC;

            if (i == 8) {
                int a = 4;
                a++;
            }

            WDC65816_cycle(cpu);

            addr = cpu->pins.Addr | (cpu->pins.BA << 16);

            if (cpu->pins.PDV && cpu->pins.RW) {
                i32 found_addr = -1;
                for (u32 j = 0; j < test->num_ram_entry; j++) {
                    if (addr == test->ram[j].addr) {
                        found_addr = j;
                        break;
                    }
                }
                if (found_addr == -1) {
                    test->ram[test->num_ram_entry].addr = addr;
                    test->ram[test->num_ram_entry].val = cpu->pins.D;
                    test->num_ram_entry++;
                } else {
                    test->ram[found_addr].val = cpu->pins.D;
                }
            }
        }
        if (!passed) {
            printf("\nFAILED TEST! %d", i);
            printf("\nP:%02x  E:%d M:%d X:%d", cpu->regs.P.v, cpu->regs.E, cpu->regs.P.M, cpu->regs.P.X);
            WDC65816_cycle(cpu);
            out->passed = 0;
            return 0;
        }

        passed &= testregs(cpu, final, last_pc);
        if (final->num_ram_entry != test->num_ram_entry) {
            printf("\nFAILED NUM RAM ENTRY!");
            passed = 0;
        }
        for (u32 x = 0; x < final->num_ram_entry; x++) {
            u32 raddr = final->ram[x].addr;
            u32 rdata = final->ram[x].val;
            i32 addr_found = -1;

            for (u32 j = 0; j < test->num_ram_entry; j++) {
                if (test->ram[j].addr == raddr) {
                    addr_found = j;
                    break;
                }
            }
            if (addr_found == -1) {
                passed = 0;
                printf("\nRAMADDR FAIL! TEST:%06x", final->ram[x].addr, test->ram[x].addr);

                printf("\ntest               me               initial");
                for (u32 j = 0; j < final->num_ram_entry; j++) {
                    printf("\n%d %06x: %02x         %06x: %02x     %06x:%02x", j, final->ram[j].addr, final->ram[j].val, test->ram[j].addr, test->ram[j].val, initial->ram[j].addr, initial->ram[j].val);
                }
            }
            else if (final->ram[x].val != test->ram[addr_found].val) {
                passed = 0;
                printf("\nRAMDATA FAIL! TEST:%02x   ME:%02x", final->ram[x].val, test->ram[x].val);
            }
        }

        if (!passed) {
            printf("\nFAILED AT END!");
            WDC65816_cycle(cpu);
            out->passed = 0;
            return 0;
        }
    }
    out->passed = 1;
    return 1;
}

static u32 test_wdc65816_ins(struct WDC65816 *cpu, u32 iclass, u32 ins)
{
    printf("\n\nTesting instruction %02x.%c", ins, iclass ? 'e' : 'n');
    char path[500];
    construct_path(path, iclass, ins);
    struct read_file_buf infile;
    rfb_init(&infile);
    rfb_read(NULL, path, &infile);
    if (infile.buf.size < 10) {
        printf("\nBAD TEST! %s", path);
        return 0;
    }

    parse_and_fill_out(&infile);
    rfb_delete(&infile);

    struct wdc65816_test_result result;

    test_wdc65816_automated(&result, cpu);
    if (!result.passed) {
        printf("\nFAILED INSTRUCTION!");
    }

    return result.passed;
}

void test_wdc65816()
{
    struct WDC65816 cpu;;
    u64 cycle_ptr = 0;
    WDC65816_init(&cpu, &cycle_ptr);
    u32 total_fail = 0;
    u32 start_test = 0;
    for (u32 iclass = 0; iclass < 2; iclass++) {
        for (u32 i = start_test; i < 0x100; i++) {
            u32 result = test_wdc65816_ins(&cpu, iclass, i);
            if (!result) {
                total_fail = 1;
                break;
            }
            printf("\nTest for %02x.%c passed.", i, iclass ? 'e' : 'n');
        }
        if (total_fail) break;
    }
}