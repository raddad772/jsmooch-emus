//
// Created by . on 10/20/24.
//

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "genesis_debugger.h"
#include "genesis.h"
#include "genesis_bus.h"
#include "component/cpu/z80/z80_disassembler.h"

#define JTHIS struct genesis* this = (struct genesis*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static int render_imask(struct cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%d", ctx->int32_data);
}

static int render_csr(struct cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
{
    return snprintf(outbuf, outbuf_sz, "%c%c%c%c%c",
                    ctx->int32_data & 0x10 ? 'X' : 'x',
                    ctx->int32_data & 8 ? 'N' : 'n',
                    ctx->int32_data & 4 ? 'Z' : 'z',
                    ctx->int32_data & 2 ? 'V' : 'v',
                    ctx->int32_data & 1 ? 'C' : 'c'
    );
}

static int render_f_z80(struct cpu_reg_context*ctx, void *outbuf, size_t outbuf_sz)
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


static void create_and_bind_registers_z80(struct genesis* this, struct disassembly_view *dv)
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
    rg->custom_render = &render_f_z80;

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


static void create_and_bind_registers_m68k(struct genesis* this, struct disassembly_view *dv)
{
    u32 tkindex = 0;
    for (u32 i = 0; i < 8; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        sprintf(rg->name, "D%d", i);
        rg->kind = RK_int32;
        rg->custom_render = NULL;
        rg->index = tkindex++;
    }
    for (u32 i = 0; i < 7; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        sprintf(rg->name, "A%d", i);
        rg->kind = RK_int32;
        rg->custom_render = NULL;
        rg->index = tkindex++;
    }
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "PC");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "USP");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "SSP");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "SR");
    rg->kind = RK_int32;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "supervisor");
    rg->kind = RK_bool;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "trace");
    rg->kind = RK_bool;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "IMASK");
    rg->kind = RK_int32;
    rg->custom_render = &render_imask;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "CSR");
    rg->kind = RK_int32;
    rg->custom_render = &render_csr;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "IR");
    rg->kind = RK_bool;
    rg->custom_render = NULL;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "IRC");
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

static void fill_disassembly_view(void *macptr, struct debugger_interface *dbgr, struct disassembly_view *dview)
{
    struct genesis* this = (struct genesis*)macptr;
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

static struct disassembly_vars get_disassembly_vars_m68k(void *macptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct genesis* this = (struct genesis*)macptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->m68k.dbg.ins_PC;
    dvar.current_clock_cycle = this->clock.master_cycle_count;
    return dvar;
}

static void get_dissasembly_m68k(void *genptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct genesis* this = (struct genesis*)genptr;
    M68k_disassemble_entry(&this->m68k, entry);
}

static void setup_m68k_disassembly(struct debugger_interface *dbgr, struct genesis* this)
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
    dv->get_disassembly.func = &get_dissasembly_m68k;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars_m68k;
}

static void get_dissasembly_z80(void *genptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct genesis* this = (struct genesis*)genptr;
    Z80_disassemble_entry(&this->z80, entry);
}

static struct disassembly_vars get_disassembly_vars_z80(void *genptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct genesis* this = (struct genesis*)genptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->z80.PCO;
    dvar.current_clock_cycle = this->clock.master_cycle_count;
    return dvar;
}

static void setup_z80_disassembly(struct debugger_interface *dbgr, struct genesis* this)
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

static void setup_waveforms_ym2612(struct genesis* this, struct debugger_interface *dbgr)
{
    this->dbg.waveforms_ym2612.view = debugger_view_new(dbgr, dview_waveforms);
    struct debugger_view *dview = cpg(this->dbg.waveforms_ym2612.view);
    struct waveform_view *wv = (struct waveform_view *)&dview->waveform;
    sprintf(wv->name, "YM2612");

    struct debug_waveform *dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.main = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 144;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[0] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "CH1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[1] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "CH2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[2] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "CH3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[3] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "CH4");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[4] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "CH5");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw = cvec_push_back(&wv->waveforms);

    debug_waveform_init(dw);
    this->dbg.waveforms_ym2612.chan[5] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "CH6/PCM");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

}


