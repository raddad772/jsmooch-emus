//
// Created by . on 2/11/25.
//

#include <cassert>
#include <cstdlib>

#include "snes.h"
#include "snes_debugger.h"
#include "snes_bus.h"
#include "snes_cart.h"
#include "snes_ppu.h"
#include "snes_apu.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2

#define JTHIS SNES* this = (SNES*)jsm->ptr
#define JSM jsm_system* jsm

#define THIS SNES* this

u32 read_trace_wdc65816(void *ptr, u32 addr) {
    SNES* this = (SNES*)ptr;
    return SNES_wdc65816_read(this, addr, 0, 0);
}

static void setup_debug_waveform(SNES *snes, debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)snes->clock.timing.frame.master_cycles / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void SNESJ_set_audiobuf(jsm_system* jsm, audiobuf *ab)
{
    
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = ((float)clock.timing.frame.master_cycles / (float)ab->samples_len);
        audio.next_sample_cycle_max = 0;
        debug_waveform *wf = cpg(dbg.waveforms.main);
        audio.master_cycles_per_max_sample = (float)clock.timing.frame.master_cycles / (float)wf->samples_requested;

        wf = (debug_waveform *)cpg(dbg.waveforms.chan[0]);
        audio.master_cycles_per_min_sample = (float)clock.timing.frame.master_cycles / (float)wf->samples_requested;
    }

    debug_waveform *wf = cpg(dbg.waveforms.main);
    setup_debug_waveform(this, wf);
    apu.dsp.ext_enable = wf->ch_output_enabled;
    for (u32 i = 0; i < 8; i++) {
        wf = (debug_waveform *)cpg(dbg.waveforms.chan[i]);
        setup_debug_waveform(this, wf);
        apu.dsp.channel[i].ext_enable = wf->ch_output_enabled;
    }

}

static void populate_opts(jsm_system *jsm)
{
    /*debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer A", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer B", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Sprites", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: trace", 1, 0, 0);*/
}

static void read_opts(jsm_system *jsm, SNES* this)
{
    /*debugger_widget *w = cvec_get(&jsm->opts, 0);
    opts.vdp.enable_A = w->checkbox.value;

    w = cvec_get(&jsm->opts, 1);
    opts.vdp.enable_B = w->checkbox.value;

    w = cvec_get(&jsm->opts, 2);
    opts.vdp.enable_sprites = w->checkbox.value;

    w = cvec_get(&jsm->opts, 3);
    opts.vdp.ex_trace = w->checkbox.value;*/
}

static void run_block(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *this = (SNES *)ptr;

    R5A22_cycle(this, 0, 0, 0);
}

static inline float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}


static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES* this = (SNES *)ptr;
    // TODO: make this stereo!
    // TODO: make debug waveform also stereo
    if (audio.buf) {
        audio.cycles++;
        if (audio.buf->upos < (audio.buf->samples_len << 1)) {
            ((float *)audio.buf->ptr)[audio.buf->upos] = i16_to_float(apu.dsp.output.l);
            ((float *)audio.buf->ptr)[audio.buf->upos+1] = i16_to_float(apu.dsp.output.r);
        }
        audio.buf->upos+=2;
    }
}

void SNES_new(JSM)
{
    SNES* this = (SNES*)malloc(sizeof(SNES));
    memset(this, 0, sizeof(*this));
    populate_opts(jsm);
    scheduler_init(&scheduler, &clock.master_cycle_count, &clock.nothing);
    scheduler.max_block_size = 20;
    scheduler.run.func = &run_block;
    scheduler.run.ptr = this;
    SNES_clock_init(&clock);
    clock.nothing = 0;
    SNES_mem_init(this);
    SNES_cart_init(this);

    R5A22_init(&r5a22, &clock.master_cycle_count);
    SNES_APU_init(this);
    apu.dsp.sample.func = &sample_audio;
    apu.dsp.sample.ptr = this;
    SNES_PPU_init(this); // must be after m68k init

    snprintf(jsm->label, sizeof(jsm->label), "SNES");

    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_wdc65816;
    dt.ptr = (void*)this;
    R5A22_setup_tracing(&r5a22, &dt);

    SPC700_setup_tracing(&apu.cpu, &dt);

    jsm.described_inputs = 0;
    jsm.IOs = NULL;

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
    

    R5A22_delete(&r5a22);
    SNES_APU_delete(this);
    SNES_cart_delete(this);
    SNES_PPU_delete(this);

    while (cvec_len(jsm.IOs) > 0) {
        physical_io_device* pio = cvec_pop_back(jsm.IOs);
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
    SNES *this = (SNES *)ptr;

    debug_waveform *dw = cpg(dbg.waveforms.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        i32 smp = (apu.dsp.output.l + apu.dsp.output.r) >> 1;
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(smp);
        dw->user.buf_pos++;
    }

    audio.next_sample_cycle_max += audio.master_cycles_per_max_sample;
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *this = (SNES *)ptr;

    // PSG
    debug_waveform *dw = cpg(dbg.waveforms.chan[0]);
    for (int j = 0; j < 8; j++) {
        dw = cpg(dbg.waveforms.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            i16 sv = apu.dsp.channel[j].output.debug;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv);
            dw->user.buf_pos++;
        }
    }

    audio.next_sample_cycle_min += audio.master_cycles_per_min_sample;
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
}


