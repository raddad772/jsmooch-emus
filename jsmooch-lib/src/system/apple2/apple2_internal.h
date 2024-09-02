//
// Created by . on 8/29/24.
//

#ifndef JSMOOCH_EMUS_APPLE2_INTERNAL_H
#define JSMOOCH_EMUS_APPLE2_INTERNAL_H

#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "helpers/simplebuf.h"

#include "component/cpu/m6502/m6502.h"

struct apple2 {

    struct M6502 cpu;
    struct cvec *IOs;

    struct {
        u64 master_cycles;
        u64 frames_since_restart;
        u64 master_frame;
        u32 crt_x, crt_y;

        u32 long_cycle_counter;
        u32 cpu_divisor;
        u32 iou_divisor;


        u32 cpu_adder;
        u32 iou_adder;
    } clock;

    struct {
        struct simplebuf8 ROM, RAM;
        u32 RAM_bank;
        u32 page1_accesses;


        struct APL2SS {           // Off state:
            u32 STORE80;   // "Page 2 does not bank switch RAM"
            u32 RAMRD;     // Read from motherboard RAM
            u32 RAMWRT;    // Write to motherboard RAM
            u32 INTCXROM;  // Slot response to $C100-CFFF
            u32 ALTZP;     // Motherboard RAM read/write
            u32 SLOTC3ROM; // Motherboard ROM response to $C3XX
            u32 BANK1;     // off = HiRAM bank 2 response to $$Dxxx
            u32 HRAMRD;    // $D000-FFFF reads from ROM
            u32 PREWRITE;  // Reset?
            u32 HRAMWRT;   // $D000-FFFF writes to hiRAM
            u32 INTC8ROM;  // Slot response to $C800-CFFF

            u32 WRTCOUNT;
        } io;
    } mmu;

    struct {
        struct JSM_DISPLAY* display;
        struct cvec_ptr display_ptr, keyboard_ptr;
        u8* cur_output;
        u32 flash, flash_counter;

        struct APPLE2IOUIO {
            u32 VBL;
            u32 PAGE2;     // Motherboard RAM read/write
            u32 HIRES;     // Page2 does not switch $2000-3FFF

            u32 MIXED;
            u32 COL80;
            u32 KEYSTROBE;
            u32 ALTCHRSET;
            u32 TEXT;
            u32 AN0, AN1, AN2, AN3;
            u32 CSSTOUT;
            u32 SPKR;
            u32 AKD;

            u32 pushbutton[3];
            u32 cassette_in;
            u32 paddle[4];
        } io;
    } iou;

    u32 described_inputs;
};

void apple2_reset(struct apple2*);
void apple2_cycle(struct apple2*);
u32 apple2_CPU_read_trace(void *ptr, u32 addr);

#endif //JSMOOCH_EMUS_APPLE2_INTERNAL_H
