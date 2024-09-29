//
// Created by Dave on 2/4/2024.
//
#include "assert.h"
#include "stdlib.h"
#include <stdio.h>

#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"
#include "helpers/debugger/debugger.h"
#include "component/audio/nes_apu/nes_apu.h"

#include "nes.h"
#include "nes_cart.h"
#include "nes_ppu.h"
#include "nes_cpu.h"
#include "nes_debugger.h"

#define JTHIS struct NES* this = (struct NES*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NES* this

static void NESJ_play(JSM);
static void NESJ_pause(JSM);
static void NESJ_stop(JSM);
static void NESJ_get_framevars(JSM, struct framevars* out);
static void NESJ_reset(JSM);
static void NESJ_killall(JSM);
static u32 NESJ_finish_frame(JSM);
static u32 NESJ_finish_scanline(JSM);
static u32 NESJ_step_master(JSM, u32 howmany);
static void NESJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void NESJ_enable_tracing(JSM);
static void NESJ_disable_tracing(JSM);
static void NESJ_describe_io(JSM, struct cvec* IOs);

static u32 read_trace(void *ptr, u32 addr)
{
    struct NES* this = (struct NES*)ptr;
    return NES_bus_CPU_read(this, addr, 0, 0);
}

#define APU_CYCLES_PER_FRAME  29780

static void setup_debug_waveform(struct debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)APU_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

static void NESJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)APU_CYCLES_PER_FRAME / (float)ab->samples_len);
        printf("\nCYCLES PER FRAME:%d PER SAMPLE:%f", APU_CYCLES_PER_FRAME, this->audio.master_cycles_per_audio_sample);
        this->audio.next_sample_cycle = 0;
        struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms.main);
        this->apu.ext_enable = wf->ch_output_enabled;
    }
    setup_debug_waveform(cpg(this->dbg.waveforms.main));
    for (u32 i = 0; i < 4; i++) {
        struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms.chan[i]);
        setup_debug_waveform(wf);
        this->apu.channels[i].ext_enable = wf->ch_output_enabled;
    }
    struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms.chan[4]);
    this->apu.dmc.ext_enable = wf->ch_output_enabled;
}


void NES_new(JSM)
{
    struct NES* this = (struct NES*)malloc(sizeof(struct NES));
    NES_clock_init(&this->clock);
    //NES_bus_init(&this, &this->clock);
    r2A03_init(&this->cpu, this);
    NES_PPU_init(&this->ppu, this);
    NES_bus_init(&this->bus, this);
    NES_cart_init(&this->cart, this);
    NES_APU_init(&this->apu);
    this->apu.master_cycles = &this->clock.master_clock;

    struct jsm_debug_read_trace dt;
    dt.read_trace = &read_trace;
    dt.ptr = (void*)this;

    M6502_setup_tracing(&this->cpu.cpu, &dt, &this->clock.master_clock);
    snprintf(jsm->label, sizeof(jsm->label), "Nintendo Entertainment System");

    this->described_inputs = 0;

    this->cycles_left = 0;
    this->display_enabled = 1;
    this->IOs = NULL;
    jsm->ptr = (void*)this;

    jsm->finish_frame = &NESJ_finish_frame;
    jsm->finish_scanline = &NESJ_finish_scanline;
    jsm->step_master = &NESJ_step_master;
    jsm->reset = &NESJ_reset;
    jsm->load_BIOS = &NESJ_load_BIOS;
    jsm->get_framevars = &NESJ_get_framevars;
    jsm->play = &NESJ_play;
    jsm->pause = &NESJ_pause;
    jsm->stop = &NESJ_stop;
    jsm->set_audiobuf = &NESJ_set_audiobuf;
    jsm->describe_io = &NESJ_describe_io;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &NESJ_setup_debugger_interface;
}


void NES_delete(JSM)
{
    JTHIS;
    //NES_CPU_delete(&this->cpu);
    //NES_PPU_delete(&this->ppu);
    /*GB_cart_delete(&this->cart);
    GB_bus_delete(&this->bus);*/
    //GB_clock_delete(&this->clock);

    NES_bus_delete(&this->bus);
    NES_cart_delete(&this->cart);

    while (cvec_len(this->IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->IOs);
        if (pio->kind == HID_CART_PORT) {
            if (pio->cartridge_port.unload_cart) pio->cartridge_port.unload_cart(jsm);
        }
        physical_io_device_delete(pio);
    }

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

static void NESIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    NES_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
    NES_bus_set_which_mapper(&this->bus, this->cart.header.mapper_number);
    NES_bus_set_cart(this, &this->cart);
    //NESJ_reset(jsm);
}

static void NESIO_unload_cart(JSM)
{
}

static void setup_crt(struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps = 60.1;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 1;
    d->pixelometry.cols.right_hblank = 85;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.max_visible = 256;
    d->pixelometry.offset.x = 1;

    d->pixelometry.rows.top_vblank = 1;
    d->pixelometry.rows.visible = 240;
    d->pixelometry.rows.max_visible = 240;
    d->pixelometry.rows.bottom_vblank = 21;
    d->pixelometry.offset.y = 1;

    //d->geometry.physical_aspect_ratio.width = 4;
    //d->geometry.physical_aspect_ratio.height = 3;
    d->geometry.physical_aspect_ratio.width = 1;
    d->geometry.physical_aspect_ratio.height = 1;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 8;
}

