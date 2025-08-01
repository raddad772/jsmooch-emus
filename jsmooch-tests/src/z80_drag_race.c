//
// Created by . on 2/6/25.
//

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "z80_drag_race.h"

#include "component/cpu/z80/z80.h"
#include "helpers/buf.h"
#include "helpers/user.h"

const u8 cpm_print_routine[29] = {
    0xF5, // PUSH AF
    0xD5, // PUSH DE
    0x79, // LD A, C
    0xFE, 0x02, // CP 2
    0x28, 0x07, // JR Z, 7
    0xFE, 0x09, // CP 9
    0x28, 0x08, // JR Z, 8
    0xD1, // POP DE
    0xF1, // POP AF
    0xC9, // RET
    0x7B, // LD A, E
    0xD3, 0x00, // OUT (0), A
    0x18, 0xF8, // JR -8
    0x1A, // LD A, (DE)
    0xFE, 0x24, // CP 36
    0x28, 0xF3, // JR Z, -13
    0xD3, 0x00, // OUT (0), A
    0x13, // INC DE
    0x18, 0xF6  // JR -10
};

u32 trace_read(void *ptr, u32 addr)
{
    return ((u8 *)ptr)[addr & 0xFFFF];
}

static u8 mem[64 * 1024];

void z80_drag_race()
{

    struct Z80 cpu;
    Z80_init(&cpu, 0);

    memset(mem, 0, sizeof(mem));
    mem[0] = 0x76; // HALT
    memcpy(mem+5, cpm_print_routine, sizeof(cpm_print_routine));
    struct read_file_buf rfb;
    rfb_init(&rfb);

    const char *homeDir = get_user_dir();
    char path[500];
    snprintf(path, sizeof(path), "%s/dev/zexall.cim", homeDir);
    rfb_read(NULL, path, &rfb);
    printf("\nFile size: %lld", rfb.buf.size);

    memcpy(mem+0x100, rfb.buf.ptr, rfb.buf.size);

    rfb_delete(&rfb);


    struct jsm_debug_read_trace dt;
    dt.read_trace = &trace_read;
    dt.ptr = mem;
    u64 numcycles = 0;
    Z80_setup_tracing(&cpu, &dt, &numcycles);

    u32 last_ins_halt = 0;
    clock_t begin = clock();
    Z80_reset(&cpu);
    cpu.regs.reset_vector = 0x100;

    //dbg.trace_on = 1;
    //dbg.traces.z80.instruction = 1;
    while(cpu.regs.HALT == 0) {
        Z80_cycle(&cpu);
        numcycles++;
        if (cpu.pins.RD && cpu.pins.MRQ) {
            cpu.pins.D = mem[cpu.pins.Addr & 0xFFFF];
        }
        if (cpu.pins.WR && cpu.pins.MRQ) {
            mem[cpu.pins.Addr & 0xFFFF] = cpu.pins.D;
        }
    }
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("\nTIME SPENT: %f seconds  cycles:%lld", time_spent, numcycles);
    double cycles_per_second = numcycles / time_spent;
    printf("\nMHz:%f", cycles_per_second / 1000000);
}