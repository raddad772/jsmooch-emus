//
// Created by . on 6/18/25.
//

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "helpers/physical_io.h"

#include "tg16.h"
#include "tg16_debugger.h"
#include "tg16_bus.h"
#include "tg16_controllerport.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2
#define DRAW_CYCLES 1108
#define TG16_SAMPLE 48000

#define JTHIS TG16* this = (TG16*)jsm->ptr
#define JSM 

#define THIS TG16* this

u32 read_trace_huc6280(void *ptr, u32 addr) {
    auto *th = static_cast<TG16::core *>(ptr);
    return th->bus_read(addr, th->cpu.pins.D, false);
}

void TG16::core::setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = (static_cast<float>(vce.regs.cycles_per_frame) / static_cast<float>(dw->samples_requested));
    dw->user.buf_pos = 0;
}

void TG16::core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    debug_waveform *wf;
        // # of cycles per frame can change per-frame
    audio.master_cycles_per_audio_sample = (static_cast<float>(vce.regs.cycles_per_frame) / (float)ab->samples_len);
    wf = &dbg.waveforms_psg.main.get();
    audio.master_cycles_per_max_sample = static_cast<float>(vce.regs.cycles_per_frame) / (float)wf->samples_requested;

    wf = &dbg.waveforms_psg.chan[0].get();
    audio.master_cycles_per_min_sample = static_cast<float>(vce.regs.cycles_per_frame) / (float)wf->samples_requested;

    // PSG
    wf = &dbg.waveforms_psg.main.get();
    setup_debug_waveform(wf);
    cpu.psg.ext_enable = wf->ch_output_enabled;
    for (u32 i = 0; i < 6; i++) {
        wf = &dbg.waveforms_psg.chan[i].get();
        setup_debug_waveform(wf);
        cpu.psg.channels[i].ext_enable = wf->ch_output_enabled;
    }

}

void TG16::core::block_step(void *ptr, u64 key, u64 clock, u32 jitter)
{
    // DO THIS
    printf("\nDO BLOCK STEP");
}

void TG16::core::vdc0_update_irqs(void *ptr, bool val)
{
    auto *th = static_cast<TG16::core *>(ptr);
    th->cpu.regs.IRQR.IRQ1 = val;
    //printf("\nIRQ1 set to %d", cpu.regs.IRQR.IRQ1);
}

events_view &TG16::core::get_events_view()
{
    return dbg.events.view.get().events;
}

jsm_system *TG16_new(jsm::systems kind)
{
    return new TG16::core();
}

TG16::core::core() {
    has.load_BIOS = false;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = false;
    has.set_audiobuf = true;
    scheduler.max_block_size = 2;
    scheduler.run.func = &block_step;
    scheduler.run.ptr = this;

    cpu.read_func = &huc_read_mem;
    cpu.write_func = &huc_write_mem;
    cpu.read_io_func = &huc_read_io;
    cpu.write_io_func = &huc_write_io;
    cpu.read_ptr = cpu.write_ptr = cpu.read_io_ptr = cpu.write_io_ptr = this;
    vdc0.irq.update_func_ptr = this;
    vdc0.irq.update_func = &vdc0_update_irqs;
    //ym2612_init(&ym2612, OPN2V_ym2612, &clock.master_cycle_count, 32 * 7 * 6);
    //SN76489_init(&psg);
    snprintf(label, sizeof(label), "TurboGraFX-16");

    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_huc6280;
    dt.ptr = (void*)this;
    cpu.setup_tracing(&dt, &clock.master_cycles, 1);

    jsm.described_inputs = false;
}

void TG16_delete() {
}

static inline float u16_to_float2(u16 val)
{
    assert(val < 47431); // (1F * FF) * 6
    return static_cast<float>(val) / 47500.0f;
}

static inline float ch_to_float(i16 val)
{
    return static_cast<float>(val) / 7906.0f;
}


static inline float u16_to_float(i16 val)
{
    return static_cast<float>(val) / 20000.0f;
}

