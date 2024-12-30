//
// Created by . on 12/4/24.
//

#include <string.h>
#include <assert.h>

#include "gba.h"
#include "gba_bus.h"
#include "gba_debugger.h"

#define JTHIS struct GBA* this = (struct GBA*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct GBA* this

static void create_and_bind_registers_ARM7TDMI(struct GBA* this, struct disassembly_view *dv)
{
    u32 tkindex = 0;
    /*struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
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
#undef BIND*/
}

static void fill_disassembly_view(void *macptr, struct debugger_interface *dbgr, struct disassembly_view *dview)
{
    struct GBA* this = (struct GBA*)macptr;
    /*for (u32 i = 0; i < 8; i++) {
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
    this->dbg.dasm_m68k.IRC->int32_data = this->m68k.regs.IRC;*/
}

static void render_image_view_window(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width, int win_num) {
    struct GBA *this = (struct GBA *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 160);
    u8 bit = 1 << win_num;

    for (u32 y = 0; y < 160; y++) {
        u32 *rowptr = outbuf + (y * out_width);
        u8 *wptr = &this->dbg_info.line[y].window_coverage[0];
        for (u32 x = 0; x < 240; x++) {
            rowptr[x]= ((*wptr) & bit) ? 0xFFFFFFFF : 0xFF000000;
            wptr++;
        }
    }
}

static void render_image_view_window0(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_window1(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_window2(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 3);
}

static void render_image_view_window3(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 2);
}


static void get_disassembly_ARM7TDMI(void *genptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct GBA* this = (struct GBA*)genptr;
    ARM7TDMI_disassemble_entry(&this->cpu, entry);
}

static struct disassembly_vars get_disassembly_vars_ARM7TDMI(void *macptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct GBA* this = (struct GBA*)macptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->cpu.trace.ins_PC;
    dvar.current_clock_cycle = this->clock.master_cycle_count;
    return dvar;
}

static void setup_cpu_trace(struct debugger_interface *dbgr, struct GBA *this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_trace);
    struct debugger_view *dview = cpg(p);
    struct trace_view *tv = &dview->trace;
    snprintf(tv->name, sizeof(tv->name), "ARM7TDMI Trace");
    trace_view_add_col(tv, "Arch", 5);
    trace_view_add_col(tv, "Cycle#", 10);
    trace_view_add_col(tv, "Addr", 8);
    trace_view_add_col(tv, "Opcode", 8);
    trace_view_add_col(tv, "Disassembly", 45);
    trace_view_add_col(tv, "Context", 32);
    tv->autoscroll = 1;
    tv->display_end_top = 0;

    this->cpu.dbg.tvptr = tv;
}

static void setup_ARM7TDMI_disassembly(struct debugger_interface *dbgr, struct GBA* this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFFFFFF;
    dv->addr_column_size = 8;
    dv->has_context = 1;
    jsm_string_sprintf(&dv->processor_name, "ARM7TDMI");

    create_and_bind_registers_ARM7TDMI(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_disassembly_ARM7TDMI;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars_ARM7TDMI;
}


static void setup_image_view_window(struct GBA* this, struct debugger_interface *dbgr, u32 wnum)
{
    struct debugger_view *dview;
    switch(wnum) {
        case 0:
            this->dbg.image_views.window0 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window0);
            break;
        case 1:
            this->dbg.image_views.window1 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window1);
            break;
        case 2:
            this->dbg.image_views.window2 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window2);
            break;
        case 3:
            this->dbg.image_views.window3 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window3);
            break;
    }
    struct image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 240, 160 };

    iv->update_func.ptr = this;
    switch(wnum) {
        case 0:
            iv->update_func.func = &render_image_view_window0;
            snprintf(iv->label, sizeof(iv->label), "Window0 Viewer");
            break;
        case 1:
            iv->update_func.func = &render_image_view_window1;
            snprintf(iv->label, sizeof(iv->label), "Window1 Viewer");
            break;
        case 2:
            iv->update_func.func = &render_image_view_window2;
            snprintf(iv->label, sizeof(iv->label), "Window Sprite Viewer");
            break;
        case 3:
            iv->update_func.func = &render_image_view_window3;
            snprintf(iv->label, sizeof(iv->label), "Window Outside Viewer");
            break;
    }

}

void GBAJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 0;
    dbgr->smallest_step = 1;

    //setup_ARM7TDMI_disassembly(dbgr, this);
    setup_cpu_trace(dbgr, this);
    /*setup_image_view_plane(this, dbgr, 0);
    setup_image_view_plane(this, dbgr, 1);
    setup_image_view_plane(this, dbgr, 2);
    setup_image_view_plane(this, dbgr, 3);
    setup_image_view_sprites(this, dbgr);*/
    setup_image_view_window(this, dbgr, 0);
    setup_image_view_window(this, dbgr, 1);
    setup_image_view_window(this, dbgr, 2);
    setup_image_view_window(this, dbgr, 3);
    //setup_events_view(this, dbgr);
    //cvec_ptr_init(&this->dbg.events.view);
    /*setup_waveforms_psg(this, dbgr);
    setup_waveforms_ym2612(this, dbgr);
    setup_image_view_tilemap(this, dbgr);
    setup_image_view_palette(this, dbgr);
    setup_image_view_plane(this, dbgr, 0);
    setup_image_view_plane(this, dbgr, 1);
    setup_image_view_plane(this, dbgr, 2);
    setup_image_view_sprites(this, dbgr);*/

}
