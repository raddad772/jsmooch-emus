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

#define MAX_DBGLOG_IDS 200
#define MAX_DBGLOG_LINES 100000

#define TRACEC_WHITE 0
#define TRACEC_DEFAULT TRACEC_WHITE


enum debugger_view_kinds {
    dview_null,
    dview_events,
    dview_memory,
    dview_disassembly,
    dview_image,
    dview_waveforms,
    dview_trace,
    dview_console,
    dview_dbglog,
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
    u32 valid;
    i64 addr_range_start;
    i64 addr_range_end;
    i64 addr_of_next_ins; // Address of next instruction we *would* disassemble
    u32 index;
    struct cvec entries; // disassembly_entry
    u32 num_entries_previously_made;
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

struct memory_view_module {
    u32 addr_start, addr_end;
    u32 addr_digits;
    u32 id;
    void *read_mem16_ptr;
    void (*read_mem16)(void *ptr, u32 addr, void *dest);
    char name[50];
    char *input_buf;
};

struct memory_view {
    struct cvec modules;

    // 16 bytes per row!

    // Input
    u32 display_start_addr;
    u32 current_id;

    u32 force_refresh;
    u32 old_top_display;
};

struct disassembly_view {
    u32 mem_start;
    u32 mem_end;
    struct cvec ranges; // disassembly_range
    struct cvec dirty_range_indices; // indices of dirty ranges we can re-use
    struct {
        struct cvec regs; // cpu_reg_context
    } cpu;
    struct jsm_string processor_name;

    u32 addr_column_size; // how many columns to put in address of disassembly view
    u32 has_context;

    struct { // get_disassembly_vars gets variables for disassembly such as address of currently-executing instructions
        void *ptr;
        struct disassembly_vars (*func)(void *, struct debugger_interface *, struct disassembly_view *);
    } get_disassembly_vars;

    struct { // fill_view fills out variables and stuff
        void *ptr;
        void (*func)(void *, struct debugger_interface *dbgr, struct disassembly_view *dview);
    } fill_view;

    struct {
        void *ptr;
        int (*func)(void *, u32 addr, char *out, size_t out_sz);
    } print_addr;

    struct { // get_disaassembly gets disasssembly
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
    u64 frame_num, mclks;
    u32 scan_x, scan_y;
};

struct debugger_event {
    struct cvec_ptr category;
    char name[50];

    struct cvec updates[2]; // debugger_event_update
    u32 updates_index;
    u32 color;
    u32 category_id;
    u32 display_enabled;

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

enum ev_timing_kind {
    ev_timing_scanxy,
    ev_timing_master_clock
};


struct console_view {
    struct jsm_string buffer[2];
    char name[100];
    u32 updated;
    u32 cur_buf;
};

enum dbglog_severity {
    DBGLS_FATAL,
    DBGLS_ERROR,
    DBGLS_WARN,
    DBGLS_INFO,
    DBGLS_DEBUG,
    DBGLS_TRACE
};

struct dbglog_entry {
    u32 category_id;
    u64 timecode;
    enum dbglog_severity severity;
    struct jsm_string text, extra;
};

struct dbglog_category_node {
    char name[100];
    char short_name[20];
    u32 category_id;
    u32 enabled;

    struct cvec children;
};

struct dbglog_view {
    char name[100];

    u32 ids_enabled[MAX_DBGLOG_IDS];
    struct dbglog_category_node *id_to_category[MAX_DBGLOG_IDS];
    u32 id_to_color[MAX_DBGLOG_IDS];
    u32 updated;
    u32 has_extra;

    struct {
        struct dbglog_entry data[MAX_DBGLOG_LINES];
        u32 first_entry, next_entry, len;
    } items;

    struct dbglog_entry *last_added;

    struct dbglog_category_node category_root;
};

struct events_view {
    struct cvec events;

    enum ev_timing_kind timing;

    struct cvec categories;
    u64 current_frame;

    u32 index_in_use;

    u64 last_frame;
    u32 keep_last_frame;
    struct cvec_ptr associated_display;

