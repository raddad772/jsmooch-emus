//
// Created by . on 8/13/24.
//

#ifndef JSMOOCH_EMUS_DEBUGGERDEFS_H
#define JSMOOCH_EMUS_DEBUGGERDEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers/cvec.h"

#define DBG_START struct { struct debugger_interface *interface;
#define DBG_CPU_REG_START struct { struct cpu_reg_context
#define DBG_CPU_REG_END ; } dasm;

#define DBG_WAVEFORM_START struct { struct cvec_ptr view;
#define DBG_WAVEFORM_MAIN struct cvec_ptr main;
#define DBG_WAVEFORM_CHANS(x) struct cvec_ptr chan[x];
#define DBG_WAVEFORM_END } waveforms;


#define DBG_EVENT_VIEW struct { struct cvec_ptr view; } events;
#define DBG_IMAGE_VIEW(name) struct { struct cvec_ptr name; } image_views;

#define DBG_IMAGE_VIEWS_START struct {
#define MDBG_IMAGE_VIEW(name) struct cvec_ptr name;
#define DBG_IMAGE_VIEWS_END } image_views;

#define DBG_EVENT_VIEW_ONLY_START struct { struct { struct cvec_ptr view; u32
#define DBG_EVENT_VIEW_ONLY_END ;} events; } dbg;

#define DBG_END } dbg;

#define DBG_EVENT_VIEW_ONLY struct { struct { struct cvec_ptr view; } events; } dbg

#define DBG_EVENT_VIEW_INIT cvec_ptr_init(&this->dbg.events.view)

#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_DEBUGGERDEFS_H
