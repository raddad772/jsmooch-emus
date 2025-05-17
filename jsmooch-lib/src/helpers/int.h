#ifndef _JSMOOCH_INT_H
#define _JSMOOCH_INT_H

#ifdef __cplusplus
extern "C" {
#endif

#define COMPILER_CLANG 3
#define BUILD_COMPILER COMPILER_CLANG
#define CPU_ARM64 5
#define HOST_CPU CPU_ARM64

/* If you are somehow on a big-endian platform, you must change this */
#define ENDIAN_LITTLE
//#define ENDIAN_BIG


#include <stdbool.h>
#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

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

typedef float f32;
typedef double f64;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif


#define SIGNe4to32(x) ((((x) >> 3) * 0xFFFFFFF0) | ((x) & 0x0F))
#define SIGNe6to32(x) (((((x) >> 5) & 1) * 0xFFFFFFC0) | ((x) & 0x1F))
#define SIGNe7to32(x) (((((x) >> 6) & 1) * 0xFFFFFF80) | ((x) & 0x3F))
#define SIGNe8to32(x) (((((x) >> 7) & 1) * 0xFFFFFF00) | ((x) & 0xFF))
#define SIGNe9to32(x) (((((x) >> 8) & 1) * 0xFFFFFE00) | ((x) & 0x1FF))
#define SIGNe8to64(x) (((((x) >> 7) & 1) * 0xFFFFFFFFFFFFFF00) | ((x) & 0xFF))
#define SIGNe10to32(x) (((((x) >> 9) & 1) * 0xFFFFFC00) | ((x) & 0x3FF))
#define SIGNe11to32(x) (((((x) >> 10) & 1) * 0xFFFFF800) | ((x) & 0x7FF))
#define SIGNe12to32(x) (((((x) >> 11) & 1) * 0xFFFFF000) | ((x) & 0xFFF))
#define SIGNe13to32(x) (((((x) >> 12) & 1) * 0xFFFFE000) | ((x) & 0x1FFF))
#define SIGNe16to32(x) (((((x) >> 15) & 1) * 0xFFFF0000) | ((x) & 0xFFFF))
#define SIGNe24to32(x) (((((x) >> 23) & 1) * 0xFF000000) | ((x) & 0xFFFFFF))

#if defined(__APPLE__)
// Mac os X
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#else
#if HAVE_BYTESWAP_H
#include <byteswap.h>
#else
#define bswap_16 __builtin_bswap16
#define bswap_32 __builtin_bswap32
#define bswap_64 __builtin_bswap64
#endif
#endif

#ifdef ENDIAN_LITTLE

#define from_big16(x) bswap_16(x)
#define from_big32(x) bswap_32(x)
#define from_big64(x) bswap_64(x)

#define from_little16(x) (x)
#define from_little32(x) (x)
#define from_little64(x) (x)

#else

#define from_big16(x) (x)
#define from_big32(x) (x)
#define from_big64(x) (x)

#define from_little16(x) bswap_16(x)
#define from_little32(x) bswap_32(x)
#define from_little64(x) bswap_64(x)


#endif

#ifdef __cplusplus
}
#endif

#ifdef DEBUG
#define NOGOHERE assert(1==2)
#else
#define NOGOHERE { abort(); __builtin_unreachable(); }
#endif
#endif