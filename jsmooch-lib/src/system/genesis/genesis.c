//
// Created by . on 6/1/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debugger/debugger.h"

#include "component/controller/genesis3/genesis3.h"
#include "component/controller/genesis6/genesis6.h"
#include "genesis.h"
#include "genesis_debugger.h"
#include "genesis_bus.h"
#include "genesis_cart.h"
#include "genesis_vdp.h"
#include "genesis_serialize.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2

#define JTHIS struct genesis* this = (struct genesis*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void genesisJ_play(JSM);
static void genesisJ_pause(JSM);
static void genesisJ_stop(JSM);
static void genesisJ_get_framevars(JSM, struct framevars* out);
static void genesisJ_reset(JSM);
static u32 genesisJ_finish_frame(JSM);
static u32 genesisJ_finish_scanline(JSM);
static u32 genesisJ_step_master(JSM, u32 howmany);
static void genesisJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void genesisJ_describe_io(JSM, struct cvec* IOs);

#define MASTER_CYCLES_PER_SCANLINE 3420
#define NTSC_SCANLINES 262

// 896040
#define MASTER_CYCLES_PER_FRAME (MASTER_CYCLES_PER_SCANLINE*NTSC_SCANLINES)
#define MASTER_CYCLES_PER_SECOND (MASTER_CYCLES_PER_FRAME*60)

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

}

static void populate_opts(struct jsm_system *jsm)
{
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer A", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer B", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Sprites", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: trace", 1, 0, 0);
}

static void read_opts(struct jsm_system *jsm, struct genesis* this)
{
    struct debugger_widget *w = cvec_get(&jsm->opts, 0);
    this->opts.vdp.enable_A = w->checkbox.value;

    w = cvec_get(&jsm->opts, 1);
    this->opts.vdp.enable_B = w->checkbox.value;

    w = cvec_get(&jsm->opts, 2);
    this->opts.vdp.enable_sprites = w->checkbox.value;

    w = cvec_get(&jsm->opts, 3);
    this->opts.vdp.ex_trace = w->checkbox.value;
}

static void c_vdp_z80_m68k(struct genesis *this, struct gensched_item *e)
{
    this->clock.master_cycle_count += e->clk_add_vdp;
    genesis_VDP_cycle(this);

    this->clock.master_cycle_count += e->clk_add_z80;
    genesis_cycle_z80(this);

    this->clock.master_cycle_count += e->clk_add_m68k;
    genesis_cycle_m68k(this);

    assert((e->clk_add_m68k+e->clk_add_vdp+e->clk_add_z80) == 7);
}

static void c_z80_vdp_m68k(struct genesis *this, struct gensched_item *e)
{
    this->clock.master_cycle_count += e->clk_add_z80;
    genesis_cycle_z80(this);

    this->clock.master_cycle_count += e->clk_add_vdp;
    genesis_VDP_cycle(this);

    this->clock.master_cycle_count += e->clk_add_m68k;
    genesis_cycle_m68k(this);
    assert((e->clk_add_m68k+e->clk_add_vdp+e->clk_add_z80) == 7);
}

static void c_vdp_m68k(struct genesis *this, struct gensched_item *e)
{
    this->clock.master_cycle_count += e->clk_add_vdp;
    genesis_VDP_cycle(this);

    this->clock.master_cycle_count += e->clk_add_m68k;
    genesis_cycle_m68k(this);
    assert((e->clk_add_m68k+e->clk_add_vdp) == 7);
}

static void c_z80_m68k(struct genesis *this, struct gensched_item *e)
{
    this->clock.master_cycle_count += e->clk_add_z80;
    genesis_cycle_z80(this);

    this->clock.master_cycle_count += e->clk_add_m68k;
    genesis_cycle_m68k(this);
    assert((e->clk_add_m68k+e->clk_add_z80) == 7);
}

static void c_m68k(struct genesis *this, struct gensched_item *e)
{
    genesis_cycle_m68k(this);
    this->clock.master_cycle_count += 7;
}