    struct events_view_master_clocks {
        u64 per_line;
        u64 lines[2][1024];
        u64 draw_starts[2][1024];
        u32 height;
        i32 cur_line;
        u64 *ptr;
        u32 back_buffer;
        u32 front_buffer;
    } master_clocks;


    struct DVDP {
        u32 width, height;
        float width_scale, height_scale;
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
    u32 ch_output_enabled;
    u32 default_clock_divider;
    u32 clock_divider;
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

#define MAX_TRACE_COLS 20
struct trace_line {
    struct jsm_string cols[MAX_TRACE_COLS];
    i32 source_id;
    u32 empty;
};

struct trace_view_col {
    char name[50];
    i32 default_size;
};

struct trace_view {
    struct cvec lines;
    struct cvec columns;
    struct trace_line *lptr;
    u32 current_output_line;
    u32 max_trace_lines;
    u32 num_trace_lines;
    u32 waiting_for_startline;
    u32 waiting_for_endline;
    u32 first_line;

    // User-set options...
    char name[150];
    u32 autoscroll;
    u32 display_end_top;
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


enum JSMD_widgets {
    JSMD_checkbox,
    JSMD_radiogroup,
    JSMD_textbox,
    JSMD_colorkey
};

struct debugger_widget_checkbox
{
    char text[100];
    u32 value;
};

struct debugger_widget_radiogroup {
    char title[100];
    u32 value;
    struct cvec buttons; // debugger_widget_checkbox
};

struct debugger_widget_colorkey_item {
    char name[50];
    u32 color;
    u32 hovered;
};

struct debugger_widget_colorkey {
    char title[100];
    u32 num_items;
    struct debugger_widget_colorkey_item items[50];
};

struct debugger_widget_textbox {
    struct jsm_string contents;
};

struct debugger_widget {
    enum JSMD_widgets kind;
    u32 same_line, enabled, visible;

    union {
        struct debugger_widget_checkbox checkbox;
        struct debugger_widget_radiogroup radiogroup;
        struct debugger_widget_textbox textbox;
        struct debugger_widget_colorkey colorkey;
    };
};

struct debugger_view {
    enum debugger_view_kinds kind;
    struct {
        u32 on_break, on_pause, on_step;
        struct { u32 enabled; u32 line_num; } on_line;
    } update;
    struct cvec options;

