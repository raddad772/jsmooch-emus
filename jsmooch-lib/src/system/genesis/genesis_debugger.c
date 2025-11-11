//
// Created by . on 10/20/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "genesis_debugger.h"
#include "genesis.h"
#include "genesis_bus.h"
#include "component/cpu/z80/z80_disassembler.h"
#include "component/audio/ym2612/ym2612.h"

#define JTHIS struct genesis* this = (genesis*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

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

static int render_f_z80(cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
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


static void create_and_bind_registers_z80(genesis* this, disassembly_view *dv)
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
    rg->custom_render = &render_f_z80;

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

#define BIND(dn, index) this->dbg.dasm_z80. dn = cvec_get(&dv->cpu.regs, index)
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


static void create_and_bind_registers_m68k(genesis* this, disassembly_view *dv)
{
    u32 tkindex = 0;
    for (u32 i = 0; i < 8; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        snprintf(rg->name, sizeof(rg->name), "D%d", i);
        rg->kind = RK_int32;
        rg->custom_render = NULL;
        rg->index = tkindex++;
    }
    for (u32 i = 0; i < 7; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        snprintf(rg->name, sizeof(rg->name), "A%d", i);
        rg->kind = RK_int32;
        rg->custom_render = NULL;
        rg->index = tkindex++;
    }
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "USP");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SSP");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SR");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "supervisor");
    rg->kind = RK_bool;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "trace");
    rg->kind = RK_bool;
    rg->custom_render = NULL;
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
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IRC");
    rg->kind = RK_bool;
    rg->custom_render = NULL;
    rg->index = tkindex++;

#define BIND(dn, index) this->dbg.dasm_m68k. dn = cvec_get(&dv->cpu.regs, index)
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
    struct genesis* this = (genesis*)macptr;
    for (u32 i = 0; i < 8; i++) {
        this->dbg.dasm_m68k.D[i]->int32_data = this->m68k.regs.D[i];
        if (i < 7) this->dbg.dasm_m68k.A[i]->int32_data = this->m68k.regs.A[i];
    }
    this->dbg.dasm_m68k.PC->int32_data = this->m68k.regs.PC;
    this->dbg.dasm_m68k.SR->int32_data = this->m68k.regs.SR.u;
    if (this->dbg.dasm_m68k.SR->int32_data & 0x2000) { // supervisor mode
        this->dbg.dasm_m68k.USP->int32_data = this->m68k.regs.ASP;
        this->dbg.dasm_m68k.SSP->int32_data = this->m68k.regs.A[7];
    }
    else { // user mode
        this->dbg.dasm_m68k.USP->int32_data = this->m68k.regs.A[7];
        this->dbg.dasm_m68k.SSP->int32_data = this->m68k.regs.ASP;
    }
    this->dbg.dasm_m68k.supervisor->bool_data = this->m68k.regs.SR.S;
    this->dbg.dasm_m68k.trace->bool_data = this->m68k.regs.SR.T;
    this->dbg.dasm_m68k.IMASK->int32_data = this->m68k.regs.SR.I;
    this->dbg.dasm_m68k.CSR->int32_data = this->m68k.regs.SR.u & 0x1F;
    this->dbg.dasm_m68k.IR->int32_data = this->m68k.regs.IR;
    this->dbg.dasm_m68k.IRC->int32_data = this->m68k.regs.IRC;
}

static struct disassembly_vars get_disassembly_vars_m68k(void *macptr, debugger_interface *dbgr, disassembly_view *dv)
{
    struct genesis* this = (genesis*)macptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->m68k.debug.ins_PC;
    dvar.current_clock_cycle = this->clock.master_cycle_count;
    return dvar;
}

static void get_disassembly_m68k(void *genptr, debugger_interface *dbgr, disassembly_view *dview, disassembly_entry *entry)
{
    struct genesis* this = (genesis*)genptr;
    M68k_disassemble_entry(&this->m68k, entry);
}

static void setup_m68k_disassembly(debugger_interface *dbgr, genesis* this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFFFF;
    dv->addr_column_size = 6;
    dv->has_context = 1;
    jsm_string_sprintf(&dv->processor_name, "m68000");

    create_and_bind_registers_m68k(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_disassembly_m68k;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars_m68k;
}

static void get_dissasembly_z80(void *genptr, debugger_interface *dbgr, disassembly_view *dview, disassembly_entry *entry)
{
    struct genesis* this = (genesis*)genptr;
    Z80_disassemble_entry(&this->z80, entry);
}

static struct disassembly_vars get_disassembly_vars_z80(void *genptr, debugger_interface *dbgr, disassembly_view *dv)
{
    struct genesis* this = (genesis*)genptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->z80.PCO;
    dvar.current_clock_cycle = this->clock.master_cycle_count;
    return dvar;
}

static void setup_z80_disassembly(debugger_interface *dbgr, genesis* this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFF;
    dv->addr_column_size = 4;
    dv->has_context = 0;
    jsm_string_sprintf(&dv->processor_name, "Z80");

    create_and_bind_registers_z80(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_dissasembly_z80;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars_z80;

}

static void setup_waveforms_ym2612(genesis* this, debugger_interface *dbgr)
{
    this->dbg.waveforms_ym2612.view = debugger_view_new(dbgr, dview_waveforms);
    struct debugger_view *dview = cpg(this->dbg.waveforms_ym2612.view);
    struct waveform_view *wv = (waveform_view *)&dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "YM2612");

    struct debug_waveform *dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.main = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 1008;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[0] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "CH1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[1] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "CH2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[2] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "CH3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[3] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "CH4");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[4] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "CH5");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[5] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "CH6/PCM");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}


static void setup_waveforms_psg(genesis* this, debugger_interface *dbgr)
{
    this->dbg.waveforms_psg.view = debugger_view_new(dbgr, dview_waveforms);
    struct debugger_view *dview = cpg(this->dbg.waveforms_psg.view);
    struct waveform_view *wv = (waveform_view *)&dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "SN76489");

    cvec_alloc_atleast(&wv->waveforms, 6);
    cvec_lock_reallocs(&wv->waveforms);

    struct debug_waveform *dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.main = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 240;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[0] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Square 0");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[1] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Square 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[2] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Square 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[3] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Noise");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}

