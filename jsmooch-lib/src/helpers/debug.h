//
// Created by Dave on 1/24/2024.
//

#pragma once

#if !defined(NDEBUG)
#define JSDEBUG
#endif

//#define TRACE_FORCE_OFF
//#define SH4_BRK 0x8c000100
//#define DC_MEM_W_BRK 0x8cf70ff4

//#define SNES_LYCODER
//#define TG16_LYCODER
//#define TG16_LYCODER2
//#define LYCODER        // lycoder-format traces for easy winmerge
//#define REICAST_DIFF

#define TRACE_ON_BRK     // Enable tracing on break
#ifdef LYCODER
#define DBG_LOG_TO_FILE // log debug to file
#endif
//#define DISABLE_BREAK

//#define DO_LAST_TRACES   // keeps last X traces, slows down emulation
//#define DUMP_LAST_TRACES_ON_BREAK

#if !defined (_MSC_VER) // not supported on Windows
#define TRACE_COLORS
#endif

//#define SH4_TRACE_INS_FETCH
//#define TEST_SH4   // this one disables certain TMU functionality due to it causing invonvenient issues with sh4 json tests
//#define SH4_IRQ_DBG

#if !defined DC_SUPPORT_ELF
#if !defined (_MSC_VER) // #TODO: Fix for MSVC
#define DC_SUPPORT_ELF
#endif
#endif

//#define DC_ELF_PRINT_FUNCS
#define DC_ELF_NO_LOOP_SYMBOLS

#ifdef SH4_IRQ_DBG
#define SH4_IRQ_DBG_printf(...) printf(__VA_ARGS__)
#else
//#define SH4_IRQ_DBG_printf(...) (void)0;
#define SH4_IRQ_DBG_printf(...) printf(__VA_ARGS__)
#endif

#define SH4_DBG_SUPPORT
#define Z80_DBG_SUPPORT
#define M6502_DBG_SUPPORT
#define SM83_DBG_SUPPORT

#define printif(x, ...) if (dbg.trace_on && dbg.traces. x) dbg_printf(__VA_ARGS__)

#include "helpers/int.h"
#include "helpers/jsm_string.h"

#define MAX_DEBUG_MSG 2000000
#define LAST_TRACES_LEN 3000
#define LAST_TRACES_MSG_LEN 10

//#define warn_printf(...) printf(__VA_ARGS__)
#define warn_printf(...) (void)0

struct DC;
struct last_traces_t {
    char entries[LAST_TRACES_LEN][LAST_TRACES_MSG_LEN]{};
    u32 head{};
    u32 len{};

    char *curptr{};
};

struct cpu_trace_struct {
    u32 irq{};
    u32 instruction{};
    u32 mem{};
    u32 ifetch{};
    u32 io{};
};

struct cpu_break_struct {
    u32 PC{};
    u32 irq{};
};

#define MAX_DEBUG_MSG 2000000
struct jsm_debug_struct {
    u32 do_break{};
    u32 did_breakpoint{};
    u32 brk_on_NMIRQ{};

    u32 first_flush{};

    u32 trace_on{};
    struct {
        cpu_trace_struct z80{};
        cpu_trace_struct m68000{};
        cpu_trace_struct arm7tdmi{}, arm946es{};
        cpu_trace_struct huc6280{};
        cpu_trace_struct r3000{};

        struct DBG_TRACE_PS1_STRUCT {
            struct {
                u32 irq{};
                u32 ack{};
                u32 rw{};
            } sio0{};

            u32 pad{};
        } ps1{};

        struct {
            struct {
                u32 display_modes{};
            } ppu{};
        } nds{};

        u32 better_irq_multiplexer{};

        u32 dma{};
        u32 ram{};
        u32 vram{};
        u32 fifo{};
        u32 vdp{};
        u32 vdp3{}; // DFT(stuff)
        u32 vdp2{}; // printf(stuff)
        u32 vdp4{}; // just VRAM writes
        u32 cpu2{}; // DFT(PC-4)
        u32 cpu3{}; // RD/WR spam
    } traces{};

