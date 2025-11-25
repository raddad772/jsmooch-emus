//
// Created by Dave on 2/7/2024.
//

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "sms_gg.h"
#include "sms_gg_clock.h"
#include "sms_gg_io.h"
#include "sms_gg_bus.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_vdp.h"
#include "smsgg_debugger.h"
#include "sms_gg_serialize.h"

#define JSM jsm_system* jsm

static float i16_to_float(i16 val)
{
    return ((static_cast<float>(static_cast<i32>(val) + 32768) / 65535.0f) * 2.0f) - 1.0f;
}

u32 SMSGG_CPU_read_trace(void *ptr, u32 addr)
{
    auto* th = static_cast<SMSGG::core *>(ptr);
    return th->main_bus_read(addr, 0);
}

#define MASTER_CYCLES_PER_FRAME 179208
static void setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = (static_cast<float>(MASTER_CYCLES_PER_FRAME) / static_cast<float>(dw->samples_requested));
    dw->user.buf_pos = 0;
}

void SMSGG::core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = (static_cast<float>(MASTER_CYCLES_PER_FRAME) / static_cast<float>(ab->samples_len));
        audio.next_sample_cycle = 0;
        auto *wf = &dbg.waveforms.main.get();
        sn76489.ext_enable = wf->ch_output_enabled;
    }
    debug_waveform *wf = &dbg.waveforms.main.get();
    setup_debug_waveform(wf);
    if (wf->clock_divider == 0) wf->clock_divider = wf->default_clock_divider;
    clock.apu_divisor = wf->clock_divider;
    for (u32 i = 0; i < 4; i++) {
        wf = &dbg.waveforms.chan[i].get();
        setup_debug_waveform(wf);
        if (i < 3) {
            sn76489.sw[i].ext_enable = wf->ch_output_enabled;
        }
        else
            sn76489.noise.ext_enable = wf->ch_output_enabled;
    }
}