static void setup_waveforms_psg(struct genesis* this, struct debugger_interface *dbgr)
{
    this->dbg.waveforms_psg.view = debugger_view_new(dbgr, dview_waveforms);
    struct debugger_view *dview = cpg(this->dbg.waveforms_psg.view);
    struct waveform_view *wv = (struct waveform_view *)&dview->waveform;
    sprintf(wv->name, "SN76489");

    struct debug_waveform *dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.main = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 240;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[0] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "Square 0");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[1] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "Square 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[2] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "Square 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms_psg.chan[3] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    sprintf(dw->name, "Noise");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}

static u32 calc_stride(u32 out_width, u32 in_width)
{
    return (out_width - in_width);
}

#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static inline u32 gen_to_rgb(u32 color, int use_palette)
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
    return o | b | (g << 8) | (r << 16);
}


static void render_image_view_plane(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width, int plane_num)
{
    struct genesis *this = (struct genesis *) ptr;

    // Clear output
    struct image_view *iv = &dview->image;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 1024);

    // Render tilemap
    u32 num_tiles_x = this->vdp.io.foreground_width;
    u32 num_tiles_y = this->vdp.io.foreground_height;

    u32 nametable_addr;

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
                nametable_addr &= (0b1111100 << 9); // Ignore lowest bit in h40
            num_tiles_x = this->vdp.io.h40 ? 40 : 32;
            num_tiles_y = 32;
            break;
    }

    u32 use_screen_palette = 1;
    u32 use_bg = 0 && use_screen_palette;
    u32 bg_color = this->vdp.CRAM[this->vdp.io.bg_color];

    static int fetch_order[4] = { 1, 0, 3, 2 };
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
            u8 *pattern_start_ptr = (u8*)this->vdp.VRAM + ((tile_data & 0x3FF) << 5);

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
                    *outptr = gen_to_rgb(v, use_screen_palette);
                    outptr++;

                    v = hflip ? px2 >> 4 : px2 & 15;
                    if ((v == 0) && use_bg) v = bg_color;
                    else v = (v == 0) ? 0 : this->vdp.CRAM[palette + v];
                    *outptr = gen_to_rgb(v, use_screen_palette);
                    outptr++;
                }
            }
        }
    }

    if (plane_num < 2) {
        // Now, we must draw the scroll frames.
        u32 col_left = 0xFF00FF00;
        u32 col_right = 0xFFFF0000;
        u32 col_top = 0xFF00FF00;
        u32 col_bottom = 0xFFFF0000;

        // To do top-line, we have a bit to do.
        // First, we use row #0. We use its hscroll, and then use the individual column v-scrolls to settle stuff
        // But remember, the scrolls may have 0-15 pixels off the left.

        struct genesis_vdp_debug_row *top = &this->vdp.debug_info[0];
        struct genesis_vdp_debug_row *bottom = &this->vdp.debug_info[0];

        u32 h_mask = (this->vdp.io.foreground_width * 8) - 1;
        u32 v_mask = (this->vdp.io.foreground_height * 8) - 1;
        u32 top_left = top->hscroll[plane_num] & h_mask;
        u32 mx = top_left;

        for (u32 sec = 1; sec < 21; sec++) {
            u32 my = (top->vscroll[plane_num][sec]) & v_mask;
            u32 bottom_my = (my + 224) & v_mask;
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
        for (u32 mline = 1; mline < 223; mline++) {
            u32 y = (mline + vscroll) & v_mask;
            u32 *line_ptr = outbuf + (y * out_width);
            struct genesis_vdp_debug_row *row = &this->vdp.debug_info[mline];
            u32 left_x = row->hscroll[plane_num] & h_mask;
            u32 right_x = (left_x + (row->h40 ? 320 : 256)) & h_mask;
            line_ptr[left_x] = col_left;
            line_ptr[right_x] = col_right;
        }
    }
}

