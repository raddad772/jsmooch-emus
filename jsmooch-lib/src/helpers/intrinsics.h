#ifndef JSMOOCH_INTRINSICS_H
#define JSMOOCH_INTRINSICS_H

#if defined (_MSC_VER)
#include <intrin.h>
#include <stdint.h>

#define __builtin_popcount(x) __popcnt(x)

inline uint32_t __builtin_clz(uint32_t value)
{
    uint32_t leading_zero = 0;

    if (_BitScanReverse(&leading_zero, value))
    {
        return 31 - leading_zero;
    }
    else
    {
        // Same remarks as above
        return 32;
    }
}
#endif // _MSC_VER

#include <stdbool.h>

static inline bool sadd_overflow(int a, int b, int *res)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_sadd_overflow(a, b, res);
#elif defined(_MSC_VER)
    return _add_overflow_i32(0, a, b, res);
#else
#error Unsupported compiler
#endif
}

static inline bool ssub_overflow(int a, int b, int *res)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ssub_overflow(a, b, res);
#elif defined(_MSC_VER)
    return _sub_overflow_i32(0, a, b, res);
#else
#error Unsupported compiler
#endif
}

#endif
