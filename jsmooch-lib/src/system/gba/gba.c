//
// Created by . on 12/4/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gba.h"
#include "gba_bus.h"
#include "gba_debugger.h"

#include "helpers/debugger/debugger.h"
#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.c"

#define JTHIS struct GBA* this = (struct GBA*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void GBAJ_play(JSM);
static void GBAJ_pause(JSM);
static void GBAJ_stop(JSM);
static void GBAJ_get_framevars(JSM, struct framevars* out);
static void GBAJ_reset(JSM);
static u32 GBAJ_finish_frame(JSM);
static u32 GBAJ_finish_scanline(JSM);
static u32 GBAJ_step_master(JSM, u32 howmany);
static void GBAJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void GBAJ_describe_io(JSM, struct cvec* IOs);

// 240x160, but 308x228 with v and h blanks
#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
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

void GBAJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    if (this->audio.master_cycles_per_audio_sample == 0) {
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
    }

}

u32 read_trace_cpu(void *ptr, u32 addr, u32 sz) {
    struct GBA* this = (struct GBA*)ptr;
    return GBA_mainbus_read(this, addr, sz, this->io.cpu.open_bus_data, 0);
}

void GBA_new(struct jsm_system *jsm)
{
    struct GBA* this = (struct GBA*)malloc(sizeof(struct GBA));
    memset(this, 0, sizeof(*this));
    ARM7TDMI_init(&this->cpu, &this->waitstates.current_transaction);
    this->cpu.read_ptr = this;
    this->cpu.write_ptr = this;
    this->cpu.read = &GBA_mainbus_read;
    this->cpu.write = &GBA_mainbus_write;
    this->cpu.fetch_ptr = this;
    this->cpu.fetch_ins = &GBA_mainbus_fetchins;
    GBA_bus_init(this);
    GBA_clock_init(&this->clock);
    GBA_cart_init(&this->cart);
    GBA_PPU_init(this);
    GBA_APU_init(this);

    snprintf(jsm->label, sizeof(jsm->label), "GameBoy Advance");
    struct jsm_debug_read_trace dt;
    dt.read_trace_arm = &read_trace_cpu;
    dt.ptr = this;
    ARM7TDMI_setup_tracing(&this->cpu, &dt, &this->clock.master_cycle_count, 1);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

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
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &GBAJ_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;
    
}

