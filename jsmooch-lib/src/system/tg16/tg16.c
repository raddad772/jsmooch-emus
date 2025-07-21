//
// Created by . on 6/18/25.
//

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debugger/debugger.h"
#include "helpers/physical_io.h"

#include "tg16.h"
#include "tg16_debugger.h"
#include "tg16_bus.h"
#include "tg16_controllerport.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2
#define DRAW_CYCLES 1108

#define JTHIS struct TG16* this = (struct TG16*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct TG16* this

static void TG16J_play(JSM);
static void TG16J_pause(JSM);
static void TG16J_stop(JSM);
static void TG16J_get_framevars(JSM, struct framevars* out);
static void TG16J_reset(JSM);
static u32 TG16J_finish_frame(JSM);
static u32 TG16J_finish_scanline(JSM);
static u32 TG16J_step_master(JSM, u32 howmany);
static void TG16J_load_BIOS(JSM, struct multi_file_set* mfs);
static void TG16J_describe_io(JSM, struct cvec* IOs);

u32 read_trace_huc6280(void *ptr, u32 addr) {
    struct TG16* this = (struct TG16*)ptr;
    return TG16_bus_read(this, addr, this->cpu.pins.D, 0);
}

static void setup_debug_waveform(struct TG16 *this, struct debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)this->vce.regs.cycles_per_frame / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}


void TG16J_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    // # of cycles per frame can change per-frame
    this->audio.master_cycles_per_audio_sample = ((float)this->vce.regs.cycles_per_frame / (float)ab->samples_len);
    this->audio.next_sample_cycle_max = 0;
    struct debug_waveform *wf = cpg(this->dbg.waveforms_psg.main);
    this->audio.master_cycles_per_max_sample = (float)this->vce.regs.cycles_per_frame / (float)wf->samples_requested;

    wf = (struct debug_waveform *)cpg(this->dbg.waveforms_psg.chan[0]);
    this->audio.master_cycles_per_min_sample = (float)this->vce.regs.cycles_per_frame / (float)wf->samples_requested;

    // PSG
    wf = cpg(this->dbg.waveforms_psg.main);
    setup_debug_waveform(this, wf);
    this->cpu.psg.ext_enable = wf->ch_output_enabled;
    for (u32 i = 0; i < 6; i++) {
        wf = (struct debug_waveform *)cpg(this->dbg.waveforms_psg.chan[i]);
        setup_debug_waveform(this, wf);
        this->cpu.psg.ch[i].ext_enable = wf->ch_output_enabled;
    }

}

static void populate_opts(struct jsm_system *jsm)
{
    /*debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer A", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Layer B", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: Enable Sprites", 1, 1, 0);
    debugger_widgets_add_checkbox(&jsm->opts, "VDP: trace", 1, 0, 0);*/
}

static void read_opts(struct jsm_system *jsm, struct TG16* this)
{
    /*struct debugger_widget *w = cvec_get(&jsm->opts, 0);
    this->opts.vdp.enable_A = w->checkbox.value;

    w = cvec_get(&jsm->opts, 1);
    this->opts.vdp.enable_B = w->checkbox.value;

    w = cvec_get(&jsm->opts, 2);
    this->opts.vdp.enable_sprites = w->checkbox.value;

    w = cvec_get(&jsm->opts, 3);
    this->opts.vdp.ex_trace = w->checkbox.value;*/
}

static void block_step(void *ptr, u64 key, u64 clock, u32 jitter)
{
    // DO THIS
    printf("\nDO BLOCK STEP");
}

static void vdc0_update_irqs(void *ptr, u32 val)
{
    struct TG16 *this = (struct TG16 *)ptr;
    this->cpu.regs.IRQR.IRQ1 = val;
}

static struct events_view *TG16J_get_events_view(JSM)
{
    JTHIS;
    return cpg(this->dbg.events.view);
}