    struct {
        cpu_break_struct z80{};
        cpu_break_struct m68000{};
    } breaks{};

    u32 watch{};
    jsm_string msg{5*1024*1024};
    char *msg_last_newline{};

    last_traces_t last_traces{};
    u32 var{};
    DC *dcptr{};
};

struct jsm_debug_read_trace {
    void *ptr{};

    u32 (*read_trace)(void *, u32){};

    u32 (*read_trace_arm)(void*, u32, u8){};

    u32 (*read_trace_m68k)(void *, u32, u32, u32){};
};

#ifndef DEBUG_IMPL
extern jsm_debug_struct dbg;
#endif

int dbg_printf(const char *format, ...);
void dbg_seek_in_line(u32 pos);
void dbg_flush();
void dbg_clear_msg();

void dbg_enable_trace();
void dbg_disable_trace();

enum debug_sources {
    DS_none,
    DS_z80,
    DS_65816,
    DS_nes6502,
    DS_m6502,
    DS_m68000,
    DS_spc700
};

void dbg_enable_cpu_trace(enum debug_sources source);
void dbg_disable_cpu_trace(enum debug_sources source);

void dbg_add_msg(char *what);
char *dbg_get_msg();
void dbg_delete();
void dbg_break(const char *reason, u64 cycles);
void dbg_unbreak();

void LT_init(last_traces_t *);
void LT_printf(last_traces_t *, char *format, ...);
void LT_endline(last_traces_t *);
void LT_seek_in_line(last_traces_t *, u32 where);
void LT_dump_to_dbg(last_traces_t *);

void DFT(const char *format, ...);

void dbg_init();

#ifdef DO_LAST_TRACES
#define dbg_LT_printf(...) LT_printf(&dbg.last_traces, __VA_ARGS__)
#define dbg_LT_endline() LT_endline(&dbg.last_traces)
#define dbg_LT_seek_in_line(where) LT_seek_in_line(&dbg.last_traces, where)
#define dbg_LT_dump() LT_dump_to_dbg(&dbg.last_traces)
#define dbg_LT_clear() LT_init(&dbg.last_traces)
#else
#define dbg_LT_printf(...) (void)0
#define dbg_LT_endline() (void)0
#define dbg_LT_seek_in_line(where) (void)0
#define dbg_LT_dump() (void)0
#define dbg_LT_clear() (void)0
#endif
void jsm_copy_read_trace(jsm_debug_read_trace *dst, jsm_debug_read_trace *src);


#ifdef TRACE_COLORS
#define DBGC_BLACK "\e[0;30m"
#define DBGC_RED "\e[0;31m"
#define DBGC_GREEN "\e[0;32m"
#define DBGC_YELLOW "\e[0;33m"
#define DBGC_BLUE "\e[0;34m"
#define DBGC_PURPLE "\e[0;35m"
#define DBGC_CYAN "\e[0;36m"
#define DBGC_WHITE "\e[0;37m"
#define DBGC_RST "\e[0m"

#else

#define DBGC_BLACK ""
#define DBGC_RED ""
#define DBGC_GREEN ""
#define DBGC_YELLOW ""
#define DBGC_BLUE ""
#define DBGC_PURPLE ""
#define DBGC_CYAN ""
#define DBGC_WHITE ""
#define DBGC_RST ""

#endif

#define DBGC_TRACE DBGC_RST
#define DBGC_READ DBGC_YELLOW
#define DBGC_WRITE DBGC_RED
#define DBGC_M68K DBGC_GREEN
#define DBGC_Z80 DBGC_CYAN

#define TRACE_BRK_POS 55

//#define printif(x, ...) if (::dbg.trace_on && dbg.traces. x) dbg_printf(__VA_ARGS__)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif
