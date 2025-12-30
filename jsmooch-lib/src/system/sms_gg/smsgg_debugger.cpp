//
// Created by . on 8/12/24.
//

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "helpers/debugger/waveform.h"

#include "component/cpu/z80/z80.h"
#include "component/cpu/z80/z80_disassembler.h"
#include "sms_gg.h"
#include "sms_gg_bus.h"

#include "smsgg_debugger.h"

static void get_dissasembly(void *smsggptr, disassembly_view &dview, disassembly_entry &entry)
{
    auto *th = static_cast<SMSGG::core *>(smsggptr);
    Z80::disassemble_entry(&th->cpu, entry);
}

static disassembly_vars get_disassembly_vars(void *smsggptr, disassembly_view &dv)
{
    auto *th = static_cast<SMSGG::core*>(smsggptr);;
    disassembly_vars dvar;
    dvar.address_of_executing_instruction = th->cpu.trace.ins_PC;
    dvar.current_clock_cycle = th->clock.trace_cycles;
    return dvar;
}


static int render_f(cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%c%c%c%c%c%c%c%c",
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

static void fill_disassembly_view(void *smsggptr, disassembly_view &dview)
{
    auto *th = static_cast<SMSGG::core*>(smsggptr);

    th->dbg.dasm.A->int8_data = th->cpu.regs.A;
    th->dbg.dasm.B->int8_data = th->cpu.regs.B;
    th->dbg.dasm.C->int8_data = th->cpu.regs.C;
    th->dbg.dasm.D->int8_data = th->cpu.regs.D;
    th->dbg.dasm.E->int8_data = th->cpu.regs.E;
    th->dbg.dasm.F->int32_data = th->cpu.regs.F.u;
    th->dbg.dasm.HL->int16_data = (th->cpu.regs.H << 8) | th->cpu.regs.L;
    th->dbg.dasm.PC->int16_data = th->cpu.regs.PC;
    th->dbg.dasm.SP->int16_data = th->cpu.regs.SP;
    th->dbg.dasm.IX->int16_data = th->cpu.regs.IX;
    th->dbg.dasm.IY->int16_data = th->cpu.regs.IY;
    th->dbg.dasm.EI->bool_data = th->cpu.regs.EI;
    th->dbg.dasm.HALT->bool_data = th->cpu.regs.HALT;
    th->dbg.dasm.AF_->int16_data = th->cpu.regs.AF_;
    th->dbg.dasm.BC_->int16_data = th->cpu.regs.BC_;
    th->dbg.dasm.DE_->int16_data = th->cpu.regs.DE_;
    th->dbg.dasm.HL_->int16_data = th->cpu.regs.HL_;

}

static void create_and_bind_registers(SMSGG::core* th, disassembly_view &dv)
{
    u32 tkindex = 0;
    cpu_reg_context *rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "B");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "C");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "D");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "E");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "F");
    rg->kind = RK_int32;
    rg->index = tkindex++;
    rg->custom_render = &render_f;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "HL");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "SP");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "IX");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "IY");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "EI");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "HALT");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "AF_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "BC_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "DE_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "HL_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = &dv.cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "CxI");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

#define BIND(dn, index) th->dbg.dasm. dn = &dv.cpu.regs.at(index)
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

static u32 get_gfx1(SMSGG::core* th, u32 hpos, u32 vpos, u32 nt_base, u32 pt_base, u32 ct_base, u32 num_lines)
{
    u32 nta = ((hpos >> 3) & 0x1F) | ((vpos  << 2) & 0x3E0) | (nt_base << 10);
    u32 pattern = th->vdp.VRAM[nta];

    u32 paddr = (vpos & 7) | (pattern << 3) | (pt_base << 11);

    u32 index = hpos ^ 7;
    if ((th->vdp.VRAM[paddr] & (1 << index)) == 0)
        return 0;
    else
        return 7;
}

static u32 get_gfx2(SMSGG::core* th, u32 hpos, u32 vpos, u32 nt_base, u32 pt_base, u32 ct_base, u32 num_lines)
{
    u32 nta = ((hpos >> 3) & 0x1F) | ((vpos  << 2) & 0x3E0) | (nt_base << 10);
    u32 pattern = th->vdp.VRAM[nta];

    u32 paddr = (vpos & 7) | (pattern << 3);
    if (vpos >= 64 && vpos <= 127) paddr |= (pt_base & 1) << 11;
    if (vpos >= 128 && vpos <= 191) paddr |= (pt_base & 2) << 11;

    u32 caddr = paddr;
    paddr |= (pt_base & 4) << 11;
    //caddr |= (th->io.bg_color_table_address & 0x80) << 4;

    //u32 cmask = ((th->io.bg_color_table_address & 0x7F) << 1) | 1;
    //u32 color = th->VRAM[caddr];
    u32 index = hpos ^ 7;
    if (!(th->vdp.VRAM[paddr] & (1 << index)))
        return 7;
    else
        return 0;
}

static u32 get_gfx3(SMSGG::core* th, u32 hpos, u32 vpos, u32 nt_base, u32 pt_base, u32 ct_base, u32 num_lines)
{
    hpos &= 0xFF;
    vpos &= 0x1FF;

    u32 nta;
    if (num_lines == 192) {
        vpos %= 224;
        nta = (nt_base & 0x0E) << 10;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
        if (th->variant == jsm::systems::SMS1) {
            // NTA bit 10 (0x400) & with io nta bit 0
            nta &= (0x3BFF | ((nt_base & 1) << 10));
        }
    } else {
        vpos &= 0xFF;
        nta = ((nt_base & 0x0C) << 10) | 0x700;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
    }

    u32 pattern = th->vdp.VRAM[nta] | (th->vdp.VRAM[nta | 1] << 8);
    if (pattern & 0x200) hpos ^= 7;
    if (pattern & 0x400) vpos ^= 7;
    u32 palette = (pattern & 0x800) >> 11;
    u32 priority = (pattern & 0x1000) >> 12;

    u32 pta = (vpos & 7) << 2;
    pta |= (pattern & 0x1FF) << 5;

    u32 index = (hpos & 7) ^ 7;
    u32 bmask = (1 << index);
    u32 color = (th->vdp.VRAM[pta] & bmask) ? 1 : 0;
    color += (th->vdp.VRAM[pta | 1] & bmask) ? 2 : 0;
    color += (th->vdp.VRAM[pta | 2] & bmask) ? 4 : 0;
    color += (th->vdp.VRAM[pta | 3] & bmask) ? 8 : 0;

    if (color == 0) priority = 0;
    return color;
}

