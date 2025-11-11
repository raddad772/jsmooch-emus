//
// Created by . on 12/4/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "galaksija.h"
#include "galaksija_bus.h"
#include "galaksija_debugger.h"

#include "fail"
#include "helpers/physical_io.h"
#include "helpers/multisize_memaccess.c"

#define JTHIS struct galaksija* this = (galaksija*)jsm->ptr
#define JSM struct jsm_system* jsm

static void galaksijaJ_play(JSM);
static void galaksijaJ_pause(JSM);
static void galaksijaJ_stop(JSM);
static void galaksijaJ_get_framevars(JSM, framevars* out);
static void galaksijaJ_reset(JSM);
static u32 galaksijaJ_finish_frame(JSM);
static u32 galaksijaJ_finish_scanline(JSM);
static u32 galaksijaJ_step_master(JSM, u32 howmany);
static void galaksijaJ_load_BIOS(JSM, multi_file_set* mfs);
static void galaksijaJ_describe_io(JSM, cvec* IOs);

// 192x320 incl. hblank
// 128x208 not inc;.
#define MASTER_CYCLES_PER_FRAME 122880
#define MASTER_CYCLES_PER_LINE 384
#define MASTER_CYCLES_PER_SECOND 6144000
#define CRT_WIDTH 384

static u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

static void setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void galaksijaJ_set_audiobuf(jsm_system* jsm, audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / (float)ab->samples_len));
        this->audio.next_sample_cycle = 0;
        struct debug_waveform *wf = (debug_waveform *)cvec_get(this->dbg.waveforms.main.vec, this->dbg.waveforms.main.index);
    }

    // PSG
    /*setup_debug_waveform(cvec_get(this->dbg.waveforms.main.vec, this->dbg.waveforms.main.index));
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(cvec_get(this->dbg.waveforms.chan[i].vec, this->dbg.waveforms.chan[i].index));
        struct debug_waveform *wf = (debug_waveform *)cvec_get(this->dbg.waveforms.chan[i].vec, this->dbg.waveforms.chan[i].index);
        if (i < 4)
            this->apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            this->apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }
    */
}

static u32 read_trace_cpu(void *ptr, u32 addr) {
    struct galaksija* this = (galaksija*)ptr;
    return galaksija_mainbus_read(this, addr, this->io.open_bus, 0);
}

void galaksija_new(jsm_system *jsm)
{
    struct galaksija* this = (galaksija*)malloc(sizeof(galaksija));
    memset(this, 0, sizeof(*this));
    Z80_init(&this->z80, 0);
    galaksija_bus_init(this);

    snprintf(jsm->label, sizeof(jsm->label), "Galaksija");
    struct jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_cpu;
    dt.ptr = this;
    Z80_setup_tracing(&this->z80, &dt, &this->clock.master_cycle_count);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &galaksijaJ_finish_frame;
    jsm->finish_scanline = &galaksijaJ_finish_scanline;
    jsm->step_master = &galaksijaJ_step_master;
    jsm->reset = &galaksijaJ_reset;
    jsm->load_BIOS = &galaksijaJ_load_BIOS;
    jsm->get_framevars = &galaksijaJ_get_framevars;
    jsm->play = &galaksijaJ_play;
    jsm->pause = &galaksijaJ_pause;
    jsm->stop = &galaksijaJ_stop;
    jsm->describe_io = &galaksijaJ_describe_io;
    jsm->set_audiobuf = NULL; //&galaksijaJ_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &galaksijaJ_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;

}

void galaksija_delete(jsm_system *jsm)
{
    JTHIS;

    while (cvec_len(this->jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->jsm.IOs);
        if (pio->kind == HID_AUDIO_CASSETTE) {
            if (pio->audio_cassette.remove_tape) pio->audio_cassette.remove_tape(jsm);
        }
        physical_io_device_delete(pio);
    }

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

u32 galaksijaJ_finish_frame(JSM)
{
    JTHIS;

    u64 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        galaksijaJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->crt.display->last_written;
}

void galaksijaJ_play(JSM)
{
}

void galaksijaJ_pause(JSM)
{
}

void galaksijaJ_stop(JSM)
{
}

void galaksijaJ_get_framevars(JSM, framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->crt.x;
    out->scanline = this->crt.y;
    out->master_cycle = this->clock.master_cycle_count;
}

void galaksijaJ_reset(JSM)
{
    JTHIS;
    Z80_reset(&this->z80);
    this->crt.x = 0;
    this->crt.y = 0;

    printf("\ngalaksija reset!");
}

static void sample_audio(galaksija* this, u32 num_cycles)
{
    return;
    /*for (u64 i = 0; i < num_cycles; i++) {
        galaksija_APU_cycle(this);
        u64 mc = this->clock.master_cycle_count + i;
        if (this->audio.buf && (mc >= (u64) this->audio.next_sample_cycle)) {
            this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
            if (this->audio.buf->upos < this->audio.buf->samples_len) {
                ((float *)(this->audio.buf->ptr))[this->audio.buf->upos] = galaksija_APU_mix_sample(this, 0);
            }
            this->audio.buf->upos++;
        }

        struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = galaksija_APU_mix_sample(this, 1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(this->dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(this->dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = galaksija_APU_sample_channel(this, j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }
    */
}

u32 galaksijaJ_finish_scanline(JSM)
{
    JTHIS;
    u32 cycles_left = MASTER_CYCLES_PER_LINE - this->crt.x;
    galaksijaJ_step_master(jsm, cycles_left);
    return 0;
}

static u32 galaksijaJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    for (u32 i = 0; i < howmany; i++) {
        galaksija_cycle(this);
        if (dbg.do_break) break;
    }
    return 0;
}

