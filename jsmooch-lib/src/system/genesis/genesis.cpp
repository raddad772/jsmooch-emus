//
// Created by . on 6/1/24.
//
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

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

jsm_system *genesis_new(jsm::systems kind)
{
    return new genesis::core(kind);
}

void genesis_delete(jsm_system *sys) {
    auto *th = dynamic_cast<genesis::core *>(sys);
    for (auto &pio : th->IOs) {
        if (pio.kind == HID_CART_PORT) {
            if (pio.cartridge_port.unload_cart) pio.cartridge_port.unload_cart(sys);
        }
    }
}

namespace genesis {

u32 read_trace_z80(void *ptr, u32 addr) {
    auto *th = static_cast<core *>(ptr);
    return th->z80_bus_read(addr, th->z80.pins.D, false);
}

u32 read_trace_m68k(void *ptr, u32 addr, u32 UDS, u32 LDS) {
    auto *th = static_cast<core *>(ptr);
    return th->mainbus_read(addr, UDS, LDS, th->io.m68k.open_bus_data, false);
}

void core::setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = (static_cast<float>(clock.timing.frame.cycles_per) / static_cast<float>(dw->samples_requested));
    dw->user.buf_pos = 0;
}

void core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = (static_cast<float>(clock.timing.frame.cycles_per) / static_cast<float>(ab->samples_len));
        audio.next_sample_cycle_max = 0;
        auto *wf = &dbg.waveforms_psg.main.get();
        audio.master_cycles_per_max_sample = static_cast<float>(clock.timing.frame.cycles_per) / static_cast<float>(wf->samples_requested);

        wf = &dbg.waveforms_psg.chan[0].get();
        audio.master_cycles_per_min_sample = static_cast<float>(clock.timing.frame.cycles_per) / static_cast<float>(wf->samples_requested);
    }

    // PSG
    debug_waveform *wf = &dbg.waveforms_psg.main.get();
    setup_debug_waveform(wf);
    psg.ext_enable = wf->ch_output_enabled;
    if (wf->clock_divider == 0) wf->clock_divider = wf->default_clock_divider;
    clock.psg.clock_divisor = wf->clock_divider;
    for (u32 i = 0; i < 4; i++) {
        wf = &dbg.waveforms_psg.chan[i].get();
        setup_debug_waveform(wf);
        if (i < 3) {
            psg.sw[i].ext_enable = wf->ch_output_enabled;
        }
        else
            psg.noise.ext_enable = wf->ch_output_enabled;
    }

    // ym2612
    wf = &dbg.waveforms_ym2612.main.get();
    ym2612.ext_enable = wf->ch_output_enabled;
    setup_debug_waveform(wf);
    if (wf->clock_divider == 0) wf->clock_divider = wf->default_clock_divider;
    clock.ym2612.clock_divisor = wf->clock_divider;
    for (u32 i = 0; i < 6; i++) {
        wf = &dbg.waveforms_ym2612.chan[i].get();
        setup_debug_waveform(wf);
        ym2612.channel[i].ext_enable = wf->ch_output_enabled;
    }

}

void core::populate_opts()
{
    debugger_widgets_add_checkbox(opts, "VDP: Enable Layer A", true, true, false);
    debugger_widgets_add_checkbox(opts, "VDP: Enable Layer B", true, true, false);
    debugger_widgets_add_checkbox(opts, "VDP: Enable Sprites", true, true, false);
    debugger_widgets_add_checkbox(opts, "VDP: trace", true, false, false);
}

void core::read_opts()
{
    debugger_widget *w = &opts.at(0);
    v_opts.vdp.enable_A = w->checkbox.value;

    w = &opts.at(1);
    v_opts.vdp.enable_B = w->checkbox.value;

    w = &opts.at(2);
    v_opts.vdp.enable_sprites = w->checkbox.value;

    w = &opts.at(3);
    v_opts.vdp.ex_trace = w->checkbox.value;
}

