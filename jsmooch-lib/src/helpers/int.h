#ifndef _JSMOOCH_INT_H
#define _JSMOOCH_INT_H

#include <stdbool.h>

#if _MSC_VER && !__INTEL_COMPILER
typedef unsigned long long uint64;
typedef signed long long int64;

typedef unsigned long uint32;
typedef signed long int32;

typedef unsigned short uint16;
typedef signed short int16;

typedef unsigned char uint8;
typedef signed char int8;


#else
#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif


typedef uint64 u64;
typedef int64 s64;
typedef int64 i64;

typedef uint32 u32;
typedef int32 s32;
typedef int32 i32;

typedef uint16 u16;
typedef int16 s16;
typedef int16 i16;

typedef uint8 u8;
typedef int8 s8;
typedef int8 i8;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif



#endif