jsm_system *SMSGG_new(jsm::systems variant, jsm::regions region) {
    auto* th = new SMSGG::core(variant, region);

    // setup tracing reads
    jsm_debug_read_trace a;
    a.ptr = static_cast<void *>(th);
    a.read_trace = &SMSGG_CPU_read_trace;
    th->cpu.setup_tracing(&a, &th->clock.master_cycles);

    // bus init
    th->io.portA.init(variant, 1);
    th->io.portB.init(variant, 2);
    th->io.portA.attached_device = &th->io.controllerA;

    th->io.disable = 0;
    th->io.gg_start = 0;

    switch(variant) {
        case jsm::systems::SG1000:
            th->cpu_in = &SMSGG::core::cpu_in_sms1;
            th->cpu_out = &SMSGG::core::cpu_out_sms1;
            snprintf(th->label, sizeof(th->label), "Sega Game 1000");
            break;
        case jsm::systems::SMS1:
            th->cpu_in = &SMSGG::core::cpu_in_sms1;
            th->cpu_out = &SMSGG::core::cpu_out_sms1;
            snprintf(th->label, sizeof(th->label), "Sega Master System");
            break;
        case jsm::systems::SMS2:
            th->cpu_in = &SMSGG::core::cpu_in_sms1;
            th->cpu_out = &SMSGG::core::cpu_out_sms1;
            snprintf(th->label, sizeof(th->label), "Sega Master System II");
            break;
        case jsm::systems::GG:
            th->cpu_in = &SMSGG::core::cpu_in_gg;
            th->cpu_out = &SMSGG::core::cpu_out_sms1;
            snprintf(th->label, sizeof(th->label), "Sega Game Gear");
            break;
        default:
            assert(1!=0);
            return nullptr;
    }

    // bus init done
    th->last_frame = 0;

    th->audio.buf = nullptr;

    th->vdp.reset();
    th->sn76489.reset();

    th->described_inputs = 0;

    return th;
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

static void setup_audio(std::vector<physical_io_device> &inIOs)
{
    physical_io_device &pio = inIOs.emplace_back();
    pio.kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio.audio_channel;
    chan->sample_rate = (MASTER_CYCLES_PER_FRAME * 60) / 48;
    chan->low_pass_filter = 16000;
}

static void SMSGGIO_unload_cart(JSM);
static void SMSGGIO_load_cart(JSM, multi_file_set &mfs, physical_io_device &pio);

void SMSGG::core::describe_io()
{
    if (described_inputs) return;
    described_inputs = 1;

    // controllers
    physical_io_device *d =  &IOs.emplace_back();
    SMSGG_gamepad::setup_pio(*d, 0, "Player A", 1, 1);
    io.controllerA.device_ptr.make(IOs, IOs.size()-1);
    if (variant != jsm::systems::GG) {
        d = &IOs.emplace_back();
        SMSGG_gamepad::setup_pio(*d, 1, "Player B", 0, 0);
        io.controllerB.device_ptr.make(IOs, IOs.size()-1);
    }

    // power, reset, and pause buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, 1, 1, 1, 1);
    HID_digital_button* b = &chassis->chassis.digital_buttons.emplace_back();;
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    if (variant != jsm::systems::GG) {
        b = &chassis->chassis.digital_buttons.emplace_back();
        b->common_id = DBCID_ch_reset;
        snprintf(b->name, sizeof(b->name), "Reset");
        b->state = 0;
    }

    io.pause_button = nullptr;

    if (variant != jsm::systems::GG) {
        b = &chassis->chassis.digital_buttons.emplace_back();
        b->common_id = DBCID_ch_pause;
        snprintf(b->name, sizeof(b->name), "Pause");
        b->state = 0;

        io.pause_button = b;
    }

    // cartridge port
    d = &IOs.emplace_back();
    d->init(HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &SMSGGIO_load_cart;
    d->cartridge_port.unload_cart = &SMSGGIO_unload_cart;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(SMSGG_DISPLAY_DRAW_SZ);
    d->display.output[1] = malloc(SMSGG_DISPLAY_DRAW_SZ);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    vdp.display_ptr.make(IOs, IOs.size()-1);
    if (variant == jsm::systems::GG)
        setup_lcd_gg(&d->display);
    else
        setup_crt_sms(&d->display);

    vdp.display = nullptr;
    vdp.cur_output = static_cast<u16 *>(d->display.output[0]);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;

    // Audio
    setup_audio(IOs);

    physical_io_device *pio = &vdp.display_ptr.get();
    vdp.display = &pio->display;
}

void SMSGG::core::get_framevars(framevars &out)
{
    out.master_frame = clock.frames_since_restart;
    out.x = clock.hpos;
    out.scanline = clock.vpos;
}

void SMSGG::core::reset()
{
    cpu.reset();
    vdp.reset();
    mapper.reset();
    sn76489.reset();
    last_frame = 0;
}

void SMSGG::core::killall()
{
}

u32 SMSGG::core::finish_frame()
{
    u32 current_frame = clock.frames_since_restart;
    u32 scanlines_done = 0;
    clock.cpu_frame_cycle = 0;
    while(current_frame == clock.frames_since_restart) {
        scanlines_done++;
        finish_scanline();
        if (::dbg.do_break) return vdp.display->last_written;
    }
    return vdp.display->last_written;
}

u32 SMSGG::core::finish_scanline()
{
    u32 cycles_left = 684 - (clock.hpos * 2);
    step_master(cycles_left);
    return 0;
}

void SMSGG::core::cpu_cycle()
{
    if (cpu.pins.RD) {
        if (cpu.pins.MRQ) {// read ROM/RAM
            cpu.pins.D = main_bus_read(cpu.pins.Addr, 1);
#ifndef LYCODER

                // Z80(    25)r   0006   $18     TCU:1
                //dbg_printf(DBGC_Z80 "\nSMS(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, cpu.trace_cycles, trace_format_read('Z80', Z80_COLOR, cpu.trace_cycles, cpu.pins.Addr, cpu.pins.D, null, cpu.regs.TCU));
            dbgloglog(this, SMSGG_CAT_CPU_READ, DBGLS_INFO, "%04x   (read)  %02x", cpu.pins.Addr, cpu.pins.D);
#endif
        } else if (cpu.pins.IO && (cpu.pins.M1 == 0)) { // read IO port
            //RD=1, IO=1, M1=1 means "IRQ ack" so we ignore that here
            cpu.pins.D = (this->*cpu_in)(cpu.pins.Addr, cpu.pins.D, 1);
#ifndef LYCODER
            //if (::dbg.trace_on)
                //dbg_printf(DBGC_Z80 "\nSMS(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
            dbgloglog(this, SMSGG_CAT_CPU_IN, DBGLS_INFO, "%04x   (in)    %02x", cpu.pins.Addr, cpu.pins.D);
#endif
        }
    }
    if (cpu.pins.WR) {
        if (cpu.pins.MRQ) { // write RAM
            if (cpu.trace.last_cycle != *cpu.trace.cycles) {
                //dbg.traces.add(D_RESOURCE_TYPES.Z80, cpu.trace_cycles, trace_format_write('Z80', Z80_COLOR, cpu.trace_cycles, cpu.pins.Addr, cpu.pins.D));
                cpu.trace.last_cycle = *cpu.trace.cycles;
#ifndef LYCODER
                //dbg_printf(DBGC_Z80 "\nSMS(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
                dbgloglog(this, SMSGG_CAT_CPU_WRITE, DBGLS_INFO, "%04x   (write) %02x", cpu.pins.Addr, cpu.pins.D);
#endif
            }
            main_bus_write(cpu.pins.Addr, cpu.pins.D);
        } else if (cpu.pins.IO) {
            (this->*cpu_out)(cpu.pins.Addr, cpu.pins.D);
#ifndef LYCODER
            //if (::dbg.trace_on)
                //dbg_printf(DBGC_Z80 "\nSMS(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
            dbgloglog(this, SMSGG_CAT_CPU_WRITE, DBGLS_INFO, "%04x   (out)   %02x", cpu.pins.Addr, cpu.pins.D);
#endif
        }
    }
    cpu.cycle();
    clock.cpu_frame_cycle += clock.cpu_divisor;
}

void SMSGG::core::notify_NMI(bool level)
{
    cpu.notify_NMI(level);
}

void SMSGG::core::notify_IRQ(bool level)
{
    cpu.notify_IRQ(level);
}

void SMSGG::core::poll_pause()
{
    if (variant != jsm::systems::GG)
        notify_NMI(io.pause_button->state);
}

void SMSGG::core::sample_audio()
{
    if (audio.buf && (clock.master_cycles >= static_cast<u64>(audio.next_sample_cycle))) {
        audio.next_sample_cycle += audio.master_cycles_per_audio_sample;
        float *sptr = static_cast<float *>(audio.buf->ptr) + (audio.buf->upos);
        //assert(audio.buf->upos < audio.buf->samples_len);
        if (audio.buf->upos >= audio.buf->samples_len) {
            audio.buf->upos++;
        }
        else {
            *sptr = i16_to_float(sn76489.mix_sample(false));
            audio.buf->upos++;
        }
    }
}

void SMSGG::core::debug_audio()
{
    debug_waveform *dw = &dbg.waveforms.main.get();
    if (clock.master_cycles >= dw->user.next_sample_cycle) {
        if (dw->user.buf_pos < dw->samples_requested) {
            //printf("\nSAMPLE AT %lld next:%f stride:%f", clock.master_cycles, dw->user.next_sample_cycle, dw->user.cycle_stride);
            dw->user.next_sample_cycle += dw->user.cycle_stride;
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sn76489.mix_sample(true));
            dw->user.buf_pos++;
        }
    }

    dw = &dbg.waveforms.chan[0].get();
    if (clock.master_cycles >= dw->user.next_sample_cycle) {
        for (int j = 0; j < 4; j++) {
            dw = &dbg.waveforms.chan[j].get();
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                i16 sv = sn76489.sample_channel(j);
                static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(sv * 4);
                dw->user.buf_pos++;
                assert(dw->user.buf_pos < 410);
            }
        }
    }
}