static void render_image_view_nametables(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    auto* th = static_cast<SMSGG::core *>(ptr);
    u32 (*get_pcolor)(SMSGG::core*, u32, u32, u32, u32, u32, u32);
    switch(th->vdp.bg_gfx_mode) {
        case 1:
            get_pcolor = &get_gfx1;
            break;
        case 2:
            get_pcolor = &get_gfx2;
            break;
        case 3:
            get_pcolor = &get_gfx3;
            break;
        default:
            assert(1==2);
    }
    image_view *iv = &dview->image;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);

    u32 numlines = 240;
    u32 i;
    int nt_rows_to_crtrow[240];
    for (i = 0; i < 240; i++)
        nt_rows_to_crtrow[i] = -1;
    i = 0;
    while(i < numlines) {
        SMSGG::core::SMSDBGDATA::DBGSMSGGROW *row = &th->dbg_data.rows[i];
        nt_rows_to_crtrow[(i + row->io.vscroll) % 240] = static_cast<i32>(i);
        numlines = row->io.num_lines;
        i++;
    }

    //u32 top = th->dbg_data.rows[0].io.vscroll;
    //u32 bottom = th->dbg_data.rows[th->dbg_data.rows[0].io.num_lines - 1].io.vscroll + th->dbg_data.rows[0].io.num_lines - 1
    // TODO: add vscroll lock
    for (u32 rownum = 0; rownum < 240; rownum++) {
        int snum = nt_rows_to_crtrow[rownum];
        SMSGG::core::SMSDBGDATA::DBGSMSGGROW *row = &th->dbg_data.rows[snum == -1 ? 0 : snum];
        u32 *rowptr = outbuf + (rownum * out_width);
        u32 nl = row->io.num_lines - 1;
        u32 hscroll = (0 - row->io.hscroll) & 0xFF;
        u32 left = row->io.left_clip ? hscroll + 8 : hscroll;
        u32 right = hscroll + 255;
        if (row->io.bg_hscroll_lock) {
            if (rownum < 16) {
                left = row->io.left_clip ? 8 : 0;
                right = 255;
            }
        }
        right &= 0xFF;
        //u32 r255t = right & 0xFF;

        for (u32 x = 0; x < 256; x++) {
            u32 c = get_pcolor(th, x, rownum, row->io.bg_name_table_address, row->io.bg_pattern_table_address, row->io.bg_color_table_address, row->io.num_lines);
            c = 0xFF000000 | (c * 0x242424);

            if ((snum != -1) && (x == left)) c = 0xFF00FF00; // green left-hand side
            if ((snum != -1) && (x == right)) c = 0xFFFF0000; // blue right-hand side
            if ((snum == 0) || (snum == nl)) {
                if (((right < left) && ((x <= right) || (x >= left))) || ((x <= right) && (x >= left))) {
                    if (snum == 0) c = 0xFF00FF00;
                    if (snum == nl) c = 0xFFFF0000;
                }
            }
            *rowptr = c;
            rowptr++;
        }
    }
}

static void setup_debugger_view_nametables(SMSGG::core* th, debugger_interface &dbgr)
{
    th->dbg.image_views.nametables = dbgr.make_view(dview_image);
    debugger_view *dview = &th->dbg.image_views.nametables.get();
    image_view *iv = &dview->image;
    // 512x480, though not all used

    iv->width = 256;
    iv->height = 240;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 255, 239 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_nametables;

    snprintf(iv->label, sizeof(iv->label), "Tilemap Viewer");
}

static int print_dasm_addr(void *smsggptr, u32 addr, char *out, size_t out_sz) {
    //auto *th = static_cast<SMSGG::core *>(smsggptr);
    snprintf(out, out_sz, "%04x", addr);
    return 4;
}

static void setup_disassembly_view(SMSGG::core* th, debugger_interface &dbgr)
{
    cvec_ptr p =dbgr.make_view(dview_disassembly);
    debugger_view *dview = &p.get();
    disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFF;
    dv->addr_column_size = 4;
    dv->has_context = 0;
    dv->processor_name.sprintf("Z80");

    create_and_bind_registers(th, *dv);
    dv->fill_view.ptr = static_cast<void *>(th);
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = static_cast<void *>(th);
    dv->get_disassembly.func = &get_dissasembly;

    dv->get_disassembly_vars.ptr = static_cast<void *>(th);
    dv->get_disassembly_vars.func = &get_disassembly_vars;

    dv->print_addr.ptr = &th;
    dv->print_addr.func = &print_dasm_addr;

}


