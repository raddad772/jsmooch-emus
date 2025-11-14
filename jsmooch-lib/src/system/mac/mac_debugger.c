//
// Created by . on 8/7/24.
//

#include <cstdio>
#include <cassert>

#include "mac.h"
#include "mac_internal.h"
#include "mac_debugger.h"

#define JTHIS struct mac* this = (mac*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct mac* this

static int render_imask(cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%d", ctx->int32_data);
}

static int render_csr(cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%c%c%c%c%c",
                    ctx->int32_data & 0x10 ? 'X' : 'x',
                    ctx->int32_data & 8 ? 'N' : 'n',
                    ctx->int32_data & 4 ? 'Z' : 'z',
                    ctx->int32_data & 2 ? 'V' : 'v',
                    ctx->int32_data & 1 ? 'C' : 'c'
                    );
}

static void create_and_bind_registers(mac* this, disassembly_view *dv)
{
    u32 tkindex = 0;
    for (u32 i = 0; i < 8; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        snprintf(rg->name, sizeof(rg->name), "D%d", i);
        rg->kind = RK_int32;
        rg->index = tkindex++;
    }
    for (u32 i = 0; i < 7; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        snprintf(rg->name, sizeof(rg->name), "A%d", i);
        rg->kind = RK_int32;
        rg->index = tkindex++;
    }
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "USP");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SSP");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SR");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "supervisor");
    rg->kind = RK_bool;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "trace");
    rg->kind = RK_bool;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IMASK");
    rg->kind = RK_int32;
    rg->custom_render = &render_imask;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "CSR");
    rg->kind = RK_int32;
    rg->custom_render = &render_csr;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IR");
    rg->kind = RK_bool;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IRC");
    rg->kind = RK_bool;
    rg->index = tkindex++;

#define BIND(dn, index) this->dbg. dn = cvec_get(&dv->cpu.regs, index)
    for (u32 i = 0; i < 8; i++) {
        BIND(D[i], i);
    }

    for (u32 i = 0; i < 7; i++) {
        BIND(A[i], i+8);
    }
    // 15 now
    BIND(PC, 15);
    BIND(USP, 16);
    BIND(SSP, 17);
    BIND(SR, 18);
    BIND(supervisor, 19);
    BIND(trace, 20);
    BIND(IMASK, 21);
    BIND(CSR, 22);
    BIND(IR, 23);
    BIND(IRC, 24);
#undef BIND
}

static void fill_disassembly_view(void *macptr, debugger_interface *dbgr, disassembly_view *dview)
{
    struct mac* this = (mac*)macptr;
    for (u32 i = 0; i < 8; i++) {
        this->dbg.D[i]->int32_data = this->cpu.regs.D[i];
        if (i < 7) this->dbg.A[i]->int32_data = this->cpu.regs.A[i];
    }
    this->dbg.PC->int32_data = this->cpu.regs.PC;
    this->dbg.SR->int32_data = this->cpu.regs.SR.u;
    if (this->dbg.SR->int32_data & 0x2000) { // supervisor mode
        this->dbg.USP->int32_data = this->cpu.regs.ASP;
        this->dbg.SSP->int32_data = this->cpu.regs.A[7];
    }
    else { // user mode
        this->dbg.USP->int32_data = this->cpu.regs.A[7];
        this->dbg.SSP->int32_data = this->cpu.regs.ASP;
    }
    this->dbg.supervisor->bool_data = this->cpu.regs.SR.S;
    this->dbg.trace->bool_data = this->cpu.regs.SR.T;
    this->dbg.IMASK->int32_data = this->cpu.regs.SR.I;
    this->dbg.CSR->int32_data = this->cpu.regs.SR.u & 0x1F;
    this->dbg.IR->int32_data = this->cpu.regs.IR;
    this->dbg.IRC->int32_data = this->cpu.regs.IRC;
}

static struct disassembly_vars get_disassembly_vars(void *macptr, debugger_interface *dbgr, disassembly_view *dv)
{
    struct mac* this = (mac*)macptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->cpu.debug.ins_PC;
    dvar.current_clock_cycle = this->clock.master_cycles;
    return dvar;
}

static void get_dissasembly(void *macptr, debugger_interface *dbgr, disassembly_view *dview, disassembly_entry *entry)
{
    struct mac* this = (mac*)macptr;
    M68k_disassemble_entry(&this->cpu, entry);
}

void macJ_setup_debugger_interface(JSM, debugger_interface *dbgr)
{
    JTHIS;
    this->dbgr = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 2;

    // Setup diassembly
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFFFF;
    dv->addr_column_size = 6;
    dv->has_context = 1;
    jsm_string_sprintf(&dv->processor_name, "m68000");

    create_and_bind_registers(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_dissasembly;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars;
}