u32 SMSGG::core::step_master(u32 howmany) {
#ifdef LYCODER
    do {
#else
        for (u32 i = 0; i < howmany; i++) {
#endif
        if (clock.frames_since_restart != last_frame) {
            last_frame = clock.frames_since_restart;
            poll_pause();
        }
        clock.cpu_master_clock++;
        clock.vdp_master_clock++;
        clock.apu_master_clock += 1;
        if (clock.cpu_master_clock > clock.cpu_divisor) {
            cpu_cycle();
            clock.cpu_master_clock -= clock.cpu_divisor;
        }
        if (clock.vdp_master_clock > clock.vdp_divisor) {
            vdp.cycle();
            clock.vdp_master_clock -= clock.vdp_divisor;
        }
        if (clock.apu_master_clock > clock.apu_divisor) {
            sn76489.cycle();
            clock.apu_master_clock -= clock.apu_divisor;
        }
        if (clock.frames_since_restart != last_frame) {
            last_frame = clock.frames_since_restart;
            poll_pause();
        }
#ifdef LYCODER
        if (*cpu.trace.cycles > 8000000) break;
#endif
        clock.master_cycles++;

        sample_audio();
        debug_audio();
        if (::dbg.do_break) break;
#ifdef LYCODER
    } while (true);
#else
    }
#endif
    return 0;
}

void SMSGG::core::load_BIOS(multi_file_set& mfs) {
    mapper.load_BIOS_from_RAM(mfs.files[0].buf);
}

static void SMSGGIO_unload_cart(JSM)
{

}

static void SMSGGIO_load_cart(JSM, multi_file_set &mfs, physical_io_device &pio) {
    auto *th = dynamic_cast<SMSGG::core *>(jsm);
    th->mapper.load_ROM_from_RAM(mfs.files[0].buf);
    th->reset();
}
