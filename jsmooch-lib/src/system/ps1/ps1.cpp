//
// Created by . on 2/11/25.
//

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <math.h>

#include "helpers/scheduler.h"
#include "ps1.h"
#include "ps1_bus.h"
#include "gpu/ps1_gpu.h"
#include "ps1_debugger.h"
#include "peripheral/ps1_sio.h"
#include "peripheral/ps1_digital_pad.h"


#include "component/cpu/r3000/r3000.h"

#include "helpers/multisize_memaccess.cpp"
#include "helpers/physical_io.h"

// 240x160, but 308x228 with v and h blanks

jsm_system *PS1_new()
{
    return new PS1::core();
}

void PS1_delete(jsm_system *sys) {
    delete sys;
}

namespace PS1 {

static void sch_sample_audio(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->sample_audio();
}

//typedef void (*scheduler_callback)(void *bound_ptr, u64 user_key, u64 current_clock, u32 jitter);
static void vblank(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    // First function call on a new frame
    auto *th = static_cast<core *>(bound_ptr);
    static u64 last = 0;
    u64 diff = current_clock - last;
    last = current_clock;
    //printf("\nVBLANK @cycle:%lld diff:%lld val:%lld", th->clock.master_cycle_count, diff, key);
    th->set_irq(IRQ_VBlank, key);
    th->gpu.new_frame();
    th->clock.in_vblank = key;
    th->timers[1].vblank(key);
}

static void hblank(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    // Do what here?
    auto *th = static_cast<core *>(bound_ptr);
    th->clock.in_hblank = key;
    th->clock.hblank_clock += key;
    th->timers[0].hblank(key);
}

void do_next_scheduled_frame(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    auto *th = static_cast<core *>(bound_ptr);
    th->schedule_frame(current_clock-jitter, 0);
}

static inline float i16_to_float(i16 val)
{
    return ((static_cast<float>(static_cast<i32>(val) + 32768) / 65535.0f) * 2.0f) - 1.0f;
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);

    /* PSG */
    auto *dw = th->dbg.waveforms2.main_cache;
    if (dw->user.buf_pos < (dw->samples_requested << 1)) {
        static_cast<float *>(dw->buf[dw->rendering_buf].ptr)[dw->user.buf_pos] = i16_to_float(th->spu.sample_l);
        dw->user.buf_pos++;
        static_cast<float *>(dw->buf[dw->rendering_buf].ptr)[dw->user.buf_pos] = i16_to_float(th->spu.sample_r);
        dw->user.buf_pos++;
    }

    th->audio.next_sample_cycle_max += th->audio.master_cycles_per_max_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_max), 0, th, &sample_audio_debug_max, nullptr);
}

static void sample_audio_to_stereo(core *th, debug::waveform2::wf *dw, i16 l, i16 r, bool &ee) {
    ee = th->audio.nosolo || dw->ch_output_solo;
    if (dw->user.buf_pos < (dw->samples_requested << 1)) {
        static_cast<float *>(dw->buf[dw->rendering_buf].ptr)[dw->user.buf_pos] = i16_to_float(l);
        dw->user.buf_pos++;
        static_cast<float *>(dw->buf[dw->rendering_buf].ptr)[dw->user.buf_pos] = i16_to_float(r);
        dw->user.buf_pos++;
    }
}

static void sample_audio_debug_med(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);

    sample_audio_to_stereo(th, th->dbg.waveforms2.cd.chan_cache[0], th->spu.capture.sample.cd_l, th->spu.capture.sample.cd_r, th->spu.capture.cd_ext_enable);
    sample_audio_to_stereo(th, th->dbg.waveforms2.reverb.chan_cache[0], th->spu.reverb.in_l, th->spu.reverb.in_r, th->spu.reverb.ext_enable_in);
    sample_audio_to_stereo(th, th->dbg.waveforms2.reverb.chan_cache[1], th->spu.reverb.sample_l, th->spu.reverb.sample_r, th->spu.reverb.ext_enable_out);

    th->audio.next_sample_cycle_med += th->audio.master_cycles_per_med_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_med), 0, th, &sample_audio_debug_med, nullptr);

}


