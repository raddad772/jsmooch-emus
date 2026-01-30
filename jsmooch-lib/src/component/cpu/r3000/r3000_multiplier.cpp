//
// Created by . on 2/11/25.
//

#include <cstdlib>
#include <cassert>
#include "r3000_multiplier.h"
namespace R3000 {
void multiplier::set(u32 hi_in, u32 lo_in, u32 op1_in, u32 op2_in, u32 op_kind_in, u32 cycles_in, u64 current_clock_in)
{
    hi = hi_in;
    lo = lo_in;
    op1 = op1_in;
    op2 = op2_in;

    op_going = true;
    op_kind = op_kind_in;
    clock_start = current_clock_in;
    clock_end = (current_clock_in + cycles_in) - 1;
}

static void u32_multiply(u32 a, u32 b, u32 *hi, u32 *lo)
{
    u64 c = a;
    u64 d = b;
    u64 e = c * d;
    *hi = static_cast<u32>(e >> 32);
    *lo = static_cast<u32>(e);
}

static void i32_multiply(u32 a, u32 b, u32 *hi, u32 *lo)
{
    i64 c = static_cast<i32>(a);
    i64 d = static_cast<i32>(b);
    i64 e = c * d;
    *hi = static_cast<u32>(e >> 32);
    *lo = static_cast<u32>(e);
}

static void u32_divide(u32 a, u32 b, u32 *hi, u32 *lo)
{
    if (b == 0) {
        *hi = a;
        *lo = 0xFFFFFFFF;
    }
    else {
        *lo = (a / b);
        *hi = a % b;
    }
}

static void i32_divide(u32 a, u32 b, u32 *hi, u32 *lo)
{
    i32 c = static_cast<i32>(a);
    i32 d = static_cast<i32>(b);
    if (d == 0) {
        if (c >= 0) {
            *hi = a;
            *lo = -1;
        }
        else if (c == -0x80000000) {
            *hi = 0x80000000;
            *lo = 1;
        }
        else {
            *hi = a;
            *lo = 1;
        }
    }
    else {
        *lo = (c / d) & 0xFFFFFFFF;
        *hi = c % d;
    }
}

void multiplier::finish()
{
    if (!op_going)
        return;

    switch(op_kind) {
        case 0: // signed multiply
            i32_multiply(op1, op2, &hi, &lo);
            break;
        case 1: // unsigned multiply
            u32_multiply(op1, op2, &hi, &lo);
            break;
        case 2: // signed divide
            i32_divide(op1, op2, &hi, &lo);
            break;
        case 3: // unsigned divide
            u32_divide(op1, op2, &hi, &lo);
            break;
        default:
            NOGOHERE;
    }

    op_going = false;
}


}