//
// Created by . on 6/1/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debugger/debugger.h"

#include "component/controller/genesis3/genesis3.h"
#include "genesis.h"
#include "genesis_debugger.h"
#include "genesis_bus.h"
#include "genesis_cart.h"
#include "genesis_vdp.h"

#define JTHIS struct genesis* this = (struct genesis*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void genesisJ_play(JSM);
static void genesisJ_pause(JSM);
static void genesisJ_stop(JSM);
static void genesisJ_get_framevars(JSM, struct framevars* out);
static void genesisJ_reset(JSM);
static void genesisJ_killall(JSM);
static u32 genesisJ_finish_frame(JSM);
static u32 genesisJ_finish_scanline(JSM);
static u32 genesisJ_step_master(JSM, u32 howmany);
static void genesisJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void genesisJ_enable_tracing(JSM);
static void genesisJ_disable_tracing(JSM);
static void genesisJ_describe_io(JSM, struct cvec* IOs);

#define MASTER_CYCLES_PER_SCANLINE 3420
#define NTSC_SCANLINES 262

// 896040
#define MASTER_CYCLES_PER_FRAME MASTER_CYCLES_PER_SCANLINE*NTSC_SCANLINES

// SMS/GG has 179208, so genesis master clock is *5
// Sn76489 divider on SMS/GG is 48, so 48*5 = 240
#define SN76489_DIVIDER 240


/*
    u32 (*read_trace)(void *,u32);
    u32 (*read_trace_m68k)(void *,u32,u32,u32);
 */
u32 read_trace_z80(void *ptr, u32 addr) {
    struct genesis* this = (struct genesis*)ptr;
    return genesis_z80_bus_read(this, addr, this->z80.pins.D, 0);
}

u32 read_trace_m68k(void *ptr, u32 addr, u32 UDS, u32 LDS) {
    struct genesis* this = (struct genesis*)ptr;
    return genesis_mainbus_read(this, addr, UDS, LDS, this->io.m68k.open_bus_data, 0);
}

static void setup_debug_waveform(struct debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void genesisJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)MASTER_CYCLES_PER_FRAME / (float)ab->samples_len);
        this->audio.next_sample_cycle = 0;
        struct debug_waveform *wf = (struct debug_waveform *)cpg(this->dbg.waveforms_psg.main);
        this->psg.ext_enable = wf->ch_output_enabled;
    }

    // PSG
    struct debug_waveform *wf = cpg(this->dbg.waveforms_psg.main);
    setup_debug_waveform(wf);
    //this->clock.apu_divisor = wf->clock_divider;
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
    setup_debug_waveform(wf);
    //this->clock.apu_divisor = wf->clock_divider;
    for (u32 i = 0; i < 6; i++) {
        wf = (struct debug_waveform *)cpg(this->dbg.waveforms_ym2612.chan[i]);
        setup_debug_waveform(wf);
        this->ym2612.channel[i].ext_enable = wf->ch_output_enabled;
    }

}


void genesis_new(JSM)
{
    struct genesis* this = (struct genesis*)malloc(sizeof(struct genesis));
    memset(this, 0, sizeof(*this));
    Z80_init(&this->z80, 0);
    M68k_init(&this->m68k, 1);
    genesis_clock_init(&this->clock);
    genesis_cart_init(&this->cart);
    genesis_VDP_init(this); // must be after m68k init
    ym2612_init(&this->ym2612, OPN2V_ym2612, &this->clock.master_cycle_count);
    SN76489_init(&this->psg);
    snprintf(jsm->label, sizeof(jsm->label), "Sega Genesis");

    struct jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_z80;
    dt.read_trace_m68k = &read_trace_m68k;
    dt.ptr = (void*)this;
    M68k_setup_tracing(&this->m68k, &dt, &this->clock.master_cycle_count);
    Z80_setup_tracing(&this->z80, &dt, &this->clock.master_cycle_count);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &genesisJ_finish_frame;
    jsm->finish_scanline = &genesisJ_finish_scanline;
    jsm->step_master = &genesisJ_step_master;
    jsm->reset = &genesisJ_reset;
    jsm->load_BIOS = &genesisJ_load_BIOS;
    jsm->get_framevars = &genesisJ_get_framevars;
    jsm->play = &genesisJ_play;
    jsm->pause = &genesisJ_pause;
    jsm->stop = &genesisJ_stop;
    jsm->describe_io = &genesisJ_describe_io;
    jsm->set_audiobuf = &genesisJ_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &genesisJ_setup_debugger_interface;
}