static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<core *>(ptr);

    /* PSG */
    for (int j = 0; j < 24; j++) {
        auto *dw = &th->dbg.waveforms2.channels.chan[j].get().data;
        if (dw->user.buf_pos < dw->samples_requested) {
            i16 sv = th->spu.voices[j].sample;
            static_cast<float *>(dw->buf[dw->rendering_buf].ptr)[dw->user.buf_pos] = i16_to_float(sv);
            dw->user.buf_pos++;
        }
    }
    th->audio.next_sample_cycle_min += th->audio.master_cycles_per_min_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_min), 0, th, &sample_audio_debug_min, nullptr);
}



void core::schedule_frame(u64 start_clock, u32 is_first)
{
    if (!is_first) {
        //printf("\nNEW FRAME %lld", clock.master_frame);
        clock.master_frame++;
        clock.frame_cycle_start = start_clock;
    }
    else {
        if (audio.master_cycles_per_audio_sample == 0) {
            audio.next_sample_cycle = 0;
            scheduler.only_add_abs(768, 0, this, &sch_sample_audio, nullptr);
        }
        scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_med), 0, this, &sample_audio_debug_med, nullptr);
        scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_max), 0, this, &sample_audio_debug_max, nullptr);
        scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_min), 0, this, &sample_audio_debug_min, nullptr);
    }
    // Schedule out a frame!
    i64 cur_clock = start_clock;
    clock.cycles_left_this_frame += clock.timing.frame.cycles;

    // End VBlank
    scheduler.only_add_abs(cur_clock, 0, this, &vblank, nullptr);

    // x lines of end hblank, start hblank
    // somewhere in there, start vblank
    for (u32 line = 0; line < clock.timing.frame.lines; line++) {
        scheduler.only_add_abs(cur_clock, 0, this, &hblank, nullptr);
        if (line == clock.timing.frame.vblank.start_on_line) {
            scheduler.only_add_abs(cur_clock, 1, this, &vblank, nullptr);
        }

        cur_clock += clock.timing.scanline.cycles;
        scheduler.only_add_abs(cur_clock, 1, this, &hblank, nullptr);
    }

    scheduler.only_add_abs(start_clock+clock.timing.frame.cycles, 0, this, &do_next_scheduled_frame, nullptr);
}

void core::setup_debug_waveform(debug::waveform2::wf *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = audio.cycles_per_frame / static_cast<float>(dw->samples_requested);
    dw->user.buf_pos = 0;
}

void core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    audio.nosolo = !dbg.waveforms2.view.get().waveform2.any_solo;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.cycles_per_frame = static_cast<double>(clock.timing.cpu.hz)/static_cast<double>(clock.timing.fps);
        audio.master_cycles_per_audio_sample = (audio.cycles_per_frame / static_cast<double>(ab->samples_len));
        audio.next_sample_cycle = 0;
        audio.next_sample_cycle_min = 0;
        audio.next_sample_cycle_med = 0;
        audio.next_sample_cycle_max = 0;
        // Setup main output view
        auto *wf = &dbg.waveforms2.main->data;
        spu.ext_enable = audio.nosolo || wf->ch_output_solo;
        audio.master_cycles_per_max_sample = audio.cycles_per_frame / static_cast<float>(wf->samples_requested);
        // Setup some math for voices
        auto *v = &dbg.waveforms2.cd.chan[0].get().data;
        audio.master_cycles_per_min_sample = audio.cycles_per_frame / static_cast<float>(wf->samples_requested);
        // Now for "bigger" displays...
        v = &dbg.waveforms2.reverb.chan[0].get().data;
        audio.master_cycles_per_med_sample = audio.cycles_per_frame / static_cast<float>(wf->samples_requested);
    }

    // Setup main channel, maxi.
    auto *wf = dbg.waveforms2.main_cache;
    setup_debug_waveform(wf);
    spu.ext_enable = audio.nosolo || wf->ch_output_solo;

    // Setup the 24 voices (mini)
    for (u32 i = 0; i < 24; i++) {
        auto *v = &dbg.waveforms2.channels.chan[i].get().data;
        setup_debug_waveform(v);
        spu.voices[i].ext_enable = audio.nosolo || v->ch_output_solo;
    }

    // Setup CD (medium)
    auto *v = &dbg.waveforms2.cd.chan[0].get().data;
    setup_debug_waveform(v);
    spu.capture.cd_ext_enable = audio.nosolo || v->ch_output_solo;

    // Setup Reverb (medium)
    v = &dbg.waveforms2.reverb.chan[0].get().data; // in
    setup_debug_waveform(v);
    spu.reverb.ext_enable_in = audio.nosolo || v->ch_output_solo;

    v = &dbg.waveforms2.reverb.chan[1].get().data; // out
    setup_debug_waveform(v);
    spu.reverb.ext_enable_out = audio.nosolo || v->ch_output_solo;
}

