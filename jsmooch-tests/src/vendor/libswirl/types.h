// Reicast bridge
// Created by . on 5/20/24.
//

#include <assert.h>

#ifndef JSMOOCH_EMUS_TYPES_H
#define JSMOOCH_EMUS_TYPES_H
#include "helpers/int.h"

#define INLINE

#define COMPILER_CLANG 1
#define COMPILER_VC 4
#define BUILD_COMPILER COMPILER_CLANG

#define CPU_ARM 1
#define CPU_X86 2
#define CPU_X64 3
#define CPU_ARM64 4
#define HOST_CPU CPU_ARM64

#define verify(x) assert(x)
#define DECL_ALIGN(x) __attribute__((aligned(x)))

struct SuperH4;
extern SuperH4* sh4_cpu;

u32 static INLINE bitscanrev(u32 v)
{
#if BUILD_COMPILER==COMPILER_GCC || BUILD_COMPILER==COMPILER_CLANG
    return 31-__builtin_clz(v);
#else
    unsigned long rv;
	_BitScanReverse(&rv,v);
	return rv;
#endif
}


#endif //JSMOOCH_EMUS_TYPES_H
