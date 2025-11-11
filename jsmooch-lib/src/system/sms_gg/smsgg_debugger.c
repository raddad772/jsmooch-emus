//
// Created by . on 8/12/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "helpers/debugger/waveform.h"

#include "component/cpu/z80/z80.h"
#include "component/cpu/z80/z80_disassembler.h"
#include "sms_gg.h"

#include "smsgg_debugger.h"

#define JTHIS struct SMSGG* this = (struct SMSGG*)jsm->ptr
#define JSM struct jsm_system* jsm

static void get_dissasembly(void *smsggptr, debugger_interface *dbgr, disassembly_view *dview, disassembly_entry *entry)
{
    struct SMSGG* this = (struct SMSGG*)smsggptr;
    Z80_disassemble_entry(&this->cpu, entry);
}

static struct disassembly_vars get_disassembly_vars(void *smsggptr, debugger_interface *dbgr, disassembly_view *dv)
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

static void fill_disassembly_view(void *smsggptr, debugger_interface *dbgr, disassembly_view *dview)
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

static void create_and_bind_registers(struct SMSGG* this, disassembly_view *dv)
{
    u32 tkindex = 0;
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "B");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "C");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "D");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "E");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "F");
    rg->kind = RK_int32;
    rg->index = tkindex++;
    rg->custom_render = &render_f;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HL");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SP");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IX");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IY");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "EI");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HALT");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "AF_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "BC_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "DE_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HL_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "CxI");
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

static u32 get_gfx1(struct SMSGG* this, u32 hpos, u32 vpos, u32 nt_base, u32 pt_base, u32 ct_base, u32 num_lines)
{
    u32 nta = ((hpos >> 3) & 0x1F) | ((vpos  << 2) & 0x3E0) | (nt_base << 10);
    u32 pattern = this->vdp.VRAM[nta];

    u32 paddr = (vpos & 7) | (pattern << 3) | (pt_base << 11);

    u32 index = hpos ^ 7;
    if ((this->vdp.VRAM[paddr] & (1 << index)) == 0)
        return 0;
    else
        return 7;
}

static u32 get_gfx2(struct SMSGG* this, u32 hpos, u32 vpos, u32 nt_base, u32 pt_base, u32 ct_base, u32 num_lines)
{
    u32 nta = ((hpos >> 3) & 0x1F) | ((vpos  << 2) & 0x3E0) | (nt_base << 10);
    u32 pattern = this->vdp.VRAM[nta];

    u32 paddr = (vpos & 7) | (pattern << 3);
    if (vpos >= 64 && vpos <= 127) paddr |= (pt_base & 1) << 11;
    if (vpos >= 128 && vpos <= 191) paddr |= (pt_base & 2) << 11;

    u32 caddr = paddr;
    paddr |= (pt_base & 4) << 11;
    //caddr |= (this->io.bg_color_table_address & 0x80) << 4;

    //u32 cmask = ((this->io.bg_color_table_address & 0x7F) << 1) | 1;
    //u32 color = this->VRAM[caddr];
    u32 index = hpos ^ 7;
    if (!(this->vdp.VRAM[paddr] & (1 << index)))
        return 7;
    else
        return 0;
}

static u32 get_gfx3(struct SMSGG* this, u32 hpos, u32 vpos, u32 nt_base, u32 pt_base, u32 ct_base, u32 num_lines)
{
    hpos &= 0xFF;
    vpos &= 0x1FF;

    u32 nta;
    if (num_lines == 192) {
        vpos %= 224;
        nta = (nt_base & 0x0E) << 10;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
        if (this->variant == jsm::systems::SMS1) {
            // NTA bit 10 (0x400) & with io nta bit 0
            nta &= (0x3BFF | ((nt_base & 1) << 10));
        }
    } else {
        vpos &= 0xFF;
        nta = ((nt_base & 0x0C) << 10) | 0x700;
        nta += (vpos & 0xF8) << 3;
        nta += ((hpos & 0xF8) >> 3) << 1;
    }

    u32 pattern = this->vdp.VRAM[nta] | (this->vdp.VRAM[nta | 1] << 8);
    if (pattern & 0x200) hpos ^= 7;
    if (pattern & 0x400) vpos ^= 7;
    u32 palette = (pattern & 0x800) >> 11;
    u32 priority = (pattern & 0x1000) >> 12;

    u32 pta = (vpos & 7) << 2;
    pta |= (pattern & 0x1FF) << 5;

    u32 index = (hpos & 7) ^ 7;
    u32 bmask = (1 << index);
    u32 color = (this->vdp.VRAM[pta] & bmask) ? 1 : 0;
    color += (this->vdp.VRAM[pta | 1] & bmask) ? 2 : 0;
    color += (this->vdp.VRAM[pta | 2] & bmask) ? 4 : 0;
    color += (this->vdp.VRAM[pta | 3] & bmask) ? 8 : 0;

    if (color == 0) priority = 0;
    return color;
}

