//
// Created by Dave on 2/7/2024.
//

#include "assert.h"
#include "stdlib.h"
#include <cstring>
#include "stdio.h"

#include "fail"
#include "sms_gg.h"
#include "sms_gg_clock.h"
#include "sms_gg_io.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_vdp.h"
#include "smsgg_debugger.h"
#include "sms_gg_serialize.h"

#define JTHIS struct SMSGG* this = (SMSGG*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct SMSGG* this

static float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}

void SMSGGJ_play(JSM) {}
void SMSGGJ_pause(JSM) {}
void SMSGGJ_stop(JSM) {}
void SMSGGJ_get_framevars(JSM, framevars* out);
void SMSGGJ_reset(JSM);
void SMSGGJ_killall(JSM);
u32 SMSGGJ_finish_frame(JSM);
u32 SMSGGJ_finish_scanline(JSM);
u32 SMSGGJ_step_master(JSM, u32 howmany);
void SMSGGJ_load_BIOS(JSM, multi_file_set* mfs);
void SMSGGJ_enable_tracing(JSM);
void SMSGGJ_disable_tracing(JSM);
void SMSGGJ_describe_io(JSM, cvec *IOs);
static void SMSGGIO_unload_cart(JSM);
static void SMSGGIO_load_cart(JSM, multi_file_set *mfs, physical_io_device *pio);


u32 SMSGG_CPU_read_trace(void *ptr, u32 addr)
{
    struct SMSGG* this = (SMSGG*)ptr;
    return SMSGG_bus_read(this, addr, 0);
}

#define MASTER_CYCLES_PER_FRAME 179208
static void setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void SMSGGJ_set_audiobuf(jsm_system* jsm, audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)MASTER_CYCLES_PER_FRAME / (float)ab->samples_len);
        this->audio.next_sample_cycle = 0;
        struct debug_waveform *wf = (debug_waveform *)cpg(this->dbg.waveforms.main);
        this->sn76489.ext_enable = wf->ch_output_enabled;
    }
    struct debug_waveform *wf = cpg(this->dbg.waveforms.main);
    setup_debug_waveform(wf);
    if (wf->clock_divider == 0) wf->clock_divider = wf->default_clock_divider;
    this->clock.apu_divisor = wf->clock_divider;
    for (u32 i = 0; i < 4; i++) {
        wf = (debug_waveform *)cpg(this->dbg.waveforms.chan[i]);
        setup_debug_waveform(wf);
        if (i < 3) {
            this->sn76489.sw[i].ext_enable = wf->ch_output_enabled;
        }
        else
            this->sn76489.noise.ext_enable = wf->ch_output_enabled;
    }
}