static inline float i16_to_float(i16 val)
{
    return ((static_cast<float>(static_cast<i32>(val) + 32768) / 65535.0f) * 2.0f) - 1.0f;
}

void TG16::core::sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<TG16::core *>(ptr);
    if (th->audio.buf) {
        th->audio.cycles++;
        th->audio.next_sample_cycle += th->audio.master_cycles_per_audio_sample;
        th->scheduler.only_add_abs((i64)th->audio.next_sample_cycle, 0, th, &sample_audio, nullptr);
        if (th->audio.buf->upos < (th->audio.buf->samples_len << 1)) {
            //if (ls != 0) printf("\nLS: %d", ls);
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos] = u16_to_float2(th->cpu.psg.out.l);
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos+1] = u16_to_float2(th->cpu.psg.out.r);
        }
        th->audio.buf->upos+=2;
    }
}

void TG16::core::sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<TG16::core *>(ptr);

    // PSG
    debug_waveform *dw = th->dbg.waveforms_psg.main_cache;
    if (dw->user.buf_pos < dw->samples_requested) {
        u32 a = (th->cpu.psg.out.l + th->cpu.psg.out.l) >> 1;
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = u16_to_float(a);
        dw->user.buf_pos++;
    }
    th->audio.next_sample_cycle_max += th->audio.master_cycles_per_max_sample;
    th->scheduler.only_add_abs((i64)th->audio.next_sample_cycle_max, 0, th, &sample_audio_debug_max, nullptr);
}

void TG16::core::sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<TG16::core *>(ptr);

    // PSG
    //dw = cpg(dbg.waveforms_psg.chan[0]);
    for (int j = 0; j < 6; j++) {
        //dw = cpg(dbg.waveforms_psg.chan[j]);
        debug_waveform *dw = th->dbg.waveforms_psg.chan_cache[j];
        if (dw->user.buf_pos < dw->samples_requested) {
            u16 sv = th->cpu.psg.channels[j].debug_sample();
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = ch_to_float(sv);
            dw->user.buf_pos++;
        }
    }

     th->audio.next_sample_cycle_min += th->audio.master_cycles_per_min_sample;
     th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_min), 0, th, &sample_audio_debug_min, nullptr);
}


static void TG16IO_load_cart(jsm_system *ptr, multi_file_set& mfs, physical_io_device &whichpio)
{
    // 512kb usless header
    // check if bit 9 is set and discard first 512kb then
    buf* b = &mfs.files[0].buf;

    auto *th = static_cast<TG16::core *>(ptr);
    u8 *bptr = static_cast<u8 *>(b->ptr);
    u64 sz = b->size;
    if (sz & 512) {
        bptr += 512;
        sz -= 512;
    }

   th->cart.load_ROM_from_RAM(bptr, sz, whichpio);
   th->reset();
}

static void TG16IO_unload_cart(jsm_system *ptr)
{
}

void TG16::core::setup_crt(JSM_DISPLAY &d)
{
    d.kind = jsm::CRT;
    d.enabled = true;

    d.fps = 60; //PAL ? 50 : 60.0988;
    d.fps_override_hint = clock.timing.second.frames;

    d.pixelometry.cols.left_hblank = 16;
    d.pixelometry.cols.visible = HUC6260::CYCLE_PER_LINE;
    d.pixelometry.cols.max_visible = HUC6260::CYCLE_PER_LINE;
    d.pixelometry.cols.right_hblank = 221;
    d.pixelometry.offset.x = 0;

    d.pixelometry.rows.top_vblank = 0;
    d.pixelometry.rows.visible = 242;
    d.pixelometry.rows.max_visible = 242;
    d.pixelometry.rows.bottom_vblank = 20;
    d.pixelometry.offset.y = 0;

    d.geometry.physical_aspect_ratio.width = 4;
    d.geometry.physical_aspect_ratio.height = 3;

    d.pixelometry.overscan.left = 192;
    d.pixelometry.overscan.right = 45;
    d.pixelometry.overscan.top = d.pixelometry.overscan.bottom = 0;
}

