//
// Created by . on 12/4/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nds.h"
#include "nds_bus.h"
#include "nds_debugger.h"
#include "nds_dma.h"
#include "nds_timers.h"

#include "helpers/debugger/debugger.h"
#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.c"

#define JTHIS struct NDS* this = (struct NDS*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void NDSJ_play(JSM);
static void NDSJ_pause(JSM);
static void NDSJ_stop(JSM);
static void NDSJ_get_framevars(JSM, struct framevars* out);
static void NDSJ_reset(JSM);
static u32 NDSJ_finish_frame(JSM);
static u32 NDSJ_finish_scanline(JSM);
static u32 NDSJ_step_master(JSM, u32 howmany);
static void NDSJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void NDSJ_describe_io(JSM, struct cvec* IOs);


// in ARM7 (33MHz) cycles.
#define MASTER_CYCLES_PER_SCANLINE 2132
#define HBLANK_CYCLES 594
#define MASTER_CYCLES_BEFORE_HBLANK 1538
#define MASTER_CYCLES_PER_FRAME 570716


// master cycles per second: 33554432 (ARM7)
//59.8261 = 560866 per frame
/*   355 x 263
 * = 2132 cycles per line
 *
H-Timing: 256 dots visible, 99 dots blanking, 355 dots total (15.7343KHz)
V-Timing: 192 lines visible, 71 lines blanking, 263 lines total (59.8261 Hz)
The V-Blank cycle for the 3D Engine consists of the 23 lines, 191..213. *
 */

#define SCANLINE_HBLANK 1006

static u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    // reload value is 0xFD
    // 0xFD ^ 0xFF = 2
    // How many ticks til 0x100? 256 - 253 = 3, correct!
    // 100. 256 - 100 = 156, correct!
    // Unfortunately if we set 0xFFFF, we need 0x1000 tiks...
    // ok but what about when we set 255? 256 - 255 = 1 which is wrong
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}

static void setup_debug_waveform(struct debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void NDSJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    /*if (this->audio.master_cycles_per_audio_sample == 0) {
        this->audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / (float)ab->samples_len));
        this->audio.next_sample_cycle = 0;
        struct debug_waveform *wf = (struct debug_waveform *)cvec_get(this->dbg.waveforms.main.vec, this->dbg.waveforms.main.index);
        this->apu.ext_enable = wf->ch_output_enabled;
    }

    // PSG
    setup_debug_waveform(cvec_get(this->dbg.waveforms.main.vec, this->dbg.waveforms.main.index));
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(cvec_get(this->dbg.waveforms.chan[i].vec, this->dbg.waveforms.chan[i].index));
        struct debug_waveform *wf = (struct debug_waveform *)cvec_get(this->dbg.waveforms.chan[i].vec, this->dbg.waveforms.chan[i].index);
        if (i < 4)
            this->apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            this->apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }*/

}

static u32 read_trace_cpu9(void *ptr, u32 addr, u32 sz) {
    struct NDS* this = (struct NDS*)ptr;
    return NDS_mainbus_read9(this, addr, sz, 0, 0);
}

static u32 read_trace_cpu7(void *ptr, u32 addr, u32 sz) {
    struct NDS* this = (struct NDS*)ptr;
    return NDS_mainbus_read7(this, addr, sz, 0, 0);
}

void NDS_new(struct jsm_system *jsm)
{
    struct NDS* this = (struct NDS*)malloc(sizeof(struct NDS));
    memset(this, 0, sizeof(*this));
    ARM7TDMI_init(&this->arm7, &this->waitstates.current_transaction);
    this->arm7.read_ptr = this;
    this->arm7.write_ptr = this;
    this->arm7.read = &NDS_mainbus_read7;
    this->arm7.write = &NDS_mainbus_write7;
    this->arm7.fetch_ptr = this;
    this->arm7.fetch_ins = &NDS_mainbus_fetchins7;

    ARM946ES_init(&this->arm9, &this->waitstates.current_transaction);
    this->arm9.read_ptr = this;
    this->arm9.read = &NDS_mainbus_read9;

    this->arm9.write_ptr = this;
    this->arm9.write = &NDS_mainbus_write9;

    this->arm9.fetch_ptr = this;
    this->arm9.fetch_ins = &NDS_mainbus_fetchins9;

    NDS_bus_init(this);
    NDS_clock_init(&this->clock);
    //NDS_cart_init(&this->cart);
    NDS_PPU_init(this);
    //NDS_APU_init(this);

    snprintf(jsm->label, sizeof(jsm->label), "Nintendo DS");
    struct jsm_debug_read_trace dt7;
    dt7.read_trace_arm = &read_trace_cpu7;
    dt7.ptr = this;
    ARM7TDMI_setup_tracing(&this->arm7, &dt7, &this->clock.master_cycle_count7, 1);

    struct jsm_debug_read_trace dt9;
    dt9.read_trace_arm = &read_trace_cpu9;
    dt9.ptr = this;
    ARM946ES_setup_tracing(&this->arm9, &dt9, &this->clock.master_cycle_count9, 2);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &NDSJ_finish_frame;
    jsm->finish_scanline = &NDSJ_finish_scanline;
    jsm->step_master = &NDSJ_step_master;
    jsm->reset = &NDSJ_reset;
    jsm->load_BIOS = &NDSJ_load_BIOS;
    jsm->get_framevars = &NDSJ_get_framevars;
    jsm->play = &NDSJ_play;
    jsm->pause = &NDSJ_pause;
    jsm->stop = &NDSJ_stop;
    jsm->describe_io = &NDSJ_describe_io;
    jsm->set_audiobuf = &NDSJ_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &NDSJ_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;
    
}

