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


//typedef void (*scheduler_callback)(void *bound_ptr, u64 user_key, u64 current_clock, u32 jitter);
static void vblank(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    // First function call on a new frame
    auto *th = static_cast<core *>(bound_ptr);
    th->set_irq(IRQ_VBlank, key);
    th->clock.in_vblank = key;
    th->timers[1].vblank(key);
}

static void hblank(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    // Do what here?
    auto *th = (core *)bound_ptr;
    th->clock.in_hblank = key;
    th->clock.hblank_clock += key;
    th->timers[0].hblank(key);
}

void do_next_scheduled_frame(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    auto *th = static_cast<core *>(bound_ptr);
    th->schedule_frame(current_clock-jitter, 0);
}


void core::schedule_frame(u64 start_clock, u32 is_first)
{
    if (!is_first) {
        clock.master_frame++;
        clock.frame_cycle_start = start_clock;
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


static void setup_debug_waveform(debug_waveform *dw)
{
    /*if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;*/
}

void core::set_audiobuf(audiobuf *ab)
{
    /*audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / (float)ab->samples_len));
        audio.next_sample_cycle = 0;
        debug_waveform *wf = (debug_waveform *)cvec_get(dbg.waveforms.main.vec, dbg.waveforms.main.index);
        apu.ext_enable = wf->ch_output_enabled;
    }

    // PSG
    setup_debug_waveform(cvec_get(dbg.waveforms.main.vec, dbg.waveforms.main.index));
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index));
        debug_waveform *wf = (debug_waveform *)cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index);
        if (i < 4)
            apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }
    */
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
    IRQ_multiplexer.setup_irq(0, "vblank", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(1, "gpu", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(2, "cdrom", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(3, "dma", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(4, "tmr0", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(5, "tmr1", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(6, "tmr2", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(7, "sio0_recv", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(8, "sio", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(9, "spu", IRQMBK_edge_0_to_1);
    IRQ_multiplexer.setup_irq(10, "lightpen_pio_dtl", IRQMBK_edge_0_to_1);
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
    dbg_enable_trace();
#endif
    u64 old_clock = clock.master_cycle_count;
    scheduler.run_for_cycles(clock.cycles_left_this_frame);
    u64 diff = clock.master_cycle_count - old_clock;
    clock.cycles_left_this_frame -= diff;

    copy_vram();
    if (::dbg.do_break) {
        printf("\nDUMP!");
        ::dbg.trace_on += 1;
        dbg_flush();
        ::dbg.trace_on -= 1;
    }
    dbg_flush();
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
            printf("\nWrite %d bytes from %08x to %08x", file_size, address_write, address_write+file_size);
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
        printf("\nInitial PC: %08x", initial_pc);
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
        amidog_print_console();
    }
}

void core::sample_audio(u32 num_cycles)
{
    /*
    for (u64 i = 0; i < num_cycles; i++) {
        PS1_APU_cycle();
        u64 mc = clock.master_cycle_count + i;
        if (audio.buf && (mc >= (u64) audio.next_sample_cycle)) {
            audio.next_sample_cycle += audio.master_cycles_per_audio_sample;
            if (audio.buf->upos < audio.buf->samples_len) {
                ((float *)(audio.buf->ptr))[audio.buf->upos] = PS1_APU_mix_sample(0);
            }
            audio.buf->upos++;
        }

        debug_waveform *dw = cpg(dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = PS1_APU_mix_sample(1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = PS1_APU_sample_channel(j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }
     */
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
    printf("\nJSM REMOVE DISC");
}

void IO_close_drive(jsm_system *ptr) {
    printf("\nJSM CLOSE DRIVE");
}

void IO_open_drive(jsm_system *ptr) {
    printf("\nJSM OPEN DRIVE");
}

void core::setup_cdrom() {
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
    chan->sample_rate = 22050;
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
    d->init(HID_DISC_DRIVE, 1, 1, 1, 0);
    // TODO: this
    d->disc_drive.open_drive = nullptr;
    d->disc_drive.remove_disc = nullptr;
    d->disc_drive.insert_disc = nullptr;
    d->disc_drive.close_drive = nullptr;

    // screen
    d = &IOs.emplace_back();
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