static void c_vdp_z80_m68k(core* th, gensched_item *e)
{
    th->clock.master_cycle_count += e->clk_add_vdp;
    th->vdp.do_cycle();

    th->clock.master_cycle_count += e->clk_add_z80;
    th->cycle_z80();

    th->clock.master_cycle_count += e->clk_add_m68k;
    th->cycle_m68k();

    //assert((e->clk_add_m68k+e->clk_add_vdp+e->clk_add_z80) == 7);
}

static void c_z80_vdp_m68k(core* th, gensched_item *e)
{
    th->clock.master_cycle_count += e->clk_add_z80;
    th->cycle_z80();

    th->clock.master_cycle_count += e->clk_add_vdp;
    th->vdp.do_cycle();

    th->clock.master_cycle_count += e->clk_add_m68k;
    th->cycle_m68k();
    //assert((e->clk_add_m68k+e->clk_add_vdp+e->clk_add_z80) == 7);
}

static void c_vdp_m68k(core* th, gensched_item *e)
{
    th->clock.master_cycle_count += e->clk_add_vdp;
    th->vdp.do_cycle();

    th->clock.master_cycle_count += e->clk_add_m68k;
    th->cycle_m68k();
    //assert((e->clk_add_m68k+e->clk_add_vdp) == 7);
}

static void c_z80_m68k(core* th, gensched_item *e)
{
    th->clock.master_cycle_count += e->clk_add_z80;
    th->cycle_z80();

    th->clock.master_cycle_count += e->clk_add_m68k;
    th->cycle_m68k();
    //assert((e->clk_add_m68k+e->clk_add_z80) == 7);
}

static void c_m68k(core* th, gensched_item *e)
{
    th->cycle_m68k();
    th->clock.master_cycle_count += 7;
}

void core::create_scheduling_lookup_table()
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
            static constexpr i32 m68k_cycles = 7;

            z80_cycles -= m68k_cycles; // 6 = -1
            vdp_cycles -= m68k_cycles; // 7 = 0

            gensched_item *item = &scheduler_lookup[lookup_add + tbl_entry];

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

void core::block_step(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    //             // Index = (z80-1 * x) + vdp-1
    u32 lu = ((th->clock.vdp.clock_divisor >> 2) - 4);
    assert(lu <= 1);
    lu *= NUM_GENSCHED;
    gensched_item *e = &th->scheduler_lookup[lu + th->scheduler_index];
    e->callback(th, e);
    u32 ni = e->next_index;
    th->scheduler_index = ni;
}


/*static void run_block(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->block_cycles_to_run += key;
    while (th->block_cycles_to_run > 0) {
        block_step();
        th->block_cycles_to_run -= th->clock.vdp.clock_divisor;
        if (dbg.do_break) break;
    }
}*/

core::core(jsm::systems in_kind) :
    clock(in_kind),
    scheduler(&clock.master_cycle_count),
    ym2612(YM2612::OPN2V_ym2612, &clock.master_cycle_count, 32 * 7 * 6),
    vdp(this)
{
    has.set_audiobuf = true;
    has.load_BIOS = false;
    has.save_state = true;
    create_scheduling_lookup_table();
    populate_opts();
    scheduler.max_block_size = 20;
    scheduler.run.func = &block_step;
    scheduler.run.ptr = this;
    PAL = kind == jsm::systems::MEGADRIVE_PAL;
    snprintf(label, sizeof(label), "Sega Genesis");

    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_z80;
    dt.read_trace_m68k = &read_trace_m68k;
    dt.ptr = static_cast<void *>(this);

    m68k.setup_tracing(&dt, &clock.master_cycle_count);
    z80.setup_tracing(&dt, &clock.master_cycle_count);
    jsm.described_inputs = false;
}