void NDS_delete(struct jsm_system *jsm)
{
    JTHIS;

    ARM7TDMI_delete(&this->arm7);
    ARM946ES_delete(&this->arm9);
    //NDS_PPU_delete(this);
    //NDS_cart_delete(&this->cart);

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

u32 NDSJ_finish_frame(JSM)
{
    JTHIS;

    u64 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        NDSJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    struct debug_waveform *dw = cpg(this->dbg.waveforms.chan[4]);
    return this->ppu.display->last_written;
}

void NDSJ_play(JSM)
{
}

void NDSJ_pause(JSM)
{
}

void NDSJ_stop(JSM)
{
}

void NDSJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.ppu.x;
    out->scanline = this->clock.ppu.y;
    out->master_cycle = this->clock.master_cycle_count7;
}

static void skip_BIOS(struct NDS* this)
{
    printf("\nSKIP NDS BIOS!");
    assert(1==2);
    dbg_break("NOT SUPPORT SKIP BIOS!", this->clock.master_cycle_count7);

}

void NDSJ_reset(JSM)
{
    JTHIS;
    ARM7TDMI_reset(&this->arm7);
    ARM946ES_reset(&this->arm9);
    NDS_clock_reset(&this->clock);
    NDS_PPU_reset(this);

    //skip_BIOS(this);
    printf("\nNDS reset!");
}


static void sample_audio(struct NDS* this, u32 num_cycles)
{
/*    for (u64 i = 0; i < num_cycles; i++) {
        NDS_APU_cycle(this);
        u64 mc = this->clock.master_cycle_count + i;
        if (this->audio.buf && (mc >= (u64) this->audio.next_sample_cycle)) {
            this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
            if (this->audio.buf->upos < this->audio.buf->samples_len) {
                ((float *)(this->audio.buf->ptr))[this->audio.buf->upos] = NDS_APU_mix_sample(this, 0);
            }
            this->audio.buf->upos++;
        }

        struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = NDS_APU_mix_sample(this, 1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(this->dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(this->dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = NDS_APU_sample_channel(this, j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }*/
}

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

static i64 run_arm7(struct NDS *this, i64 num_cycles)
{
    this->waitstates.current_transaction = 0;
    // Run DMA & CPU
    if (NDS_dma7_go(this)) {
    }
    else {
        ARM7TDMI_run(&this->arm7);
    }
    NDS_tick_timers7(this, this->waitstates.current_transaction);
    this->clock.master_cycle_count7 += this->waitstates.current_transaction;

    return this->waitstates.current_transaction;
}

static i64 run_arm9(struct NDS *this, i64 num_cycles)
{
    this->waitstates.current_transaction = 0;
    // Run DMA & CPU
    if (NDS_dma7_go(this)) {
    }
    else {
        ARM946ES_run(&this->arm9);
    }
    NDS_tick_timers9(this, this->waitstates.current_transaction);
    this->clock.master_cycle_count7 += this->waitstates.current_transaction;

    return this->waitstates.current_transaction;
}

static void run_system(struct NDS *this, u64 num_cycles)
{
    this->clock.cycles7 += (i64)num_cycles;
    this->clock.cycles9 += (i64)num_cycles;
    while((this->clock.cycles7 > 0) || (this->clock.cycles9 > 0)) {
        if (this->clock.cycles7 > 0) {
            this->clock.cycles7 -= run_arm7(this, MIN(16, this->clock.cycles7));
        }

        if (this->clock.cycles9 > 0) {
            this->clock.cycles9 -= run_arm9(this, MIN(16, this->clock.cycles9));
        }
    }

}

u32 NDSJ_finish_scanline(JSM)
{
    JTHIS;
    NDS_PPU_start_scanline(this);
    if (dbg.do_break) return 0;
    run_system(this, MASTER_CYCLES_BEFORE_HBLANK);
    if (dbg.do_break) return 0;
    NDS_PPU_hblank(this);
    if (dbg.do_break) return 0;
    run_system(this, HBLANK_CYCLES);
    if (dbg.do_break) return 0;
    NDS_PPU_finish_scanline(this);
    return 0;
}

static u32 NDSJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    ARM7TDMI_run(&this->arm7);
    ARM946ES_run(&this->arm9);
    return 0;
}

static void NDSJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    // 7, 9, firmware
    memcpy(this->mem.bios7, mfs->files[0].buf.ptr, 16384);
    memcpy(this->mem.bios9, mfs->files[0].buf.ptr, 4096);
    memcpy(this->mem.firmware, mfs->files[0].buf.ptr, 256 * 1024);
}

static void NDSIO_unload_cart(JSM)
{
}

static void NDSIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    u32 r;
    //NDS_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, pio, &r);
    NDSJ_reset(jsm);
}

static void setup_lcd(struct JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

    d->fps = 59.8261;
    d->fps_override_hint = 60;
    // 256x192, but 355x263 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.max_visible = 256;
    d->pixelometry.cols.right_hblank = 99;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 192;
    d->pixelometry.rows.max_visible = 192;
    d->pixelometry.rows.bottom_vblank = 71;
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
    chan->sample_rate = 262144;
    chan->low_pass_filter = 24000;
}


static void NDSJ_describe_io(JSM, struct cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *controller = cvec_push_back(this->jsm.IOs);
    NDS_controller_setup_pio(controller);
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
    d->cartridge_port.load_cart = &NDSIO_load_cart;
    d->cartridge_port.unload_cart = &NDSIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(256 * 384 * 2);
    d->display.output[1] = malloc(256 * 384 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_lcd(&d->display);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    this->ppu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
}