static void galaksijaJ_load_BIOS(JSM, multi_file_set* mfs)
{
    JTHIS;
    // File 0, chargen
    // File 1, ROM A
    // File 2, if exist, ROMB
    this->ROMB_present = mfs->num_files > 2;
    memcpy(this->ROM_char, mfs->files[0].buf.ptr, 2048);
    memcpy(this->ROMA, mfs->files[1].buf.ptr, 4096);
    if (mfs->num_files > 2) memcpy(this->ROMB, mfs->files[2].buf.ptr, 4096);
}

static void setup_crt(JSM_DISPLAY *d)
{
    d->standard = JSS_CRT;
    d->enabled = 1;

    d->fps = 50;
    d->fps_override_hint = 50;
    // 240x160, but 308x228 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = CRT_WIDTH;
    d->pixelometry.cols.max_visible = CRT_WIDTH;
    d->pixelometry.cols.right_hblank = 0;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 320;
    d->pixelometry.rows.max_visible = 320;
    d->pixelometry.rows.bottom_vblank = 0;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 1;
    d->geometry.physical_aspect_ratio.height = 1;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_audio(cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = MASTER_CYCLES_PER_SECOND;
    chan->low_pass_filter = 24000;
}

#define NUMKEYS 53
static const u32 galaksija_keyboard_keymap[NUMKEYS] = {
      JK_A, JK_B, JK_C, JK_D, JK_E, JK_F, JK_G,
        JK_H, JK_I, JK_J, JK_K, JK_L, JK_M, JK_N, JK_O,
        JK_P, JK_Q, JK_R, JK_S, JK_T, JK_U, JK_V, JK_W,
        JK_X, JK_Y, JK_Z, JK_UP, JK_DOWN, JK_LEFT, JK_RIGHT, JK_SPACE,
        JK_0, JK_1, JK_2, JK_3, JK_4, JK_5, JK_6, JK_7,
        JK_8, JK_9, JK_SEMICOLON, //41
        JK_COMMA, // 42
        JK_EQUALS, // 43
        JK_DOT, // 44
        JK_SLASH_FW, // 45
        JK_ENTER, // 46
        JK_BACKSPACE, // 47
        JK_SHIFT, // 48
        JK_F1, // 49
        JK_F2, // 50
        JK_QUOTE, // 51
        JK_ESC // 52
};

static void setup_keyboard(galaksija* this)
{
    struct physical_io_device *d = cvec_push_back(this->jsm.IOs);
    physical_io_device_init(d, HID_KEYBOARD, 0, 0, 1, 1);

    d->id = 0;
    d->kind = HID_KEYBOARD;
    d->connected = 1;
    d->enabled = 1;

    struct JSM_KEYBOARD* kbd = &d->keyboard;
    memset(kbd, 0, sizeof(JSM_KEYBOARD));
    kbd->num_keys = NUMKEYS;

    for (u32 i = 0; i < NUMKEYS; i++) {
        kbd->key_defs[i] = galaksija_keyboard_keymap[i];
    }
}


static void galaksijaJ_describe_io(JSM, cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    setup_keyboard(this);

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;

    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    /*physical_io_device_init(d, HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &galaksijaIO_load_cart;
    d->cartridge_port.unload_cart = &galaksijaIO_unload_cart;*/

    // screen
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(384 * 320);
    d->display.output[1] = malloc(384 * 320);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_crt(&d->display);
    this->crt.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    this->crt.cur_output = (u8 *)(d->display.output[0]);

    setup_audio(IOs);

    this->crt.display = &((physical_io_device *)cpg(this->crt.display_ptr))->display;
}
