//
// Created by . on 11/25/25.
//

#include "c64_debugger.h"
#include "c64_bus.h"
namespace C64 {

static void render_image_view_sys_info(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    debugger_widget_textbox &tb = dview->options[0].textbox;
    tb.clear();
    tb.sprintf("VIC II\n~~~~~~");
    tb.sprintf("\nECM/BMM/MCM = %d/%d/%d: ", th->vic2.io.CR1.ECM, th->vic2.io.CR1.BMM, th->vic2.io.CR2.MCM);
    u32 mode = (th->vic2.io.CR1.ECM << 2) | (th->vic2.io.CR1.BMM << 1) | th->vic2.io.CR2.MCM;
    switch(mode) {
        case 0b000:
            tb.sprintf("standard text mode");
            break;
        case 0b001:
            tb.sprintf("multi-color text mode");
            break;
        case 0b010:
            tb.sprintf("standard bitmap mode");
            break;
        case 0b011:
            tb.sprintf("multi-color bitmap mode");
            break;
        case 0b100:
            tb.sprintf("ECM text mode");
            break;
        case 0b101:
            tb.sprintf("invalid text mode");
            break;
        case 0b110:
            tb.sprintf("invalid bitmap mode 1");
            break;
        case 0b111:
            tb.sprintf("invalid bitmap mode 2");
            break;
    }

    tb.sprintf("\n\n           CIA1      CIA2");
      tb.sprintf("\n           ~~~~      ~~~~");
      tb.sprintf("\nTA.on         %d        %d", th->cia1.regs.CRA.START, th->cia2.regs.CRA.START);
      tb.sprintf("\nTA.latch   %04x      %04x", th->cia1.timerA.latch, th->cia2.timerA.latch);
      tb.sprintf("\nTA.count   %04x      %04x", th->cia1.timerA.count, th->cia2.timerB.count);
      tb.sprintf("\nTA.contin.    %d        %d", th->cia1.regs.CRA.RUNMODE ^ 1, th->cia2.regs.CRA.RUNMODE ^ 1);
      tb.sprintf("\nTA.inmode     %d        %d", th->cia1.regs.CRA.INMODE, th->cia2.regs.CRA.INMODE);;
    tb.sprintf("\n\nTB.on         %d        %d", th->cia1.regs.CRB.START, th->cia2.regs.CRB.START);
      tb.sprintf("\nTB.latch   %04x      %04x", th->cia1.timerB.latch, th->cia2.timerB.latch);
      tb.sprintf("\nTB.count   %04x      %04x", th->cia1.timerB.count, th->cia2.timerB.count);
      tb.sprintf("\nTB.contin.    %d        %d", th->cia1.regs.CRB.RUNMODE ^ 1, th->cia2.regs.CRB.RUNMODE ^ 1);
      tb.sprintf("\nTB.inmode     %d        %d", th->cia1.regs.CRB.INMODE, th->cia2.regs.CRB.INMODE);;

}

static disassembly_vars get_disassembly_vars(void *ptr, disassembly_view &dv)
{
    auto *th = static_cast<core *>(ptr);
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


static void readram(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    auto *out = static_cast<u8 *>(dest);
    auto *th = static_cast<core *>(ptr);
    for (u32 i = 0; i < 16; i++) {
        *out = th->mem.RAM[(addr + i) & 0xFFFF];
        out++;
    }
}


static void readcpumem(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    auto *out = static_cast<u8 *>(dest);
    auto *th = static_cast<core *>(ptr);
    for (u32 i = 0; i < 16; i++) {
        *out = th->mem.read_main_bus((addr + i) & 0xFFFF, 0, false);
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
    dv.has_extra = true;

    static constexpr u32 cpu_color = 0x8080FF;

    dbglog_category_node &root = dv.get_category_root();
    dbglog_category_node &cpucat = root.add_node(dv, "M6502", nullptr, 0, 0);
    cpucat.children.reserve(15);
    cpucat.add_node(dv, "Instruction Trace", "cpu", C64_CAT_CPU_INSTRUCTION, cpu_color);
    th.cpu.trace.dbglog.view = &dv;
    th.cpu.trace.dbglog.id = C64_CAT_CPU_INSTRUCTION;
    cpucat.add_node(dv, "Reads", "cpu.read", C64_CAT_CPU_READ, cpu_color);
    cpucat.add_node(dv, "Writes", "cpu.write", C64_CAT_CPU_WRITE, cpu_color);
}

static u8 chargen_to_ascii(u8 db) {
    db &= 0x7F;
    if (db == 0) return '@';
    if (db < 0x20) return db + 0x40;
    if ((db >= 0x20) && (db < 0x40)) return db;
    return '.';
}

static u8 render_mv_ascii(void *ptr, u32 kind, u8 db) {
    switch (kind) {
        case 0: // CHARGEN
            db = chargen_to_ascii(db);
            break;
        default: // ASCII
            if ((db >= 32) && (db <= 126)) {

            }
            else {
                db = '.';
            }
            break;
    }
    return db;
}


static void setup_memory_view(core& th, debugger_interface &dbgr) {
    th.dbg.memory = dbgr.make_view(dview_memory);
    debugger_view *dview = &th.dbg.memory.get();
    memory_view *mv = &dview->memory;
    mv->add_module("CPU Main Bus", 0, 4, 0, 0xFFFF, &th, &readcpumem);
    mv->add_module("RAM", 1, 4, 0, 0xFFFF, &th, &readram);

    auto *mm = mv->get_module(0);
    mm->text_views.num = 2;
    snprintf(mm->text_views.names[0], sizeof(mm->text_views.names[0]), "CHARGEN");
    snprintf(mm->text_views.names[1], sizeof(mm->text_views.names[1]), "ASCII");
    mm->render_ascii_ptr = nullptr;
    mm->render_ascii = &render_mv_ascii;

    mm = mv->get_module(1);
    mm->text_views.num = 2;
    snprintf(mm->text_views.names[0], sizeof(mm->text_views.names[0]), "CHARGEN");
    snprintf(mm->text_views.names[1], sizeof(mm->text_views.names[1]), "ASCII");
    mm->render_ascii_ptr = nullptr;
    mm->render_ascii = &render_mv_ascii;

}

static void setup_waveforms(core& th, debugger_interface *dbgr)
{
    th.dbg.waveforms.view = dbgr->make_view(dview_waveforms);
    debugger_view &dview = th.dbg.waveforms.view.get();
    waveform_view &wv = dview.waveform;
    snprintf(wv.name, sizeof(wv.name), "SID");

    debug_waveform *dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.main.make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;

    dw->default_clock_divider = 1;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[0].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[1].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[2].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}

static void setup_image_view_sys_info(core &th, debugger_interface &dbgr) {

    th.dbg.image_views.sysinfo = dbgr.make_view(dview_image);
    debugger_view &dview = th.dbg.image_views.sysinfo.get();
    image_view &iv = dview.image;

    iv.width = 10;
    iv.height = 10;
    iv.viewport.exists = 1;
    iv.viewport.enabled = 1;
    iv.viewport.p[0] = (ivec2){ 0, 0 };
    iv.viewport.p[1] = (ivec2){ 10, 10 };

    iv.update_func.ptr = &th;
    iv.update_func.func = &render_image_view_sys_info;

    snprintf(iv.label, sizeof(iv.label), "Sys Info View");

    debugger_widgets_add_textbox(dview.options, "blah!", 1);

}

void core::setup_debugger_interface(debugger_interface &intf) {
    dbg.interface = &intf;
    auto *dbgr = dbg.interface;

    dbgr->supported_by_core = true;
    dbgr->smallest_step = 8;
    dbgr->views.reserve(15);

    setup_dbglog(*this, *dbgr);
    setup_disassembly_view(*this, dbgr);
    setup_memory_view(*this, *dbgr);
    setup_waveforms(*this, dbgr);
    setup_image_view_sys_info(*this, *dbgr);
}
}
