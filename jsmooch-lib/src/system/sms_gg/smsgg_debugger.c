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

static void get_dissasembly(void *smsggptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct SMSGG* this = (struct SMSGG*)smsggptr;
    Z80_disassemble_entry(&this->cpu, entry);
}


static struct disassembly_vars get_disassembly_vars(void *smsggptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct SMSGG* this = (struct SMSGG*)smsggptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->cpu.PCO;
    dvar.current_clock_cycle = this->clock.trace_cycles;
    return dvar;
}


static int render_f(struct cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%c%c%c%c%c%c%c%c",
                    ctx->int32_data & 0x80 ? 'S' : 's',
                    ctx->int32_data & 0x40 ? 'Z' : 'z',
                    ctx->int32_data & 0x20 ? 'Y' : 'y',
                    ctx->int32_data & 0x10 ? 'H' : 'h',
                    ctx->int32_data & 8 ? 'X' : 'x',
                    ctx->int32_data & 4 ? 'V' : 'v',
                    ctx->int32_data & 2 ? 'N' : 'n',
                    ctx->int32_data & 1 ? 'C' : 'c'
    );
}

static void fill_disassembly_view(void *smsggptr, struct debugger_interface *dbgr, struct disassembly_view *dview)
{
    struct SMSGG* this = (struct SMSGG*)smsggptr;

    this->dbg.dasm.A->int8_data = this->cpu.regs.A;
    this->dbg.dasm.B->int8_data = this->cpu.regs.B;
    this->dbg.dasm.C->int8_data = this->cpu.regs.C;
    this->dbg.dasm.D->int8_data = this->cpu.regs.D;
    this->dbg.dasm.E->int8_data = this->cpu.regs.E;
    this->dbg.dasm.F->int32_data = Z80_regs_F_getbyte(&this->cpu.regs.F);
    this->dbg.dasm.HL->int16_data = (this->cpu.regs.H << 8) | this->cpu.regs.L;
    this->dbg.dasm.PC->int16_data = this->cpu.regs.PC;
    this->dbg.dasm.SP->int16_data = this->cpu.regs.SP;
    this->dbg.dasm.IX->int16_data = this->cpu.regs.IX;
    this->dbg.dasm.IY->int16_data = this->cpu.regs.IY;
    this->dbg.dasm.EI->bool_data = this->cpu.regs.EI;
    this->dbg.dasm.HALT->bool_data = this->cpu.regs.HALT;
    this->dbg.dasm.AF_->int16_data = this->cpu.regs.AF_;
    this->dbg.dasm.BC_->int16_data = this->cpu.regs.BC_;
    this->dbg.dasm.DE_->int16_data = this->cpu.regs.DE_;
    this->dbg.dasm.HL_->int16_data = this->cpu.regs.HL_;

}

static void create_and_bind_registers(struct SMSGG* this, struct disassembly_view *dv)
{
    u32 tkindex = 0;
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "B");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "C");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "D");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "E");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "F");
    rg->kind = RK_int32;
    rg->index = tkindex++;
    rg->custom_render = &render_f;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "HL");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "SP");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "IX");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "IY");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "EI");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "HALT");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "AF_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "BC_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "DE_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "HL_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "CxI");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

#define BIND(dn, index) this->dbg.dasm. dn = cvec_get(&dv->cpu.regs, index)
    BIND(A, 0);
    BIND(B, 1);
    BIND(C, 2);
    BIND(D, 3);
    BIND(E, 4);
    BIND(F, 5);
    BIND(HL, 6);
    BIND(PC, 7);
    BIND(SP, 8);
    BIND(IX, 9);
    BIND(IY, 10);
    BIND(EI, 11);
    BIND(HALT, 12);
    BIND(AF_, 13);
    BIND(BC_, 14);
    BIND(DE_, 15);
    BIND(HL_, 16);
    BIND(CE, 17);
#undef BIND
}


void SMSGGJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
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
    jsm_string_sprintf(&dv->processor_name, "Z80");

    create_and_bind_registers(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_dissasembly;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars;


    // Setup event view

    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    dview = cpg(this->dbg.events.view);
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
