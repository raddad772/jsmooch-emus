//
// Created by . on 12/4/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gba.h"
#include "gba_bus.h"
#include "gba_debugger.h"

#include "helpers/debugger/debugger.h"
#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.c"

#define JTHIS struct GBA* this = (struct GBA*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void GBAJ_play(JSM);
static void GBAJ_pause(JSM);
static void GBAJ_stop(JSM);
static void GBAJ_get_framevars(JSM, struct framevars* out);
static void GBAJ_reset(JSM);
static u32 GBAJ_finish_frame(JSM);
static u32 GBAJ_finish_scanline(JSM);
static u32 GBAJ_step_master(JSM, u32 howmany);
static void GBAJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void GBAJ_describe_io(JSM, struct cvec* IOs);

// 240x160, but 308x228 with v and h blanks
#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
#define SCANLINE_HBLANK 1006

void GBAJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)MASTER_CYCLES_PER_FRAME / (float)ab->samples_len);
        this->audio.next_sample_cycle = 0;
        //struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms_psg.main);
        //this->psg.ext_enable = wf->ch_output_enabled;
    }

    // PSG
    //struct debug_waveform *wf = cpg(this->dbg.waveforms_psg.main);
    //setup_debug_waveform(wf);

}

u32 read_trace_cpu(void *ptr, u32 addr, u32 sz) {
    struct GBA* this = (struct GBA*)ptr;
    return GBA_mainbus_read(this, addr, sz, this->io.cpu.open_bus_data, 0);
}

void GBA_new(struct jsm_system *jsm)
{
    struct GBA* this = (struct GBA*)malloc(sizeof(struct GBA));
    memset(this, 0, sizeof(*this));
    ARM7TDMI_init(&this->cpu);
    this->cpu.read_ptr = this;
    this->cpu.write_ptr = this;
    this->cpu.read = &GBA_mainbus_read;
    this->cpu.write = &GBA_mainbus_write;
    this->cpu.fetch_ptr = this;
    this->cpu.fetch_ins = &GBA_mainbus_fetchins;
    GBA_bus_init(this);
    GBA_clock_init(&this->clock);
    GBA_cart_init(&this->cart);
    GBA_PPU_init(this);

    snprintf(jsm->label, sizeof(jsm->label), "GameBoy Advance");
    struct jsm_debug_read_trace dt;
    dt.read_trace_arm = &read_trace_cpu;
    dt.ptr = this;
    ARM7TDMI_setup_tracing(&this->cpu, &dt, &this->clock.master_cycle_count);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &GBAJ_finish_frame;
    jsm->finish_scanline = &GBAJ_finish_scanline;
    jsm->step_master = &GBAJ_step_master;
    jsm->reset = &GBAJ_reset;
    jsm->load_BIOS = &GBAJ_load_BIOS;
    jsm->get_framevars = &GBAJ_get_framevars;
    jsm->play = &GBAJ_play;
    jsm->pause = &GBAJ_pause;
    jsm->stop = &GBAJ_stop;
    jsm->describe_io = &GBAJ_describe_io;
    jsm->set_audiobuf = &GBAJ_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &GBAJ_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;
    
}

void GBA_delete(struct jsm_system *jsm)
{
    JTHIS;

    ARM7TDMI_delete(&this->cpu);
    GBA_PPU_delete(this);
    GBA_cart_delete(&this->cart);

    while (cvec_len(this->jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->jsm.IOs);
        if (pio->kind == HID_CART_PORT) {
            if (pio->cartridge_port.unload_cart) pio->cartridge_port.unload_cart(jsm);
        }
        physical_io_device_delete(pio);
    }

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

u32 GBAJ_finish_frame(JSM)
{
    JTHIS;
    ARM7TDMI_delete(&this->cpu);

    u64 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        GBAJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->ppu.display->last_written;
}

void GBAJ_play(JSM)
{
}

void GBAJ_pause(JSM)
{
}

void GBAJ_stop(JSM)
{
}

void GBAJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.ppu.x;
    out->scanline = this->clock.ppu.y;
    out->master_cycle = this->clock.master_cycle_count;
}