static void create_scheduling_lookup_table(struct genesis *this)
{
    u32 lookup_add = 0;
    for (i32 cycles=16; cycles<24; cycles+=4) { // 4 and 5 are the two possible vdp divisors
        for (i32 tbl_entry = 0; tbl_entry < NUM_GENSCHED; tbl_entry++) {
            // Decode values from tbl_entry
            // Index = (z80-1 * x) + vdp-1
            i32 z80_cycles = (tbl_entry / GENSCHED_MUL) + 1; // 0-14/1-15
            i32 vdp_cycles = (tbl_entry % GENSCHED_MUL) + 1; // 0-19/1-20
            assert(z80_cycles < 16);
            assert(vdp_cycles < 21);

            // Apply m68k divider, 7
            static const i32 m68k_cycles = 7;

            z80_cycles -= m68k_cycles; // 6 = -1
            vdp_cycles -= m68k_cycles; // 7 = 0

            struct gensched_item *item = &this->scheduler_lookup[lookup_add + tbl_entry];

            // Determine which function to use
            if ((z80_cycles <= 0) && (vdp_cycles <= 0)) {
                if (z80_cycles < vdp_cycles) {
                    item->callback = &c_z80_vdp_m68k;

                    i32 num = m68k_cycles + z80_cycles;
                    item->clk_add_z80 = num;

                    num = vdp_cycles - z80_cycles;
                    item->clk_add_vdp = num;

                    item->clk_add_m68k = m68k_cycles - (item->clk_add_z80 + item->clk_add_vdp);
                }
                else {
                    item->callback = &c_vdp_z80_m68k;

                    i32 num = m68k_cycles + vdp_cycles;
                    item->clk_add_vdp = num;

                    num = z80_cycles - vdp_cycles;
                    item->clk_add_z80 = num;

                    item->clk_add_m68k = m68k_cycles - (item->clk_add_z80 + item->clk_add_vdp);
                }
                z80_cycles += 15;
                vdp_cycles += cycles;
            }
            else if (z80_cycles <= 0) { // just Z80
                item->callback = &c_z80_m68k;

                i32 num = m68k_cycles + z80_cycles;
                item->clk_add_z80 = num;

                item->clk_add_m68k = m68k_cycles - item->clk_add_z80;

                z80_cycles += 15;
            }
            else if (vdp_cycles <= 0) { // just VDP
                item->callback = &c_vdp_m68k;

                i32 num = m68k_cycles + vdp_cycles;
                item->clk_add_vdp = num;

                item->clk_add_m68k = m68k_cycles - item->clk_add_vdp;

                vdp_cycles += cycles;
            }
            else {
                item->callback = &c_m68k;
            }

            // Encode values of next to tbl_entry
            // Index = (z80-1 * x) + vdp-1
            assert(z80_cycles>0);
            assert(z80_cycles < 16);
            assert(vdp_cycles>0);
            assert(vdp_cycles<21);
            u32 ni = ((z80_cycles - 1) * GENSCHED_MUL) + (vdp_cycles - 1);
            assert(ni < NUM_GENSCHED);
            item->next_index = ni;
        }
        lookup_add += NUM_GENSCHED;
    }
}

static void block_step(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis *this = (struct genesis *)ptr;
    //             // Index = (z80-1 * x) + vdp-1
    u32 lu = ((this->clock.vdp.clock_divisor >> 2) - 4);
    assert(lu >= 0);
    assert(lu <= 1);
    lu *= NUM_GENSCHED;
    struct gensched_item *e = &this->scheduler_lookup[lu + this->scheduler_index];
    e->callback(this, e);
    u32 ni = e->next_index;
    this->scheduler_index = ni;
}


/*static void run_block(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis *this = (struct genesis *)ptr;
    this->block_cycles_to_run += key;
    while (this->block_cycles_to_run > 0) {
        block_step(this);
        this->block_cycles_to_run -= this->clock.vdp.clock_divisor;
        if (dbg.do_break) break;
    }
}*/

void genesis_new(JSM, enum jsm_systems kind)
{
    struct genesis* this = (struct genesis*)malloc(sizeof(struct genesis));
    memset(this, 0, sizeof(*this));
    populate_opts(jsm);
    create_scheduling_lookup_table(this);
    scheduler_init(&this->scheduler, &this->clock.master_cycle_count, &this->clock.waitstates);
    this->scheduler.max_block_size = 20;
    this->scheduler.run.func = &block_step;
    this->scheduler.run.ptr = this;

    Z80_init(&this->z80, 0);
    M68k_init(&this->m68k, 1);
    genesis_clock_init(&this->clock, kind);
    genesis_cart_init(&this->cart);
    genesis_VDP_init(this); // must be after m68k init
    ym2612_init(&this->ym2612, OPN2V_ym2612, &this->clock.master_cycle_count, 32 * 7 * 6);
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
    jsm->save_state = genesisJ_save_state;
    jsm->load_state = genesisJ_load_state;
}

