//
// Created by . on 12/4/24.
//
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "gba.h"
#include "gba_bus.h"
#include "gba_dma.h"
#include "gba_debugger.h"

#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.cpp"
#include "system/genesis/genesis_bus.h"


// 240x160, but 308x228 with v and h blanks
#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
#define MASTER_CYCLES_PER_SECOND (MASTER_CYCLES_PER_FRAME * 60)
#define SCANLINE_HBLANK 1006

static void setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = (static_cast<float>(MASTER_CYCLES_PER_FRAME) / static_cast<float>(dw->samples_requested));
    dw->user.buf_pos = 0;
}

void GBA::core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / static_cast<float>(ab->samples_len)));
        audio.next_sample_cycle_max = 0;

        auto *wf = &dbg.waveforms.main.get();
        audio.master_cycles_per_max_sample = static_cast<float>(MASTER_CYCLES_PER_FRAME) / static_cast<float>(wf->samples_requested);

        wf = &dbg.waveforms.chan[0].get();
        audio.master_cycles_per_min_sample = static_cast<float>(MASTER_CYCLES_PER_FRAME) / static_cast<float>(wf->samples_requested);
    }

    // PSG
    setup_debug_waveform(&dbg.waveforms.main.get());
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(&dbg.waveforms.chan[i].get());
        auto *wf = &dbg.waveforms.chan[i].get();
        if (i < 4)
            apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }

}

jsm_system *GBA_new()
{
    return new GBA::core();
}

void GBA_delete(jsm_system *sys)
{
    auto *th = dynamic_cast<genesis::core *>(sys);
    for (auto &pio : th->IOs) {
        if (pio.kind == HID_CART_PORT) {
            if (pio.cartridge_port.unload_cart) pio.cartridge_port.unload_cart(sys);
        }
    }
}

u32 GBA::core::finish_frame()
{
   audio.main_waveform = &dbg.waveforms.main.get();
#ifdef GBA_STATS
    u64 master_start = clock.master_cycle_count;
    u64 arm_start = timing.arm_cycles;
    u64 tm0_start = timing.timer0_cycles;
#endif

    for (u32 i = 0; i < 6; i++) {
        audio.waveforms[i] = &dbg.waveforms.chan[i].get();
    }
    scheduler.run_til_tag(2);

#ifdef GBA_STATS
    u64 master_num_cycles = (clock.master_cycle_count - master_start) * 60;
    u64 arm_num_cycles = (timing.arm_cycles - arm_start) * 60;
    u64 tm0_num_cycles = (timing.timer0_cycles - tm0_start) * 60;
    double master_div = (double)MASTER_CYCLES_PER_SECOND / (double)master_num_cycles;
    double arm_div = (double)MASTER_CYCLES_PER_SECOND / (double)arm_num_cycles;
    double tm0_div = (double)MASTER_CYCLES_PER_SECOND / (double)tm0_num_cycles;
    double arm_spd = (arm_div) * 100.0;
    double tm0_spd = ((double)timer[0].reload_ticks / tm0_div) * 100.0;
    double master_spd = master_div * 100.0;
    printf("\n\nSCANLINE:%d FRAME:%lld", clock.ppu.y, clock.master_frame);
    printf("\nEFFECTIVE MASTER FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", master_num_cycles, master_div, master_spd);
    printf("\nEFFECTIVE ARM FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", arm_num_cycles, arm_div, arm_spd);
    printf("\nEFFECTIVE TIMER0 FREQ IS %lld. DIVISOR %f, RUNNING AT %f SPEED", tm0_num_cycles, tm0_div, tm0_spd);
#endif

    return ppu.display->last_written;
}

void GBA::core::play()
{
}

void GBA::core::pause()
{
}

void GBA::core::stop()
{
}

void GBA::core::get_framevars(framevars &out)
{
    out.master_frame = clock.master_frame;
    out.x = static_cast<i32>(clock.ppu.x);
    out.scanline = clock.ppu.y;
    out.master_cycle = clock.master_cycle_count;
}

void GBA::core::skip_BIOS()
{
/*
SWI 00h (GBA/NDS7/NDS9) - SoftReset
Clears 200h bytes of RAM (containing stacks, and BIOS IRQ vector/flags)
*/
    printf("\nSKIP GBA BIOS!");
    for (u32 i = 0x3007E00; i < 0x3008000; i++) {
        cW[1](WRAM_fast, i - 0x3000000, 0);
    }

    // , initializes system, supervisor, and irq stack pointers,
    // sets R0-R12, LR_svc, SPSR_svc, LR_irq, and SPSR_irq to zero, and enters system mode.
    for (u32 i = 0; i < 13; i++) {
        cpu.regs.R[i] = 0;
    }
    cpu.regs.R_svc[1] = 0;
    cpu.regs.R_irq[1] = 0;
    cpu.regs.SPSR_svc = 0;
    cpu.regs.SPSR_irq = 0;
    cpu.regs.CPSR.mode = ARM7TDMI::ARM7_system;
    cpu.fill_regmap();
    /*
Host  sp_svc    sp_irq    sp_svc    zerofilled area       return address
  GBA   3007FE0h  3007FA0h  3007F00h  [3007E00h..3007FFFh]  Flag[3007FFAh] */

    cpu.regs.R_svc[0] = 0x03007FE0;
    cpu.regs.R_irq[0] = 0x03007FA0;
    cpu.regs.R[13] = 0x03007F00;

    cpu.regs.R[15] = 0x08000000;
    cpu.reload_pipeline();
}

