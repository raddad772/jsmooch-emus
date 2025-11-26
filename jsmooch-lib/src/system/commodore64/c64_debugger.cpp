//
// Created by . on 11/25/25.
//

#include "c64_debugger.h"
#include "c64_bus.h"
namespace C64 {

static disassembly_vars get_disassembly_vars(void *nesptr, disassembly_view &dv)
{
    auto *th = static_cast<core *>(nesptr);
    disassembly_vars dvar;
    dvar.address_of_executing_instruction = th->cpu.trace.ins_PC;
    dvar.current_clock_cycle = th->master_clock;
    return dvar;
}

static int print_dasm_addr(void *nesptr, u32 addr, char *out, size_t out_sz) {
    auto *th = static_cast<core *>(nesptr);
    snprintf(out, out_sz, "%04x", addr);
    return 4;
}

static void get_disassembly(void *nesptr, disassembly_view &dview, disassembly_entry &entry)
{
    auto *th = static_cast<core *>(nesptr);
    M6502::disassemble_entry(&th->cpu, entry);
}

    
static void fill_disassembly_view(void *nesptr, disassembly_view &dview)
{
    auto *th = static_cast<core *>(nesptr);

    th->dbg.dasm.A->int8_data = th->cpu.regs.A;
    th->dbg.dasm.X->int8_data = th->cpu.regs.X;
    th->dbg.dasm.Y->int8_data = th->cpu.regs.Y;
    th->dbg.dasm.PC->int16_data = th->cpu.regs.PC;
    th->dbg.dasm.S->int8_data = th->cpu.regs.S;
    th->dbg.dasm.P->int8_data = th->cpu.regs.P.getbyte();
}

    
static void readcpumem(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    auto *out = static_cast<u8 *>(dest);
    auto *th = static_cast<core *>(ptr);
    for (u32 i = 0; i < 16; i++) {
        *out = th->read_main_bus((addr + i) & 0x7FFF, 0, false);
        out++;
    }
}

static int render_p(cpu_reg_context *ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%c%c-%c%c%c%c%c",
                    ctx->int32_data & 0x80 ? 'N' : 'n',
                    ctx->int32_data & 0x40 ? 'V' : 'v',
                    ctx->int32_data & 0x10 ? 'B' : 'b',
                    ctx->int32_data & 8 ? 'D' : 'd',
                    ctx->int32_data & 4 ? 'I' : 'i',
                    ctx->int32_data & 2 ? 'Z' : 'z',
                    ctx->int32_data & 1 ? 'C' : 'c'
    );
}


static void create_and_bind_registers(core &th, disassembly_view &dv)
{
    u32 tkindex = 0;
    cpu_reg_context *rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "X");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "Y");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "S");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "P");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_p;

#define BIND(dn, index) th.dbg.dasm. dn = &dv.cpu.regs.at(index)
    BIND(A, 0);
    BIND(X, 1);
    BIND(Y, 2);
    BIND(PC, 3);
    BIND(S, 4);
    BIND(P, 5);
#undef BIND
}

static void setup_disassembly_view(core& th, debugger_interface *dbgr)
{
    cvec_ptr<debugger_view> p = dbgr->make_view(dview_disassembly);

    debugger_view &dview = p.get();
    disassembly_view &dv = dview.disassembly;
    dv.addr_column_size = 4;
    dv.has_context = 0;
    dv.processor_name.sprintf("m6502");

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
    dbglog_category_node &cpucat = root.add_node(dv, "CDP1802", nullptr, 0, 0);
    cpucat.add_node(dv, "Instruction Trace", "cpu", C64_CAT_CPU_INSTRUCTION, cpu_color);
    th.cpu.trace.dbglog.view = &dv;
    th.cpu.trace.dbglog.id = C64_CAT_CPU_INSTRUCTION;
    cpucat.add_node(dv, "Reads", "cpu.read", C64_CAT_CPU_READ, cpu_color);
    cpucat.add_node(dv, "Writes", "cpu.write", C64_CAT_CPU_WRITE, cpu_color);
}
static void setup_memory_view(core& th, debugger_interface &dbgr) {
    th.dbg.memory = dbgr.make_view(dview_memory);
    debugger_view *dview = &th.dbg.memory.get();
    memory_view *mv = &dview->memory;
    mv->add_module("CPU Memory", 0, 4, 0, 0x7FFF, &th, &readcpumem);
}


void core::setup_debugger_interface(debugger_interface &intf) {
    dbg.interface = &intf;
    auto *dbgr = dbg.interface;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 8;
    dbgr->views.reserve(15);

    setup_dbglog(*this, *dbgr);
    setup_disassembly_view(*this, dbgr);
    setup_memory_view(*this, *dbgr);
}
}
