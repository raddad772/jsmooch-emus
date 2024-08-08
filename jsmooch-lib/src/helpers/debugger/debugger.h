//
// Created by . on 8/2/24.
//

#ifndef JSMOOCH_EMUS_DEBUGGER_H
#define JSMOOCH_EMUS_DEBUGGER_H

#ifdef __cplusplus
extern "C" {
#endif



#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/jsm_string.h"


enum debugger_view_kinds {
    dview_null,
    dview_events,
    dview_memory,
    dview_disassembly
};


/*
 * a dissassembly view...should default to the last-executedi nstruction.
 *
 * it should show
 *
 *
 *   addr   |  disassembly
 *
 *
 *   addr-len should be a param
 *
 *   disassemblies should come in ranges, where some ranges can't be disassembled, i.e. back-areas
 *   disassemblies should be managed in that way, if possible, to allow back-traces, but anyway.
 *
 */

struct disassembly_entry {
    u32 addr;
    struct jsm_string dasm;
    struct jsm_string context;
    u32 ins_size_bytes;
};

struct disassembly_entry_strings {
    char addr[100];
    char dasm[200];
    char context[400];
};

struct disassembly_range {
    u32 knowable;
    u64 addr_range_start;
    u64 addr_range_end;
    struct cvec entries; // disassembly_entry
};

enum cpu_reg_kinds {
    RK_bitflags,
    RK_bool,
    RK_int8,
    RK_int16,
    RK_int32,
    RK_float,
    RK_double
};

struct disassembly_vars {
    u64 current_clock_cycle;
    u32 address_of_executing_instruction;
};

struct debugger_interface;
struct cpu_reg_context {
    enum cpu_reg_kinds kind;
    union {
        u8 bool_data;
        u8 int8_data;
        u16 int16_data;
        u32 int32_data;
        float float_data;
        double double_data;
    };

    char name[20];
    u32 is_bitflag;
    u32 index;
    int (*custom_render)(struct cpu_reg_context*, void *outbuf, size_t outbuf_sz);
};

struct disassembly_view {
    struct cvec ranges; // disassembly_range
    struct {
        struct cvec regs; // cpu_reg_context
    } cpu;
    struct jsm_string processor_name;

    u32 addr_column_size; // how many columns to put in address of disassembly view
    u32 has_context;

    struct {
        void *ptr;
        struct disassembly_vars (*func)(void *, struct debugger_interface *, struct disassembly_view *);
    } get_disassembly_vars;

    struct {
        void *ptr;
        void (*func)(void *, struct debugger_interface *dbgr, struct disassembly_view *dview);
    } fill_view;

    struct {
        void *ptr;
        void (*func)(void *, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry);
    } get_disassembly;
};

struct debugger_view {
    enum debugger_view_kinds kind;
    union {
        struct disassembly_view disassembly;
    };
};

/* This need to be used on two sides. The application side, and the core side. */
struct debugger_interface {
    struct cvec views; // debugger_view
    u32 update_on_break;
    u32 update_on_pause;

    u32 smallest_step; // smallest possible step
    u32 supported_by_core;
};


void debugger_interface_init(struct debugger_interface *);
void debugger_interface_delete(struct debugger_interface *);

void debugger_view_init(struct debugger_view *, enum debugger_view_kinds kind);
void debugger_view_delete(struct debugger_view *);
void disassembly_view_delete(struct disassembly_view *);
void cpu_reg_context_delete(struct cpu_reg_context *);

void cpu_reg_context_init(struct cpu_reg_context *);
void disassembly_range_delete(struct disassembly_range *);
void disassembly_range_init(struct disassembly_range *);
void disassembly_entry_delete(struct disassembly_entry*);
void disassembly_entry_init(struct disassembly_entry *);

int disassembly_view_get_rows(struct debugger_interface *di, struct disassembly_view *dview, u32 instruction_addr, u32 bytes_before, u32 bytes_after, struct cvec *out_lines);

void cpu_reg_context_render(struct cpu_reg_context*, char* outbuf, size_t outbuf_sz);

#ifdef __cplusplus
}
#endif
#endif //JSMOOCH_EMUS_DEBUGGER_H
