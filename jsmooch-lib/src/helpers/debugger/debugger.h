//
// Created by . on 8/2/24.
//

#pragma once
#include "helpers/physical_io.h"

#include <array>
#include <algorithm>

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
    u32 addr{};
    jsm_string dasm{100};
    jsm_string context{100};
    u32 ins_size_bytes{};

    void clear_for_reuse();
};

struct disassembly_entry_strings {
    char addr[100]{};
    char dasm[200]{};
    char context[400]{};
};

struct disassembly_range {
    u32 valid{};
    i64 addr_range_start{};
    i64 addr_range_end{};
    i64 addr_of_next_ins{}; // Address of next instruction we *would* disassemble
    u32 index{};
    std::vector<disassembly_entry> entries; // disassembly_entry
    u32 num_entries_previously_made{};
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
    u64 current_clock_cycle{};
    u32 address_of_executing_instruction{};
};

struct debugger_interface;
struct cpu_reg_context {
    cpu_reg_kinds kind{};
    union {
        u8 bool_data;
        u8 int8_data;
        u16 int16_data;
        u32 int32_data;
        float float_data;
        double double_data{};
    };

    char name[20]{};
    u32 is_bitflag{};
    u32 index{};
    int (*custom_render)(cpu_reg_context*, void *outbuf, size_t outbuf_sz){};
};

struct memory_view_module {
    u32 addr_start{}, addr_end{};
    u32 addr_digits{};
    u32 id{};
    void *read_mem16_ptr{};
    void (*read_mem16)(void *ptr, u32 addr, void *dest){};
    char name[50]{};
    char *input_buf{};
};

struct memory_view {
    std::vector<memory_view_module> modules; //50

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
    std::vector<disassembly_range> ranges; // 100
    std::vector<u32> dirty_range_indices; // 100 indices of dirty ranges we can re-use
    struct {
        std::vector<cpu_reg_context> regs; // 32 cpu_reg_context
    } cpu;
    jsm_string processor_name{40};

    u32 addr_column_size{}; // how many columns to put in address of disassembly view
    u32 has_context{};

    struct { // get_disassembly_vars gets variables for disassembly such as address of currently-executing instructions
        void *ptr{};
        disassembly_vars (*func)(void *, struct debugger_interface *, struct disassembly_view *){};
    } get_disassembly_vars{};

    struct { // fill_view fills out variables and stuff
        void *ptr{};
        void (*func)(void *, struct debugger_interface *dbgr, struct disassembly_view *dview){};
    } fill_view{};

    struct {
        void *ptr{};
        int (*func)(void *, u32 addr, char *out, size_t out_sz){};
    } print_addr{};

    struct { // get_disaassembly gets disasssembly
        void *ptr{};
        void (*func)(void *, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry){};
    } get_disassembly{};
};

struct event_category {
    char name[50]{};
    u32 color{};
    u32 id{};
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
    cvec_ptr<event_category> category{};
    char name[50]{};

    std::vector<debugger_event_update>updates[2]{};
    u32 updates_index{};
    u32 color{};
    u32 category_id{};
    u32 display_enabled{};

    struct {
        char context[50]{};
    } tag{};

    struct {
        u32 order{};
    } list{};

    struct {
        u32 enabled{};
        debugger_event_kind kind{};
    } display{};
};

enum ev_timing_kind {
    ev_timing_scanxy,
    ev_timing_master_clock
};


struct console_view {
    void add_char(u8 c);
    void add_cstr(char *s);
    void render_to_buffer(char *output, u64 sz);

    jsm_string buffer[2] = {jsm_string(1024*1024), jsm_string(1024*1024)};
    char name[100]{};
    u32 updated{};
    u32 cur_buf{};
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
    u32 category_id{};
    u64 timecode{};
    dbglog_severity severity{};
    jsm_string text{200}, extra{250};
};

struct dbglog_view;
struct dbglog_category_node {
    char name[100]{};
    char short_name[20]{};
    u32 category_id{};
    u32 enabled{};

    std::vector<dbglog_category_node> children{};

    dbglog_category_node &add_node(dbglog_view &dv, const char *name, const char *short_name, u32 id, u32 color);
};

struct dbglog_view {
    void add_item(u32 id, u64 timecode, dbglog_severity severity, const char *txt);
    void extra_printf( const char *format, ...);
    void add_printf(u32 id, u64 timecode, dbglog_severity severity, const char *format, ...);
    void render_to_buffer(char *output, u64 sz);
    u32 count_visible_lines();
    u32 get_nth_visible(u32 n);
    u32 get_next_visible(u32 start);

    char name[100]{};

    u32 ids_enabled[MAX_DBGLOG_IDS]{};
    struct dbglog_category_node *id_to_category[MAX_DBGLOG_IDS]{};
    u32 id_to_color[MAX_DBGLOG_IDS]{};
    u32 updated{};
    u32 has_extra{};

    struct {
        dbglog_entry data[MAX_DBGLOG_LINES]{};
        u32 first_entry{}, next_entry{}, len{};
    } items{};

