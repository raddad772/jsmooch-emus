//
// Created by . on 8/7/24.
//

#include <cstdio>
#include <cassert>
#include <cstring>

#include "mac.h"
#include "mac_bus.h"
#include "mac_debugger.h"

#define JTHIS mac* this = (mac*)jsm->ptr
#define JSM jsm_system* jsm

#define THIS mac* this

namespace mac {

static int render_imask(cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%d", ctx->int32_data);
}

static int render_csr(cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%c%c%c%c%c",
                    ctx->int32_data & 0x10 ? 'X' : 'x',
                    ctx->int32_data & 8 ? 'N' : 'n',
                    ctx->int32_data & 4 ? 'Z' : 'z',
                    ctx->int32_data & 2 ? 'V' : 'v',
                    ctx->int32_data & 1 ? 'C' : 'c'
    );
}

static void create_and_bind_registers(core* th, disassembly_view *dv)
{
    u32 tkindex = 0;
    for (u32 i = 0; i < 8; i++) {
        cpu_reg_context *rg = &dv->cpu.regs.emplace_back();
        snprintf(rg->name, sizeof(rg->name), "D%d", i);
        rg->kind = RK_int32;
        rg->custom_render = nullptr;
        rg->index = tkindex++;
    }
    for (u32 i = 0; i < 7; i++) {
        cpu_reg_context *rg = &dv->cpu.regs.emplace_back();
        snprintf(rg->name, sizeof(rg->name), "A%d", i);
        rg->kind = RK_int32;
        rg->custom_render = nullptr;
        rg->index = tkindex++;
    }
    cpu_reg_context *rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int32;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "USP");
    rg->kind = RK_int32;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "SSP");
    rg->kind = RK_int32;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "SR");
    rg->kind = RK_int32;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "supervisor");
    rg->kind = RK_bool;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "trace");
    rg->kind = RK_bool;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "IMASK");
    rg->kind = RK_int32;
    rg->custom_render = &render_imask;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "CSR");
    rg->kind = RK_int32;
    rg->custom_render = &render_csr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "IR");
    rg->kind = RK_bool;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

    rg = &dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "IRC");
    rg->kind = RK_bool;
    rg->custom_render = nullptr;
    rg->index = tkindex++;

#define BIND(dn, index) th->dbg.dasm. dn = &dv->cpu.regs.at(index)
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

static disassembly_vars get_disassembly_vars(void *genptr, disassembly_view &dv)
{
    auto *th = static_cast<core *>(genptr);
    disassembly_vars dvar;
    dvar.address_of_executing_instruction = th->cpu.debug.ins_PC;
    dvar.current_clock_cycle = th->clock.master_cycles;
    return dvar;
}

static void get_disassembly(void *genptr, disassembly_view &dview, disassembly_entry &entry)
{
    auto *th = static_cast<core*>(genptr);
    th->cpu.disassemble_entry(entry);
}

static void fill_disassembly_view(void *genptr, disassembly_view &dview)
{
    auto *th = static_cast<core *>(genptr);
    for (u32 i = 0; i < 8; i++) {
        th->dbg.dasm.D[i]->int32_data = th->cpu.regs.D[i];
        if (i < 7) th->dbg.dasm.A[i]->int32_data = th->cpu.regs.A[i];
    }
    th->dbg.dasm.PC->int32_data = th->cpu.regs.PC;
    th->dbg.dasm.SR->int32_data = th->cpu.regs.SR.u;
    if (th->dbg.dasm.SR->int32_data & 0x2000) { // supervisor mode
        th->dbg.dasm.USP->int32_data = th->cpu.regs.ASP;
        th->dbg.dasm.SSP->int32_data = th->cpu.regs.A[7];
    }
    else { // user mode
        th->dbg.dasm.USP->int32_data = th->cpu.regs.A[7];
        th->dbg.dasm.SSP->int32_data = th->cpu.regs.ASP;
    }
    th->dbg.dasm.supervisor->bool_data = th->cpu.regs.SR.S;
    th->dbg.dasm.trace->bool_data = th->cpu.regs.SR.T;
    th->dbg.dasm.IMASK->int32_data = th->cpu.regs.SR.I;
    th->dbg.dasm.CSR->int32_data = th->cpu.regs.SR.u & 0x1F;
    th->dbg.dasm.IR->int32_data = th->cpu.regs.IR;
    th->dbg.dasm.IRC->int32_data = th->cpu.regs.IRC;
}

static void setup_disassembly_view(core& th, debugger_interface *dbgr) {
    cvec_ptr<debugger_view> p = dbgr->make_view(dview_disassembly);
    debugger_view *dview = &p.get();
    disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFFFF;
    dv->addr_column_size = 6;
    dv->has_context = 1;
    dv->processor_name.sprintf("m68000");

    create_and_bind_registers(&th, dv);
    dv->fill_view.ptr = static_cast<void *>(&th);
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = static_cast<void *>(&th);
    dv->get_disassembly.func = &get_disassembly;

    dv->get_disassembly_vars.ptr = static_cast<void *>(&th);
    dv->get_disassembly_vars.func = &get_disassembly_vars;
}

void core::setup_debugger_interface(debugger_interface &dbgr)
{
    dbg.interface = &dbgr;

    dbgr.supported_by_core = true;
    dbgr.smallest_step = 2;
    dbgr.views.reserve(15);

    setup_disassembly_view(*this, &dbgr);
}
}