void genesis_delete(JSM) {
    JTHIS;

    M68k_delete(&this->m68k);
    ym2612_delete(&this->ym2612);
    genesis_VDP_delete(this);

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

static void load_symbols(struct genesis* this) {
    FILE *f = fopen("/Users/dave/s1symbols", "r");
    if (f == 0) abort();
    char buf[200];
    this->debugging.num_symbols = 0;
    while (fgets(buf, sizeof(buf), f)) {
        char *num = strtok(buf, "=");
        char *name = strtok(NULL, "\n");
        u32 addr = atoi(num);
        this->debugging.symbols[this->debugging.num_symbols].addr = addr;
        snprintf(this->debugging.symbols[this->debugging.num_symbols].name, 50, "%s", name);
        this->debugging.num_symbols++;
        if (this->debugging.num_symbols >= 20000) abort();
    }
    fclose(f);
}



static inline float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}

static void run_ym2612(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis* this = (struct genesis *)ptr;
    this->timing.ym2612_cycles++;
    u64 cur = clock - jitter;
    scheduler_only_add_abs(&this->scheduler, cur+this->clock.ym2612.clock_divisor, 0, this, &run_ym2612, NULL);
    ym2612_cycle(&this->ym2612);
}

static void run_psg(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis* this = (struct genesis *)ptr;
    this->timing.psg_cycles++;
    u64 cur = clock - jitter;
    scheduler_only_add_abs(&this->scheduler, cur+this->clock.psg.clock_divisor, 0, this, &run_psg, NULL);
    SN76489_cycle(&this->psg);
}

static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis* this = (struct genesis *)ptr;
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
    }
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis *this = (struct genesis *)ptr;

    /* PSG */
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(SN76489_mix_sample(&this->psg, 1));
        dw->user.buf_pos++;
    }

    /* YM2612 */
    dw = cpg(this->dbg.waveforms_ym2612.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(this->ym2612.mix.mono_output << 3);
        dw->user.buf_pos++;
    }
    this->audio.next_sample_cycle_max += this->audio.master_cycles_per_max_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct genesis *this = (struct genesis *)ptr;

    /* PSG */
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.chan[0]);
    for (int j = 0; j < 4; j++) {
        dw = cpg(this->dbg.waveforms_psg.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            i16 sv = SN76489_sample_channel(&this->psg, j);
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv * 4);
            dw->user.buf_pos++;
        }
    }

    /* YM2612 */
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
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
}


static void schedule_first(struct genesis *this)
{
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle, 0, this, &sample_audio, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->clock.ym2612.clock_divisor, 0, this, &run_ym2612, NULL);
    scheduler_only_add_abs(&this->scheduler, this->clock.psg.clock_divisor, 0, this, &run_psg, NULL);
    genesis_VDP_schedule_first(this);
}

static void genesisIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *which_pio)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    genesis_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, which_pio, &this->io.SRAM_enabled);
    if ((this->cart.header.region_japan) && (!this->cart.header.region_usa)) {
        this->opts.JP = 1;
    }
    else
        this->opts.JP = 0;
    //load_symbols(this);
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
    genesis_controller_6button_init(&this->controller1, &this->clock.master_cycle_count);
    genesis6_setup_pio(c1, 0, "Player 1", 1);
    genesis3_setup_pio(c2, 1, "Player 2", 0);
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
    this->clock.current_back_buffer = 0;
    this->clock.current_front_buffer = 1;
    this->vdp.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->vdp.display = &((struct physical_io_device *)cpg(this->vdp.display_ptr))->display;
    genesis_controllerport_connect(&this->io.controller_port1, genesis_controller_6button, &this->controller1);
    //genesis_controllerport_connect(&this->io.controller_port2, genesis_controller_3button, &this->controller2);
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
    this->scheduler_index = 0;

    scheduler_clear(&this->scheduler);
    schedule_first(this);
    printf("\nGenesis reset!");
}