static void schedule_first(SNES *this)
{
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
    //scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle, 0, this, &sample_audio, NULL);

    SNES_APU_schedule_first(this);
    SNES_PPU_schedule_first(this);
    R5A22_schedule_first(this);
}

static void SNESIO_load_cart(JSM, multi_file_set *mfs, physical_io_device *which_pio)
{
    
    buf* b = &mfs->files[0].buf;

    SNES_cart_load_ROM_from_RAM(this, b->ptr, b->size, which_pio);
    SNESJ_reset(jsm);
}

static void SNESIO_unload_cart(JSM)
{
}

static void setup_crt(JSM_DISPLAY *d)
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

static void setup_audio(cvec* IOs)
{
    physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->num = 2;
    chan->left = chan->right = 1;
    chan->sample_rate = 32000;
    //chan->sample_rate = 64000;
    chan->low_pass_filter = 16000;
}

void SNESJ_describe_io(JSM, cvec *IOs)
{
    cvec_lock_reallocs(IOs);
    
    if (jsm.described_inputs) return;
    jsm.described_inputs = 1;

    jsm.IOs = IOs;

    // controllers
    physical_io_device *c1 = cvec_push_back(jsm.IOs);
    physical_io_device *c2 = cvec_push_back(jsm.IOs);
    SNES_joypad_init(&controller1);
    SNES_joypad_init(&controller2);
    SNES_joypad_setup_pio(c1, 0, "Player 1", 1);
    controller1.pio = c1;
    controller2.pio = c2;

    // power and reset buttons
    physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    HID_digital_button* b;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    // cartridge port
    physical_io_device *d = cvec_push_back(IOs);
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
    ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    ppu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    ppu.display = &((physical_io_device *)cpg(ppu.display_ptr))->display;
    SNES_controllerport_connect(&r5a22.controller_port[0], SNES_CK_standard, &controller1);
    //SNES_controllerport_connect(&io.controller_port2, SNES_controller_3button, &controller2);
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

void SNESJ_get_framevars(JSM, framevars* out)
{
    
    out->master_frame = clock.master_frame;
    out->x = (i32)((clock.ppu.scanline_start / clock.master_cycle_count) >> 2) - 21;
    out->scanline = clock.ppu.y;
    out->master_cycle = clock.master_cycle_count;
}

void SNESJ_reset(JSM)
{
    
    R5A22_reset(&r5a22);
    SNES_APU_reset(this);
    SNES_PPU_reset(this);

    scheduler_clear(&scheduler);
    schedule_first(this);
    printf("\nSNES reset!");
}

//#define DO_STATS

u32 SNESJ_finish_frame(JSM)
{
    
    read_opts(jsm, this);
#ifdef DO_STATS
    u64 spc_start = apu.cpu.int_clock;
    u64 wdc_start = r5a22.cpu.int_clock;
#endif
    scheduler_run_til_tag(&scheduler, TAG_FRAME);
#ifdef DO_STATS
    u64 spc_num_cycles = (apu.cpu.int_clock - spc_start) * 60;
    u64 wdc_num_cycles = (r5a22.cpu.int_clock - wdc_start) * 60;
    double spc_div = (double)clock.timing.frame.master_cycles / (double)spc_num_cycles;
    double wdc_div = (double)clock.timing.frame.master_cycles / (double)wdc_num_cycles;
    printf("\nSCANLINE:%d FRAME:%lld", clock.ppu.y, clock.master_frame);
    printf("\n\nEFFECTIVE 65816 FREQ IS %lld. RUNNING AT SPEED",wdc_num_cycles);
    printf("\nEFFECTIVE SPC700 FREQ IS %lld. RUNNING AT SPEED", spc_num_cycles);

#endif
    return ppu.display->last_written;
}

u32 SNESJ_finish_scanline(JSM)
{
    
    read_opts(jsm, this);
    scheduler_run_til_tag(&scheduler, TAG_SCANLINE);

    return ppu.display->last_written;
}

u32 SNESJ_step_master(JSM, u32 howmany)
{
    
    read_opts(jsm, this);
    //printf("\nRUN FOR %d CYCLES:", howmany);
    //u64 cur = clock.master_cycle_count;
    scheduler_run_for_cycles(&scheduler, howmany);
    //u64 dif = clock.master_cycle_count - cur;
    //printf("\nRAN %lld CYCLES", dif);
    return 0;
}

void SNESJ_load_BIOS(JSM, multi_file_set* mfs)
{
    printf("\nSega SNES doesn't have a BIOS...?");
}