static void setup_events_view(SMSGG::core& th, debugger_interface &dbgr)
{
    // Setup event view
    th.dbg.events.view = dbgr.make_view(dview_events);
    debugger_view *dview = &th.dbg.events.view.get();
    events_view &ev = dview->events;

    for (u32 i = 0; i < 2; i++) {
        ev.display[i].width = 342;
        ev.display[i].height = 262;
        ev.display[i].buf = nullptr;
        ev.display[i].frame_num = 0;
    }
    ev.associated_display = th.vdp.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_SMSGG_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("VDP events", DBG_SMSGG_CATEGORY_VDP);

    ev.events.reserve(ev.events.capacity() + DBG_SMSGG_EVENT_MAX);
    ///void events_view_add_event(debugger_interface *dbgr, events_view *ev, u32 category_id, const char *name, u32 color, debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id);
    DEBUG_REGISTER_EVENT("IRQ", 0xFF0000, DBG_SMSGG_CATEGORY_CPU, DBG_SMSGG_EVENT_IRQ, 1);
    DEBUG_REGISTER_EVENT("NMI", 0x808000, DBG_SMSGG_CATEGORY_CPU, DBG_SMSGG_EVENT_NMI, 1);
    DEBUG_REGISTER_EVENT("Write H scroll", 0x00FF00, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_HSCROLL, 1);
    DEBUG_REGISTER_EVENT("Write V scroll", 0x0000FF, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_VSCROLL, 1);
    DEBUG_REGISTER_EVENT("Write VRAM", 0x0080FF, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_VRAM, 1);

    SET_EVENT_VIEW(th.cpu);
    SET_EVENT_VIEW(th.vdp);
    SET_CPU_EVENT_ID(DBG_SMSGG_EVENT_IRQ, IRQ);
    SET_CPU_EVENT_ID(DBG_SMSGG_EVENT_NMI, NMI);

    debugger_report_frame(th.dbg.interface);
}

static void setup_waveforms(SMSGG::core* th, debugger_interface &dbgr)
{
    th->dbg.waveforms.view = dbgr.make_view(dview_waveforms);
    debugger_view *dview = &th->dbg.waveforms.view.get();
    waveform_view *wv = (waveform_view *)&dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "SN76489");

    debug_waveform *dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.main.make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 48;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[0].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Square 0");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[1].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Square 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[2].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Square 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[3].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Noise");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}

static void setup_dbglog(SMSGG::core &th, debugger_interface &dbgr)
{
    cvec_ptr p = dbgr.make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th.dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = 1;

    static constexpr u32 cpu_color = 0x8080FF;
    static constexpr u32 vdc_color = 0xFF8080;

    dbglog_category_node &root = dv.get_category_root();
    dbglog_category_node &dcpu = root.add_node(dv, "CPU", nullptr, 0, 0);
    dcpu.children.reserve(15);
    dcpu.add_node(dv, "Instruction Trace", "CPU", SMSGG_CAT_CPU_INSTRUCTION, cpu_color);
    th.cpu.trace.dbglog.view = &dv;
    th.cpu.trace.dbglog.id = SMSGG_CAT_CPU_INSTRUCTION;
    dcpu.add_node(dv, "Reads", "cpu_read", SMSGG_CAT_CPU_READ, cpu_color);
    dcpu.add_node(dv, "Writes", "cpu_write", SMSGG_CAT_CPU_WRITE, cpu_color);
    dcpu.add_node(dv, "IN", "cpu_in", SMSGG_CAT_CPU_IN, cpu_color);
    dcpu.add_node(dv, "OUT", "cpu_out", SMSGG_CAT_CPU_OUT, cpu_color);


    //dbglog_category_node &ppu = root.add_node(dv, "PPU", nullptr, 0, 0);
    //ppu.add_node(dv, "VRAM Write", "PPU", SMSGG_CAT_PPU_VRAM_WRITE, ppu_color);
}


void SMSGG::core::setup_debugger_interface(debugger_interface &dbgr)
{
    dbg.interface = &dbgr;

    dbgr.supported_by_core = 1;
    dbgr.smallest_step = 1;
    dbgr.views.reserve(15);

    setup_dbglog(*this, dbgr);
    setup_disassembly_view(this, dbgr);
    setup_events_view(*this, dbgr);
    setup_debugger_view_nametables(this, dbgr);
    setup_waveforms(this, dbgr);
}
