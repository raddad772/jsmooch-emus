//
// Created by . on 8/2/24.
//

#ifndef JSMOOCH_EMUS_DEBUGGER_H
#define JSMOOCH_EMUS_DEBUGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/buf.h"
#include "helpers/jsm_string.h"


enum debugger_view_kinds {
    dview_null,
    dview_events,
    dview_memory,
    dview_disassembly,
    dview_image,
    dview_waveforms,
    dview_trace,
    dview_log
};


#define NESMEM_CPUBUS 0
#define NESMEM_PPUBUS 1

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
    u32 valid;
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

struct event_category {
    char name[50];
    u32 color;
    u32 id;
};

enum debugger_event_kind {
    dek_pix_square,
    dek_line
};

struct debugger_event_update {
    u64 frame_num;
    u32 scan_x, scan_y;
};

struct debugger_event {
    struct cvec_ptr category;
    char name[50];

    struct cvec updates[2]; // debugger_event_update
    u32 updates_index;
    u32 color;
    u32 category_id;

    struct {
        char context[50];
    } tag;

    struct {
        u32 order;
    } list;

    struct {
        u32 enabled;
        enum debugger_event_kind kind;
    } display;
};

struct events_view {
    struct cvec events;

    struct cvec categories;
    u64 current_frame;

    u32 index_in_use;

    u64 last_frame;
    u32 keep_last_frame;
    struct cvec_ptr associated_display;


    struct DVDP {
        u32 width, height;
        u32 *buf;
        u64 frame_num;
    } display[2];
};

struct ivec2 {
    i32 x, y;
};

struct debugger_view;
struct debugger_update_func {
    void *ptr;
    void (*func)(struct debugger_interface*, struct debugger_view *, void *, u32);
};

enum debug_waveform_kinds {
    dwk_none,
    dwk_main,
    dwk_channel
};

struct debug_waveform {
    char name[50];
    u32 samples_requested;
    u32 samples_rendered;
    struct buf buf; // height*width. value -1...1
    enum debug_waveform_kinds kind;

    struct {
        double next_sample_cycle;
        double cycle_stride;
        u32 buf_pos;
    } user;
};

struct waveform_view {
    char name[50];
    struct cvec waveforms;
};



struct image_view {
    char label[50];
    u32 ready_for_display;
    u32 width, height;
    u32 draw_which_buf;

    struct {
        u32 exists;
        u32 enabled;
        struct ivec2 p[2];
    } viewport;

    struct debugger_update_func update_func;

    struct buf img_buf[2];
};

struct debugger_view {
    enum debugger_view_kinds kind;
    struct {
        u32 on_break, on_pause, on_step;
        struct { u32 enabled; u32 line_num; } on_line;
    } update;

    union {
        struct disassembly_view disassembly;
        struct events_view events;
        struct image_view image;
        struct waveform_view waveform;
    };
};

/* This need to be used on two sides. The application side, and the core side. */
struct debugger_interface {
    struct cvec views; // debugger_view

    u32 smallest_step; // smallest possible step
    u32 supported_by_core;
};


// General functions
void debugger_interface_init(struct debugger_interface *);
void debugger_interface_delete(struct debugger_interface *);
struct cvec_ptr debugger_view_new(struct debugger_interface *, enum debugger_view_kinds kind);

void debugger_view_init(struct debugger_view *, enum debugger_view_kinds kind);
void debugger_interface_dirty_mem(struct debugger_interface *, u32 mem_bus, u32 addr_start, u32 addr_end);

// Disassembly view functions
int disassembly_view_get_rows(struct debugger_interface *di, struct disassembly_view *dview, u32 instruction_addr, u32 bytes_before, u32 bytes_after, struct cvec *out_lines);
void cpu_reg_context_render(struct cpu_reg_context*, char* outbuf, size_t outbuf_sz);

// Event view functions
void events_view_add_category(struct debugger_interface *dbgr, struct events_view *ev, const char *name, u32 color, u32 id);
void events_view_add_event(struct debugger_interface *dbgr, struct events_view *ev, u32 category_id, const char *name, u32 color, enum debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id);
void event_view_begin_frame(struct cvec_ptr event_view);
void debugger_report_event(struct cvec_ptr viewptr, u32 event_id);
void debugger_report_line(struct debugger_interface *dbgr, i32 line_num);
void events_view_render(struct debugger_interface *dbgr, struct events_view *, u32 *buf, u32 out_width, u32 out_height);

#define DEBUG_REGISTER_EVENT_CATEGORY(name, id) events_view_add_category(dbgr, ev, name, 0, id)
#define DEBUG_REGISTER_EVENT(name, color, category, id) events_view_add_event(dbgr, ev, category, name, color, dek_pix_square, 1, 0, NULL, id)
#define SET_EVENT_VIEW(x) x .dbg.events.view = this->dbg.events.view
#define SET_CPU_CPU_EVENT_ID(id, k) this->cpu.cpu.dbg.events. k = id
#define SET_CPU_EVENT_ID(id, k) this->cpu.dbg.events. k = id

#define DBG_EVENT(x) debugger_report_event(this->dbg.events.view, x)

#ifdef __cplusplus
}
#endif
#endif //JSMOOCH_EMUS_DEBUGGER_H