static u32 calc_stride(u32 out_width, u32 in_width)
{
    return (out_width - in_width);
}

#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static inline u32 gen_to_rgb(u32 color, int use_palette, u32 priority, u32 shade_priorities)
{
    u32 o = 0xFF000000;
    u32 r, g, b;
    if (use_palette) {
        b = ((color >> 0) & 7) * 0x24;
        g = ((color >> 3) & 7) * 0x24;
        r = ((color >> 6) & 7) * 0x24;
    }
    else {
        b = g = r = (color & 15) * 16;
    }
    if (shade_priorities && priority) b = g = r = 0xFFFFFF;
    return o | b | (g << 8) | (r << 16);
}

static inline u32 shade_boundary_func(u32 kind, u32 incolor)
{
    switch(kind) {
        case 0: {// Shaded
            u32 c = (incolor & 0xFFFFFF00) | 0xFF000000;
            u32 v = incolor & 0xFF;
            v += 0xFF;
            v >>= 1;
            if (v > 255) v = 255;
            return c | v; }
        case 1: // black
            return 0xFF000000;
        case 2: // white
            return 0xFFFFFFFF;
    }
    NOGOHERE;
    return 0;
}

static int fetch_order[4] = { 1, 0, 3, 2 };

static void render_image_view_plane(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width, int plane_num)
{
    struct genesis *this = (genesis *) ptr;
    if (this->clock.master_frame == 0) return;

    // Clear output
    struct image_view *iv = &dview->image;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 1024);
    u32 fw = this->vdp.io.foreground_width * 8;
    u32 fh = this->vdp.io.foreground_height * 8;
    for (u32 y = 0; y < fh; y++) {
        u32 *rowptr = outbuf + (y * out_width);
        for (u32 x = 0; x < fh; x++) {
            rowptr[x] = 0xFF000000;
        }
    }

    // Render tilemap
    u32 num_tiles_x = this->vdp.io.foreground_width;
    u32 num_tiles_y = this->vdp.io.foreground_height;

    u32 nametable_addr;

    struct debugger_widget_checkbox *shade_priorities_cb = NULL;
    struct debugger_widget_checkbox *draw_box_cb = NULL;
    struct debugger_widget_radiogroup *shade_by_rg = NULL;
    struct debugger_widget_radiogroup *shade_kind_rg = NULL;
    enum shade_by_e {
        SB_NONE = 0,
        SB_INSIDE = 1,
        SB_OUTSIDE = 2
    } shade_by;
    enum shade_kind_e {
        SHADE_RED = 0,
        SHADE_BLACK = 1,
        SHADE_WHITE = 2
    } shade_kind;
    u32 draw_bounding_box = 0;
    u32 shade_priorities = 0;
    shade_by = SB_NONE;
    shade_kind = SHADE_RED;
    if (plane_num < 2) {
        //draw_box_cb = &((debugger_widget *)cvec_get(&dview->options, 0))->checkbox;
        shade_priorities_cb = &((debugger_widget *)cvec_get(&dview->options, 0))->checkbox;
        shade_by_rg = &((debugger_widget *)cvec_get(&dview->options, 1))->radiogroup;
        shade_kind_rg = &((debugger_widget *)cvec_get(&dview->options, 2))->radiogroup;
        shade_by = shade_by_rg->value;
        shade_kind = shade_kind_rg->value;
        shade_priorities = shade_priorities_cb->value;
    }
    switch(plane_num) {
        case 0:
            nametable_addr = this->vdp.io.plane_a_table_addr;
            break;
        case 1:
            nametable_addr = this->vdp.io.plane_b_table_addr;
            break;
        case 2:
            nametable_addr = this->vdp.io.window_table_addr;
            if (this->vdp.io.h40)
                nametable_addr &= 0b111110; // Ignore lowest bit in h40
            nametable_addr <<= 10;
            num_tiles_x = this->vdp.io.h40 ? 64 : 32;
            num_tiles_y = 32;
            break;
    }

    u32 use_screen_palette = 1;
    u32 use_bg = 0 && use_screen_palette;
    u32 bg_color = this->vdp.CRAM[this->vdp.io.bg_color];

    // Loop through rows of tiles
    for (u32 screen_ty = 0; screen_ty < num_tiles_y; screen_ty++) {
        u16 *tile_row_ptr = this->vdp.VRAM + nametable_addr;
        tile_row_ptr += screen_ty * num_tiles_x;

        u32 *screen_ty_ptr = outbuf + (screen_ty * 8 * out_width); // 8 line sdown

        for (u32 screen_tx = 0; screen_tx < num_tiles_x; screen_tx++) {
            u32 tile_data = tile_row_ptr[screen_tx];
            u32 hflip = (tile_data >> 11) & 1;
            u32 vflip = (tile_data >> 12) & 1;
            u32 palette = ((tile_data >> 13) & 3) << 4;
            u8 *pattern_start_ptr = (u8*)this->vdp.VRAM + ((tile_data & 0x7FF) << 5);
            u32 priority = (tile_data >> 15) & 1;

            u32 *screen_tx_ptr = screen_ty_ptr + (screen_tx * 8); // however many x in

            // Now draw the tile
            for (u32 tile_y = 0; tile_y < 8; tile_y++) {
                // Calculate scroll stuff

                u32 *outptr = screen_tx_ptr + (tile_y * out_width); // more lines down
                u32 ty = vflip? 7 - tile_y : tile_y;
                u8 *pattern_ptr = pattern_start_ptr + (ty << 2);
                for (u32 tile_halfx = 0; tile_halfx < 4; tile_halfx++) {
                    u32 offset = hflip ? 3 - tile_halfx : tile_halfx;
                    u8 px2 = pattern_ptr[fetch_order[offset]];

                    u32 v;
                    v = hflip ? px2 & 15 : px2 >> 4;
                    if ((v == 0) && use_bg) v = bg_color;
                    else v = (v == 0) ? 0 : this->vdp.CRAM[palette + v];
                    *outptr = gen_to_rgb(v, use_screen_palette, priority, shade_priorities);
                    outptr++;

                    v = hflip ? px2 >> 4 : px2 & 15;
                    if ((v == 0) && use_bg) v = bg_color;
                    else v = (v == 0) ? 0 : this->vdp.CRAM[palette + v];
                    *outptr = gen_to_rgb(v, use_screen_palette, priority, shade_priorities);
                    outptr++;
                }
            }
        }
    }

    if (draw_bounding_box) {
        // Now, we must draw the scroll frames.
        u32 col_left = 0xFF00FF00;
        u32 col_right = 0xFFFF0000;
        u32 col_top = 0xFF00FF00;
        u32 col_bottom = 0xFFFF0000;

        // To do top-line, we have a bit to do.
        // First, we use row #0. We use its hscroll, and then use the individual column v-scrolls to settle stuff
        // But remember, the scrolls may have 0-15 pixels off the left.

        struct genesis_vdp_debug_row *top = &this->vdp.debug.output_rows[0];
        struct genesis_vdp_debug_row *bottom = &this->vdp.debug.output_rows[0];

        u32 h_mask = (this->vdp.io.foreground_width * 8) - 1;
        u32 v_mask = (this->vdp.io.foreground_height * 8) - 1;
        u32 top_left = top->hscroll[plane_num] & h_mask;
        u32 mx = top_left;

        for (u32 sec = 1; sec < 21; sec++) {
            u32 my = (top->vscroll[plane_num][sec]) & v_mask;
            u32 bottom_my = (my + 239) & v_mask;
            u32 *top_y_pointer = outbuf + (my * out_width);
            u32 *bottom_y_pointer = outbuf + (bottom_my * out_width);
            for (u32 i = 0; i < 16; i++) {
                top_y_pointer[mx] = col_top;
                bottom_y_pointer[mx] = col_bottom;
                mx = (mx + 1) & h_mask;
            }
        }

        u32 vscroll = top->vscroll[plane_num][1];

        // Now we do left and right lines!
        for (u32 mline = 1; mline < 239; mline++) {
            u32 y = (mline + vscroll) & v_mask;
            u32 *line_ptr = outbuf + (y * out_width);
            struct genesis_vdp_debug_row *row = &this->vdp.debug.output_rows[mline];
            u32 left_x = row->hscroll[plane_num] & h_mask;
            u32 right_x = (left_x + (row->h40 ? 319 : 255)) & h_mask;
            line_ptr[left_x] = col_left;
            line_ptr[right_x] = col_right;
        }
    }

    u32 front_buffer = this->clock.current_front_buffer;
    //printf("\nUSING FRONT BUFFER: %d", this->clock.current_front_buffer);
    if (shade_by != SB_NONE) {
        assert(plane_num<2);
        u32 sflip = shade_by == SB_INSIDE ? 0 : 1;
        for (u32 y = 0; y < fh; y++) {
            u32 fetched = 0;
            u32 *px_line = outbuf + (y * out_width);
            u32 *shaded_ptr_line = this->vdp.debug.px_displayed[front_buffer][plane_num] + (y * 32);
            for (u32 x = 0; x < fw; x++) {
                if ((x & 31) == 0) fetched = shaded_ptr_line[x >> 5];
                u32 shift = x & 31;
                u32 do_shade = ((fetched >> shift) & 1) ^ sflip;
                if (do_shade) {
                    //px_line[x] = 0xFF000000;
                    px_line[x] = shade_boundary_func(shade_kind, px_line[x]);
                }
            }
            //px_line += out_width;
            //shaded_ptr_line += 32;
        }
    }
}

