//
// Created by . on 2/11/25.
//

#include "snes.h"


#define TAG_SCANLINE 1
#define TAG_FRAME 2

//
// Created by . on 6/1/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debugger/debugger.h"

#include "snes.h"
#include "snes_debugger.h"
#include "snes_bus.h"
#include "snes_cart.h"
#include "snes_ppu.h"
#include "snes_apu.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2

#define JTHIS struct SNES* this = (struct SNES*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct SNES* this

static void SNESJ_play(JSM);
static void SNESJ_pause(JSM);
static void SNESJ_stop(JSM);
static void SNESJ_get_framevars(JSM, struct framevars* out);
static void SNESJ_reset(JSM);
static u32 SNESJ_finish_frame(JSM);
static u32 SNESJ_finish_scanline(JSM);
static u32 SNESJ_step_master(JSM, u32 howmany);
static void SNESJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void SNESJ_describe_io(JSM, struct cvec* IOs);

u32 read_trace_wdc65816(void *ptr, u32 addr) {
    struct SNES* this = (struct SNES*)ptr;
    return SNES_wdc65816_read(this, addr, 0, 0);
}

u32 read_trace_spc700(void *ptr, u32 addr) {
    struct SNES* this = (struct SNES*)ptr;
    return SNES_spc700_read(this, addr, 0, 0);
}

static void setup_debug_waveform(struct debug_waveform *dw)
{
    /*if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;*/
}

void SNESJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    /*this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)MASTER_CYCLES_PER_FRAME / (float)ab->samples_len);
        this->audio.next_sample_cycle_max = 0;
        struct debug_waveform *wf = cpg(this->dbg.waveforms_psg.main);
        this->audio.master_cycles_per_max_sample = (float)MASTER_CYCLES_PER_FRAME / (float)wf->samples_requested;

        wf = (struct debug_waveform *)cpg(this->dbg.waveforms_psg.chan[0]);
        this->audio.master_cycles_per_min_sample = (float)MASTER_CYCLES_PER_FRAME / (float)wf->samples_requested;
    }

    // PSG
    struct debug_waveform *wf = cpg(this->dbg.waveforms_psg.main);
    setup_debug_waveform(wf);
    this->psg.ext_enable = wf->ch_output_enabled;
    if (wf->clock_divider == 0) wf->clock_divider = wf->default_clock_divider;
    this->clock.psg.clock_divisor = wf->clock_divider;
    for (u32 i = 0; i < 4; i++) {
        wf = (struct debug_waveform *)cpg(this->dbg.waveforms_psg.chan[i]);
        setup_debug_waveform(wf);
        if (i < 3) {
            this->psg.sw[i].ext_enable = wf->ch_output_enabled;
        }
        else
            this->psg.noise.ext_enable = wf->ch_output_enabled;
    }

    // ym2612
    wf = cpg(this->dbg.waveforms_ym2612.main);
    this->ym2612.ext_enable = wf->ch_output_enabled;
    setup_debug_waveform(wf);
    if (wf->clock_divider == 0) wf->clock_divider = wf->default_clock_divider;
    this->clock.ym2612.clock_divisor = wf->clock_divider;
    for (u32 i = 0; i < 6; i++) {
        wf = (struct debug_waveform *)cpg(this->dbg.waveforms_ym2612.chan[i]);
        setup_debug_waveform(wf);
        this->ym2612.channel[i].ext_enable = wf->ch_output_enabled;
    }
*/
}

static void populate_opts(struct jsm_system *jsm)
{
    /*debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer A", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer B", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Sprites", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: trace", 1, 0, 0);*/
}

static void read_opts(struct jsm_system *jsm, struct SNES* this)
{
    /*struct debugger_widget *w = cvec_get(&jsm->opts, 0);
    this->opts.vdp.enable_A = w->checkbox.value;

    w = cvec_get(&jsm->opts, 1);
    this->opts.vdp.enable_B = w->checkbox.value;

    w = cvec_get(&jsm->opts, 2);
    this->opts.vdp.enable_sprites = w->checkbox.value;

    w = cvec_get(&jsm->opts, 3);
    this->opts.vdp.ex_trace = w->checkbox.value;*/
}

