//
// Created by . on 8/13/24.
//

#pragma once

#include "helpers/cvec.h"
struct debugger_view;
struct memory_view;

#ifdef interface
#undef interface
#endif

#define DBG_START struct { struct debugger_interface *interface{};

#define DBG_CPU_REG_START1 struct { struct cpu_reg_context
#define DBG_CPU_REG_END1 ; } dasm{};

#define DBG_CPU_REG_START(x) struct { struct cpu_reg_context
#define DBG_CPU_REG_END(x) ; } dasm_##x{};

#define DBG_WAVEFORM_START1 struct { cvec_ptr<debugger_view> view{};
#define DBG_WAVEFORM_END1 } waveforms{};
#define DBG_WAVEFORM_START(x) struct { cvec_ptr<debugger_view> view{};
#define DBG_WAVEFORM_MAIN cvec_ptr<debug_waveform> main{}; struct debug_waveform *main_cache{};
#define DBG_WAVEFORM_CHANS(x) cvec_ptr<debug_waveform> chan[x]{}; struct debug_waveform *chan_cache[x]{};
#define DBG_WAVEFORM_END(x) } waveforms_##x{};

#define DBG_MEMORY_VIEW cvec_ptr<debugger_view> memory{};

#define DBG_EVENT_VIEW struct { cvec_ptr<debugger_view> view{}; } events{};
#define DBG_EVENT_VIEW_START struct { cvec_ptr<debugger_view> view{}; u32
#define DBG_EVENT_VIEW_END ;} events{};

#define DBG_TRACE_VIEW struct trace_view *tvptr{};
#define DBG_LOG_VIEW struct dbglog_view *dvptr{}; u32 dv_id{};
#define DBG_IMAGE_VIEW(name) struct { cvec_ptr name{}; } image_views{};

#define DBG_IMAGE_VIEWS_START struct {
#define MDBG_IMAGE_VIEW(name) cvec_ptr<debugger_view> name{};
#define DBG_IMAGE_VIEWS_END } image_views{};

#define DBG_EVENT_VIEW_ONLY_START struct { struct { cvec_ptr<debugger_view> view{}; u32
#define DBG_EVENT_VIEW_ONLY_END ;} events{}; } dbg{};

#define DBG_END } dbg{};

#define DBG_EVENT_VIEW_ONLY struct { struct { cvec_ptr view{}; } events{}; } dbg{}
#define DBG_EVENT_VIEW_ONLY_IDS_START struct { struct { cvec_ptr view{}; u32
#define DBG_EVENT_VIEW_ONLY_IDS_END ; } events{}; } dbg{};
#define DBG_TRACE_VIEW_INIT this->dbg.tvptr = NULL