static void render_image_view_ym_info(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *gen = (genesis *) ptr;
    struct ym2612 *this = &gen->ym2612;
    //memset(ptr, 0, out_width * 4 * 10);
    struct debugger_widget_textbox *tb = &((debugger_widget *) cvec_get(&dview->options, 0))->textbox;
    debugger_widgets_textbox_clear(tb);
    ///         debugger_widgets_textbox_sprintf(tb, "eng%c  display_mode:%d  bg_mode:%d  bg_ext_pal:%d", ppun == 0 ? 'A' : 'B', eng->io.display_mode, eng->io.bg_mode, eng->io.bg.extended_palettes);
#define tbp(...) debugger_widgets_textbox_sprintf(tb, __VA_ARGS__)
#define YN(x) (x) ? "yes" : "no"
    for (u32 chn = 0; chn < 6; chn++) {
        struct YM2612_CHANNEL *ch = &this->channel[chn];
        tbp("\nch:%d  mode:", chn+1);
        u32 single = 1;
        if ((ch->num == 2) && (ch->mode == YFM_multiple)) {
            tbp("multi-freq");
            single = 0;
        }
        else if ((ch->num == 5) && (this->dac.enable))
            tbp("dac       ");
        else
            tbp("normal    ");

        tbp("  L:%d  R:%d  out:%04x  alg:%d  ch0_feedback:%d", ch->left_enable, ch->right_enable, ch->output & 0xFFFF, ch->algorithm, ch->feedback);
        if (single) {
            tbp("  fnum:%d  octave:%d", ch->f_num.value, ch->block.value);
        }

        for (u32 opn = 0; opn < 4; opn++) {
            struct YM2612_OPERATOR *op = &ch->operator[opn];
            tbp("\n--S%d  out:%04x", opn+1, op->output & 0xFFFF);
            if ((ch->num == 2) && (ch->mode == YFM_multiple)) {
                tbp("  f_num:%d  octave:%d", op->f_num.value, op->block.value);
            }
            tbp("\n  env    am:%d  phase:", op->am_enable);
            switch(op->envelope.state) {
                case EP_attack:
                    tbp("attack ");
                    break;
                case EP_decay:
                    tbp("decay  ");
                    break;
                case EP_sustain:
                    tbp("sustain");
                    break;
                case EP_release:
                    tbp("release");
                    break;
            }
            tbp("  att:%04x  attack:%d  sustain:%d  decay:%d  release:%d", op->envelope.attenuation, op->envelope.attack_rate, op->envelope.sustain_rate, op->envelope.decay_rate, op->envelope.release_rate);
            tbp("\n         ks:%d  ksr:%d  sus:%d  calc_sus:%03x  ssg:%d", op->envelope.key_scale, op->envelope.key_scale_rate, op->envelope.sustain_level, op->envelope.sustain_level == 15 ? 0x3E0 : (op->envelope.sustain_level << 5), op->envelope.ssg.enabled);
        }
        tbp("\n");
    }
}
#undef tbp
#undef YN