static void skip_BIOS(struct GBA* this)
{
/*
SWI 00h (GBA/NDS7/NDS9) - SoftReset
Clears 200h bytes of RAM (containing stacks, and BIOS IRQ vector/flags)
*/
    for (u32 i = 0x3007E00; i < 0x3008000; i++) {
        cW[1](this->WRAM_fast, i - 0x3000000, 0);
    }

    // , initializes system, supervisor, and irq stack pointers,
    // sets R0-R12, LR_svc, SPSR_svc, LR_irq, and SPSR_irq to zero, and enters system mode.
    for (u32 i = 0; i < 13; i++) {
        this->cpu.regs.R[i] = 0;
    }
    this->cpu.regs.R_svc[1] = 0;
    this->cpu.regs.R_irq[1] = 0;
    this->cpu.regs.SPSR_svc = 0;
    this->cpu.regs.SPSR_irq = 0;
    this->cpu.regs.CPSR.mode = ARM7_system;
    ARM7TDMI_fill_regmap(&this->cpu);
    /*
Host  sp_svc    sp_irq    sp_svc    zerofilled area       return address
  GBA   3007FE0h  3007FA0h  3007F00h  [3007E00h..3007FFFh]  Flag[3007FFAh] */

    this->cpu.regs.R_svc[0] = 0x03007FE0;
    this->cpu.regs.R_irq[0] = 0x03007FA0;
    this->cpu.regs.R[13] = 0x03007F00;

    this->cpu.regs.R[15] = 0x08000000;
    ARM7TDMI_reload_pipeline(&this->cpu);
}

void GBAJ_reset(JSM)
{
    JTHIS;
    ARM7TDMI_reset(&this->cpu);
    GBA_clock_reset(&this->clock);
    GBA_PPU_reset(this);

    skip_BIOS(this);


    printf("\nGBA reset!");
}

u32 GBAJ_finish_scanline(JSM)
{
    JTHIS;
    GBA_PPU_start_scanline(this);
    if (dbg.do_break) return 0;
    ARM7TDMI_cycle(&this->cpu, MASTER_CYCLES_BEFORE_HBLANK);
    if (dbg.do_break) return 0;
    GBA_PPU_hblank(this);
    if (dbg.do_break) return 0;
    ARM7TDMI_cycle(&this->cpu, HBLANK_CYCLES);
    if (dbg.do_break) return 0;
    GBA_PPU_finish_scanline(this);
    //GBAJ_step_master(jsm, MASTER_CYCLES_PER_SCANLINE);
    return 0;
}

static u32 GBAJ_step_master(JSM, u32 howmany)
{
    printf("\nNOT YET SUPPORT!");
    return 0;
}

static void GBAJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    memcpy(this->BIOS.data, mfs->files[0].buf.ptr, 16384);
    this->BIOS.has = 1;
}

static void GBAIO_unload_cart(JSM)
{
}

static void GBAIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    u32 r;
    GBA_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, pio, &r);
    GBAJ_reset(jsm);
}

void setup_lcd(struct JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

    d->fps = 59.727;
    d->fps_override_hint = 60;
    // 240x160, but 308x228 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 240;
    d->pixelometry.cols.max_visible = 160;
    d->pixelometry.cols.right_hblank = 68;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 160;
    d->pixelometry.rows.max_visible = 160;
    d->pixelometry.rows.bottom_vblank = 68;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 3;
    d->geometry.physical_aspect_ratio.height = 2;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_audio(struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 48000;
    chan->low_pass_filter = 24000;
}


static void GBAJ_describe_io(JSM, struct cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    cvec_lock_reallocs(IOs);

    // controllers
    struct physical_io_device *controller = cvec_push_back(this->jsm.IOs);
    GBA_controller_setup_pio(controller);
    this->controller.pio = controller;

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &GBAIO_load_cart;
    d->cartridge_port.unload_cart = &GBAIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(240 * 160 * 2);
    d->display.output[1] = malloc(240 * 160 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_lcd(&d->display);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    this->ppu.cur_output = (u16 *)(d->display.output[0]);
    this->ppu.cur_pixel = 0;

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
}