static void run_block(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES *)ptr;

    this->clock.master_cycle_count = this->scheduler.first_event->timecode;
}

void SNES_new(JSM)
{
    struct SNES* this = (struct SNES*)malloc(sizeof(struct SNES));
    memset(this, 0, sizeof(*this));
    populate_opts(jsm);
    scheduler_init(&this->scheduler, &this->clock.master_cycle_count, &this->clock.nothing);
    this->scheduler.max_block_size = 20;
    this->scheduler.run.func = &run_block;
    this->scheduler.run.ptr = this;
    SNES_clock_init(&this->clock);
    SNES_mem_init(this);
    SNES_cart_init(this);

    R5A22_init(&this->r5a22, &this->clock.master_cycle_count);
    SNES_APU_init(this);
    //SNES_cart_init(&this->cart);
    SNES_PPU_init(this); // must be after m68k init

    snprintf(jsm->label, sizeof(jsm->label), "SNES");

    struct jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_wdc65816;
    dt.ptr = (void*)this;
    R5A22_setup_tracing(&this->r5a22, &dt);

    dt.read_trace = &read_trace_spc700;

    SPC700_setup_tracing(&this->apu.cpu, &dt);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &SNESJ_finish_frame;
    jsm->finish_scanline = &SNESJ_finish_scanline;
    jsm->step_master = &SNESJ_step_master;
    jsm->reset = &SNESJ_reset;
    jsm->load_BIOS = &SNESJ_load_BIOS;
    jsm->get_framevars = &SNESJ_get_framevars;
    jsm->play = &SNESJ_play;
    jsm->pause = &SNESJ_pause;
    jsm->stop = &SNESJ_stop;
    jsm->describe_io = &SNESJ_describe_io;
    jsm->set_audiobuf = &SNESJ_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &SNESJ_setup_debugger_interface;
    //jsm->save_state = SNESJ_save_state;
    //jsm->load_state = SNESJ_load_state;
    jsm->save_state = NULL;
    jsm->load_state = NULL;
}

void SNES_delete(JSM) {
    JTHIS;

    R5A22_delete(&this->r5a22);
    SNES_APU_delete(this);
    SNES_cart_delete(this);
    SNES_PPU_delete(this);

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

static inline float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}

static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    /*struct SNES* this = (struct SNES *)ptr;
    if (this->audio.buf) {
        this->audio.cycles++;
        this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
        scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle, 0, this, &sample_audio, NULL);
        if (this->audio.buf->upos < this->audio.buf->samples_len) {
            i32 v = 0;
            if (this->psg.ext_enable)
                v += (i32)(SN76489_mix_sample(&this->psg, 0) >> 5);
            if (this->ym2612.ext_enable)
                v += (i32)this->ym2612.mix.mono_output;
            ((float *)this->audio.buf->ptr)[this->audio.buf->upos] = i16_to_float((i16)v);
        }
        this->audio.buf->upos++;
    }*/
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    /*struct SNES *this = (struct SNES *)ptr;

    // PSG
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(SN76489_mix_sample(&this->psg, 1));
        dw->user.buf_pos++;
    }

    // YM2612
    dw = cpg(this->dbg.waveforms_ym2612.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(this->ym2612.mix.mono_output << 3);
        dw->user.buf_pos++;
    }
    this->audio.next_sample_cycle_max += this->audio.master_cycles_per_max_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);*/
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    /*struct SNES *this = (struct SNES *)ptr;

    // PSG
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.chan[0]);
    for (int j = 0; j < 4; j++) {
        dw = cpg(this->dbg.waveforms_psg.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            i16 sv = SN76489_sample_channel(&this->psg, j);
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv * 4);
            dw->user.buf_pos++;
        }
    }

    // YM2612
    for (int j = 0; j < 6; j++) {
        dw = cpg(this->dbg.waveforms_ym2612.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            i16 sv = ym2612_sample_channel(&this->ym2612, j);
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv << 2);
            dw->user.buf_pos++;
        }
    }
    this->audio.next_sample_cycle_min += this->audio.master_cycles_per_min_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);*/
}