static void tick_APU(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<GBA::core *>(ptr);
    th->apu.cycle();
    clock -= jitter;
    th->scheduler.only_add_abs(clock + 16, 0, th, &tick_APU, nullptr);
}


static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *th = static_cast<GBA::core *>(ptr);
    if (th->audio.buf) {

        th->audio.next_sample_cycle += th->audio.master_cycles_per_audio_sample;
        th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle), 0, th, &sample_audio, nullptr);
        if (th->audio.buf->upos < (th->audio.buf->samples_len << 1)) {
            th->apu.mix_sample(false);
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos] = th->apu.output.float_l;
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos+1] = th->apu.output.float_r;
        }
        th->audio.buf->upos+=2;
    }
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<GBA::core *>(ptr);

    debug_waveform *dw = th->audio.main_waveform;

    if (dw->user.buf_pos < dw->samples_requested) {
        dw->user.next_sample_cycle += dw->user.cycle_stride;
        static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = th->apu.mix_sample(true);
        dw->user.buf_pos++;
    }

    th->audio.next_sample_cycle_max += th->audio.master_cycles_per_max_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_max), 0, th, &sample_audio_debug_max, nullptr);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<GBA::core *>(ptr);

    for (int j = 0; j < 6; j++) {
        debug_waveform *dw = &th->dbg.waveforms.chan[j].get();
        if (dw->user.buf_pos < dw->samples_requested) {
            const float sv = th->apu.sample_channel(j);
            static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = sv;
            dw->user.buf_pos++;
            assert(dw->user.buf_pos < 410);
        }
    }

    th->audio.next_sample_cycle_min += th->audio.master_cycles_per_min_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_min), 0, th, &sample_audio_debug_min, nullptr);
}

void GBA::core::schedule_first()
{
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_max), 0, this, &sample_audio_debug_max, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_min), 0, this, &sample_audio_debug_min, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle), 0, this, &sample_audio, nullptr);
    scheduler.only_add_abs(16, 0, this, &tick_APU, nullptr);
    PPU::core::schedule_frame(&ppu, 0, 0, 0);
}

void GBA::core::reset()
{
    cpu.reset();
    clock.reset();
    ppu.reset();

    for (unsigned int & i : io.SIO.multi) {
        i = 0xFFFF;
    }
    io.SIO.send = 0xFFFF;

    //skip_BIOS(th);

    scheduler.clear();
    schedule_first();
    printf("\nGBA reset!");
}


u32 GBA::core::finish_scanline()
{
    scheduler.run_til_tag(1);
    return ppu.display->last_written;
}

u32 GBA::core::step_master(u32 howmany)
{
    scheduler.run_for_cycles(howmany);
    return 0;
}

void GBA::core::load_BIOS(multi_file_set &mfs)
{
    memcpy(BIOS.data, mfs.files[0].buf.ptr, 16384);
    BIOS.has = true;
}

static void GBAIO_unload_cart(jsm_system *sys)
{
}

static void GBAIO_load_cart(jsm_system *sys, multi_file_set &mfs, physical_io_device &pio) {
    auto *th = dynamic_cast<GBA::core *>(sys);
    buf* b = &mfs.files[0].buf;

    u32 r;
    th->cart.load_ROM_from_RAM(static_cast<const char *>(b->ptr), b->size, &pio, &r);
    th->reset();
}

void GBA::core::setup_lcd(JSM_DISPLAY &d)
{
    d.kind = jsm::LCD;
    d.enabled = true;

    d.fps = 59.727;
    d.fps_override_hint = 60;
    // 240x160, but 308x228 with v and h blanks

    d.pixelometry.cols.left_hblank = 0;
    d.pixelometry.cols.visible = 240;
    d.pixelometry.cols.max_visible = 240;
    d.pixelometry.cols.right_hblank = 68;
    d.pixelometry.offset.x = 0;

    d.pixelometry.rows.top_vblank = 0;
    d.pixelometry.rows.visible = 160;
    d.pixelometry.rows.max_visible = 160;
    d.pixelometry.rows.bottom_vblank = 68;
    d.pixelometry.offset.y = 0;

    d.geometry.physical_aspect_ratio.width = 3;
    d.geometry.physical_aspect_ratio.height = 2;

    d.pixelometry.overscan.left = d.pixelometry.overscan.right = 0;
    d.pixelometry.overscan.top = d.pixelometry.overscan.bottom = 0;
}

void GBA::core::setup_audio()
{
    physical_io_device &pio = IOs.emplace_back();
    pio.kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio.audio_channel;
    chan->num = 2;
    chan->sample_rate = 262144;
    chan->low_pass_filter = 24000;
}


void GBA::core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = true;

    IOs.reserve(15);

    // controllers
    physical_io_device *cnt = &IOs.emplace_back();
    controller.setup_pio(cnt);

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, 1, 1, 1, 1);
    HID_digital_button* b;

    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_CART_PORT, 1, 1, 1, 0);
    d->cartridge_port.load_cart = &GBAIO_load_cart;
    d->cartridge_port.unload_cart = &GBAIO_unload_cart;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(240 * 160 * 2);
    d->display.output[1] = malloc(240 * 160 * 2);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_lcd(d->display);
    ppu.display_ptr.make(IOs, IOs.size()-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    ppu.cur_output = static_cast<u16 *>(d->display.output[0]);

    setup_audio();

    ppu.display = &ppu.display_ptr.get().display;
}