void SMSGG_new(jsm_system* jsm, enum jsm::systems variant, enum jsm_regions region) {
    struct SMSGG* this = (SMSGG*)malloc(sizeof(SMSGG));
    memset(this, 0, sizeof(SMSGG));
    SMSGG_clock_init(&this->clock, variant, region);
    SMSGG_VDP_init(&this->vdp, this, variant);
    SMSGG_mapper_sega_init(&this->mapper, variant);
    SMSGG_io_init(this);
    Z80_init(&this->cpu, 0);

    // setup tracing reads
    struct jsm_debug_read_trace a;
    a.ptr = (void *)this;
    a.read_trace = &SMSGG_CPU_read_trace;
    Z80_setup_tracing(&this->cpu, &a, &this->clock.master_cycles);

    // bus init
    SMSGG_gamepad_init(&this->io.controllerA, variant, 1);
    SMSGG_controller_port_init(&this->io.portA, variant, 1);
    SMSGG_controller_port_init(&this->io.portB, variant, 2);
    this->io.portA.attached_device = (void *)&this->io.controllerA;

    this->io.disable = 0;
    this->io.gg_start = 0;

    switch(variant) {
        case jsm::systems::SG1000:
            this->cpu_in = &SMSGG_bus_cpu_in_sms1;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Game 1000");
            break;
        case jsm::systems::SMS1:
            this->cpu_in = &SMSGG_bus_cpu_in_sms1;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Master System");
            break;
        case jsm::systems::SMS2:
            this->cpu_in = &SMSGG_bus_cpu_in_sms1;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Master System II");
            break;
        case jsm::systems::GG:
            this->cpu_in = &SMSGG_bus_cpu_in_gg;
            this->cpu_out = &SMSGG_bus_cpu_out_sms1;
            snprintf(jsm->label, sizeof(jsm->label), "Sega Game Gear");
            break;
        default:
            assert(1!=0);
            return;
    }

    // bus init done
    this->last_frame = 0;

    this->audio.buf = NULL;

    SMSGG_VDP_reset(&this->vdp);
    SN76489_reset(&this->sn76489);

    this->described_inputs = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &SMSGGJ_finish_frame;
    jsm->finish_scanline = &SMSGGJ_finish_scanline;
    jsm->step_master = &SMSGGJ_step_master;
    jsm->reset = &SMSGGJ_reset;
    jsm->load_BIOS = &SMSGGJ_load_BIOS;
    jsm->get_framevars = &SMSGGJ_get_framevars;
    jsm->play = &SMSGGJ_play;
    jsm->pause = &SMSGGJ_pause;
    jsm->stop = &SMSGGJ_stop;
    jsm->describe_io = &SMSGGJ_describe_io;
    jsm->sideload = NULL;
    jsm->set_audiobuf = &SMSGGJ_set_audiobuf;
    jsm->setup_debugger_interface = &SMSGGJ_setup_debugger_interface;
    jsm->save_state = &SMSGGJ_save_state;
    jsm->load_state = &SMSGGJ_load_state;
    //SMSGGJ_reset(jsm);
}

static void new_button(JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}



static void setup_crt_sms(JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps = 59.922743;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 12;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.max_visible = 256;
    d->pixelometry.cols.right_hblank = 74;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 192;
    d->pixelometry.rows.max_visible = 192;
    d->pixelometry.rows.bottom_vblank = 70;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 8;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

void setup_lcd_gg(JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

    d->fps = 59.922751;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 12;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.max_visible = 256;
    d->pixelometry.cols.right_hblank = 74;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 192;
    d->pixelometry.rows.max_visible = 192;
    d->pixelometry.rows.bottom_vblank = 70;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4; // 69.4mm
    d->geometry.physical_aspect_ratio.height = 3; // 53.24mm

    // displays only the inner 160x144 of 256x192   -96x-48   so -48x-24
    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 48;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 24;
}

static void setup_audio(cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = (MASTER_CYCLES_PER_FRAME * 60) / 48;
    chan->low_pass_filter = 16000;
}

void SMSGGJ_describe_io(JSM, cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    struct physical_io_device *d = cvec_push_back(this->IOs);
    SMSGG_gamepad_setup_pio(d, 0, "Player A", 1, 1);
    this->io.controllerA.device_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    if (this->variant != jsm::systems::GG) {
        d = cvec_push_back(this->IOs);
        SMSGG_gamepad_setup_pio(d, 1, "Player B", 0, 0);
        this->io.controllerB.device_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    }

    // power, reset, and pause buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    if (this->variant != jsm::systems::GG) {
        b = cvec_push_back(&chassis->chassis.digital_buttons);
        b->common_id = DBCID_ch_reset;
        snprintf(b->name, sizeof(b->name), "Reset");
        b->state = 0;
    }

    this->io.pause_button = NULL;

    if (this->variant != jsm::systems::GG) {
        b = cvec_push_back(&chassis->chassis.digital_buttons);
        b->common_id = DBCID_ch_pause;
        snprintf(b->name, sizeof(b->name), "Pause");
        b->state = 0;

        this->io.pause_button = b;
    }

    // cartridge port
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &SMSGGIO_load_cart;
    d->cartridge_port.unload_cart = &SMSGGIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(SMSGG_DISPLAY_DRAW_SZ);
    d->display.output[1] = malloc(SMSGG_DISPLAY_DRAW_SZ);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    this->vdp.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    if (this->variant == jsm::systems::GG)
        setup_lcd_gg(&d->display);
    else
        setup_crt_sms(&d->display);

    this->vdp.display = NULL;
    this->vdp.cur_output = (u16 *)d->display.output[0];
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    // Audio
    setup_audio(IOs);

    struct physical_io_device *pio = cpg(this->vdp.display_ptr);
    this->vdp.display = &pio->display;
}