void core::load_symbols() {
    // Hard-coded to sonic 1 symbols atm
    FILE *f = fopen("/Users/dave/s1symbols", "r");
    if (!f) abort();
    char buf[200];
    debugging.num_symbols = 0;
    while (fgets(buf, sizeof(buf), f)) {
        char *num = strtok(buf, "=");
        char *name = strtok(nullptr, "\n");
        u32 addr = atoi(num);
        debugging.symbols[debugging.num_symbols].addr = addr;
        snprintf(debugging.symbols[debugging.num_symbols].name, 50, "%s", name);
        debugging.num_symbols++;
        if (debugging.num_symbols >= 20000) abort();
    }
    fclose(f);
}



static inline float i16_to_float(i16 val)
{
    return ((static_cast<float>(static_cast<i32>(val) + 32768) / 65535.0f) * 2.0f) - 1.0f;
}

static void run_ym2612(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->timing.ym2612_cycles++;
    u64 cur = clock - jitter;
    th->scheduler.only_add_abs(cur+th->clock.ym2612.clock_divisor, 0, th, &run_ym2612, nullptr);
    th->ym2612.cycle();
}

static void run_psg(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->timing.psg_cycles++;
    u64 cur = clock - jitter;
    th->scheduler.only_add_abs(cur+th->clock.psg.clock_divisor, 0, th, &run_psg, nullptr);
    th->psg.cycle();
}

static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    if (th->audio.buf) {
        th->audio.cycles++;
        th->audio.next_sample_cycle += th->audio.master_cycles_per_audio_sample;
        th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle), 0, th, &sample_audio, nullptr);
        if (th->audio.buf->upos < (th->audio.buf->samples_len << 1)) {
            i32 l = 0, r = 0;
            if (th->psg.ext_enable) {
                l = r = static_cast<i32>(th->psg.mix_sample(false)) >> 5;
            }
            if (th->ym2612.ext_enable) {
                l += th->ym2612.mix.left_output;
                r += th->ym2612.mix.right_output;
            }
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos] = i16_to_float(static_cast<i16>(l));
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos+1] = i16_to_float(static_cast<i16>(r));
        }
        th->audio.buf->upos+=2;
    }
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);

    /* PSG */
    debug_waveform *dw = &th->dbg.waveforms_psg.main.get();
    if (dw->user.buf_pos < dw->samples_requested) {
        static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(th->psg.mix_sample(true));
        dw->user.buf_pos++;
    }

    /* YM2612 */
    dw = &th->dbg.waveforms_ym2612.main.get();
    if (dw->user.buf_pos < dw->samples_requested) {
        static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(th->ym2612.mix.mono_output);
        dw->user.buf_pos++;
    }
    th->audio.next_sample_cycle_max += th->audio.master_cycles_per_max_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_max), 0, th, &sample_audio_debug_max, nullptr);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);

    /* PSG */
    debug_waveform *dw = &th->dbg.waveforms_psg.chan[0].get();
    for (int j = 0; j < 4; j++) {
        dw = &th->dbg.waveforms_psg.chan[j].get();
        if (dw->user.buf_pos < dw->samples_requested) {
            i16 sv = th->psg.sample_channel(j);
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv * 4);
            dw->user.buf_pos++;
        }
    }

    /* YM2612 */
    for (int j = 0; j < 6; j++) {
        dw = &th->dbg.waveforms_ym2612.chan[j].get();
        if (dw->user.buf_pos < dw->samples_requested) {
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            i16 sv = th->ym2612.sample_channel(j);
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv << 2);
            dw->user.buf_pos++;
        }
    }
    th->audio.next_sample_cycle_min += th->audio.master_cycles_per_min_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_min), 0, th, &sample_audio_debug_min, nullptr);
}


void core::schedule_first()
{
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_max), 0, this, &sample_audio_debug_max, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_min), 0, this, &sample_audio_debug_min, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle), 0, this, &sample_audio, nullptr);
    scheduler.only_add_abs(clock.ym2612.clock_divisor, 0, this, &run_ym2612, nullptr);
    scheduler.only_add_abs(clock.psg.clock_divisor, 0, this, &run_psg, nullptr);
    vdp.schedule_first();
}