void TG16_new(JSM, enum jsm_systems kind)
{
    struct TG16* this = (struct TG16*)malloc(sizeof(struct TG16));
    memset(this, 0, sizeof(*this));
    //populate_opts(jsm);
    /*create_scheduling_lookup_table(this);*/
    scheduler_init(&this->scheduler, &this->clock.master_cycles, &this->clock.unused);
    this->scheduler.max_block_size = 2;
    this->scheduler.run.func = &block_step;
    this->scheduler.run.ptr = this;

    TG16_clock_init(&this->clock);
    HUC6280_init(&this->cpu, &this->scheduler, this->clock.timing.second.cycles);
    this->cpu.read_func = &TG16_huc_read_mem;
    this->cpu.write_func = &TG16_huc_write_mem;
    this->cpu.read_io_func = &TG16_huc_read_io;
    this->cpu.write_io_func = &TG16_huc_write_io;
    this->cpu.read_ptr = this->cpu.write_ptr = this->cpu.read_io_ptr = this->cpu.write_io_ptr = this;
    TG16_cart_init(&this->cart);
    HUC6270_init(&this->vdc0, &this->scheduler);
    HUC6270_init(&this->vdc1, &this->scheduler);
    HUC6260_init(&this->vce, &this->scheduler, &this->vdc0, NULL);
    this->vdc0.irq.update_func_ptr = this;
    this->vdc0.irq.update_func = &vdc0_update_irqs;
    //ym2612_init(&this->ym2612, OPN2V_ym2612, &this->clock.master_cycle_count, 32 * 7 * 6);
    //SN76489_init(&this->psg);
    snprintf(jsm->label, sizeof(jsm->label), "TurboGraFX-16");

    struct jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_huc6280;
    dt.ptr = (void*)this;
    HUC6280_setup_tracing(&this->cpu, &dt, &this->clock.master_cycles, 1);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &TG16J_finish_frame;
    jsm->finish_scanline = &TG16J_finish_scanline;
    jsm->step_master = &TG16J_step_master;
    jsm->reset = &TG16J_reset;
    jsm->load_BIOS = &TG16J_load_BIOS;
    jsm->get_framevars = &TG16J_get_framevars;
    jsm->play = &TG16J_play;
    jsm->pause = &TG16J_pause;
    jsm->stop = &TG16J_stop;
    jsm->describe_io = &TG16J_describe_io;
    jsm->set_audiobuf = &TG16J_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &TG16J_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;
}

void TG16_delete(JSM) {
    JTHIS;

    HUC6280_delete(&this->cpu);
    HUC6270_delete(&this->vdc0);
    HUC6270_delete(&this->vdc1);
    HUC6260_delete(&this->vce);

    while (cvec_len(this->jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->jsm.IOs);
        if (pio->kind == HID_CART_PORT) {
            if (pio->cartridge_port.unload_cart) pio->cartridge_port.unload_cart(jsm);
        }
        physical_io_device_delete(pio);
    }

    TG16_cart_delete(&this->cart);
    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

static inline float u16_to_float2(i16 val)
{
    return (float)val / 512.0f;
}

static inline float u16_to_float(i16 val)
{
    return (float)val / 65535.0f;
}

static inline float i16_to_float(i16 val)
{
    return ((((float)(((i32)val) + 32768)) / 65535.0f) * 2.0f) - 1.0f;
}

static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct TG16* this = (struct TG16 *)ptr;
    if (this->audio.buf) {
        this->audio.cycles++;
        this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
        scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle, 0, this, &sample_audio, NULL);
        if (this->audio.buf->upos < (this->audio.buf->samples_len << 1)) {
            u16 ls, rs;
            HUC6280_PSG_mix_sample(&this->cpu.psg, &ls, &rs);
            //if (ls != 0) printf("\nLS: %d", ls);
            ((float *)this->audio.buf->ptr)[this->audio.buf->upos] = u16_to_float2(ls);
            ((float *)this->audio.buf->ptr)[this->audio.buf->upos+1] = u16_to_float2(rs);
        }
        this->audio.buf->upos+=2;
    }
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct TG16 *this = (struct TG16 *)ptr;

    // PSG
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.main);
    if (dw->user.buf_pos < dw->samples_requested) {
        u16 l, r;
        HUC6280_PSG_mix_sample(&this->cpu.psg, &l, &r);
        u32 a = (l + r) >> 1;
        ((float *) dw->buf.ptr)[dw->user.buf_pos] = u16_to_float(a);
        dw->user.buf_pos++;
    }
    this->audio.next_sample_cycle_max += this->audio.master_cycles_per_max_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
}