static void setup_audio(struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 48000;
}

void NESJ_describe_io(JSM, struct cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    struct physical_io_device *c1 = cvec_push_back(this->IOs); //0
    //struct physical_io_device *c2 = cvec_push_back(this->IOs); //1
    NES_joypad_setup_pio(c1, 0, "Player 1", 1);
    //NES_joypad_setup_pio(c2, 1, "Player 2", 0);

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs); //2
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->chassis.digital_buttons); //3
    b->common_id = DBCID_ch_reset;
    sprintf(b->name, "Reset");
    b->state = 0;

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs); //4
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &NESIO_load_cart;
    d->cartridge_port.unload_cart = &NESIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1); //5
    d->display.output[0] = malloc(256 * 224 * 2);
    d->display.output[1] = malloc(256 * 224 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    this->ppu.cur_output = (u16 *)d->display.output[0];
    setup_crt(&d->display);
    d->display.last_written = 1;
    d->display.last_displayed = 1;

    setup_audio(IOs);

    this->cpu.joypad1.devices = IOs;
    this->cpu.joypad1.device_index = NES_INPUTS_PLAYER1;
    this->cpu.joypad2.devices = IOs;
    this->cpu.joypad2.device_index = NES_INPUTS_PLAYER2;

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
}

void NESJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void NESJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void NESJ_play(JSM)
{
}

void NESJ_pause(JSM)
{
}

void NESJ_stop(JSM)
{
}

void NESJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->ppu.line_cycle;
    out->scanline = this->clock.ppu_y;
}

void NESJ_reset(JSM)
{
    JTHIS;
    NES_clock_reset(&this->clock);
    r2A03_reset(&this->cpu);
    NES_PPU_reset(&this->ppu);
    NES_APU_reset(&this->apu);
    NES_bus_reset(this);
}


void NESJ_killall(JSM)
{

}

static void sample_audio(struct NES* this)
{
    this->clock.apu_master_clock++;
    if (this->audio.buf && (this->clock.apu_master_clock >= (u64)this->audio.next_sample_cycle)) {
        this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
        float *sptr = ((float *)this->audio.buf->ptr) + (this->audio.buf->upos);
        if (this->audio.buf->upos <= this->audio.buf->samples_len) {
            *sptr = NES_APU_mix_sample(&this->apu, 0);
        }
        this->audio.buf->upos++;
    }

    struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
    if (this->clock.master_clock >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = NES_APU_mix_sample(&this->apu, 1);
            dw->user.buf_pos++;
        }
    }

    dw = cpg(this->dbg.waveforms.chan[0]);
    if (this->clock.master_clock >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 5; j++) {
            dw = cpg(this->dbg.waveforms.chan[j]);
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                float sv = NES_APU_sample_channel(&this->apu, j);
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                dw->user.buf_pos++;
                assert(dw->user.buf_pos < 410);
            }
        }
    }
}


u32 NESJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        NESJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->ppu.display->last_written;
}

u32 NESJ_finish_scanline(JSM)
{
    JTHIS;
    i32 cpu_step = (i32)this->clock.timing.cpu_divisor;
    i64 ppu_step = (i64)this->clock.timing.ppu_divisor;
    i32 done = 0;
    i32 start_y = this->clock.ppu_y;
    while (this->clock.ppu_y == start_y) {
        this->clock.master_clock += cpu_step;
        //this->apu.cycle(this->clock.master_clock);
        r2A03_run_cycle(&this->cpu);
        NES_APU_cycle(&this->apu);
        sample_audio(this);
        NES_bus_CPU_cycle(this);
        this->clock.cpu_frame_cycle++;
        this->clock.cpu_master_clock += cpu_step;
        i64 ppu_left = (i64)this->clock.master_clock - (i64)this->clock.ppu_master_clock;
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        NES_PPU_cycle(&this->ppu, done);
        this->cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }
    return 0;
}


u32 NESJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->cycles_left += howmany;
    i64 cpu_step = (i64)this->clock.timing.cpu_divisor;
    i64 ppu_step = (i64)this->clock.timing.ppu_divisor;
    u32 done = 0;
    while (this->cycles_left >= cpu_step) {
        //this->apu.cycle(this->clock.master_clock);
        this->clock.master_clock += cpu_step;
        r2A03_run_cycle(&this->cpu);
        NES_APU_cycle(&this->apu);
        sample_audio(this);
        NES_bus_CPU_cycle(this);
        this->clock.cpu_frame_cycle++;
        this->clock.cpu_master_clock += cpu_step;
        i64 ppu_left = (i64)this->clock.master_clock - (i64)this->clock.ppu_master_clock;
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        NES_PPU_cycle(&this->ppu, done);
        this->cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }
    return 0;
}

void NESJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nNES doesn't have a BIOS...?");
}

