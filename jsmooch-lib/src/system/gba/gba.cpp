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

#include "fail"
#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.c"

#define JTHIS struct GBA* this = (GBA*)jsm->ptr
#define JSM struct jsm_system* jsm

static void GBAJ_play(JSM);
static void GBAJ_pause(JSM);
static void GBAJ_stop(JSM);
static void GBAJ_get_framevars(JSM, framevars* out);
static void GBAJ_reset(JSM);
static u32 GBAJ_finish_frame(JSM);
static u32 GBAJ_finish_scanline(JSM);
static u32 GBAJ_step_master(JSM, u32 howmany);
static void GBAJ_load_BIOS(JSM, multi_file_set* mfs);
static void GBAJ_describe_io(JSM, cvec* IOs);

// 240x160, but 308x228 with v and h blanks
#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
#define MASTER_CYCLES_PER_SECOND (MASTER_CYCLES_PER_FRAME * 60)
#define SCANLINE_HBLANK 1006

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

void GBAJ_set_audiobuf(jsm_system* jsm, audiobuf *ab)
{
    JTHIS;
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / (float)ab->samples_len));
        audio.next_sample_cycle_max = 0;

        struct debug_waveform *wf = cpg(dbg.waveforms.main);
        audio.master_cycles_per_max_sample = (float)MASTER_CYCLES_PER_FRAME / (float)wf->samples_requested;

        wf = (debug_waveform *)cpg(dbg.waveforms.chan[0]);
        audio.master_cycles_per_min_sample = (float)MASTER_CYCLES_PER_FRAME / (float)wf->samples_requested;
    }

    // PSG
    setup_debug_waveform(cvec_get(dbg.waveforms.main.vec, dbg.waveforms.main.index));
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index));
        struct debug_waveform *wf = (debug_waveform *)cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index);
        if (i < 4)
            apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }

}

static u32 read_trace_cpu(void *ptr, u32 addr, u32 sz) {
    struct GBA* this = (GBA*)ptr;
    return GBA_mainbus_read(this, addr, sz, io.cpu.open_bus_data, 0);
}

void GBA_new(jsm_system *jsm)
{
    struct GBA* this = (GBA*)malloc(sizeof(GBA));
    memset(this, 0, sizeof(*this));

    scheduler_init(&scheduler, &clock.master_cycle_count, &waitstates.current_transaction);
    scheduler.max_block_size = 8;
    scheduler.run.func = &GBA_block_step_cpu;
    scheduler.run.ptr = this;
    ARM7TDMI_init(&cpu, &clock.master_cycle_count, &waitstates.current_transaction, &scheduler);
    cpu.read_ptr = this;
    cpu.write_ptr = this;
    cpu.read = &GBA_mainbus_read;
    cpu.write = &GBA_mainbus_write;
    cpu.fetch_ptr = this;
    cpu.fetch_ins = &GBA_mainbus_fetchins;
    GBA_DMA_init(this);
    GBA_bus_init(this);
    GBA_clock_init(&clock);
    GBA_cart_init(&cart);
    GBA_PPU_init(this);
    GBA_APU_init(this);

    snprintf(jsm->label, sizeof(jsm->label), "GameBoy Advance");
    struct jsm_debug_read_trace dt;
    dt.read_trace_arm = &read_trace_cpu;
    dt.ptr = this;
    ARM7TDMI_setup_tracing(&cpu, &dt, &clock.master_cycle_count, 1);

    jsm.described_inputs = 0;
    jsm.IOs = nullptr;
    jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &GBAJ_finish_frame;
    jsm->finish_scanline = &GBAJ_finish_scanline;
    jsm->step_master = &GBAJ_step_master;
    jsm->reset = &GBAJ_reset;
    jsm->load_BIOS = &GBAJ_load_BIOS;
    jsm->get_framevars = &GBAJ_get_framevars;
    jsm->play = &GBAJ_play;
    jsm->pause = &GBAJ_pause;
    jsm->stop = &GBAJ_stop;
    jsm->describe_io = &GBAJ_describe_io;
    jsm->set_audiobuf = &GBAJ_set_audiobuf;
    jsm->sideload = nullptr;
    jsm->setup_debugger_interface = &GBAJ_setup_debugger_interface;
    jsm->save_state = nullptr;
    jsm->load_state = nullptr;
    
}