void GBA_delete(struct jsm_system *jsm)
{
    JTHIS;

    ARM7TDMI_delete(&this->cpu);
    GBA_PPU_delete(this);
    GBA_cart_delete(&this->cart);

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

u32 GBAJ_finish_frame(JSM)
{
    JTHIS;

    u64 current_frame = this->clock.master_frame;
    while (this->clock.master_frame == current_frame) {
        GBAJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    struct debug_waveform *dw = cpg(this->dbg.waveforms.chan[4]);
    return this->ppu.display->last_written;
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

void GBAJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.ppu.x;
    out->scanline = this->clock.ppu.y;
    out->master_cycle = this->clock.master_cycle_count;
}

static void skip_BIOS(struct GBA* this)
{
/*
SWI 00h (GBA/NDS7/NDS9) - SoftReset
Clears 200h bytes of RAM (containing stacks, and BIOS IRQ vector/flags)
*/
    printf("\nSKIP GBA BIOS!");
    for (u32 i = 0x3007E00; i < 0x3008000; i++) {
        cW[1](this->WRAM_fast, i - 0x3000000, 0);
    }

    // , initializes system, supervisor, and irq stack pointers,
    // sets R0-R12, LR_svc, SPSR_svc, LR_irq, and SPSR_irq to zero, and enters system mode.
    for (u32 i = 0; i < 13; i++) {
        this->cpu.regs.R[i] = 0;
    }
    this->cpu.regs.R_svc[1] = 0;
    this->cpu.regs.R_irq[1] = 0;
    this->cpu.regs.SPSR_svc = 0;
    this->cpu.regs.SPSR_irq = 0;
    this->cpu.regs.CPSR.mode = ARM7_system;
    ARM7TDMI_fill_regmap(&this->cpu);
    /*
Host  sp_svc    sp_irq    sp_svc    zerofilled area       return address
  GBA   3007FE0h  3007FA0h  3007F00h  [3007E00h..3007FFFh]  Flag[3007FFAh] */

    this->cpu.regs.R_svc[0] = 0x03007FE0;
    this->cpu.regs.R_irq[0] = 0x03007FA0;
    this->cpu.regs.R[13] = 0x03007F00;

    this->cpu.regs.R[15] = 0x08000000;
    ARM7TDMI_reload_pipeline(&this->cpu);
}

void GBAJ_reset(JSM)
{
    JTHIS;
    ARM7TDMI_reset(&this->cpu);
    GBA_clock_reset(&this->clock);
    GBA_PPU_reset(this);


    for (u32 i = 0; i < 4; i++) {
        this->io.SIO.multi[i] = 0xFFFF;
    }
    this->io.SIO.send = 0xFFFF;

    skip_BIOS(this);
    printf("\nGBA reset!");
}

static void raise_irq_for_dma(struct GBA *this, u32 num)
{
    u32 shift = 8 + num;
    this->io.IF |= (1 << shift);
}

static u32 dma_go_ch(struct GBA *this, u32 num) {
    struct GBA_DMA_ch *ch = &this->dma[num];
    if (ch->run_counter && ch->io.enable) {
        ch->run_counter--;
        if (ch->run_counter == 0) GBA_dma_start(ch, num, 0);
        else return 0;
    }
    if ((ch->io.enable) && (ch->op.started)) {
        if (ch->op.sz == 2) {
            u16 value;
            if (ch->op.src_addr >= 0x02000000) {
                value = GBA_mainbus_read(this, ch->op.src_addr, 2, ch->op.src_access, 1);
                ch->io.open_bus = (value << 16) | value;
            } else {
                if (ch->op.dest_addr & 2) {
                    value = ch->io.open_bus >> 16;
                } else {
                    value = ch->io.open_bus & 0xFFFF;
                }
                this->waitstates.current_transaction++;
            }
            /*if (num == 3){
               if (((ch->op.dest_addr >= 0x0d000000) && (ch->op.dest_addr < 0x0d100000)) ||
                       (ch->op.src_addr >= 0x0d000000) && (ch->op.src_addr < 0x0d100000)) {
                   printf("\nDMA TRANSFER %d from %08x to %08x (%d/%d)", value & 1, ch->op.src_addr, ch->op.dest_addr, (ch->op.dest_addr - ch->io.dest_addr)>>1, ch->io.word_count);
               }
            }*/

            GBA_mainbus_write(this, ch->op.dest_addr, 2, ch->op.dest_access, value);
        }
        else {
            if (ch->op.src_addr >= 0x02000000)
                ch->io.open_bus = GBA_mainbus_read(this, ch->op.src_addr, 4, ch->op.src_access, 1);
            else
                this->waitstates.current_transaction++;
            GBA_mainbus_write(this, ch->op.dest_addr, 4, ch->op.dest_access, ch->io.open_bus);
        }

        ch->op.src_access = ARM7P_sequential | ARM7P_dma;
        ch->op.dest_access = ARM7P_sequential | ARM7P_dma;

        switch(ch->io.src_addr_ctrl) {
            case 0: // increment
                ch->op.src_addr = (ch->op.src_addr + ch->op.sz) & 0x0FFFFFFF;
                if (num == 0) ch->op.src_addr &= 0x07FFFFFF;
                break;
            case 1: // decrement
                ch->op.src_addr = (ch->op.src_addr - ch->op.sz) & 0x0FFFFFFF;
                if (num == 0) ch->op.src_addr &= 0x07FFFFFF;
                break;
            case 2: // constant
                break;
            case 3: // prohibited
                printf("\nPROHIBITED!");
                break;
        }

        switch(ch->io.dest_addr_ctrl) {
            case 0: // increment
                ch->op.dest_addr = (ch->op.dest_addr + ch->op.sz) & 0x0FFFFFFF;
                if (num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
            case 1: // decrement
                ch->op.dest_addr = (ch->op.dest_addr - ch->op.sz) & 0x0FFFFFFF;
                if (num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
            case 2: // constant
                break;
            case 3: // increment & reload on repeat
                ch->op.dest_addr = (ch->op.dest_addr + ch->op.sz) & 0x0FFFFFFF;
                if (num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
        }

        ch->op.word_count = (ch->op.word_count - 1) & ch->op.word_mask;
        if (ch->op.word_count == 0) {
            ch->op.started = 0; // Disable
            ch->op.first_run = 0;
            if (ch->io.irq_on_end) {
                raise_irq_for_dma(this, num);
            }

            if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
        }
        return 1;
    }
    return 0;
}

static u32 dma_go(struct GBA *this) {
    for (u32 i = 0; i < 4; i++) {
        if (dma_go_ch(this, i)) return 1;
    }
    return 0;
}

static u32 timer_enabled(struct GBA *this, u32 tn) {
    return GBA_clock_current(this) >= this->timer[tn].enable_at;
}


static void overflow_timer(struct GBA *this, u32 tn, u64 current_time);

static void cascade_timer_step(struct GBA *this, u32 tn, u64 current_time)
{
    //printf("\nCASCADE TIMER STEP!");
    struct GBA_TIMER *t = &this->timer[tn];
    t->val_at_stop = (t->val_at_stop + 1) & 0xFFFF;
    if (t->val_at_stop == 0) {
        overflow_timer(this, tn, current_time);
    }
}

static void overflow_timer(struct GBA *this, u32 tn, u64 current_time) {
    struct GBA_TIMER *t = &this->timer[tn];
    //printf("\nOVERFLOW: %d", tn);
    if (!t->cascade) {
        t->enable_at = current_time;
        t->reload_ticks = timer_reload_ticks(t->reload) << t->shift;
        t->overflow_at = t->enable_at + t->reload_ticks;
        //printf("\nNew overflow in %lld cycles", t->overflow_at - t->enable_at);
    }
    t->val_at_stop = t->reload;
    if (t->irq_on_overflow) {
        //printf("\nIRQ!");
        this->io.IF |= 1 << (3 + tn);
        GBA_eval_irqs(this);
    }
    if (this->apu.fifo[0].timer_id == tn) GBA_APU_sound_FIFO(this, 0);
    if (this->apu.fifo[1].timer_id == tn) GBA_APU_sound_FIFO(this, 1);

    if (tn < 3) {
        // Check for cascade!
        struct GBA_TIMER *tp1 = &this->timer[tn+1];
        if (timer_enabled(this, tn+1) && tp1->cascade) {
            cascade_timer_step(this, tn+1, current_time);
        }
    }
}

static void tick_timers(struct GBA *this, u32 num_ticks) {
    for (u32 ticks = 0; ticks < num_ticks; ticks++) {
        u64 current_time = this->clock.master_cycle_count + ticks;
        // Check for overflow...
        for (u32 tn = 0; tn < 4; tn++) {
            struct GBA_TIMER *t = &this->timer[tn];
            if (!t->cascade && (current_time >= t->overflow_at)) {
                overflow_timer(this, tn, current_time);
            }
        }
    }
}

static void sample_audio(struct GBA* this, u32 num_cycles)
{
    for (u64 i = 0; i < num_cycles; i++) {
        GBA_APU_cycle(this);
        u64 mc = this->clock.master_cycle_count + i;
        if (this->audio.buf && (mc >= (u64) this->audio.next_sample_cycle)) {
            this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
            if (this->audio.buf->upos < this->audio.buf->samples_len) {
                ((float *)(this->audio.buf->ptr))[this->audio.buf->upos] = GBA_APU_mix_sample(this, 0);
            }
            this->audio.buf->upos++;
        }

        struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = GBA_APU_mix_sample(this, 1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(this->dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(this->dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = GBA_APU_sample_channel(this, j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }
}

static void cycle_DMA_and_CPU(struct GBA *this, u32 num_cycles)
{
    // add in DMA here
    this->scanline_cycles_to_execute += (i32)num_cycles;
    while(this->scanline_cycles_to_execute > 0) {
        this->waitstates.current_transaction = 0;
        if (dma_go(this)) {
        }
        else {
            if (this->io.halted) {
                this->io.halted &= ((!!(this->io.IF & this->io.IE)) ^ 1);
                this->waitstates.current_transaction++;
            }
            else {
                ARM7TDMI_run(&this->cpu);
            }
        }
        tick_timers(this, this->waitstates.current_transaction);
        sample_audio(this, this->waitstates.current_transaction);
        this->scanline_cycles_to_execute -= (i32)this->waitstates.current_transaction;
        this->clock.master_cycle_count += this->waitstates.current_transaction;

        if (dbg.do_break) {
            this->scanline_cycles_to_execute = 0;
            break;
        }
    }
}

u64 GBA_clock_current(struct GBA *this)
{
    return this->clock.master_cycle_count + this->waitstates.current_transaction;
}

u32 GBAJ_finish_scanline(JSM)
{
    JTHIS;
    GBA_PPU_start_scanline(this);
    if (dbg.do_break) return 0;
    cycle_DMA_and_CPU(this, MASTER_CYCLES_BEFORE_HBLANK);
    if (dbg.do_break) return 0;
    GBA_PPU_hblank(this);
    if (dbg.do_break) return 0;
    cycle_DMA_and_CPU(this, HBLANK_CYCLES);
    if (dbg.do_break) return 0;
    GBA_PPU_finish_scanline(this);
    return 0;
}

static u32 GBAJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    ARM7TDMI_run(&this->cpu);
    return 0;
}

static void GBAJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    memcpy(this->BIOS.data, mfs->files[0].buf.ptr, 16384);
    this->BIOS.has = 1;
}

static void GBAIO_unload_cart(JSM)
{
}

static void GBAIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    u32 r;
    GBA_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, pio, &r);
    GBAJ_reset(jsm);
}

static void setup_lcd(struct JSM_DISPLAY *d)
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

static void setup_audio(struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 262144;
    chan->low_pass_filter = 24000;
}


static void GBAJ_describe_io(JSM, struct cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *controller = cvec_push_back(this->jsm.IOs);
    GBA_controller_setup_pio(controller);
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
    d->cartridge_port.load_cart = &GBAIO_load_cart;
    d->cartridge_port.unload_cart = &GBAIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(240 * 160 * 2);
    d->display.output[1] = malloc(240 * 160 * 2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_lcd(&d->display);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    this->ppu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
}