static void render_image_view_nametables(struct debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    struct SMSGG* this = (struct SMSGG*)ptr;
    u32 (*get_pcolor)(struct SMSGG*, u32, u32, u32, u32, u32, u32);
    switch(this->vdp.bg_gfx_mode) {
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
    struct image_view *iv = &dview->image;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;

    u32 numlines = 240;
    u32 i;
    int nt_rows_to_crtrow[240];
    for (i = 0; i < 240; i++)
        nt_rows_to_crtrow[i] = -1;
    i = 0;
    while(i < numlines) {
        struct DBGSMSGGROW *row = &this->dbg_data.rows[i];
        nt_rows_to_crtrow[(i + row->io.vscroll) % 240] = (i32)i;
        numlines = row->io.num_lines;
        i++;
    }

    //u32 top = this->dbg_data.rows[0].io.vscroll;
    //u32 bottom = this->dbg_data.rows[this->dbg_data.rows[0].io.num_lines - 1].io.vscroll + this->dbg_data.rows[0].io.num_lines - 1
    // TODO: add vscroll lock
    for (u32 rownum = 0; rownum < 240; rownum++) {
        int snum = nt_rows_to_crtrow[rownum];
        struct DBGSMSGGROW *row = &this->dbg_data.rows[snum == -1 ? 0 : snum];
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
            u32 c = get_pcolor(this, x, rownum, row->io.bg_name_table_address, row->io.bg_pattern_table_address, row->io.bg_color_table_address, row->io.num_lines);
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

static void setup_debugger_view_nametables(struct SMSGG* this, debugger_interface *dbgr)
{
    this->dbg.image_views.nametables = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables);
    struct image_view *iv = &dview->image;
    // 512x480, though not all used

    iv->width = 256;
    iv->height = 240;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 255, 239 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_nametables;

    snprintf(iv->label, sizeof(iv->label), "Tilemap Viewer");
}

static void setup_disassembly_view(struct SMSGG* this, debugger_interface *dbgr)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFF;
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
}


static void setup_events_view(struct SMSGG* this, debugger_interface *dbgr)
{
    // Setup event view
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
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

    cvec_grow_by(&ev->events, DBG_SMSGG_EVENT_MAX);
    ///void events_view_add_event(struct debugger_interface *dbgr, events_view *ev, u32 category_id, const char *name, u32 color, enum debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id);
    DEBUG_REGISTER_EVENT("IRQ", 0xFF0000, DBG_SMSGG_CATEGORY_CPU, DBG_SMSGG_EVENT_IRQ, 1);
    DEBUG_REGISTER_EVENT("NMI", 0x808000, DBG_SMSGG_CATEGORY_CPU, DBG_SMSGG_EVENT_NMI, 1);
    DEBUG_REGISTER_EVENT("Write H scroll", 0x00FF00, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_HSCROLL, 1);
    DEBUG_REGISTER_EVENT("Write V scroll", 0x0000FF, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_VSCROLL, 1);
    DEBUG_REGISTER_EVENT("Write VRAM", 0x0080FF, DBG_SMSGG_CATEGORY_VDP, DBG_SMSGG_EVENT_WRITE_VRAM, 1);

    SET_EVENT_VIEW(this->cpu);
    SET_EVENT_VIEW(this->vdp);
    SET_CPU_EVENT_ID(DBG_SMSGG_EVENT_IRQ, IRQ);
    SET_CPU_EVENT_ID(DBG_SMSGG_EVENT_NMI, NMI);

    debugger_report_frame(this->dbg.interface);
}

static void setup_waveforms(struct SMSGG* this, debugger_interface *dbgr)
{
    this->dbg.waveforms.view = debugger_view_new(dbgr, dview_waveforms);
    struct debugger_view *dview = cpg(this->dbg.waveforms.view);
    struct waveform_view *wv = (struct waveform_view *)&dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "SN76489");

    struct debug_waveform *dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.main = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 48;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[0] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Square 0");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[1] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Square 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[2] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Square 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[3] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Noise");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}

void SMSGGJ_setup_debugger_interface(JSM, debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;

    setup_disassembly_view(this, dbgr);
    setup_events_view(this, dbgr);
    setup_debugger_view_nametables(this, dbgr);
    setup_waveforms(this, dbgr);
}
