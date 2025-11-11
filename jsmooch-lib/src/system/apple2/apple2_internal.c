//
// Created by . on 8/30/24.
//

#include <stdlib.h>
#include <assert.h>

#include "apple2.h"
#include "apple2_internal.h"
#include "iou.h"
#include "mmu.h"

static void apple2_CPU_cycle(apple2* this)
{
    M6502_cycle(&this->cpu);
    if (!this->cpu.pins.RW) {
        this->cpu.pins.D = apple2_cpu_bus_read(this, this->cpu.pins.Addr, this->cpu.pins.D, 1);
    }
    else {
        apple2_cpu_bus_write(this, this->cpu.pins.Addr, this->cpu.pins.D);
    }
}

void apple2_reset(apple2* this)
{
    M6502_reset(&this->cpu);
    this->clock.cpu_divisor = 14;
    this->clock.iou_divisor = 14;
    this->clock.long_cycle_counter = 0;
    this->clock.cpu_adder = this->clock.iou_adder = 0;

    apple2_IOU_reset(this);
}

void apple2_cycle(apple2* this)
{
    this->clock.cpu_adder++;
    //this->clock.iou_adder++;
    if (this->clock.cpu_adder >= this->clock.cpu_divisor) {
        this->clock.cpu_adder = 0;
        this->clock.long_cycle_counter = (this->clock.long_cycle_counter + 1) % 65;
        if (this->clock.long_cycle_counter == 64)
            this->clock.cpu_divisor = 16;
        else
            this->clock.cpu_divisor = 14;


        apple2_CPU_cycle(this);
        apple2_IOU_cycle(this);

    }
    this->clock.master_cycles++;
}

u32 apple2_CPU_read_trace(void *ptr, u32 addr)
{
    struct apple2* this = (apple2*)ptr;
    return apple2_cpu_bus_read(this, addr, 0, 0);
}