void genesis_delete(JSM) {
    JTHIS;

    M68k_delete(&this->m68k);
    ym2612_delete(&this->ym2612);

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

static void genesisIO_load_cart(JSM, struct multi_file_set *mfs, struct buf* sram)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    genesis_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size);
    genesisJ_reset(jsm);
}

static void genesisIO_unload_cart(JSM)
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
    chan->sample_rate = (MASTER_CYCLES_PER_FRAME * 60) / (7 * 144); // ~55kHz
    printf("\nSAMPLERATE IS %d", chan->sample_rate);
    chan->low_pass_filter = 16000;
}

void genesisJ_describe_io(JSM, struct cvec *IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *c1 = cvec_push_back(this->jsm.IOs);
    struct physical_io_device *c2 = cvec_push_back(this->jsm.IOs);
    genesis_controller_3button_init(&this->controller1);
    genesis3_setup_pio(c1, 0, "Player 1", 1);
    genesis3_setup_pio(c2, 1, "Player 2", 0);
    this->controller1.pio = c1;
    this->controller2.pio = c2;

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    sprintf(b->name, "Reset");
    b->state = 0;

    // cartridge port
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &genesisIO_load_cart;
    d->cartridge_port.unload_cart = &genesisIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(1280 * 448 * 2);
    d->display.output[1] = malloc(1280 * 448 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_crt(&d->display);
    this->vdp.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    d->display.last_displayed = 1;
    this->vdp.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->vdp.display = &((struct physical_io_device *)cpg(this->vdp.display_ptr))->display;
    genesis_controllerport_connect(&this->io.controller_port1, genesis_controller_3button, &this->controller1);
    //genesis_controllerport_connect(&this->io.controller_port2, genesis_controller_3button, &this->controller2);
}

void genesisJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void genesisJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void genesisJ_play(JSM)
{
}

void genesisJ_pause(JSM)
{
}

void genesisJ_stop(JSM)
{
}

void genesisJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.vdp.hcount;
    out->scanline = this->clock.vdp.vcount;
    out->master_cycle = this->clock.master_cycle_count;
}

void genesisJ_reset(JSM)
{
    JTHIS;
    Z80_reset(&this->z80);
    M68k_reset(&this->m68k);
    SN76489_reset(&this->psg);
    ym2612_reset(&this->ym2612);
    genesis_clock_reset(&this->clock);
    genesis_VDP_reset(this);
    this->io.z80.reset_line = 1;
    this->io.z80.reset_line_count = 500;
    this->io.z80.bus_request = this->io.z80.bus_ack = 1;
    this->io.m68k.VDP_FIFO_stall = 0;
    this->io.m68k.VDP_prefetch_stall = 0;
    printf("\nGenesis reset!");
}


void genesisJ_killall(JSM)
{

}

u32 genesisJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        genesisJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->vdp.display->last_written;
}

u32 genesisJ_finish_scanline(JSM)
{
    JTHIS;
    genesisJ_step_master(jsm, MASTER_CYCLES_PER_SCANLINE);
    /*i32 cpu_step = (i32)this->clock.timing.cpu_divisor;
    i64 ppu_step = (i64)this->clock.timing.ppu_divisor;
    i32 done = 0;
    i32 start_y = this->clock.ppu_y;
    while (this->clock.ppu_y == start_y) {
        this->clock.master_clock += cpu_step;
        //this->apu.cycle(this->clock.master_clock);
        r2A03_run_cycle(&this->cpu);
        this->bus.cycle(this);
        this->clock.cpu_frame_cycle++;
        this->clock.cpu_master_clock += cpu_step;
        i64 ppu_left = (i64)this->clock.master_clock - (i64)this->clock.ppu_master_clock;
        done = 0;
        while (ppu_left >= ppu_step) {
            ppu_left -= ppu_step;
            done++;
        }
        genesis_PPU_cycle(&this->ppu, done);
        this->cycles_left -= cpu_step;
        if (dbg.do_break) break;
    }*/
    return 0;
}

