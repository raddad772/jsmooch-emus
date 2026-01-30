//
// Created by . on 2/11/25.
//

#include <cstdlib>
#include <cassert>
#include "r3000_multiplier.h"

void R3000_multiplier_set(R3000_multiplier *this, u32 hi, u32 lo, u32 op1, u32 op2, u32 op_kind, u32 cycles, u64 current_clock)
{
    this->hi = hi;
    this->lo = lo;
    this->op1 = op1;
    this->op2 = op2;

    this->op_going = 1;
    this->op_kind = op_kind;
    this->clock_start = current_clock;
    this->clock_end = (current_clock + cycles) - 1;
}

static inline void u32_multiply(u32 a, u32 b, u32 *hi, u32 *lo)
{
    u64 c = (u64)a;
    u64 d = (u64)b;
    u64 e = c * d;
    *hi = (u32)(e >> 32);
    *lo = (u32)e;
}

static inline void i32_multiply(u32 a, u32 b, u32 *hi, u32 *lo)
{
    i64 c = (i64)(i32)a;
    i64 d = (i64)(i32)b;
    i64 e = c * d;
    *hi = (u32)(e >> 32);
    *lo = (u32)e;
}

static inline void u32_divide(u32 a, u32 b, u32 *hi, u32 *lo)
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

static inline void i32_divide(u32 a, u32 b, u32 *hi, u32 *lo)
{
    i32 c = (i32)a;
    i32 d = (i32)b;
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

#ifdef _MSC_VER
#pragma warning(pop)
#endif

void R3000_multiplier_finish(R3000_multiplier *this)
{
    if (!this->op_going)
        return;

    switch(this->op_kind) {
        case 0: // signed multiply
            i32_multiply(this->op1, this->op2, &this->hi, &this->lo);
            break;
        case 1: // unsigned multiply
            u32_multiply(this->op1, this->op2, &this->hi, &this->lo);
            break;
        case 2: // signed divide
            i32_divide(this->op1, this->op2, &this->hi, &this->lo);
            break;
        case 3: // unsigned divide
            u32_divide(this->op1, this->op2, &this->hi, &this->lo);
            break;
        default:
            NOGOHERE;
    }

    this->op_going = 0;
}