void SMSGG_delete(jsm_system* jsm)
{
    JTHIS;
    SMSGG_mapper_sega_delete(&this->mapper);

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

void SMSGGJ_get_framevars(JSM, framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.frames_since_restart;
    out->x = this->clock.hpos;
    out->scanline = this->clock.vpos;
}

void SMSGGJ_reset(JSM)
{
    JTHIS;
    Z80_reset(&this->cpu);
    SMSGG_VDP_reset(&this->vdp);
    SMSGG_mapper_sega_reset(&this->mapper);
    SN76489_reset(&this->sn76489);
    this->last_frame = 0;
}

void SMSGGJ_killall(JSM)
{
}

u32 SMSGGJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.frames_since_restart;
    u32 scanlines_done = 0;
    this->clock.cpu_frame_cycle = 0;
    while(current_frame == this->clock.frames_since_restart) {
        scanlines_done++;
        SMSGGJ_finish_scanline(jsm);
        if (dbg.do_break) return this->vdp.display->last_written;
    }
    return this->vdp.display->last_written;
}

u32 SMSGGJ_finish_scanline(JSM)
{
    JTHIS;
    u32 cycles_left = 684 - (this->clock.hpos * 2);
    SMSGGJ_step_master(jsm, cycles_left);
    return 0;
}

static void cpu_cycle(SMSGG* this)
{
    if (this->cpu.pins.RD) {
        if (this->cpu.pins.MRQ) {// read ROM/RAM
            this->cpu.pins.D = SMSGG_bus_read(this, this->cpu.pins.Addr, 1);
#ifndef LYCODER
            if (dbg.trace_on) {
                // Z80(    25)r   0006   $18     TCU:1
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, this->cpu.trace_cycles, trace_format_read('Z80', Z80_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D, null, this->cpu.regs.TCU));
            }
#endif
        } else if (this->cpu.pins.IO && (this->cpu.pins.M1 == 0)) { // read IO port
            //RD=1, IO=1, M1=1 means "IRQ ack" so we ignore that here
            this->cpu.pins.D = this->cpu_in(this, this->cpu.pins.Addr, this->cpu.pins.D, 1);
#ifndef LYCODER
            if (dbg.trace_on)
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
        }
    }
    if (this->cpu.pins.WR) {
        if (this->cpu.pins.MRQ) { // write RAM
            if (dbg.trace_on && (this->cpu.trace.last_cycle != *this->cpu.trace.cycles)) {
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, this->cpu.trace_cycles, trace_format_write('Z80', Z80_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D));
                this->cpu.trace.last_cycle = *this->cpu.trace.cycles;
#ifndef LYCODER
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
            }
            SMSGG_bus_write(this, this->cpu.pins.Addr, this->cpu.pins.D);
        } else if (this->cpu.pins.IO) {
            this->cpu_out(this, this->cpu.pins.Addr, this->cpu.pins.D);
#ifndef LYCODER
            if (dbg.trace_on)
                dbg_printf(DBGC_Z80 "\nSMS(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
#endif
        }
    }
    Z80_cycle(&this->cpu);
    this->clock.cpu_frame_cycle += this->clock.cpu_divisor;
}

static void SMSGG_notify_NMI(SMSGG* this, u32 level)
{
    Z80_notify_NMI(&this->cpu, level);
}