    dbglog_entry *last_added{};

    dbglog_category_node category_root{};
};

struct events_view {
    std::vector<debugger_event> events{}; // prealloc 1000?

    ev_timing_kind timing{};

    std::vector<event_category> categories{};
    u64 current_frame{};

    u32 index_in_use{};

    u64 last_frame{};
    u32 keep_last_frame{};
    cvec_ptr<JSM_DISPLAY> associated_display{};

    struct events_view_master_clocks {
        u64 per_line{};
        u64 lines[2][1024]{};
        u64 draw_starts[2][1024]{};
        u32 height{};
        i32 cur_line{};
        u64 *ptr{};
        u32 back_buffer{};
        u32 front_buffer{};
    } master_clocks{};


    struct DVDP {
        u32 width{}, height{};
        float width_scale{}, height_scale{};
        u32 *buf{};
        u64 frame_num{};
    } display[2]{};
};

struct ivec2 {
    i32 x, y;
};

struct debugger_view;
struct debugger_update_func {
    void *ptr;
    void (*func)(debugger_interface*, struct debugger_view *, void *, u32);
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
    u32 is_unsigned;
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
    std::array<jsm_string, MAX_TRACE_COLS> cols;
    trace_line() :
    cols(make_jsm_string_array<MAX_TRACE_COLS>(80))
    {};

    i32 source_id{};
    u32 empty{};
};

struct trace_view_col {
    char name[50]{};
    i32 default_size{};
};

struct trace_view {
    std::vector<trace_line> lines{};
    std::vector<trace_view_col> columns{};
    trace_view() {lines.reserve(this->max_trace_lines); columns.reserve(MAX_TRACE_COLS); }
    trace_line *lptr{};
    u32 current_output_line{};
    u32 max_trace_lines=100;
    u32 num_trace_lines{};
    u32 waiting_for_startline{};
    u32 waiting_for_endline=1;
    u32 first_line=1;

    // User-set options...
    char name[150]{};
    u32 autoscroll{};
    u32 display_end_top{};
};

struct waveform_view {
    char name[50]{};
    std::vector<debug_waveform> waveforms{};
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
    char text[100]{};
    u32 value{};
};

struct debugger_widget;
struct debugger_widget_radiogroup {
    char title[100]{};
    u32 value{};
    std::vector<debugger_widget> buttons{}; // debugger_widget_checkbox
};

struct debugger_widget_colorkey_item {
    char name[50]{};
    u32 color{};
    u32 hovered{};
};

struct debugger_widget_colorkey {
    char title[100]{};
    u32 num_items{};
    debugger_widget_colorkey_item items[50]{};
};

struct debugger_widget_textbox {
    jsm_string contents{4000};
};

struct debugger_widget {
    explicit debugger_widget(JSMD_widgets kind);
    ~debugger_widget();

    JSMD_widgets kind = JSMD_textbox;
    u32 same_line{}, enabled{}, visible{};

    union {
        debugger_widget_checkbox checkbox{};
        debugger_widget_radiogroup radiogroup;
        debugger_widget_textbox textbox;
        debugger_widget_colorkey colorkey;
    };

private:
    void destroy_active();
};


struct debugger_widget;
struct debugger_view {
    explicit debugger_view(debugger_view_kinds kind);
    ~debugger_view();

    debugger_view_kinds kind;

    struct {
        u32 on_break{}, on_pause{}, on_step{};
        struct { u32 enabled{}; u32 line_num{}; } on_line{};
    } update{};

    std::vector<debugger_widget> options{}; //5

    union {
        disassembly_view disassembly;
        events_view events;
        image_view image;
        waveform_view waveform;
        trace_view trace{};
        console_view console;
        dbglog_view dbglog;
        memory_view memory;
    };

private:

    void destroy_active();
};

/* This need to be used on two sides. The application side, and the core side. */
struct debugger_interface {
    std::vector<debugger_view> views; // debugger_view
    cvec_ptr<debugger_view> &make_view(enum debugger_view_kinds kind);
    u32 smallest_step; // smallest possible step
    u32 supported_by_core;
};

dbglog_category_node &dbglog_category_get_root(dbglog_view &dv);

u32 dbglog_count_visible_lines(dbglog_view *);
u32 dbglog_get_nth_visible(dbglog_view *, u32 n);
u32 dbglog_get_next_visible(dbglog_view *, u32 start);

#define DEBUG_REGISTER_EVENT_CATEGORY(name, id) events_view_add_category(dbgr, ev, name, 0, id)
#define DEBUG_REGISTER_EVENT(name, color, category, id, default_enable) events_view_add_event(dbgr, ev, category, name, color, dek_pix_square, default_enable, 0, NULL, id)
#define SET_EVENT_VIEW(x) x .dbg.events.view = this->dbg.events.view
#define SET_CPU_CPU_EVENT_ID(id, k) this->cpu.cpu.dbg.events. k = id
#define SET_CPU_EVENT_ID(id, k) this->cpu.dbg.events. k = id

#define DBG_EVENT(x) debugger_report_event(this->dbg.events.view, x)