void core::amidog_print_console()
{
    static constexpr char args[3][15] = {"auto", "console", "release"};
    int argLen = 2;
    int len = 0;

    for (int i = 0; i < argLen; i++) {
        mainbus_write(static_cast<u32>(0x1f800004 + i * 4), 4, (u32)(0x1f800044+len));
        int x;
        unsigned long n = strlen(args[i]) + 1;
        for (x = len; x < len + n; x++) {
            mainbus_write(static_cast<u32>(0x1f800044 + x), 1, args[i][x-len]);
        }

        len = x;
    }

    mainbus_write(0x1f800000, 4, argLen);
}

void core::BIOS_patch_reset()
{
    memcpy(mem.BIOS, mem.BIOS_unpatched, 512 * 1024);
}

void core::BIOS_patch(u32 addr, u32 val)
{
    cW32(mem.BIOS, addr, val);
}

void core::sideload(multi_file_set &fs) {
    sideloaded.allocate(fs.files[0].buf.size);
    memcpy(sideloaded.ptr, fs.files[0].buf.ptr, fs.files[0].buf.size);
}


void core::setup_IRQs()
{
    IRQ_multiplexer.setup_irq(IRQ_VBlank, "vblank", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_GPU, "gpu", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_CDROM, "cdrom", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_DMA, "dma", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_TMR0, "tmr0", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_TMR1, "tmr1", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_TMR2, "tmr2", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_SIO0, "sio0_recv", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_SIO1, "sio", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_SPU, "spu", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(IRQ_PIOLightpen, "lightpen_pio_dtl", IRQMBK_edge_0_to_1);
}

void core::copy_vram()
{
    memcpy(gpu.cur_output, gpu.VRAM, 1024*1024);
    gpu.cur_output = ((u16 *)gpu.display->output[gpu.display->last_written ^ 1]);
    gpu.display->last_written ^= 1;
}

u32 core::finish_frame()
{
#ifdef LYCODER
    //dbg_enable_trace();
#endif
    u64 old_clock = clock.master_cycle_count;
    scheduler.run_for_cycles(clock.cycles_left_this_frame);
    u64 diff = clock.master_cycle_count - old_clock;
    clock.cycles_left_this_frame -= diff;

    copy_vram();
    if (::dbg.do_break) {
        ::dbg.trace_on += 1;
        dbg_flush();
        ::dbg.trace_on -= 1;
    }
        ::dbg.trace_on += 1;
    dbg_flush();
        ::dbg.trace_on -= 1;
    return 0;
}

void core::play()
{
}

void PS1J_pause()
{
}

void PS1J_stop()
{
}

void core::get_framevars(framevars& out)
{
    out.master_frame = clock.master_frame;
    out.x = 0;
    out.scanline = clock.crt.y;
    out.master_cycle = clock.master_cycle_count;
}

void core::skip_BIOS()
{
}

void core::sideload_EXE(buf *w)
{
    u8 *r = static_cast<u8 *>(w->ptr);
    if ((r[0] == 80) && (r[1] == 83) && (r[2] == 45) &&
        (r[3] == 88) && (r[4] == 32) && (r[5] == 69) &&
        (r[6] == 88) && (r[7] == 69)) {
        printf("\nOK EXE!");
        u32 initial_pc = cR32(r, 0x10);
        u32 initial_gp = cR32(r, 0x14);
        u32 load_addr = cR32(r, 0x18);
        u32 file_size = cR32(r, 0x1C);
        u32 memfill_start = cR32(r, 0x28);
        u32 memfill_size = cR32(r, 0x2C);
        u32 initial_sp_base = cR32(r, 0x30);
        u32 initial_sp_offset = cR32(r, 0x34);
        BIOS_patch_reset();

        if (file_size >= 4) {
            u32 address_read = 0x800;
            u32 address_write = load_addr & 0x1FFFFF;
            //printf("\nWrite %d bytes from %08x to %08x", file_size, address_write, address_write+file_size);
            for (u32 i = 0; i < file_size; i+=4) {
                u32 data = cR32(r, address_read);
                cW32(mem.MRAM, address_write, data);
                address_read += 4;
                address_write += 4;
            }
        }

        // PC has to  e done first because we can't load it in thedelay slot?
        BIOS_patch(0x6FF0, 0x3C080000 | (initial_pc >> 16));    // lui $t0, (r_pc >> 16)
        BIOS_patch(0x6FF4, 0x35080000 | (initial_pc & 0xFFFF));  // ori $t0, $t0, (r_pc & 0xFFFF)
        //printf("\nInitial PC: %08x", initial_pc);
        BIOS_patch(0x6FF8, 0x3C1C0000 | (initial_gp >> 16));    // lui $gp, (r_gp >> 16)
        BIOS_patch(0x6FFC, 0x379C0000 | (initial_gp & 0xFFFF));  // ori $gp, $gp, (r_gp & 0xFFFF)

        u32 r_sp = initial_sp_base + initial_sp_offset;
        if (r_sp != 0) {
            BIOS_patch(0x7000, 0x3C1D0000 | (r_sp >> 16));   // lui $sp, (r_sp >> 16)
            BIOS_patch(0x7004, 0x37BD0000 | (r_sp & 0xFFFF)); // ori $sp, $sp, (r_sp & 0xFFFF)
        } else {
            BIOS_patch(0x7000, 0); // NOP
            BIOS_patch(0x7004, 0); // NOP
        }

        u32 r_fp = r_sp;
        if (r_fp != 0) {
            BIOS_patch(0x7008, 0x3C1E0000 | (r_fp >> 16));   // lui $fp, (r_fp >> 16)
            BIOS_patch(0x700C, 0x01000008);                   // jr $t0
            BIOS_patch(0x7010, 0x37DE0000 | (r_fp & 0xFFFF)); // // ori $fp, $fp, (r_fp & 0xFFFF)
        } else {
            BIOS_patch(0x7008, 0);   // nop
            BIOS_patch(0x700C, 0x01000008);                   // jr $t0
            BIOS_patch(0x7010, 0); // // nop
        }
        printf("\nBIOS patched!");
    }
    else {
        printf("\nNOT OK EXE!");
    }
}

void core::reset()
{
    cpu.reset();
    dma.reset();
    spu.reset();
    io.cached_isolated = false;

    gpu.reset();
    clock.reset();
    for (auto &t : timers) {
        t.reset();
    }
    scheduler.clear();
    clock.cycles_left_this_frame = 0;
    schedule_frame(0, 1); // Schedule first frame

    printf("\nPS1 reset!");
    if (sideloaded.size > 0) {
        sideload_EXE(&sideloaded);
        //amidog_print_console();
    }

    cdrom.open_drive();
}

void core::sample_audio()
{
    if (audio.buf) {
        audio.cycles++;
        spu.cycle();

        if (audio.buf->upos < (audio.buf->samples_len << 1)) {
            float l = i16_to_float(static_cast<i16>(spu.sample_l));
            float r = i16_to_float(static_cast<i16>(spu.sample_r));;
            static_cast<float *>(audio.buf->ptr)[audio.buf->upos] = l;
            static_cast<float *>(audio.buf->ptr)[audio.buf->upos+1] = r;
        }
        audio.buf->upos+=2;
    }

    audio.next_sample_cycle += audio.master_cycles_per_audio_sample;
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle), 0, this, &sch_sample_audio, nullptr);
}

