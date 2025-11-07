//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_R3000_MULTIPLIER_H
#define JSMOOCH_EMUS_R3000_MULTIPLIER_H

#include "helpers_c/int.h"

/*
export class R3000_multiplier_t {
    clock: PS1_clock
    hi: u32 = 0
    lo: u32 = 0

    op1: u32 = 0
    op2: u32 = 0
    op_going: u32 = 0
    op_kind: u32 = 0
    clock_start: u64 = 0
    clock_end: u64 = 0
    constructor(clock: PS1_clock) {
        this.clock = clock;
    }

    // Finishes up multiply or divide
    finish(): u32 {
        if (!this.op_going)
            return 0;

        let ret_cycles: u32 = 0;
        if (this.clock.cpu_master_clock < this.clock_end) {
            ret_cycles = <u32>(this.clock_end - this.clock.cpu_master_clock);
        }

        let ret: u32_dual_return = new u32_dual_return();
        switch(this.op_kind) {
            case 0: // signed multiply
                ret = i32_multiply(this.op1, this.op2);
                break;
            case 1: // unsigned multiply
                ret = u32_multiply(this.op1, this.op2);
                break;
            case 2: // signed divide
                ret = i32_divide(this.op1, this.op2);
                break;
            case 3: // unsigned divide
                ret = u32_divide(this.op1, this.op2);
                break;
            default:
                unreachable();
        }
        this.hi = ret.hi;
        this.lo = ret.lo;

        this.op_going = 0;
        return ret_cycles;
    }
}
 */
struct R3000_multiplier {
    u32 hi, lo, op1, op2, op_going, op_kind;
    u64 clock_start, clock_end;
};

void R3000_multiplier_set(struct R3000_multiplier *, u32 hi, u32 lo, u32 op1, u32 op2, u32 op_kind, u32 cycles, u64 current_clock);
void R3000_multiplier_finish(struct R3000_multiplier *);

#endif //JSMOOCH_EMUS_R3000_MULTIPLIER_H
