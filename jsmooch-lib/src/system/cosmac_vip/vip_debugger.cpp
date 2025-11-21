//
// Created by . on 11/21/25.
//


#include <cstdio>
#include <cassert>

#include "vip_debugger.h"
#include "vip_bus.h"
#include "component/cpu/rca1802/rca1802_disassembler.h"


namespace VIP {
static void render_image_view_video(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    core *th = static_cast<core *>(ptr);
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    auto *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 256 * 4); // Clear out at least 4 rows worth
    u16 addr = 0x400;

    for (u32 y = 0; y < 128; y++) {
        u32 *optr = &outbuf[y * out_width];
        for (u32 xstep=0; xstep < 8; xstep++) {
            u8 data = th->read_main_bus(addr++, 0, false);
            for (u8 i = 0; i < 8; i++) {
                u8 v = (data >> 7) & 1;
                data <<= 1;
                *optr = v ? 0xFFFFFFFF : 0xFF000000;
                optr++;
            }
        }
    }
}

    
static void readcpumem(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    u8 *out = static_cast<u8 *>(dest);
    core *th = static_cast<core *>(ptr);
    for (u32 i = 0; i < 16; i++) {
        *out = th->read_main_bus((addr + i) & 0x7FFF, 0, false);
        out++;
    }
}

static void setup_memory_view(core& th, debugger_interface &dbgr) {
    th.dbg.memory = dbgr.make_view(dview_memory);
    debugger_view *dview = &th.dbg.memory.get();
    memory_view *mv = &dview->memory;
    mv->add_module("CPU Memory", 0, 4, 0, 0x7FFF, &th, &readcpumem);
}

static void setup_waveforms(core& th, debugger_interface *dbgr)
{
    th.dbg.waveforms.view = dbgr->make_view(dview_waveforms);
    debugger_view &dview = th.dbg.waveforms.view.get();
    waveform_view &wv = dview.waveform;
    snprintf(wv.name, sizeof(wv.name), "Beeper");

    debug_waveform *dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.main.make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
}

static void fill_disassembly_view(void *coreptr, disassembly_view &dview)
{
    core* th = static_cast<core *>(coreptr);

    th->dbg.dasm_cpu.X->int4_data = th->cpu.regs.X;
    th->dbg.dasm_cpu.N->int4_data = th->cpu.regs.N;
    th->dbg.dasm_cpu.P->int4_data = th->cpu.regs.P;
    th->dbg.dasm_cpu.I->int4_data = th->cpu.regs.I;
    th->dbg.dasm_cpu.T->int8_data = th->cpu.regs.T.u;
    th->dbg.dasm_cpu.IE->bool_data = th->cpu.regs.IE;
    th->dbg.dasm_cpu.D->int8_data = th->cpu.regs.D;
    th->dbg.dasm_cpu.DF->bool_data = th->cpu.regs.DF;
    th->dbg.dasm_cpu.Q->bool_data = th->cpu.pins.Q;
    for (u32 i = 0; i < 16; i++) {
        th->dbg.dasm_cpu.R[i]->int16_data = th->cpu.regs.R[i].u;
    }
}


static void create_and_bind_registers(core &th, disassembly_view &dv)
{
    u32 tkindex = 0;
    cpu_reg_context *rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "X");
    rg->kind = RK_int4;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "N");
    rg->kind = RK_int4;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "P");
    rg->kind = RK_int4;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "I");
    rg->kind = RK_int4;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "T");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "IE");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "D");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "DF");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "Q");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    for (u32 i = 0; i < 16; i++) {
        rg = &dv.cpu.regs.emplace_back();
        snprintf(rg->name, sizeof(rg->name), "R%02d", i);
        rg->kind = RK_int16;
        rg->index = tkindex++;
        rg->custom_render = nullptr;
    }