void SMSGG_bus_notify_IRQ(SMSGG* this, u32 level)
{
    Z80_notify_IRQ(&this->cpu, level);
}

static void poll_pause(SMSGG* this)
{
    if (this->variant != jsm::systems::GG)
        SMSGG_notify_NMI(this, this->io.pause_button->state);
}

static void sample_audio(SMSGG* this)
{
    if (this->audio.buf && (this->clock.master_cycles >= (u64)this->audio.next_sample_cycle)) {
        this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
        float *sptr = ((float *)this->audio.buf->ptr) + (this->audio.buf->upos);
        //assert(this->audio.buf->upos < this->audio.buf->samples_len);
        if (this->audio.buf->upos >= this->audio.buf->samples_len) {
            this->audio.buf->upos++;
        }
        else {
            *sptr = i16_to_float(SN76489_mix_sample(&this->sn76489, 0));
            this->audio.buf->upos++;
        }
    }
}

static void debug_audio(SMSGG* this)
{
    struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
    if (this->clock.master_cycles >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            //printf("\nSAMPLE AT %lld next:%f stride:%f", this->clock.master_cycles, dw->user.next_sample_cycle, dw->user.cycle_stride);
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(SN76489_mix_sample(&this->sn76489, 1));
            dw->user.buf_pos++;
        }
    }

    dw = cpg(this->dbg.waveforms.chan[0]);
    if (this->clock.master_cycles >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 4; j++) {
            dw = cpg(this->dbg.waveforms.chan[j]);
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                i16 sv = SN76489_sample_channel(&this->sn76489, j);
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv * 4);
                dw->user.buf_pos++;
                assert(dw->user.buf_pos < 410);
            }
        }
    }
}

u32 SMSGGJ_step_master(JSM, u32 howmany) {
    JTHIS;
#ifdef LYCODER
    do {
#else
        for (u32 i = 0; i < howmany; i++) {
#endif
        if (this->clock.frames_since_restart != this->last_frame) {
            this->last_frame = this->clock.frames_since_restart;
            poll_pause(this);
        }
        this->clock.cpu_master_clock++;
        this->clock.vdp_master_clock++;
        this->clock.apu_master_clock += 1;
        if (this->clock.cpu_master_clock > this->clock.cpu_divisor) {
            cpu_cycle(this);
            this->clock.cpu_master_clock -= this->clock.cpu_divisor;
        }
        if (this->clock.vdp_master_clock > this->clock.vdp_divisor) {
            SMSGG_VDP_cycle(&this->vdp);
            this->clock.vdp_master_clock -= this->clock.vdp_divisor;
        }
        if (this->clock.apu_master_clock > this->clock.apu_divisor) {
            SN76489_cycle(&this->sn76489);
            this->clock.apu_master_clock -= this->clock.apu_divisor;
        }
        if (this->clock.frames_since_restart != this->last_frame) {
            this->last_frame = this->clock.frames_since_restart;
            poll_pause(this);
        }
#ifdef LYCODER
        if (*this->cpu.trace.cycles > 8000000) break;
#endif
        this->clock.master_cycles++;

        sample_audio(this);
        debug_audio(this);
        if (dbg.do_break) break;
#ifdef LYCODER
    } while (true);
#else
    }
#endif
    return 0;
}

void SMSGGJ_load_BIOS(JSM, multi_file_set* mfs) {
    JTHIS;
    SMSGG_mapper_load_BIOS_from_RAM(&this->mapper, &mfs->files[0].buf);
}

static void SMSGGIO_unload_cart(JSM)
{

}

static void SMSGGIO_load_cart(JSM, multi_file_set *mfs, physical_io_device *pio) {
    JTHIS;
    struct buf *b = &mfs->files[0].buf;
    SMSGG_mapper_load_ROM_from_RAM(&this->mapper, b);
    SMSGGJ_reset(jsm);
}
