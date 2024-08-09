//
// Created by . on 8/8/24.
//

#include <stdio.h>
#include <string.h>

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

    this->dbg.A->int8_data = this->cpu.cpu.regs.A;
    this->dbg.X->int8_data = this->cpu.cpu.regs.X;
    this->dbg.Y->int8_data = this->cpu.cpu.regs.Y;
    this->dbg.PC->int16_data = this->cpu.cpu.regs.PC;
    this->dbg.S->int8_data = this->cpu.cpu.regs.S;
    this->dbg.P->int8_data = M6502_regs_P_getbyte(&this->cpu.cpu.regs.P);
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

#define BIND(dn, index) this->dbg. dn = cvec_get(&dv->cpu.regs, index)
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
    this->dbgr = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;

    // Setup diassembly
    struct debugger_view *dview = cvec_push_back(&dbgr->views);
    debugger_view_init(dview, dview_disassembly);

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

}