//
// Created by . on 8/12/24.
//

//
// Created by . on 8/12/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "helpers/debugger/debugger.h"

#include "component/cpu/sm83/sm83.h"
#include "component/cpu/sm83/sm83_disassembler.h"

#include "gb.h"
#include "gb_debugger.h"

#define JTHIS struct GB* this = (struct GB*)jsm->ptr
#define JSM struct jsm_system* jsm


void GBJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
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
    ev->associated_display = this->ppu.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("Timer events", DBG_GB_CATEGORY_TIMER);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_GB_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_GB_CATEGORY_PPU);

    cvec_grow(&ev->events, DBG_GB_EVENT_MAX);
    DEBUG_REGISTER_EVENT("IRQ: joypad", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_JOYPAD);
    DEBUG_REGISTER_EVENT("IRQ: vblank", 0x00FF00, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_VBLANK);
    DEBUG_REGISTER_EVENT("IRQ: stat", 0x802060, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_STAT);
    DEBUG_REGISTER_EVENT("IRQ: timer", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_TIMER);
    DEBUG_REGISTER_EVENT("IRQ: serial", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_SERIAL);
    DEBUG_REGISTER_EVENT("HALT end", 0xFF00FF, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_HALT_END);
    DEBUG_REGISTER_EVENT("OAM DMA start", 0x004090, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_OAM_DMA_START);

    DEBUG_REGISTER_EVENT("tick", 0xFF4090, DBG_GB_CATEGORY_TIMER, DBG_GB_EVENT_TIMER_TICK);
    DEBUG_REGISTER_EVENT("IRQ", 0xFF4090, DBG_GB_CATEGORY_TIMER, DBG_GB_EVENT_TIMER_IRQ);

    DEBUG_REGISTER_EVENT("VRAM write", 0x0000FF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_VRAM_WRITE);
    DEBUG_REGISTER_EVENT("SCX write", 0x00FFFF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_SCX_WRITE);
    DEBUG_REGISTER_EVENT("SCY write", 0xFFFF00, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_SCY_WRITE);

    SET_EVENT_VIEW(this->cpu.cpu);
    SET_EVENT_VIEW(this->cpu);
    SET_EVENT_VIEW(this->ppu);
    SET_EVENT_VIEW(this->cpu.timer);
    SET_EVENT_VIEW(this->bus);

    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_JOYPAD, IRQ_joypad);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_VBLANK, IRQ_vblank);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_STAT, IRQ_stat);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_TIMER, IRQ_timer);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_SERIAL, IRQ_serial);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_HALT_END, HALT_end);

    event_view_begin_frame(this->dbg.events.view);
}
