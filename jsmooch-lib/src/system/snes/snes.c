//
// Created by . on 2/11/25.
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

static void setup_debug_waveform(struct SNES *snes, struct debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)snes->clock.timing.frame.master_cycles / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void SNESJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)this->clock.timing.frame.master_cycles / (float)ab->samples_len);
        this->audio.next_sample_cycle_max = 0;
        struct debug_waveform *wf = cpg(this->dbg.waveforms.main);
        this->audio.master_cycles_per_max_sample = (float)this->clock.timing.frame.master_cycles / (float)wf->samples_requested;

        wf = (struct debug_waveform *)cpg(this->dbg.waveforms.chan[0]);
        this->audio.master_cycles_per_min_sample = (float)this->clock.timing.frame.master_cycles / (float)wf->samples_requested;
    }

    struct debug_waveform *wf = cpg(this->dbg.waveforms.main);
    setup_debug_waveform(this, wf);
    this->apu.dsp.ext_enable = wf->ch_output_enabled;
    for (u32 i = 0; i < 8; i++) {
        wf = (struct debug_waveform *)cpg(this->dbg.waveforms.chan[i]);
        setup_debug_waveform(this, wf);
        this->apu.dsp.channel[i].ext_enable = wf->ch_output_enabled;
    }

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

    R5A22_cycle(this, 0, 0, 0);
    this->clock.master_cycle_count += this->clock.cpu.divider;
}

static inline float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}


static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES* this = (struct SNES *)ptr;
    // TODO: make this stereo!
    // TODO: make debug waveform also stereo
    if (this->audio.buf) {
        this->audio.cycles++;
        if (this->audio.buf->upos < (this->audio.buf->samples_len << 1)) {
            ((float *)this->audio.buf->ptr)[this->audio.buf->upos] = i16_to_float(this->apu.dsp.output.l);
            ((float *)this->audio.buf->ptr)[this->audio.buf->upos+1] = i16_to_float(this->apu.dsp.output.r);
        }
        this->audio.buf->upos+=2;
    }
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
    this->clock.nothing = 0;
    SNES_mem_init(this);
    SNES_cart_init(this);

    R5A22_init(&this->r5a22, &this->clock.master_cycle_count);
    SNES_APU_init(this);
    this->apu.dsp.sample.func = &sample_audio;
    this->apu.dsp.sample.ptr = this;
    SNES_PPU_init(this); // must be after m68k init

    snprintf(jsm->label, sizeof(jsm->label), "SNES");

    struct jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_wdc65816;
    dt.ptr = (void*)this;
    R5A22_setup_tracing(&this->r5a22, &dt);

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

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES *)ptr;

    struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        i32 smp = (this->apu.dsp.output.l + this->apu.dsp.output.r) >> 1;
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(smp);
        dw->user.buf_pos++;
    }

    this->audio.next_sample_cycle_max += this->audio.master_cycles_per_max_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES *)ptr;

    // PSG
    struct debug_waveform *dw = cpg(this->dbg.waveforms.chan[0]);
    for (int j = 0; j < 8; j++) {
        dw = cpg(this->dbg.waveforms.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            i16 sv = this->apu.dsp.channel[j].output.debug;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv);
            dw->user.buf_pos++;
        }
    }

    this->audio.next_sample_cycle_min += this->audio.master_cycles_per_min_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
}


static void schedule_first(struct SNES *this)
{
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
    //scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle, 0, this, &sample_audio, NULL);

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

    d->fps = 60.0988;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 42;
    d->pixelometry.cols.visible = 512;
    d->pixelometry.cols.max_visible = 512;
    d->pixelometry.cols.right_hblank = 42;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 224;
    d->pixelometry.rows.max_visible = 224;
    d->pixelometry.rows.bottom_vblank = 38;
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
    chan->num = 2;
    chan->left = chan->right = 1;
    chan->sample_rate = 32000;
    //chan->sample_rate = 64000;
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
    SNES_joypad_init(&this->controller1);
    SNES_joypad_init(&this->controller2);
    SNES_joypad_setup_pio(c1, 0, "Player 1", 1);
    this->controller1.pio = c1;
    this->controller2.pio = c2;

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
    d->display.output[0] = malloc(512 * 224 * 2);
    d->display.output[1] = malloc(512 * 224 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_crt(&d->display);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    this->ppu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
    SNES_controllerport_connect(&this->r5a22.controller_port[0], SNES_CK_standard, &this->controller1);
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
    //printf("\nNEW FRAME FINISH");
    JTHIS;
    read_opts(jsm, this);
#ifdef DO_STATS
    u64 spc_start = this->apu.cpu.int_clock;
    u64 wdc_start = this->r5a22.cpu.int_clock;
#endif
    scheduler_run_til_tag(&this->scheduler, TAG_FRAME);
#ifdef DO_STATS
    u64 spc_num_cycles = (this->apu.cpu.int_clock - spc_start) * 60;
    u64 wdc_num_cycles = (this->r5a22.cpu.int_clock - wdc_start) * 60;
    double spc_div = (double)this->clock.timing.frame.master_cycles / (double)spc_num_cycles;
    double wdc_div = (double)this->clock.timing.frame.master_cycles / (double)wdc_num_cycles;
    printf("\nSCANLINE:%d FRAME:%lld", this->clock.ppu.y, this->clock.master_frame);
    printf("\n\nEFFECTIVE 65816 FREQ IS %lld. RUNNING AT SPEED",wdc_num_cycles);
    printf("\nEFFECTIVE SPC700 FREQ IS %lld. RUNNING AT SPEED", spc_num_cycles);

#endif
    return this->ppu.display->last_written;
}

u32 SNESJ_finish_scanline(JSM)
{
    JTHIS;
    read_opts(jsm, this);
    scheduler_run_til_tag(&this->scheduler, TAG_SCANLINE);

    return this->ppu.display->last_written;
}

u32 SNESJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    read_opts(jsm, this);
    //printf("\nRUN FOR %d CYCLES:", howmany);
    //u64 cur = this->clock.master_cycle_count;
    scheduler_run_for_cycles(&this->scheduler, howmany);
    //u64 dif = this->clock.master_cycle_count - cur;
    //printf("\nRAN %lld CYCLES", dif);
    return 0;
}

void SNESJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nSega SNES doesn't have a BIOS...?");
}

