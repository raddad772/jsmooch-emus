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

    this->dbg.events.category.CPU_timer = events_view_add_category(dbgr, ev, "Timer events", 0xFF0000, 0);
    this->dbg.events.category.CPU = events_view_add_category(dbgr, ev, "SM83 events", 0xFF0000, 0);
    this->dbg.events.category.PPU = events_view_add_category(dbgr, ev, "PPU events", 0x0000FF, 1);

#define MK_CPU_CPU(x, str, color) this->cpu.cpu.dbg.event. x =  events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, str, color, dek_pix_square, 1, 0, NULL)
    MK_CPU_CPU(IRQ_joypad, "IRQ: joypad", 0xFF0000);
    MK_CPU_CPU(IRQ_vblank, "IRQ: vblank", 0x00FF00);
    MK_CPU_CPU(IRQ_stat, "IRQ: stat", 0x802060);
    MK_CPU_CPU(IRQ_timer, "IRQ: timer", 0xFF0000);
    MK_CPU_CPU(IRQ_serial, "IRQ: serial", 0xFF0000);
    MK_CPU_CPU(HALT_end, "HALT end", 0xFF00FF);

#define MK_CPU(x, str, color) this->cpu.dbg.event. x =  events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, str, color, dek_pix_square, 1, 0, NULL)
    MK_CPU(OAM_DMA_start, "OAM DMA start", 0x004090);

#define MK_CPU_TIMER(x, str, color) this->cpu.timer.dbg.event. x =  events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, str, color, dek_pix_square, 1, 0, NULL)
    MK_CPU_TIMER(timer_tick, "tick", 0xFF4090);
    MK_CPU_TIMER(timer_IRQ, "IRQ triggered", 0x409040);

#define MK_BUS(x, str, color) this->bus.dbg.event. x =  events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, str, color, dek_pix_square, 1, 0, NULL)
    MK_BUS(VRAM_write, "VRAM write", 0x0000FF);

#define MK_PPU(x, str, color) this->ppu.dbg.event. x =  events_view_add_event(dbgr, ev, this->dbg.events.category.PPU, str, color, dek_pix_square, 1, 0, NULL)
    MK_PPU(SCY_write, "SCY", 0x00FFFF);
    MK_PPU(SCX_write, "SCX", 0xFFFF00);


#define SET_EVENT_VIEW(x) x .dbg.event.view = this->dbg.events.view
    SET_EVENT_VIEW(this->cpu.cpu);
    SET_EVENT_VIEW(this->cpu);
    SET_EVENT_VIEW(this->ppu);
    SET_EVENT_VIEW(this->cpu.timer);
    SET_EVENT_VIEW(this->bus);

    event_view_begin_frame(this->dbg.events.view);
}