#define BIND(dn, index) th.dbg.dasm_cpu. dn = &dv.cpu.regs.at(index)
    u32 idx = 0;
    BIND(X, idx++);
    BIND(N, idx++);
    BIND(P, idx++);
    BIND(I, idx++);
    BIND(T, idx++);
    BIND(IE, idx++);
    BIND(D, idx++);
    BIND(DF, idx++);
    BIND(Q, idx++);
    for (u32 i = 0; i < 16; i++) {
        BIND(R[i], idx++);
    }
#undef BIND
}

static int print_dasm_addr(void *coreptr, u32 addr, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%04x", addr);
    return 4;
}

static void get_disassembly(void *coreptr, disassembly_view &dview, disassembly_entry &entry)
{
    core* th = static_cast<core *>(coreptr);
    RCA1802::disassemble_entry(&th->cpu, entry);
}

static disassembly_vars get_disassembly_vars(void *coreptr, disassembly_view &dv)
{
    core* th = static_cast<core *>(coreptr);
    disassembly_vars dvar;
    dvar.address_of_executing_instruction = th->cpu.most_recent_fetch;
    dvar.current_clock_cycle = th->clock.master_cycle_count;
    printf("\nGIVEN %04x", th->cpu.most_recent_fetch);
    return dvar;
}


static void setup_disassembly_view(core& th, debugger_interface *dbgr)
{
    cvec_ptr<debugger_view> p = dbgr->make_view(dview_disassembly);

    debugger_view &dview = p.get();
    disassembly_view &dv = dview.disassembly;
    dv.addr_column_size = 4;
    dv.has_context = 0;
    dv.processor_name.sprintf("CDP1802");

    create_and_bind_registers(th, dv);
    dv.mem_end = 0xFFFF;
    dv.fill_view.ptr = &th;
    dv.fill_view.func = &fill_disassembly_view;

    dv.print_addr.ptr = &th;
    dv.print_addr.func = &print_dasm_addr;

    dv.get_disassembly.ptr = &th;;
    dv.get_disassembly.func = &get_disassembly;

    dv.get_disassembly_vars.ptr = &th;;
    dv.get_disassembly_vars.func = &get_disassembly_vars;
}

static void setup_dbglog(core &th, debugger_interface &dbgr)
{
    cvec_ptr p = dbgr.make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th.dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = 1;

    static constexpr u32 cpu_color = 0x8080FF;

    dbglog_category_node &root = dv.get_category_root();
    dbglog_category_node &wdc = root.add_node(dv, "CDP1802", nullptr, 0, 0);
    wdc.add_node(dv, "Instruction Trace", "cpu", VIP_CAT_CPU_INSTRUCTION, cpu_color);
    th.cpu.trace.dbglog.view = &dv;
    th.cpu.trace.dbglog.id = VIP_CAT_CPU_INSTRUCTION;
    wdc.add_node(dv, "Reads", "cpu.read", VIP_CAT_CPU_READ, cpu_color);
    wdc.add_node(dv, "Writes", "cpu.write", VIP_CAT_CPU_WRITE, cpu_color);
}

static void setup_image_view_video(core& th, debugger_interface &dbgr)
{
    th.dbg.image_views.video = dbgr.make_view(dview_image);
    debugger_view *dview = &th.dbg.image_views.video.get();
    image_view *iv = &dview->image;

    iv->width = 64;
    iv->height = 128;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = ivec2(static_cast<i32>(iv->width), static_cast<i32>(iv->height));

    iv->update_func.ptr = &th;
    iv->update_func.func = &render_image_view_video;
    snprintf(iv->label, sizeof(iv->label), "Video");
}

    
void core::setup_debugger_interface(debugger_interface &intf) {
    dbg.interface = &intf;
    auto *dbgr = dbg.interface;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 8;
    dbgr->views.reserve(15);

    setup_dbglog(*this, *dbgr);
    setup_image_view_video(*this, *dbgr);

    setup_disassembly_view(*this, dbgr);
    setup_memory_view(*this, *dbgr);
    setup_waveforms(*this, dbgr);
}
}