static void sample_audio_debug_min(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct TG16 *this = (struct TG16 *)ptr;

    // PSG
    struct debug_waveform *dw = cpg(this->dbg.waveforms_psg.chan[0]);
    for (int j = 0; j < 6; j++) {
        dw = cpg(this->dbg.waveforms_psg.chan[j]);
        if (dw->user.buf_pos < dw->samples_requested) {
            u16 sv = HUC6280_PSG_debug_ch_sample(&this->cpu.psg, j);
            ((float *) dw->buf.ptr)[dw->user.buf_pos] = u16_to_float(sv);
            dw->user.buf_pos++;
        }
    }

    this->audio.next_sample_cycle_min += this->audio.master_cycles_per_min_sample;
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
}


static void TG16IO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *which_pio)
{
    JTHIS;

    // 512kb usless header
    // check if bit 9 is set and discard first 512kb then
    struct buf* b = &mfs->files[0].buf;

    uint8_t *ptr = (uint8_t *)b->ptr;
    u64 sz = b->size;
    if (sz & 512) {
        ptr += 512;
        sz -= 512;
    }

   TG16_cart_load_ROM_from_RAM(&this->cart, ptr, sz, which_pio);
   TG16J_reset(jsm);
}

static void TG16IO_unload_cart(JSM)
{
}