//#define DO_STATS

u32 genesisJ_finish_frame(JSM)
{
    JTHIS;
    read_opts(jsm, this);

#ifdef DO_STATS
    u64 ym_start = this->timing.ym2612_cycles;
    u64 z80_start = this->timing.z80_cycles;
    u64 m68k_start = this->timing.m68k_cycles;
    u64 vdp_start = this->timing.vdp_cycles;
    u64 clock_start = this->clock.master_cycle_count;
    u64 psg_start = this->timing.psg_cycles;
    u64 audio_start = this->audio.cycles;
#endif
    scheduler_run_til_tag(&this->scheduler, TAG_FRAME);

#ifdef DO_STATS
    u64 ym_num_cycles = (this->timing.ym2612_cycles - ym_start) * 60;
    u64 psg_num_cycles = (this->timing.psg_cycles - psg_start) * 60;
    u64 z80_num_cycles = (this->timing.z80_cycles - z80_start) * 60;
    u64 m68k_num_cycles = (this->timing.m68k_cycles - m68k_start) * 60;
    u64 vdp_num_cycles = (this->timing.vdp_cycles - vdp_start) * 60;
    u64 clock_num_cycles = (this->clock.master_cycle_count - clock_start) * 60;
    u64 per_frame = this->clock.master_cycle_count - clock_start;
    u64 per_scanline = per_frame / 262;
    u64 audio_num_cycles = (this->audio.cycles - audio_start) * 60;
    double ym_div = (double)MASTER_CYCLES_PER_SECOND / (double)ym_num_cycles;
    double z80_div = (double)MASTER_CYCLES_PER_SECOND / (double)z80_num_cycles;
    double m68k_div = (double)MASTER_CYCLES_PER_SECOND / (double)m68k_num_cycles;
    double vdp_div = (double)MASTER_CYCLES_PER_SECOND / (double)vdp_num_cycles;
    double psg_div = (double)MASTER_CYCLES_PER_SECOND / (double)psg_num_cycles;

    double ym_spd = (1008.0 / ym_div) * 100.0;
    double psg_spd = (240.0 / psg_div) * 100.0;
    double m68k_spd = (7.0 / m68k_div) * 100.0;
    double z80_spd = (15.0 / z80_div) * 100.0;
    double vdp_spd = (16.0 / vdp_div) * 100.0;
    double audio_spd = ((double)audio_num_cycles / ((MASTER_CYCLES_PER_FRAME * 60) / (7 * 144))) * 100.0;

    printf("\nSCANLINE:%d FRAME:%lld", this->clock.vdp.vcount, this->clock.master_frame);
    printf("\n\nEFFECTIVE AUDIO FREQ IS %lld. RUNNING AT %f SPEED", audio_num_cycles, audio_spd);
    printf("\nEFFECTIVE YM FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", ym_num_cycles, ym_div, ym_spd);
    printf("\nEFFECTIVE PSG FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", psg_num_cycles, psg_div, psg_spd);
    printf("\nEFFECTIVE Z80 SPEED IS %lld. DIVISOR %f, RUNNING AT %f SPEED", z80_num_cycles, z80_div, z80_spd);
    printf("\nEFFECTIVE M68K SPEED IS %lld. DIVISOR %f, RUNNING AT %f SPEED", m68k_num_cycles, m68k_div, m68k_spd);
    printf("\nEFFECTIVE VDP SPEED IS %lld. DIVISOR %f, RUNNING AT %f SPEED", vdp_num_cycles, vdp_div, vdp_spd);
    printf("\nEFFECTIVE MASTER CLOCK IS %lld. PER FRAME:%lld PER SCANLINE:%lld", clock_num_cycles, per_frame, per_scanline);
#endif
    return this->vdp.display->last_written;
}

u32 genesisJ_finish_scanline(JSM)
{
    JTHIS;
    scheduler_run_til_tag(&this->scheduler, TAG_SCANLINE);

    return this->vdp.display->last_written;
}

u32 genesisJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    scheduler_run_for_cycles(&this->scheduler, howmany);
    return 0;
}

void genesisJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nSega Genesis doesn't have a BIOS...?");
}