static float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}

static void sample_audio(struct genesis* this)
{
    if (this->audio.buf && (this->clock.master_cycle_count >= (u64)this->audio.next_sample_cycle)) {
        this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
        if (this->audio.buf->upos < this->audio.buf->samples_len) {
            i32 v = ((i32)SN76489_mix_sample(&this->psg, 0) + (i32)this->ym2612.mix.mono_output) / 2;
            //v = SN76489_mix_sample(&this->psg, 0);
            ((float *)this->audio.buf->ptr)[this->audio.buf->upos] = i16_to_float((i16)v);
        }
        this->audio.buf->upos++;
    }
}

static void debug_audio(struct genesis* this)
{
    /* PSG */
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.main);
    if (this->clock.master_cycle_count >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            //printf("\nSAMPLE AT %lld next:%f stride:%f", this->clock.master_cycles, dw->user.next_sample_cycle, dw->user.cycle_stride);
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(SN76489_mix_sample(&this->psg, 1));
            dw->user.buf_pos++;
        }
    }

    dw = cpg(this->dbg.waveforms_psg.chan[0]);
    if (this->clock.master_cycle_count >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 4; j++) {
            dw = cpg(this->dbg.waveforms_psg.chan[j]);
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                i16 sv = SN76489_sample_channel(&this->psg, j);
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv * 4);
                dw->user.buf_pos++;
            }
        }
    }

    /* YM2612 */
    dw = cpg(this->dbg.waveforms_ym2612.main);
    if (this->clock.master_cycle_count >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            //printf("\nSAMPLE AT %lld next:%f stride:%f", this->clock.master_cycles, dw->user.next_sample_cycle, dw->user.cycle_stride);
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(this->ym2612.mix.mono_output);
            dw->user.buf_pos++;
        }
    }

    dw = cpg(this->dbg.waveforms_ym2612.chan[0]);
    if (this->clock.master_cycle_count >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 6; j++) {
            dw = cpg(this->dbg.waveforms_ym2612.chan[j]);
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                i16 sv = ym2612_sample_channel(&this->ym2612, j);
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv);
                dw->user.buf_pos++;
            }
        }
    }

}

#define MIN(a,b) ((a) < (b) ? (a) : (b))

u32 genesisJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->clock.mem_break = 0;
    //this->jsm.cycles_left += howmany;
    this->jsm.cycles_left = howmany;
    while (this->jsm.cycles_left >= 0) {
        i32 biggest_step = MIN(MIN(MIN(this->clock.vdp.cycles_til_clock, this->clock.m68k.cycles_til_clock), this->clock.z80.cycles_til_clock), this->clock.psg.cycles_til_clock);
        this->jsm.cycles_left -= biggest_step;
        this->clock.master_cycle_count += biggest_step;
        this->clock.m68k.cycles_til_clock -= biggest_step;
        this->clock.z80.cycles_til_clock -= biggest_step;
        this->clock.vdp.cycles_til_clock -= biggest_step;
        this->clock.psg.cycles_til_clock -= biggest_step;
        if (this->clock.m68k.cycles_til_clock <= 0) {
            this->clock.m68k.cycles_til_clock += this->clock.m68k.clock_divisor;
            genesis_cycle_m68k(this);
            ym2612_cycle(&this->ym2612);
        }
        if (this->clock.z80.cycles_til_clock <= 0) {
            this->clock.z80.cycles_til_clock += this->clock.z80.clock_divisor;
            genesis_cycle_z80(this);
        }
        if (this->clock.vdp.cycles_til_clock <= 0) {
            genesis_VDP_cycle(this);
            this->clock.vdp.cycles_til_clock += this->clock.vdp.clock_divisor;
        }
        if (this->clock.psg.cycles_til_clock <= 0) {
            SN76489_cycle(&this->psg);
            this->clock.psg.cycles_til_clock += this->clock.psg.clock_divisor;
        }
        sample_audio(this);
        debug_audio(this);

        if (dbg.do_break) break;
    }
    return 0;
}

void genesisJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nSega Genesis doesn't have a BIOS...?");
}

