//
// Created by . on 8/8/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "component/cpu/m6502/m6502.h"
#include "component/cpu/m6502/m6502_disassembler.h"
#include "nes_debugger.h"
#include "nes.h"

#define JTHIS struct NES* this = (struct NES*)jsm->ptr
#define JSM struct jsm_system* jsm

static void get_dissasembly(void *nesptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct NES* this = (struct NES*)nesptr;
    M6502_disassemble_entry(&this->cpu.cpu, entry);
}


static struct disassembly_vars get_disassembly_vars(void *nesptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct NES* this = (struct NES*)nesptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->cpu.cpu.PCO;
    dvar.current_clock_cycle = this->clock.trace_cycles;
    return dvar;
}

static int render_p(struct cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%c%c-%c%c%c%c%c",
                    ctx->int32_data & 0x80 ? 'N' : 'n',
                    ctx->int32_data & 0x40 ? 'V' : 'v',
                    ctx->int32_data & 0x10 ? 'B' : 'b',
                    ctx->int32_data & 8 ? 'D' : 'd',
                    ctx->int32_data & 4 ? 'I' : 'i',
                    ctx->int32_data & 2 ? 'Z' : 'z',
                    ctx->int32_data & 1 ? 'C' : 'c'
    );
}

static void fill_disassembly_view(void *nesptr, struct debugger_interface *dbgr, struct disassembly_view *dview)
{
    struct NES* this = (struct NES*)nesptr;

    this->dbg.dasm.A->int8_data = this->cpu.cpu.regs.A;
    this->dbg.dasm.X->int8_data = this->cpu.cpu.regs.X;
    this->dbg.dasm.Y->int8_data = this->cpu.cpu.regs.Y;
    this->dbg.dasm.PC->int16_data = this->cpu.cpu.regs.PC;
    this->dbg.dasm.S->int8_data = this->cpu.cpu.regs.S;
    this->dbg.dasm.P->int8_data = M6502_regs_P_getbyte(&this->cpu.cpu.regs.P);
}

static void create_and_bind_registers(struct NES* this, struct disassembly_view *dv)
{
    u32 tkindex = 0;
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "X");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "Y");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "S");
    rg->kind = RK_int8;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "P");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_p;

#define BIND(dn, index) this->dbg.dasm. dn = cvec_get(&dv->cpu.regs, index)
    BIND(A, 0);
    BIND(X, 1);
    BIND(Y, 2);
    BIND(PC, 3);
    BIND(S, 4);
    BIND(P, 5);
#undef BIND
}

void NESJ_setup_debugger_interface(struct jsm_system *jsm, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;

    // Setup diassembly
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->addr_column_size = 4;
    dv->has_context = 0;
    jsm_string_sprintf(&dv->processor_name, "m6502");

    create_and_bind_registers(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_dissasembly;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars;

    // Setup events view
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 341;
        ev->display[i].height = 262;
        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }
    ev->associated_display = this->ppu.display_ptr;

    this->dbg.events.category.CPU = events_view_add_category(dbgr, ev, "CPU events", 0xFF0000);
    this->dbg.events.category.PPU = events_view_add_category(dbgr, ev, "PPU events", 0x0000FF);

    this->dbg.events.IRQ = events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, "IRQ", 0xFF0000, dek_pix_square, 1, 0, NULL);
    this->dbg.events.NMI = events_view_add_event(dbgr, ev, this->dbg.events.category.CPU, "NMI", 0x0FF000, dek_pix_square, 1, 0, NULL);
    this->dbg.events.w2006 = events_view_add_event(dbgr, ev, this->dbg.events.category.PPU, "Write $2006", 0x00FF00, dek_pix_square, 1, 0, NULL);
    this->dbg.events.w2007 = events_view_add_event(dbgr, ev, this->dbg.events.category.PPU, "Write $2007", 0x000FF0, dek_pix_square, 1, 0, NULL);

    this->cpu.cpu.dbg.event.view = this->dbg.events.view;
    this->cpu.cpu.dbg.event.IRQ = this->dbg.events.IRQ;
    this->cpu.cpu.dbg.event.NMI = this->dbg.events.NMI;
    event_view_begin_frame(this->dbg.events.view);
}

