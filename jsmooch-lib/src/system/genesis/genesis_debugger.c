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
        rg->index = tkindex++;
    }
    for (u32 i = 0; i < 7; i++) {
        struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
        sprintf(rg->name, "A%d", i);
        rg->kind = RK_int32;
        rg->index = tkindex++;
    }
    struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "PC");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "USP");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "SSP");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "SR");
    rg->kind = RK_int32;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "supervisor");
    rg->kind = RK_bool;
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "trace");
    rg->kind = RK_bool;
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
    rg->index = tkindex++;

    rg = cvec_push_back(&dv->cpu.regs);
    sprintf(rg->name, "IRC");
    rg->kind = RK_bool;
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
}