u32 core::finish_scanline()
{
    printf("\nNOT SUPPORT SCANLINE ADVANCE");
    return 0;
}


u32 core::step_master(u32 howmany)
{
    cycles_left = 0;
    scheduler.run_for_cycles(howmany);
    return 0;
}

void core::load_BIOS(multi_file_set& mfs)
{
    if (mfs.files[0].buf.size != (512*1024)) {
        printf("\nbad BIOS!");
        return;
    }
    memcpy(mem.BIOS, mfs.files[0].buf.ptr, 512*1024);
    memcpy(mem.BIOS_unpatched, mfs.files[0].buf.ptr, 512*1024);
    printf("\nLOADED BIOS!");
}

static void setup_crt(JSM_DISPLAY *d)
{
    d->kind = jsm::CRT;
    d->enabled = true;

    d->fps = 59.727;
    d->fps_override_hint = 60;
    // 240x160, but 308x228 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 1024;
    d->pixelometry.cols.max_visible = 1024;
    d->pixelometry.cols.right_hblank = 0;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 512;
    d->pixelometry.rows.max_visible = 512;
    d->pixelometry.rows.bottom_vblank = 0;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 2;
    d->geometry.physical_aspect_ratio.height = 1;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}
/*
    void (*insert_disc)(jsm_system *ptr, physical_io_device &pio, multi_file_set& mfs){};
    void (*remove_disc)(jsm_system *ptr){};
    void (*close_drive)(jsm_system *ptr){};
    void (*open_drive)(jsm_system *ptr){};
*/