static void genesisIO_load_cart(jsm_system *sys, multi_file_set &mfs, physical_io_device &which_pio)
{
    auto *th = dynamic_cast<core *>(sys);
    buf* b = &mfs.files[0].buf;

    th->cart.load_ROM_from_RAM(static_cast<char *>(b->ptr), b->size, &which_pio, &th->io.SRAM_enabled);
    if ((th->cart.header.region_japan) && (!th->cart.header.region_usa)) {
        th->v_opts.JP = 1;
    }
    else
        th->v_opts.JP = 0;
    //load_symbols();
    th->reset();
}

static void genesisIO_unload_cart(jsm_system *sys)
{
}

void core::setup_crt(JSM_DISPLAY &d)
{
    d.kind = jsm::CRT;
    d.enabled = 1;

    d.fps = PAL ? 50 : 60.0988;
    d.fps_override_hint = clock.timing.second.frames_per;

    d.pixelometry.cols.left_hblank = 0; // 0
    d.pixelometry.cols.visible = 1280;  // 320x224   *4
    d.pixelometry.cols.max_visible = 1280;  // 320x224    *4
    d.pixelometry.cols.right_hblank = 430; // 107.5, ick   *4
    d.pixelometry.offset.x = 0;

    d.pixelometry.rows.top_vblank = 0;
    d.pixelometry.rows.visible = clock.vdp.bottom_max_rendered_line;
    d.pixelometry.rows.max_visible = clock.vdp.bottom_max_rendered_line;
    d.pixelometry.rows.bottom_vblank = 76; // TODO: update these for PAL. they're currently not really used
    d.pixelometry.offset.y = 0;

    if (PAL) {
        d.geometry.physical_aspect_ratio.width = 5;
        d.geometry.physical_aspect_ratio.height = 4;
    }
    else {
        d.geometry.physical_aspect_ratio.width = 4;
        d.geometry.physical_aspect_ratio.height = 3;
    }

    d.pixelometry.overscan.left = d.pixelometry.overscan.right = 0;
    d.pixelometry.overscan.top = d.pixelometry.overscan.bottom = 0;
}

void core::setup_audio()
{
    physical_io_device *pio = &IOs.emplace_back();
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = (clock.timing.frame.cycles_per * clock.timing.second.frames_per) / (clock.ym2612.clock_divisor); // ~55kHz
    chan->left = chan->right = 1;
    chan->num = 2;
    chan->low_pass_filter = 24000;
}

void core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = true;
    IOs.reserve(15);

    // controllers
    physical_io_device *c1 = &IOs.emplace_back();
    physical_io_device *c2 = &IOs.emplace_back();
    controller1.setup_pio(c1, 0, "Player 1", 1);
    controller2.setup_pio(c2, 1, "Player 2", 0);
    controller1.master_clock = &clock.master_cycle_count;
    controller2.master_clock = &clock.master_cycle_count;

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, 1, 1, 1, 1);
    HID_digital_button* b;

    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = &chassis->chassis.digital_buttons.emplace_back();
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    // cartridge port
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &genesisIO_load_cart;
    d->cartridge_port.unload_cart = &genesisIO_unload_cart;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(1280 * 480 * 2);
    d->display.output[1] = malloc(1280 * 480 * 2);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_crt(d->display);
    vdp.display_ptr.make(IOs, IOs.size()-1);
    d->display.last_written = 1;
    clock.current_back_buffer = 0;
    clock.current_front_buffer = 1;
    vdp.cur_output = static_cast<u16 *>(d->display.output[0]);

    setup_audio();

    vdp.display = &vdp.display_ptr.get().display;
    io.controller_port1.connect(controller_6button, &controller1);
    //genesis_controllerport_connect(&io.controller_port2, controller_3button, &controller2);
}