static void render_image_view_planea(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    render_image_view_plane(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_planeb(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    render_image_view_plane(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_planew(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    render_image_view_plane(dbgr, dview, ptr, out_width, 2);
}

// sprite_tile_index(sprite_tile_x, sprite_tile_y, hsize, vsize, hflip, vflip);
static u32 sprite_tile_index(u32 tile_x, u32 tile_y, u32 hsize, u32 vsize, u32 hflip, u32 vflip)
{
    u32 ty = vflip ? (vsize-1) - tile_y : tile_y;
    u32 tx = hflip ? (hsize-1) - tile_x : tile_x;
    return (vsize * tx) + ty;
}

static u32 genesis_color_lookup[4][8] =  {
        {  0,  29,  52,  70,  87, 101, 116, 130},  //shadow
        {  0,  52,  87, 116, 144, 172, 206, 255},  //normal
        {130, 144, 158, 172, 187, 206, 228, 255},  //highlight
        {0,0,0,0,0,0,0,0}
};

static void render_image_view_output(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *this = (genesis *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width*(4*240)); // Clear out 240 lines

    struct debugger_widget *shade_normal_b_w = (debugger_widget *)cvec_get(&dview->options, 0);
    struct debugger_widget *shade_shadow_r_w =(debugger_widget *)cvec_get(&dview->options, 1);
    struct debugger_widget *shade_highlight_g_w = (debugger_widget *)cvec_get(&dview->options, 2);
    struct debugger_widget *draw_normal_w = (debugger_widget *)cvec_get(&dview->options, 3);
    struct debugger_widget *draw_shadow_w = (debugger_widget *)cvec_get(&dview->options, 4);
    struct debugger_widget *draw_highlight_w = (debugger_widget *)cvec_get(&dview->options, 5);
    struct debugger_widget *apply_shadow_w = (debugger_widget *)cvec_get(&dview->options, 6);
    struct debugger_widget *apply_highlight_w = (debugger_widget *)cvec_get(&dview->options, 7);
    struct debugger_widget *highlight_a_w = (debugger_widget *)cvec_get(&dview->options, 8);
    struct debugger_widget *highlight_b_w = (debugger_widget *)cvec_get(&dview->options, 9);
    struct debugger_widget *highlight_sp_w = (debugger_widget *)cvec_get(&dview->options, 10);
    struct debugger_widget *highlight_bg_w = (debugger_widget *)cvec_get(&dview->options, 11);
    u32 shade_shadow_r = shade_shadow_r_w->checkbox.value;
    u32 shade_highlight_g = shade_highlight_g_w->checkbox.value;
    u32 shade_normal_b = shade_normal_b_w->checkbox.value;
    u32 apply_shadow = apply_shadow_w->checkbox.value;
    u32 apply_highlight = apply_highlight_w->checkbox.value;
    u32 draw_normal = draw_normal_w->checkbox.value;
    u32 draw_shadow = draw_shadow_w->checkbox.value;
    u32 draw_highlight = draw_highlight_w->checkbox.value;
    u32 highlight_a = highlight_a_w->checkbox.value;
    u32 highlight_b = highlight_b_w->checkbox.value;
    u32 highlight_sp = highlight_sp_w->checkbox.value;
    u32 highlight_bg = highlight_bg_w->checkbox.value;
    u16 *geno = (u16 *)this->vdp.display->output[this->vdp.display->last_written];

    for (u32 sy = 0; sy < 240; sy++) {
        u32 h40 = this->vdp.debug.output_rows[sy].h40;
        u32 sx_max = h40 ? 320 : 256;
        u16 *geno_row_ptr = geno + (sy * 1280);
        u32 *output_row_ptr = outbuf + (sy * out_width);
        u32 mul = h40 ? 4 : 5;
        for (u32 sx = 0; sx < sx_max; sx++) {
            u32 f = geno_row_ptr[sx * mul];
            u32 mode = (f >> 10) & 3;
            u32 lmode = mode;
            if ((mode == 0) && (!apply_shadow)) lmode = 1;
            if ((mode == 2) && (!apply_highlight)) lmode = 1;
            u32 src = (f >> 12) & 3;
            // 0 = plane a. 1 = plane b. 2 = sprite, 3 = background
            u32 b = genesis_color_lookup[lmode][((f >> 6) & 7)];
            u32 g = genesis_color_lookup[lmode][((f >> 3) & 7)];
            u32 r = genesis_color_lookup[lmode][((f >> 0) & 7)];
            if ((src == 0) && highlight_a) {
                r = 255;
                b = g = 0;
            }
            else if ((src == 1) && highlight_b) {
                r = b = 0;
                g = 255;
            }
            else if ((src == 2) && highlight_sp) {
                r = g = 0;
                b = 255;
            }
            else if ((src == 3) && highlight_bg) {
                r = b = 255;
                g = 0;
            }
            else {
                switch (mode) {
                    case 0: // shadow
                        if (draw_shadow) {
                            if (shade_shadow_r) {
                                r = 255;
                                g = b = 0;
                            }
                        } else {
                            r = g = b = 0;
                        }
                        break;
                    case 1: // normal
                        if (draw_normal) {
                            if (shade_normal_b) {
                                r = g = 0;
                                b = 255;
                            }
                        } else {
                            r = g = b = 0;
                        }
                        break;
                    case 2: // highlight
                        if (draw_highlight) {
                            if (shade_highlight_g) {
                                r = b = 0;
                                g = 255;
                            }
                        } else {
                            r = g = b = 0;
                        }
                }
            }
            output_row_ptr[sx] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
    }

}

static void render_image_view_sprites(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *this = (genesis *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width*(4*512)); // Clear out 512 lines


    struct debugger_widget *shade_priorities_w =(debugger_widget *)cvec_get(&dview->options, 0);
    struct debugger_widget *draw_boxes_only_w =(debugger_widget *)cvec_get(&dview->options, 1);
    struct debugger_widget *use_palette_w = (debugger_widget *)cvec_get(&dview->options, 2);
    struct debugger_widget *highlight_shadow_w = (debugger_widget *)cvec_get(&dview->options, 3);
    struct debugger_widget *highlight_highlight_w = (debugger_widget *)cvec_get(&dview->options, 4);
    struct debugger_widget *highlight_force_normal_w = (debugger_widget *)cvec_get(&dview->options, 5);
    u32 shade_priorities = shade_priorities_w->checkbox.value;
    u32 use_palette = use_palette_w->checkbox.value;
    u32 draw_boxes_only = draw_boxes_only_w->checkbox.value;
    u32 highlight_shadow = highlight_shadow_w->checkbox.value;
    u32 highlight_highlight = highlight_highlight_w->checkbox.value;
    u32 highlight_force_normal = highlight_force_normal_w->checkbox.value;
    if (draw_boxes_only) {
        use_palette = 0;
        use_palette_w->enabled = 0;
    }
    else {
        use_palette_w->enabled = 1;
    }

    // We need to paint back to front to preserve order
    // First gather sprite order
    u32 sprites_total_max = this->vdp.io.h40 ? 80 : 64;
    u32 num_sprites = 0;
    u32 sprite_order[80] = {};

    u16 *sprite_table_ptr = this->vdp.VRAM + this->vdp.io.sprite_table_addr;
    u16 next_sprite = 0;
    for (u32 i = 0; i < sprites_total_max; i++) {
        u16 *sp = sprite_table_ptr + (4 * next_sprite);
        sprite_order[num_sprites++] = next_sprite;
        next_sprite = sp[1] & 127;
        if (next_sprite == 0) break;
        if (next_sprite > sprites_total_max) break;
    }

    for (u32 spr_index = 0; spr_index < num_sprites; spr_index++) {
        u32 spr_num = sprite_order[(num_sprites-1) - spr_index];
        u16 *sp = sprite_table_ptr + (4 * spr_num);
        u32 hflip, vflip, hpos, vpos, hsize, vsize, palette, gfx, priority;
        vpos = sp[0] & 0x1FF;
        hsize = ((sp[1] >> 10) & 3) + 1;
        vsize = ((sp[1] >> 8) & 3) + 1;
        hflip = (sp[2] >> 11) & 1;
        vflip = (sp[2] >> 12) & 1;
        priority = (sp[2] >> 15) & 1;
        palette = ((sp[2] >> 13) & 3) << 4;
        hpos = sp[3] & 0x1FF;
        gfx = sp[2] & 0x7FF;

        // Now....start drawing tiles, top-to-bottom then left-to-right
        for (u32 sprite_tile_x = 0; sprite_tile_x < hsize; sprite_tile_x++) {
            for (u32 sprite_tile_y = 0; sprite_tile_y < vsize; sprite_tile_y++) {
                u32 tile_num = gfx + sprite_tile_index(sprite_tile_x, sprite_tile_y, hsize, vsize, hflip, vflip);
                u32 tile_sy = (sprite_tile_y * 8) + vpos;
                u32 tile_sx = (sprite_tile_x * 8) + hpos;
                u32 *screen_corner_ptr = outbuf + (out_width * tile_sy) + tile_sx;
                if (draw_boxes_only) {
                    for (u32 y = 0; y < 8; y++) {
                        u32 screeny = y + tile_sy;
                        u32 *line_ptr = screen_corner_ptr + (out_width * y);
                        if (screeny < 511) {
                            for (u32 x = 0; x < 8; x++) {
                                u32 screenx = x + tile_sx;
                                if (screenx < 511) line_ptr[x] = 0xFFFFFFFF;
                            }
                        }
                    }
                }
                else { // !draw_boxes_only
                    u8 *pattern_start_ptr = (u8*)this->vdp.VRAM + (tile_num << 5);
                    for (u32 tile_y = 0; tile_y < 8; tile_y++) {
                        u32 screeny = tile_y + tile_sy;
                        if (screeny < 511) {
                            // Fetch the tile
                            u32 fine_y = vflip ? 7 - tile_y : tile_y;
                            // 4 bytes = 1 line
                            // 4 * 8 = 32 bytes per tile
                            u8 *tile_row_ptr = pattern_start_ptr + (fine_y << 2);
                            u32 *outptr = screen_corner_ptr + (out_width * tile_y);
                            u32 screenx = tile_sx;
                            for (u32 half = 0; half < 4; half++) {
                                u32 offset = hflip ? 3 - half : half;
                                u8 px2 = tile_row_ptr[fetch_order[offset]];

                                u32 v[2];
                                v[hflip] = px2 >> 4;
                                v[hflip ^ 1] = px2 & 15;

                                for (u32 i = 0; i < 2; i++) {
                                    if (screenx < 511) {
                                        if (v[i] != 0) {
                                            u32 colo = palette | v[i];
                                            u32 c = 0;
                                            if ((highlight_shadow) && (colo == 0x3F)) {
                                                c = 0xFFFFFFFF;
                                            }
                                            else if ((highlight_highlight) && (colo == 0x3E)) {
                                                c = 0xFFFFFFFF;
                                            }
                                            else if ((highlight_force_normal_w) && ((colo == 0x0E) || (colo == 0x1E) || (colo == 0x2E))) {
                                                c = 0xFFFFFFFF;
                                            }
                                            else {
                                                u32 n = use_palette ? this->vdp.CRAM[colo] : v[i];
                                                c = gen_to_rgb(n, use_palette, priority, shade_priorities);
                                            }
                                            *outptr = c;
                                        }
                                    }
                                    screenx++;
                                    outptr++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Now shade in non-visible space slightly green
    u32 top = 0x80;
    u32 left = 0x80;
    u32 bottom = top + 240;
    u32 right = left + (this->vdp.io.h40 ? 320 : 256);
    u32 *line_ptr = outbuf;
    for (u32 y = 0; y < 511; y++) {
        for (u32 x = 0; x < 511; x++) {
            u32 doit = 0;
            if ((y < top) || (y >= bottom))
                doit = 1;
            else if ((x < left) || (x >= right))
                doit = 1;
            if (doit) {
                u32 g = (line_ptr[x] & 0xFF00) >> 8;
                g = ((g + 0x80) / 2);
                if (g > 255) g = 255;
                line_ptr[x] = (line_ptr[x] & 0xFFFF00FF) | (g << 8) | 0xFF000000;
            }
        }
        line_ptr += out_width;
    }
}

static void render_image_view_palette(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *this = (genesis *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width*(4*PAL_BOX_SIZE_W_BORDER)*4); // Clear out at least 4 rows worth

    u32 line_stride = calc_stride(out_width, iv->width);
    for (u32 palette = 0; palette < 4; palette++) {
        for (u32 color = 0; color < 16; color++) {
            u32 y_top = palette * PAL_BOX_SIZE_W_BORDER;
            u32 x_left = color * PAL_BOX_SIZE_W_BORDER;
            u32 c = this->vdp.CRAM[(palette * 16) + color];
            //printf("\nP:%d C:%d VAL:%04x", palette, color, c);
            u32 r, g, b;
            b = ((c >> 6) & 7) * 0x24;
            g = ((c >> 3) & 7) * 0x24;
            r = ((c >> 0) & 7) * 0x24;
            u32 cout = 0xFF000000 | (b << 16) | (g << 8) | r;
            for (u32 y = 0; y < PAL_BOX_SIZE; y++) {
                u32 *box_ptr = (outbuf + ((y_top + y) * out_width) + x_left);
                for (u32 x = 0; x < PAL_BOX_SIZE; x++) {
                    // ALPHA BLUE GREEN RED
                    box_ptr[x] = cout;
                }
            }
        }
    }

}

static void render_image_view_tilemap(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *this = (genesis *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;

    u32 line_stride = calc_stride(out_width, 256);
    u32 tile_line_stride = out_width - 8; // Amount to get to next line after drawing 8 pixel

    // Physical layout in RAM of big-endian 16-bit VRAM is...

    //           3 2 1 0
    //    addr+3  +2 +1 +0
    u32 x_col = 0, y_row = 0;
    for (u32 tile_num = 0; tile_num < 0x7FF; tile_num++) {
        u32 *draw_ptr = (outbuf + (out_width * y_row * 8) + (x_col * 8));

        for (u32 tile_y = 0; tile_y < 8; tile_y++) {
            u8 *tile_ptr = ((u8*)this->vdp.VRAM) + (tile_num * 32) + (tile_y * 4);
            for (u32 half = 0; half < 4; half++) {
                u32 px2 =   tile_ptr[fetch_order[half]];
                u32 v;
                v = px2 >> 4;
                *draw_ptr = 0xFF000000 | (0x111111 * v);
                draw_ptr++;
                v = px2 & 15;
                *draw_ptr = 0xFF000000 | (0x111111 * v);
                draw_ptr++;
            }
            draw_ptr += tile_line_stride;
        }

        x_col++;
        if (x_col == 64) {
            x_col = 0;
            y_row++;
        }
    }
}

static void setup_image_view_plane(genesis* this, debugger_interface *dbgr, int plane_num)
{
    // 0 = plane A
    // 1 = plane B
    // 2 = window
    this->dbg.image_views.nametables[plane_num] = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables[plane_num]);
    struct image_view *iv = &dview->image;

    switch(plane_num) {
        case 0:
        case 1:
            iv->width = 1024;
            iv->height = 1024;
            iv->viewport.p[0] = (ivec2){ 0, 0 };
            iv->viewport.p[1] = (ivec2){ 1024, 1024 };
            //debugger_widgets_add_checkbox(&dview->options, "Draw scroll boundary", 1, 0, 1);
            debugger_widgets_add_checkbox(&dview->options, "Shade priorities", 1, 0, 1);
            struct debugger_widget *rg = debugger_widgets_add_radiogroup(&dview->options, "Shade scroll area", 1, 0, 1);
            debugger_widget_radiogroup_add_button(rg, "None", 0, 1);
            debugger_widget_radiogroup_add_button(rg, "Inside", 1, 1);
            debugger_widget_radiogroup_add_button(rg, "Outside", 2, 1);
            rg = debugger_widgets_add_radiogroup(&dview->options, "Shading method", 1, 0, 1);
            debugger_widget_radiogroup_add_button(rg, "Shaded", 0, 1);
            debugger_widget_radiogroup_add_button(rg, "Black", 1, 1);
            debugger_widget_radiogroup_add_button(rg, "White", 2, 1);
            /*for (u32 i = 0; i < cvec_len(&rg->radiogroup.buttons); i++) {
                struct debugger_widget *cb = cvec_get(&rg->radiogroup.buttons, i);
            }
            break;*/
            break;
        case 2:
            iv->width = 320;
            iv->height = 256;
            iv->viewport.p[0] = (ivec2){ 0, 0 };
            iv->viewport.p[1] = (ivec2){ 320, 256 };
            break;
        default:
            NOGOHERE;
    }
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;

    // Up to 2048 tiles
    // 8x8
    // so, 32x64 tiles.
    iv->update_func.ptr = this;
    switch(plane_num) {
        case 0:
            iv->update_func.func = &render_image_view_planea;
            snprintf(iv->label, sizeof(iv->label), "BG Plane A");
            break;
        case 1:
            iv->update_func.func = &render_image_view_planeb;
            snprintf(iv->label, sizeof(iv->label), "BG Plane B");
            break;
        case 2:
            iv->update_func.func = &render_image_view_planew;
            snprintf(iv->label, sizeof(iv->label),  "BG Window");
            break;

    }




}


static void setup_image_view_tilemap(genesis* this, debugger_interface *dbgr)
{
    this->dbg.image_views.tiles = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.tiles);
    struct image_view *iv = &dview->image;

    iv->width = 512;
    iv->height = 256;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 512, 256 };

    // Up to 2048 tiles
    // 8x8
    // so, 64x32 tiles.
    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_tilemap;

    snprintf(iv->label, sizeof(iv->label), "Pattern Table Viewer");
}

static void setup_image_view_output(genesis* this, debugger_interface *dbgr)
{
    this->dbg.image_views.sprites = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.sprites);
    struct image_view *iv = &dview->image;

    iv->width = 1280;
    iv->height = 240;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ iv->width, iv->height };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_output;

    snprintf(iv->label, sizeof(iv->label), "Output Debug");

    debugger_widgets_add_checkbox(&dview->options, "Shade normal B", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Shade shadow R", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Shade highlights G", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Draw normal", 1, 1, 0);
    debugger_widgets_add_checkbox(&dview->options, "Draw shadowed", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Draw highlighted", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Apply shadows", 1, 1, 0);
    debugger_widgets_add_checkbox(&dview->options, "Apply highlights", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight Plane A px R", 1, 0, 0);
    debugger_widgets_add_checkbox(&dview->options, "Highlight Plane B px G", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight Sprite px B", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight BG px Purple", 1, 0, 1);


}

static void setup_image_view_sprites(genesis* this, debugger_interface *dbgr)
{
    this->dbg.image_views.sprites = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.sprites);
    struct image_view *iv = &dview->image;

    iv->width = 512;
    iv->height = 512;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ iv->width, iv->height };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_sprites;

    snprintf(iv->label, sizeof(iv->label), "Sprites Debug");

    debugger_widgets_add_checkbox(&dview->options, "Shade priorities", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Draw boxes only", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Use palette", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight shadow color", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight highlight color", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight force-normal color", 1, 0, 1);
}


static void setup_image_view_palette(genesis* this, debugger_interface *dbgr)
{
    this->dbg.image_views.palette = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.palette);
    struct image_view *iv = &dview->image;

    iv->width = 16 * PAL_BOX_SIZE_W_BORDER;
    iv->height = 4 * PAL_BOX_SIZE_W_BORDER;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ iv->width, iv->height };

    // Up to 2048 tiles
    // 8x8
    // so, 32x64 tiles.
    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_palette;

    snprintf(iv->label, sizeof(iv->label), "Palette Viewer");
}

static void setup_events_view(genesis* this, debugger_interface *dbgr)
{
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    ev->timing = ev_timing_master_clock;
    ev->master_clocks.per_line = 3420;
    ev->master_clocks.height = 262;
    ev->master_clocks.ptr = &this->clock.master_cycle_count;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 424; // 320 pixels + 104 hblank
        ev->display[i].height = 262; // 262 pixels high

        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }

    ev->associated_display = this->vdp.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("VDP events", DBG_GEN_CATEGORY_VDP);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_GEN_CATEGORY_CPU);

    cvec_grow_by(&ev->events, DBG_GEN_EVENT_MAX);
    cvec_lock_reallocs(&ev->events);
    DEBUG_REGISTER_EVENT("VRAM write", 0x0000FF, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_WRITE_VRAM, 1);
    DEBUG_REGISTER_EVENT("VSRAM write", 0x00FF00, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_WRITE_VSRAM, 1);
    DEBUG_REGISTER_EVENT("CRAM write", 0x802060, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_WRITE_CRAM, 1);
    DEBUG_REGISTER_EVENT("IRQ: HBlank", 0xFF0000, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_HBLANK_IRQ, 1);
    DEBUG_REGISTER_EVENT("IRQ: VBlank", 0x00FFFF, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_VBLANK_IRQ, 1);
    DEBUG_REGISTER_EVENT("DMA fill", 0xFF00FF, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_DMA_FILL, 1);
    DEBUG_REGISTER_EVENT("DMA copy", 0x004090, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_DMA_COPY, 1);
    DEBUG_REGISTER_EVENT("DMA load", 0xFF4090, DBG_GEN_CATEGORY_VDP, DBG_GEN_EVENT_DMA_LOAD, 1);

    SET_EVENT_VIEW(this->z80);
    SET_EVENT_VIEW(this->m68k);
    SET_EVENT_VIEW(this->ym2612);
    SET_EVENT_VIEW(this->psg);

    debugger_report_frame(this->dbg.interface);
}

static void setup_image_view_ym_info(genesis *this, debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.ym_info = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.ym_info);
    struct image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_ym_info;

    snprintf(iv->label, sizeof(iv->label), "YM2612 Info");

    debugger_widgets_add_textbox(&dview->options, "blah!", 1);
}

void genesisJ_setup_debugger_interface(JSM, debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 2;
    setup_m68k_disassembly(dbgr, this);
    setup_z80_disassembly(dbgr, this);
    setup_waveforms_psg(this, dbgr);
    setup_waveforms_ym2612(this, dbgr);
    setup_events_view(this, dbgr);
    setup_image_view_tilemap(this, dbgr);
    setup_image_view_palette(this, dbgr);
    setup_image_view_plane(this, dbgr, 0);
    setup_image_view_plane(this, dbgr, 1);
    setup_image_view_plane(this, dbgr, 2);
    setup_image_view_sprites(this, dbgr);
    setup_image_view_output(this, dbgr);
    setup_image_view_ym_info(this, dbgr);
}