static void schedule_first(struct SNES *this)
{
    /*scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle, 0, this, &sample_audio, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->clock.ym2612.clock_divisor, 0, this, &run_ym2612, NULL);
    scheduler_only_add_abs(&this->scheduler, this->clock.psg.clock_divisor, 0, this, &run_psg, NULL);*/

    SNES_APU_schedule_first(this);
    SNES_PPU_schedule_first(this);
    R5A22_schedule_first(this);
}

static void SNESIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *which_pio)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    SNES_cart_load_ROM_from_RAM(this, b->ptr, b->size, which_pio);
    SNESJ_reset(jsm);
}

static void SNESIO_unload_cart(JSM)
{
}

static void setup_crt(struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    // 320x224 or 256x224, but, can be x448, and because 256 and 320 can change in the middle of a line, we will do a special output

    // 1280 x 448 output resolution so that changes mid-line are fine, scaled down

    d->fps = 60.0988;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 0; // 0
    d->pixelometry.cols.visible = 1280;  // 320x224   *4
    d->pixelometry.cols.max_visible = 1280;  // 320x224    *4
    d->pixelometry.cols.right_hblank = 430; // 107.5, ick   *4
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 240;
    d->pixelometry.rows.max_visible = 240;
    d->pixelometry.rows.bottom_vblank = 76;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_audio(struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 32000;
    chan->low_pass_filter = 16000;
}

void SNESJ_describe_io(JSM, struct cvec *IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *c1 = cvec_push_back(this->jsm.IOs);
    struct physical_io_device *c2 = cvec_push_back(this->jsm.IOs);
    //SNES_controller_6button_init(&this->controller1, &this->clock.master_cycle_count);
    //SNES6_setup_pio(c1, 0, "Player 1", 1);
    //SNES3_setup_pio(c2, 1, "Player 2", 0);
    //this->controller1.pio = c1;
    //this->controller2.pio = c2;

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &SNESIO_load_cart;
    d->cartridge_port.unload_cart = &SNESIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(256 * 224 * 2);
    d->display.output[1] = malloc(256 * 224 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_crt(&d->display);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    this->ppu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
    //SNES_controllerport_connect(&this->io.controller_port1, SNES_controller_6button, &this->controller1);
    //SNES_controllerport_connect(&this->io.controller_port2, SNES_controller_3button, &this->controller2);
}

void SNESJ_play(JSM)
{
}

void SNESJ_pause(JSM)
{
}

void SNESJ_stop(JSM)
{
}

void SNESJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = (i32)((this->clock.ppu.scanline_start / this->clock.master_cycle_count) >> 2) - 21;
    out->scanline = this->clock.ppu.y;
    out->master_cycle = this->clock.master_cycle_count;
}

void SNESJ_reset(JSM)
{
    JTHIS;
    R5A22_reset(&this->r5a22);
    SNES_APU_reset(this);
    SNES_PPU_reset(this);

    scheduler_clear(&this->scheduler);
    schedule_first(this);
    printf("\nSNES reset!");
}

//#define DO_STATS

u32 SNESJ_finish_frame(JSM)
{
    JTHIS;
    read_opts(jsm, this);

    scheduler_run_til_tag(&this->scheduler, TAG_FRAME);
    return this->ppu.display->last_written;
}

u32 SNESJ_finish_scanline(JSM)
{
    JTHIS;
    scheduler_run_til_tag(&this->scheduler, TAG_SCANLINE);

    return this->ppu.display->last_written;
}

u32 SNESJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    scheduler_run_for_cycles(&this->scheduler, howmany);
    return 0;
}

void SNESJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nSega SNES doesn't have a BIOS...?");
}