void core::play()
{
}

void core::pause()
{
}

void core::stop()
{
}

void core::get_framevars(framevars& out)
{
    out.master_frame = clock.master_frame;
    out.x = clock.vdp.hcount;
    out.scanline = clock.vdp.vcount;
    out.master_cycle = clock.master_cycle_count;
}

void core::reset()
{
    z80.reset();
    m68k.reset();
    psg.reset();
    ym2612.reset();
    clock.reset();
    vdp.reset();
    io.z80.reset_line = true;
    io.z80.reset_line_count = 500;
    io.z80.bus_request = io.z80.bus_ack = true;
    io.m68k.VDP_FIFO_stall = false;
    io.m68k.VDP_prefetch_stall = false;
    scheduler_index = 0;

    scheduler.clear();
    schedule_first();
    printf("\nGenesis reset!");
}

//#define DO_STATS

u32 core::finish_frame()
{
    read_opts();

#ifdef DO_STATS
    u64 ym_start = th->timing.ym2612_cycles;
    u64 z80_start = th->timing.z80_cycles;
    u64 m68k_start = th->timing.m68k_cycles;
    u64 vdp_start = th->timing.vdp_cycles;
    u64 clock_start = th->clock.master_cycle_count;
    u64 psg_start = th->timing.psg_cycles;
    u64 audio_start = th->audio.cycles;
#endif
    scheduler.run_til_tag(TAG_FRAME);

#ifdef DO_STATS
    u64 ym_num_cycles = (th->timing.ym2612_cycles - ym_start) * 60;
    u64 psg_num_cycles = (th->timing.psg_cycles - psg_start) * 60;
    u64 z80_num_cycles = (th->timing.z80_cycles - z80_start) * 60;
    u64 m68k_num_cycles = (th->timing.m68k_cycles - m68k_start) * 60;
    u64 vdp_num_cycles = (th->timing.vdp_cycles - vdp_start) * 60;
    u64 clock_num_cycles = (th->clock.master_cycle_count - clock_start) * 60;
    u64 per_frame = th->clock.master_cycle_count - clock_start;
    u64 per_scanline = per_frame / 262;
    u64 audio_num_cycles = (th->audio.cycles - audio_start) * 60;
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

    printf("\nSCANLINE:%d FRAME:%lld", th->clock.vdp.vcount, th->clock.master_frame);
    printf("\n\nEFFECTIVE AUDIO FREQ IS %lld. RUNNING AT %f SPEED", audio_num_cycles, audio_spd);
    printf("\nEFFECTIVE YM FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", ym_num_cycles, ym_div, ym_spd);
    printf("\nEFFECTIVE PSG FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", psg_num_cycles, psg_div, psg_spd);
    printf("\nEFFECTIVE Z80 SPEED IS %lld. DIVISOR %f, RUNNING AT %f SPEED", z80_num_cycles, z80_div, z80_spd);
    printf("\nEFFECTIVE M68K SPEED IS %lld. DIVISOR %f, RUNNING AT %f SPEED", m68k_num_cycles, m68k_div, m68k_spd);
    printf("\nEFFECTIVE VDP SPEED IS %lld. DIVISOR %f, RUNNING AT %f SPEED", vdp_num_cycles, vdp_div, vdp_spd);
    printf("\nEFFECTIVE MASTER CLOCK IS %lld. PER FRAME:%lld PER SCANLINE:%lld", clock_num_cycles, per_frame, per_scanline);
#endif
    return vdp.display->last_written;
}

u32 core::finish_scanline()
{
    scheduler.run_til_tag(TAG_SCANLINE);

    return vdp.display->last_written;
}

u32 core::step_master(u32 howmany)
{
    scheduler.run_for_cycles(howmany);
    return 0;
}

void core::load_BIOS(multi_file_set &mfs)
{
    printf("\nSega Genesis doesn't have a BIOS...?");
}

}