static void render_image_view_planea(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    render_image_view_plane(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_planeb(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    render_image_view_plane(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_planew(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    render_image_view_plane(dbgr, dview, ptr, out_width, 2);
}


static void render_image_view_palette(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *this = (struct genesis *) ptr;
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

static void render_image_view_tilemap(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct genesis *this = (struct genesis *) ptr;
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
    const static u32 offset[4] = {0, 1, 2, 3}; // D7...0, so take upper 4 first
    for (u32 tile_num = 0; tile_num < 0x7FF; tile_num++) {
        u32 *draw_ptr = (outbuf + (out_width * y_row * 8) + (x_col * 8));

        for (u32 tile_y = 0; tile_y < 8; tile_y++) {
            u8 *tile_ptr = ((u8*)this->vdp.VRAM) + (tile_num * 32) + (tile_y * 4);
            for (u32 x = 0; x < 4; x++) {
                /* IN BIG ENDIAN,
                      16    bits          16 bits
                 * D7...D0   D7...D0   D7...D0 D7...D0
                 * so let's label it
                 * B7...B0   A7...A0
                 *
                 * We want the upper 8 bits first.
                 * In a big-endian system, that means we want the first 8 bits physically in RAM.
                 * */

                u32 px2 =   tile_ptr[offset[x]];
                u32 v;
                //v = px2 >> 4;
                v = px2 & 15;
                //if (tile_y == 0) v = 15;

                //if (x == 0) v = 15;

                *draw_ptr = 0xFF000000 | (0x111111 * v);
                draw_ptr++;
                v = px2 >> 4;
                //v = px2 & 15;
                //if (tile_y == 0) v = 15;
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

static void setup_image_view_plane(struct genesis* this, struct debugger_interface *dbgr, int plane_num)
{
    // 0 = plane A
    // 1 = plane B
    // 2 = window
    this->dbg.image_views.nametables = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables);
    struct image_view *iv = &dview->image;

    switch(plane_num) {
        case 0:
        case 1:
            iv->width = 1024;
            iv->height = 1024;
            iv->viewport.p[0] = (struct ivec2){ 0, 0 };
            iv->viewport.p[1] = (struct ivec2){ 1024, 1024 };
            break;
        case 2:
            iv->width = 320;
            iv->height = 256;
            iv->viewport.p[0] = (struct ivec2){ 0, 0 };
            iv->viewport.p[1] = (struct ivec2){ 320, 256 };
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
            sprintf(iv->label, "BG Plane A");
            break;
        case 1:
            iv->update_func.func = &render_image_view_planeb;
            sprintf(iv->label, "BG Plane B");
            break;
        case 2:
            iv->update_func.func = &render_image_view_planew;
            sprintf(iv->label, "BG Window");
            break;

    }




}


static void setup_image_view_tilemap(struct genesis* this, struct debugger_interface *dbgr)
{
    this->dbg.image_views.nametables = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables);
    struct image_view *iv = &dview->image;

    iv->width = 512;
    iv->height = 256;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 512, 256 };

    // Up to 2048 tiles
    // 8x8
    // so, 32x64 tiles.
    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_tilemap;

    sprintf(iv->label, "Pattern Table Viewer");
}

static void setup_image_view_palette(struct genesis* this, struct debugger_interface *dbgr)
{
    this->dbg.image_views.nametables = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables);
    struct image_view *iv = &dview->image;

    iv->width = 16 * PAL_BOX_SIZE_W_BORDER;
    iv->height = 4 * PAL_BOX_SIZE_W_BORDER;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ iv->width, iv->height };

    // Up to 2048 tiles
    // 8x8
    // so, 32x64 tiles.
    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_palette;

    sprintf(iv->label, "Palette Table Viewer");
}


void genesisJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 2;

    // Setup diassembly for m68k, z80
    setup_m68k_disassembly(dbgr, this);
    setup_z80_disassembly(dbgr, this);
    setup_waveforms_psg(this, dbgr);
    setup_waveforms_ym2612(this, dbgr);
    setup_image_view_tilemap(this, dbgr);
    setup_image_view_palette(this, dbgr);
    setup_image_view_plane(this, dbgr, 0);
    setup_image_view_plane(this, dbgr, 1);
    setup_image_view_plane(this, dbgr, 2);
}
