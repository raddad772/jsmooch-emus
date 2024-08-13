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

    this->dbg.events.category.CPU = events_view_add_category(dbgr, ev, "CPU events", 0xFF0000, 0);
    this->dbg.events.category.VDP = events_view_add_category(dbgr, ev, "VDP events", 0x0000FF, 1);

    this->dbg.events.IRQ = events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, "IRQ", 0xFF0000, dek_pix_square, 1, 0, NULL);
    this->dbg.events.NMI = events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, "NMI", 0x808000, dek_pix_square, 1, 0, NULL);
    this->dbg.events.write_hscroll = events_view_add_event(dbgr, ev, this->dbg.events.category.VDP, "Write H scroll", 0x00FF00, dek_pix_square, 1, 0, NULL);
    this->dbg.events.write_vscroll = events_view_add_event(dbgr, ev, this->dbg.events.category.VDP, "Write V scroll", 0x0000FF, dek_pix_square, 1, 0, NULL);
    this->dbg.events.write_vram = events_view_add_event(dbgr, ev, this->dbg.events.category.VDP, "Write VRAM", 0x0080FF, dek_pix_square, 1, 0, NULL);

    this->cpu.dbg.event.view = this->dbg.events.view;
    this->cpu.dbg.event.IRQ = this->dbg.events.IRQ;
    this->cpu.dbg.event.NMI = this->dbg.events.NMI;
    event_view_begin_frame(this->dbg.events.view);
}