void GBA_delete(jsm_system *jsm)
{
    JTHIS;

    ARM7TDMI_delete(&cpu);
    GBA_PPU_delete(this);
    GBA_cart_delete(&cart);

    while (cvec_len(jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(jsm.IOs);
        if (pio->kind == HID_CART_PORT) {
            if (pio->cartridge_port.unload_cart) pio->cartridge_port.unload_cart(jsm);
        }
        physical_io_device_delete(pio);
    }

    free(jsm->ptr);
    jsm->ptr = nullptr;

    jsm_clearfuncs(jsm);
}

u32 GBAJ_finish_frame(JSM)
{
    JTHIS;
   audio.main_waveform = cpg(dbg.waveforms.main);
#ifdef GBA_STATS
    u64 master_start = clock.master_cycle_count;
    u64 arm_start = timing.arm_cycles;
    u64 tm0_start = timing.timer0_cycles;
#endif

    for (u32 i = 0; i < 6; i++) {
        audio.waveforms[i] = cpg(dbg.waveforms.chan[i]);
    }
    scheduler_run_til_tag(&scheduler, 2);

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

void GBAJ_play(JSM)
{
}

void GBAJ_pause(JSM)
{
}

void GBAJ_stop(JSM)
{
}

void GBAJ_get_framevars(JSM, framevars* out)
{
    JTHIS;
    out->master_frame = clock.master_frame;
    out->x = clock.ppu.x;
    out->scanline = clock.ppu.y;
    out->master_cycle = clock.master_cycle_count;
}

static void skip_BIOS(GBA* this)
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
    cpu.regs.CPSR.mode = ARM7_system;
    ARM7TDMI_fill_regmap(&cpu);
    /*
Host  sp_svc    sp_irq    sp_svc    zerofilled area       return address
  GBA   3007FE0h  3007FA0h  3007F00h  [3007E00h..3007FFFh]  Flag[3007FFAh] */

    cpu.regs.R_svc[0] = 0x03007FE0;
    cpu.regs.R_irq[0] = 0x03007FA0;
    cpu.regs.R[13] = 0x03007F00;

    cpu.regs.R[15] = 0x08000000;
    ARM7TDMI_reload_pipeline(&cpu);
}

static void tick_APU(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (GBA *)ptr;
    GBA_APU_cycle(this);
    i64 cur = clock - jitter;
    scheduler_only_add_abs(&scheduler, cur + 16, 0, this, &tick_APU, nullptr);
}


static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter) {
    struct GBA *this = (GBA *) ptr;
    if (audio.buf) {

        audio.next_sample_cycle += audio.master_cycles_per_audio_sample;
        scheduler_only_add_abs(&scheduler, (i64) audio.next_sample_cycle, 0, this, &sample_audio, nullptr);
        if (audio.buf->upos < (audio.buf->samples_len << 1)) {
            GBA_APU_mix_sample(this, 0);
            ((float *) (audio.buf->ptr))[audio.buf->upos] = apu.output.float_l;
            ((float *) (audio.buf->ptr))[audio.buf->upos+1] = apu.output.float_r;
        }
        audio.buf->upos+=2;
    }

}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (GBA *)ptr;

    struct debug_waveform *dw = audio.main_waveform;

    if (dw->user.buf_pos < dw->samples_requested) {
        dw->user.next_sample_cycle += dw->user.cycle_stride;
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = GBA_APU_mix_sample(this, 1);
        dw->user.buf_pos++;
    }

    audio.next_sample_cycle_max += audio.master_cycles_per_max_sample;
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, nullptr);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (GBA *)ptr;

    struct debug_waveform *dw;
    for (int j = 0; j < 6; j++) {
        dw = cpg(dbg.waveforms.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            float sv = GBA_APU_sample_channel(this, j);
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
            dw->user.buf_pos++;
            assert(dw->user.buf_pos < 410);
        }
    }

    audio.next_sample_cycle_min += audio.master_cycles_per_min_sample;
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, nullptr);
}