    union {
        struct disassembly_view disassembly;
        struct events_view events;
        struct image_view image;
        struct waveform_view waveform;
        struct trace_view trace;
        struct console_view console;
        struct dbglog_view dbglog;
        struct memory_view memory;
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

void debugger_widget_init(struct debugger_widget *, enum JSMD_widgets kind);
void debugger_widget_delete(struct debugger_widget *);
void debugger_widgets_add_checkbox(struct cvec *widgets, const char *text, u32 enabled, u32 default_value, u32 same_line);
struct debugger_widget *debugger_widgets_add_radiogroup(struct cvec* widgets, const char *text, u32 enabled, u32 default_value, u32 same_line);
struct debugger_widget *debugger_widgets_add_color_key(struct cvec *widgets, const char *default_text, u32 default_visible);
void debugger_widget_radiogroup_add_button(struct debugger_widget *radiogroup, const char *text, u32 value, u32 same_line);
void debugger_widgets_textbox_clear(struct debugger_widget_textbox *tb);
int debugger_widgets_textbox_sprintf(struct debugger_widget_textbox *tb, const char *format, ...);
void debugger_widgets_add_textbox(struct cvec *widgets, char *text, u32 same_line);

// Memory view functions
void memory_view_add_module(struct debugger_interface *dbgr, struct memory_view *mv, const char *name, u32 id, u32 addr_digits, u32 range_start, u32 range_end, void *readptr, void (*readmem16func)(void *ptr, u32 addr, void *dest));
u32 memory_view_num_modules(struct memory_view *mv);
struct memory_view_module *memory_view_get_module(struct memory_view *mv, u32 id);
void memory_view_get_line(struct memory_view_module *mm, u32 addr, char *out);

// Disassembly view functions
int disassembly_view_get_rows(struct debugger_interface *di, struct disassembly_view *dview, u32 instruction_addr, u32 bytes_before, u32 total_lines, struct cvec *out_lines);
void cpu_reg_context_render(struct cpu_reg_context*, char* outbuf, size_t outbuf_sz);

// Event view functions
void events_view_add_category(struct debugger_interface *dbgr, struct events_view *ev, const char *name, u32 color, u32 id);
void events_view_add_event(struct debugger_interface *dbgr, struct events_view *ev, u32 category_id, const char *name, u32 color, enum debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id);
void events_view_report_draw_start(struct events_view *);

void debugger_report_frame(struct debugger_interface *dbgr);
void debugger_report_event(struct cvec_ptr viewptr, i32 event_id);
u64 events_view_get_current_line_pos(struct cvec_ptr viewptr);
u64 events_view_get_current_line_start(struct cvec_ptr viewptr);
u64 events_view_get_current_line(struct cvec_ptr viewptr);

void debugger_report_line(struct debugger_interface *dbgr, i32 line_num);
void debugger_report_line_mclks(struct cvec_ptr event_view, u64 mclks, u32 line_num);

void events_view_render(struct debugger_interface *dbgr, struct events_view *, u32 *buf, u32 out_width, u32 out_height);

void debug_waveform_init(struct debug_waveform *);

void waveform_view_init(struct waveform_view *);
void waveform_view_delete(struct waveform_view *);

void trace_view_add_col(struct trace_view *, const char *name, i32 default_size);
void trace_view_clear(struct trace_view *);
void trace_view_printf(struct trace_view *, u32 col, char *format, ...);
void trace_view_startline(struct trace_view *, i32 source);
void trace_view_endline(struct trace_view *);
struct trace_line *trace_view_get_line(struct trace_view *, int row);

void events_view_report_line(struct events_view *, i32 line_num);

void console_view_add_char(struct console_view *, u8 c);
void console_view_add_cstr(struct console_view *, char *s);
void console_view_render_to_buffer(struct console_view *tv, char *output, u64 sz);

struct dbglog_category_node *dbglog_category_get_root(struct dbglog_view *);
struct dbglog_category_node *dbglog_category_add_node(struct dbglog_view *dv, struct dbglog_category_node *, const char *name, const char *short_name, u32 id, u32 color);
void dbglog_view_add_item(struct dbglog_view *, u32 id, u64 timecode, enum dbglog_severity severity, const char *txt);
void dbglog_view_add_printf(struct dbglog_view *, u32 id, u64 timecode, enum dbglog_severity severity, const char *format, ...);
void dbglog_view_extra_printf(struct dbglog_view *, const char *format, ...);
void dbglog_view_render_to_buffer(struct dbglog_view *, char *output, u64 sz);
u32 dbglog_count_visible_lines(struct dbglog_view *);
u32 dbglog_get_nth_visible(struct dbglog_view *, u32 n);
u32 dbglog_get_next_visible(struct dbglog_view *, u32 start);

void debugger_widgets_colorkey_set_title(struct debugger_widget_colorkey *, const char *str);
void debugger_widgets_colorkey_add_item(struct debugger_widget_colorkey *, const char *str, u32 color);


#define DEBUG_REGISTER_EVENT_CATEGORY(name, id) events_view_add_category(dbgr, ev, name, 0, id)
#define DEBUG_REGISTER_EVENT(name, color, category, id, default_enable) events_view_add_event(dbgr, ev, category, name, color, dek_pix_square, default_enable, 0, NULL, id)
#define SET_EVENT_VIEW(x) x .dbg.events.view = this->dbg.events.view
#define SET_CPU_CPU_EVENT_ID(id, k) this->cpu.cpu.dbg.events. k = id
#define SET_CPU_EVENT_ID(id, k) this->cpu.dbg.events. k = id

#define DBG_EVENT(x) debugger_report_event(this->dbg.events.view, x)

#ifdef __cplusplus
}
#endif
#endif //JSMOOCH_EMUS_DEBUGGER_H
