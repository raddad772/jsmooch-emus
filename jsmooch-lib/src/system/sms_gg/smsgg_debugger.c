//
// Created by . on 8/12/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "component/cpu/z80/z80.h"
#include "component/cpu/z80/z80_disassembler.h"
#include "sms_gg.h"

#include "smsgg_debugger.h"

#define JTHIS struct SMSGG* this = (struct SMSGG*)jsm->ptr
#define JSM struct jsm_system* jsm


void SMSGGJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;

    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 342;
        ev->display[i].height = 262;
        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }
    ev->associated_display = this->vdp.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_SMSGG_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("VDP events", DBG_SMSGG_CATEGORY_VDP);

    cvec_grow(&ev->events, DBG_SMSGG_EVENT_MAX);
    ///void events_view_add_event(struct debugger_interface *dbgr, struct events_view *ev, u32 category_id, const char *name, u32 color, enum debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id);
    DEBUG_REGISTER_EVENT("IRQ", 0xFF0000, DBG_SMSGG_CATEGORY_CPU, DBG_SMSGG_EVENT_IRQ);
    DEBUG_REGISTER_EVENT("NMI", 0x808000, DBG_SMSGG_CATEGORY_CPU, DBG_SMSGG_EVENT_NMI);
    DEBUG_REGISTER_EVENT("Write H scroll", 0x00FF00, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_HSCROLL);
    DEBUG_REGISTER_EVENT("Write V scroll", 0x0000FF, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_VSCROLL);
    DEBUG_REGISTER_EVENT("Write VRAM", 0x0080FF, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_VRAM);

    SET_EVENT_VIEW(this->cpu);
    SET_EVENT_VIEW(this->vdp);
    SET_CPU_EVENT_ID(DBG_SMSGG_EVENT_IRQ, IRQ);
    SET_CPU_EVENT_ID(DBG_SMSGG_EVENT_NMI, NMI);

    event_view_begin_frame(this->dbg.events.view);
}