static void schedule_first(GBA *this)
{
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, nullptr);
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, nullptr);
    scheduler_only_add_abs(&scheduler, (i64)audio.next_sample_cycle, 0, this, &sample_audio, nullptr);
    scheduler_only_add_abs(&scheduler, 16, 0, this, &tick_APU, nullptr);
    GBA_PPU_schedule_frame(this, 0, 0, 0);
}

void GBAJ_reset(JSM)
{
    JTHIS;
    ARM7TDMI_reset(&cpu);
    GBA_clock_reset(&clock);
    GBA_PPU_reset(this);

    for (u32 i = 0; i < 4; i++) {
        io.SIO.multi[i] = 0xFFFF;
    }
    io.SIO.send = 0xFFFF;

    //skip_BIOS(this);

    scheduler_clear(&scheduler);
    schedule_first(this);
    printf("\nGBA reset!");
}


void GBA_block_step_halted(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (GBA *)ptr;
    io.halted &= ((!!(io.IF & io.IE)) ^ 1);
    if (!io.halted) {
        waitstates.current_transaction = 1;
        scheduler.run.func = &GBA_block_step_cpu;
    }
    else {
        clock.master_cycle_count = scheduler.first_event->timecode;
        waitstates.current_transaction = 0;
    }
}

u32 GBAJ_finish_scanline(JSM)
{
    JTHIS;
    scheduler_run_til_tag(&scheduler, 1);
    return ppu.display->last_written;
}

static u32 GBAJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    scheduler_run_for_cycles(&scheduler, howmany);
    return 0;
}

static void GBAJ_load_BIOS(JSM, multi_file_set* mfs)
{
    JTHIS;
    memcpy(BIOS.data, mfs->files[0].buf.ptr, 16384);
    BIOS.has = 1;
}

static void GBAIO_unload_cart(JSM)
{
}

static void GBAIO_load_cart(JSM, multi_file_set *mfs, physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    u32 r;
    GBA_cart_load_ROM_from_RAM(&cart, b->ptr, b->size, pio, &r);
    GBAJ_reset(jsm);
}

static void setup_lcd(JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

    d->fps = 59.727;
    d->fps_override_hint = 60;
    // 240x160, but 308x228 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 240;
    d->pixelometry.cols.max_visible = 240;
    d->pixelometry.cols.right_hblank = 68;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 160;
    d->pixelometry.rows.max_visible = 160;
    d->pixelometry.rows.bottom_vblank = 68;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 3;
    d->geometry.physical_aspect_ratio.height = 2;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_audio(cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->num = 2;
    chan->sample_rate = 262144;
    chan->low_pass_filter = 24000;
}


static void GBAJ_describe_io(JSM, cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (jsm.described_inputs) return;
    jsm.described_inputs = 1;

    jsm.IOs = IOs;

    // controllers
    struct physical_io_device *controller = cvec_push_back(jsm.IOs);
    GBA_controller_setup_pio(controller);
    controller.pio = controller;

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
    d->cartridge_port.load_cart = &GBAIO_load_cart;
    d->cartridge_port.unload_cart = &GBAIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(240 * 160 * 2);
    d->display.output[1] = malloc(240 * 160 * 2);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_lcd(&d->display);
    ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    ppu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    ppu.display = &((physical_io_device *)cpg(ppu.display_ptr))->display;
}