void IO_insert_disc(jsm_system *ptr, physical_io_device &pio, multi_file_set& mfs) {
    printf("\nJSM INSERT DISC");
    auto *th = static_cast<core *>(ptr);
    th->cdrom.insert_disc(mfs);
}

void IO_remove_disc(jsm_system *ptr) {
    auto *th = static_cast<core *>(ptr);
    th->cdrom.remove_disc();
}

void IO_close_drive(jsm_system *ptr) {
    auto *th = static_cast<core *>(ptr);
    th->cdrom.close_drive();
}

void IO_open_drive(jsm_system *ptr) {
    auto *th = static_cast<core *>(ptr);
    th->cdrom.open_drive();
}

void core::setup_cdrom() {
    printf("\nSETTING UP CDROM!");
    physical_io_device *pio = &IOs.emplace_back();
    pio->init(HID_DISC_DRIVE, true, true, true, false);
    cdrom.pio_ptr.make(IOs, IOs.size()-1);
    cdrom.dd = &pio->disc_drive;
    cdrom.dd->open_drive = &IO_open_drive;
    cdrom.dd->remove_disc = &IO_remove_disc;
    cdrom.dd->insert_disc = &IO_insert_disc;
    cdrom.dd->close_drive = &IO_close_drive;
}

void core::setup_audio()
{
    physical_io_device *pio = &IOs.emplace_back();
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 44100;
    chan->left = chan->right = 1;
    chan->num = 2;
    chan->low_pass_filter = 22050;
}


void core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = true;
    IOs.reserve(15);

    // controllers
    physical_io_device *controller = &IOs.emplace_back();
    sio0.gamepad_setup_pio(controller, 1, "Controller 1", 1);
    io.controller1.pio = controller;
    io.controller1.bus = this;
    sio0.io.controller1 = &io.controller1.interface;


    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
    HID_digital_button* b;

    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(1024*1024);
    d->display.output[1] = malloc(1024*1024);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_crt(&d->display);
    gpu.display_ptr.make(IOs, IOs.size() - 1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    gpu.cur_output = static_cast<u16 *>(d->display.output[0]);

    setup_audio();
    setup_cdrom();

    gpu.display = &gpu.display_ptr.get().display;
}

void core::stop() {}
void core::pause() {}
}