static void setup_crt(struct TG16 *this, struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps = 60; //this->PAL ? 50 : 60.0988;
    d->fps_override_hint = this->clock.timing.second.frames;

    d->pixelometry.cols.left_hblank = 16;
    d->pixelometry.cols.visible = HUC6260_CYCLE_PER_LINE;
    d->pixelometry.cols.max_visible = HUC6260_CYCLE_PER_LINE;
    d->pixelometry.cols.right_hblank = 221;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 242;
    d->pixelometry.rows.max_visible = 242;
    d->pixelometry.rows.bottom_vblank = 20;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = 192;
    d->pixelometry.overscan.right = 45;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_audio(struct TG16 *this, struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 48000;
    chan->left = chan->right = 1;
    chan->num = 2;
    chan->low_pass_filter = 24000;
}

void TG16J_describe_io(JSM, struct cvec *IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *c1 = cvec_push_back(this->jsm.IOs);
    TG16_2button_init(&this->controller);
    TG16_2button_setup_pio(c1, 0, "Player 1", 1);
    this->controller.pio = c1;

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
    d->cartridge_port.load_cart = &TG16IO_load_cart;
    d->cartridge_port.unload_cart = &TG16IO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(HUC6260_CYCLE_PER_LINE * 480 * 2);
    d->display.output[1] = malloc(HUC6260_CYCLE_PER_LINE * 480 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_crt(this, &d->display);
    this->vce.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    this->vce.cur_output = (u16 *)(d->display.output[0]);
    this->vce.display = &((struct physical_io_device *)cpg(this->vce.display_ptr))->display;

    setup_audio(this, IOs);

    this->vce.display = &((struct physical_io_device *)cpg(this->vce.display_ptr))->display;
    TG16_controllerport_connect(&this->controller_port, TG16CK_2button, &this->controller);
}

void TG16J_play(JSM)
{
}

void TG16J_pause(JSM)
{
}

void TG16J_stop(JSM)
{
}

#define PSG_CYCLES 6
static void psg_go(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct TG16 *this = (struct TG16 *)ptr;
    u64 cur = clock - jitter;
    HUC6280_PSG_cycle(&this->cpu.psg);

    scheduler_only_add_abs(&this->scheduler, cur + PSG_CYCLES, 0, this, &psg_go, NULL);
}

static void schedule_first(struct TG16 *this)
{
    HUC6280_schedule_first(&this->cpu, 0);
    HUC6260_schedule_first(&this->vce);
    scheduler_only_add_abs(&this->scheduler, PSG_CYCLES, 0, this, &psg_go, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_max, 0, this, &sample_audio_debug_max, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle_min, 0, this, &sample_audio_debug_min, NULL);
    scheduler_only_add_abs(&this->scheduler, (i64)this->audio.next_sample_cycle, 0, this, &sample_audio, NULL);
}

void TG16J_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->vce.master_frame;
    out->x = 0;
    out->scanline = this->vce.regs.y - 64;
    out->master_cycle = this->clock.master_cycles;
}

void TG16J_reset(JSM)
{
    JTHIS;
    TG16_clock_reset(&this->clock);
    HUC6280_reset(&this->cpu);
    HUC6270_reset(&this->vdc0);
    HUC6270_reset(&this->vdc1);
    HUC6260_reset(&this->vce);
    TG16_cart_reset(&this->cart);

    /*this->io.z80.reset_line = 1;
    this->io.z80.reset_line_count = 500;
    this->io.z80.bus_request = this->io.z80.bus_ack = 1;
    this->io.m68k.VDP_FIFO_stall = 0;
    this->io.m68k.VDP_prefetch_stall = 0;
    this->scheduler_index = 0;*/

    scheduler_clear(&this->scheduler);
    schedule_first(this);
    printf("\nTG16 reset!");
}

//#define DO_STATS

u32 TG16J_finish_frame(JSM)
{
    JTHIS;
    read_opts(jsm, this);

    //u32 current_frame = this->vce.master_frame;
#ifdef DO_STATS
    u64 ym_start = this->timing.ym2612_cycles;
    u64 z80_start = this->timing.z80_cycles;
    u64 m68k_start = this->timing.m68k_cycles;
    u64 vdp_start = this->timing.vdp_cycles;
    u64 clock_start = this->clock.master_cycle_count;
    u64 psg_start = this->timing.psg_cycles;
    u64 audio_start = this->audio.cycles;
#endif
    scheduler_run_til_tag_tg16(&this->scheduler, TAG_FRAME);

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
    return this->vce.display->last_written;
}

static void cycle_cpu(struct TG16 *this)
{

}

#define MIN(x,y) ((x) < (y) ? (x) : (y))
u32 TG16J_finish_scanline(JSM)
{
    JTHIS;
    /*u32 start_y = this->vce.regs.y;
    while (this->vce.regs.y == start_y) {
        i64 min_step = MIN(this->clock.next.cpu, this->clock.next.vce);
        min_step = MIN(min_step, this->clock.next.timer);
        this->clock.master_cycles += min_step;

        if (this->clock.master_cycles >= this->clock.next.cpu) {
            HUC6280_internal_cycle(&this->cpu);
            this->clock.next.cpu += this->cpu.regs.clock_div;
        }
        if (this->clock.master_cycles >= this->clock.next.timer) {
            HUC6280_tick_timer(&this->cpu);
            this->clock.next.timer += 3072;
        }
        if (this->clock.master_cycles >= this->clock.next.vce) {
            HUC6260_cycle(&this->vce);
            this->clock.next.vce += this->vce.regs.clock_div;
        }
    }*/
    scheduler_run_til_tag_tg16(&this->scheduler, TAG_SCANLINE);

    return this->vce.display->last_written;
}

u32 TG16J_step_master(JSM, u32 howmany)
{
    JTHIS;
    scheduler_run_for_cycles_tg16(&this->scheduler, howmany);
    return 0;
}

void TG16J_load_BIOS(JSM, struct multi_file_set* mfs)
{
    printf("\nTG16 doesn't have a BIOS...?");
}