void TG16::core::setup_audio()
{
    physical_io_device *pio = &IOs.emplace_back();
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    //chan->sample_rate = 3576000;
    chan->sample_rate = 52000;
    chan->left = chan->right = 1;
    chan->num = 2;
    chan->low_pass_filter = 24000;
}

void TG16::core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = true;
    IOs.reserve(15);

    // controllers
    physical_io_device *c1 = &IOs.emplace_back();
    controller.setup_pio(*c1, 0, "Player 1", 1);

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
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
    d->init(HID_CART_PORT, true, true, true, true);;
    d->cartridge_port.load_cart = &TG16IO_load_cart;
    d->cartridge_port.unload_cart = &TG16IO_unload_cart;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, true, true, false, true);
    d->display.output[0] = malloc(HUC6260::CYCLE_PER_LINE * 480 * 2);
    d->display.output[1] = malloc(HUC6260::CYCLE_PER_LINE * 480 * 2);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_crt(d->display);
    vce.display_ptr.make(IOs, IOs.size()-1);
    d->display.last_written = 1;
    vce.cur_output = static_cast<u16 *>(d->display.output[0]);
    vce.display = &vce.display_ptr.get().display;

    setup_audio();

    vce.display = &vce.display_ptr.get().display;
    controller_port.connect(TG16::controller_kinds::CK_2button, &controller);
}

void TG16::core::play()
{
}

void TG16::core::pause()
{
}

void TG16::core::stop()
{
}

#define PSG_CYCLES 6

void TG16::core::schedule_first()
{
    cpu.schedule_first(0);
    vce.schedule_first();
    scheduler.only_add_abs(PSG_CYCLES, 0, this, &psg_go, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_max), 0, this, &sample_audio_debug_max, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_min), 0, this, &sample_audio_debug_min, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle), 0, this, &sample_audio, nullptr);
}

void TG16::core::psg_go(void *ptr, u64 key, u64 clock, u32 jitter)
{
     auto *th = static_cast<TG16::core *>(ptr);
    u64 cur = clock - jitter;
    th->cpu.psg.cycle();

    th->scheduler.only_add_abs(cur + PSG_CYCLES, 0, th, &psg_go, nullptr);
}

void TG16::core::get_framevars(framevars& out)
{
    out.master_frame = vce.master_frame;
    out.x = 0;
    out.scanline = vce.regs.y - 64;
    out.master_cycle = clock.master_cycles;
}

void TG16::core::reset()
{
    clock.reset();
    cpu.reset();
    vdc0.reset();
    vdc1.reset();
    vce.reset();
    cart.reset();

    scheduler.clear();
    schedule_first();
    printf("\nTG16 reset!");
}

//#define DO_STATS

void TG16::core::fixup_audio()
{
    if (!dbg.waveforms_psg.main_cache) {
        dbg.waveforms_psg.main_cache = &dbg.waveforms_psg.main.get();
        for (u32 i = 0; i < 6; i++) {
            dbg.waveforms_psg.chan_cache[i] = &dbg.waveforms_psg.chan[i].get();
        }
    }
}

u32 TG16::core::finish_frame()
{
    fixup_audio();

#ifdef TG16_LYCODER2
    dbg.trace_on = 1;
#endif
    scheduler.run_til_tag_tg16(TAG_FRAME);
#ifdef TG16_LYCODER2
    dbg_flush();
#endif

    return vce.display->last_written;
}

#define MIN(x,y) ((x) < (y) ? (x) : (y))
u32 TG16::core::finish_scanline()
{
    fixup_audio();
    scheduler.run_til_tag_tg16(TAG_SCANLINE);

    return vce.display->last_written;
}

u32 TG16::core::step_master(u32 howmany)
{
    fixup_audio();
    scheduler.run_for_cycles_tg16(howmany);
    return 0;
}

void TG16::core::load_BIOS(multi_file_set& mfs)
{
    printf("\nTG16 doesn't have a BIOS...?");
}


