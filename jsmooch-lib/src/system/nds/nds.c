//
// Created by . on 12/4/24.
//
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nds.h"
#include "nds_bus.h"
#include "nds_debugger.h"
#include "nds_dma.h"
#include "nds_timers.h"
#include "nds_rtc.h"
#include "nds_irq.h"
#include "nds_ge.h"
#include "nds_regs.h"
#include "nds_spi.h"

#include "helpers/debugger/debugger.h"
#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.c"

#define JTHIS struct NDS* this = (struct NDS*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

static void NDSJ_play(JSM);
static void NDSJ_pause(JSM);
static void NDSJ_stop(JSM);
static void NDSJ_get_framevars(JSM, struct framevars* out);
static void NDSJ_reset(JSM);
static u32 NDSJ_finish_frame(JSM);
static u32 NDSJ_finish_scanline(JSM);
static u32 NDSJ_step_master(JSM, u32 howmany);
static void NDSJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void NDSJ_describe_io(JSM, struct cvec* IOs);

enum NDS_frame_events {
    evt_EMPTY=0,
    evt_SCANLINE_START,
    evt_HBLANK_IN,
    evt_SCANLINE_END
};
/*
Especially when games have very bizarre bugs, such as bowsers inside story purposefully sabotaging inputs at the save select screen because I allowed cart reads from the secure area, which made it think it was running on a flashcart
Or Mario jumping way way way too high because I calculated the zero flag incorrectly for a certain multiply instruction */

// master cycles per second: 33554432 (ARM7)
//59.8261 = 560866 per frame
/*   355 x 263
 * = 2132 cycles per line
 *
H-Timing: 256 dots visible, 99 dots blanking, 355 dots total (15.7343KHz)
V-Timing: 192 lines visible, 71 lines blanking, 263 lines total (59.8261 Hz)
The V-Blank cycle for the 3D Engine consists of the 23 lines, 191..213. *
 */

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
    dw->user.cycle_stride = ((float)(33000000 / 60) / (float)dw->samples_requested);
    dw->user.buf_pos = 0;
}

void NDSJ_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    this->audio.buf = ab;
    /*if (this->audio.master_cycles_per_audio_sample == 0) {
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
    }*/

}

static u32 read_trace_cpu9(void *ptr, u32 addr, u32 sz) {
    struct NDS* this = (struct NDS*)ptr;
    return NDS_mainbus_read9(this, addr, sz, 0, 0);
}

static u32 read_trace_cpu7(void *ptr, u32 addr, u32 sz) {
    struct NDS* this = (struct NDS*)ptr;
    return NDS_mainbus_read7(this, addr, sz, 0, 0);
}

static void schedule_frame(struct NDS *this, u64 start_clock, u32 is_first);

static void do_next_scheduled_frame(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)bound_ptr;
    schedule_frame(this, current_clock-jitter, 0);
}

static void schedule_frame(struct NDS *this, u64 start_clock, u32 is_first)
{
    //printf("\nSCHEDULE FRAME START AT CYCLE %lld CUR:%lld LINE CYCLES:%lld LINE:%d!", start_clock, NDS_clock_current7(this), this->clock.timing.scanline.cycles_total, this->clock.ppu.y);
    // Schedule out a frame!
    i64 cur_clock = start_clock;
    this->clock.cycles_left_this_frame += this->clock.timing.frame.cycles;

    //this->clock.cycles_left_this_frame += this->clock.timing.frame.cycles;

    // x lines of end hblank, start hblank
    // somewhere in there, start vblank
    for (u32 line = 0; line < 263; line++) {
        // vblank start
        if (line == this->clock.timing.frame.vblank_up_on) {
            scheduler_only_add_abs(&this->scheduler, cur_clock, 1, this, &NDS_PPU_vblank, NULL);
        }
        // vblank end
        if (line == this->clock.timing.frame.vblank_down_on) {
            scheduler_only_add_abs(&this->scheduler, cur_clock, 0, this, &NDS_PPU_vblank, NULL);
        }

        // hblank down...
        scheduler_only_add_abs(&this->scheduler, cur_clock, 0, this, &NDS_PPU_hblank, NULL);
        // hblank up...
        scheduler_only_add_abs(&this->scheduler, cur_clock+this->clock.timing.scanline.cycle_of_hblank, 1, this, NDS_PPU_hblank, NULL);

        // Advance clock
        cur_clock += this->clock.timing.scanline.cycles_total;
    }

    //printf("\nSCHEDULE NEW FRAME SET FOR CYCLE %lld", start_clock+this->clock.timing.frame.cycles);
    scheduler_only_add_abs(&this->scheduler, start_clock+this->clock.timing.frame.cycles, 0, this, &do_next_scheduled_frame, NULL);
}

void NDS_schedule_more(void *ptr, u64 key, u64 clock, u32 jitter)
{
    assert(1==0);
}

static i64 run_arm7(struct NDS *this, i64 num_cycles)
{
    // Run DMA & CPU
    if (NDS_dma7_go(this)) {
    }
    else {
        if (this->io.arm7.halted) {
            this->io.arm7.halted &= ((!!(this->io.arm7.IF & this->io.arm7.IE)) ^ 1);
            if (!this->io.arm7.halted) {
                //printf("\nIF:%08x IE:%08x", this->io.arm7.IF, this->io.arm7.IE);
                dbgloglog(NDS_CAT_ARM7_HALT, DBGLS_INFO, "ARM7 RESUME! cyc:%lld IF:%08x IE:%08x", NDS_clock_current7(this), this->io.arm7.IF, this->io.arm7.IE);
            }
            this->waitstates.current_transaction++;
        }
        else {
            this->arm7_ins = 1;
            ARM7TDMI_run_noIRQcheck(&this->arm7);
            this->arm7_ins = 0;
        }
    }
    this->clock.master_cycle_count7 += this->waitstates.current_transaction;
    u32 r = this->waitstates.current_transaction;
    this->waitstates.current_transaction = 0;

    return r;
}

static i64 run_arm9(struct NDS *this, i64 num_cycles)
{
    this->arm9_ins = 1;
    // Run DMA & CPU
    if (NDS_dma9_go(this)) {
    }
    else {
        if (!this->ge.fifo.pausing_cpu) {
            this->arm9_ins = 1;
            ARM946ES_run_noIRQcheck(&this->arm9);
            this->arm9_ins = 0;
        }
        else {
            this->waitstates.current_transaction += 1;
        }
    }
    this->clock.master_cycle_count9 += this->waitstates.current_transaction;
    u32 r = this->waitstates.current_transaction;
    this->waitstates.current_transaction = 0;

    return r;
}

static void NDS_run_block(void *ptr, u64 num_cycles, u64 clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;

    this->clock.cycles7 += (i64)num_cycles;
    this->clock.cycles9 += (i64)num_cycles;
    //printf("\nRUN %lld CYCLES!", num_cycles);
    while((this->clock.cycles7 > 0) || (this->clock.cycles9 > 0)) {
        if (this->clock.cycles7 > 0) {
            this->clock.cycles7 -= run_arm7(this, this->clock.cycles7);
        }
        if (this->clock.cycles9 > 0) {
            this->clock.cycles9 -= run_arm9(this, this->clock.cycles9);
        }

        if (dbg.do_break) break;
    }
    // TODO: let this be scheduled.
    ARM7TDMI_IRQcheck(&this->arm7);
    ARM946ES_IRQcheck(&this->arm9);
}

void NDS_new(struct jsm_system *jsm)
{
    struct NDS* this = (struct NDS*)malloc(sizeof(struct NDS));
    memset(this, 0, sizeof(*this));
    this->waitstates.current_transaction = 0;
    scheduler_init(&this->scheduler, &this->clock.master_cycle_count7, &this->waitstates.current_transaction);
    this->scheduler.max_block_size = 45;

    this->scheduler.schedule_more.func = &NDS_schedule_more;
    this->scheduler.schedule_more.ptr = this;

    this->scheduler.run.func = &NDS_run_block;
    this->scheduler.run.ptr = this;

    //ARM7TDMI_init(&this->arm7, &this->clock.master_cycle_count7, &this->waitstates.current_transaction, &this->scheduler);
    ARM7TDMI_init(&this->arm7, &this->clock.master_cycle_count7, &this->waitstates.current_transaction, NULL);
    this->arm7.read_ptr = this;
    this->arm7.write_ptr = this;
    this->arm7.read = &NDS_mainbus_read7;
    this->arm7.write = &NDS_mainbus_write7;
    this->arm7.fetch_ptr = this;
    this->arm7.fetch_ins = &NDS_mainbus_fetchins7;

    ARM946ES_init(&this->arm9, &this->clock.master_cycle_count9, &this->waitstates.current_transaction, NULL);
    //ARM946ES_init(&this->arm9, &this->clock.master_cycle_count9, &this->waitstates.current_transaction, &this->scheduler);
    this->arm9.read_ptr = this;
    this->arm9.read = &NDS_mainbus_read9;

    this->arm9.write_ptr = this;
    this->arm9.write = &NDS_mainbus_write9;

    this->arm9.fetch_ptr = this;
    this->arm9.fetch_ins = &NDS_mainbus_fetchins9;

    NDS_clock_init(&this->clock);
    NDS_bus_init(this);
    NDS_cart_init(this);
    NDS_PPU_init(this);
    NDS_GE_init(this);
    NDS_RE_init(this);
    //NDS_APU_init(this);

    snprintf(jsm->label, sizeof(jsm->label), "Nintendo DS");
    struct jsm_debug_read_trace dt7;
    dt7.read_trace_arm = &read_trace_cpu7;
    dt7.ptr = this;
    ARM7TDMI_setup_tracing(&this->arm7, &dt7, &this->clock.master_cycle_count7, 1);

    struct jsm_debug_read_trace dt9;
    dt9.read_trace_arm = &read_trace_cpu9;
    dt9.ptr = this;
    ARM946ES_setup_tracing(&this->arm9, &dt9, &this->clock.master_cycle_count9, 2);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &NDSJ_finish_frame;
    jsm->finish_scanline = &NDSJ_finish_scanline;
    jsm->step_master = &NDSJ_step_master;
    jsm->reset = &NDSJ_reset;
    jsm->load_BIOS = &NDSJ_load_BIOS;
    jsm->get_framevars = &NDSJ_get_framevars;
    jsm->play = &NDSJ_play;
    jsm->pause = &NDSJ_pause;
    jsm->stop = &NDSJ_stop;
    jsm->describe_io = &NDSJ_describe_io;
    jsm->set_audiobuf = &NDSJ_set_audiobuf;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &NDSJ_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;
}

void NDS_delete(struct jsm_system *jsm)
{
    JTHIS;

    ARM7TDMI_delete(&this->arm7);
    ARM946ES_delete(&this->arm9);
    //NDS_PPU_delete(this);
    NDS_cart_delete(this);

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

u32 NDSJ_finish_frame(JSM)
{
    JTHIS;

    u64 old_clock = NDS_clock_current7(this);
    scheduler_run_for_cycles(&this->scheduler, this->clock.cycles_left_this_frame);
    u64 diff = NDS_clock_current7(this) - old_clock;
    this->clock.cycles_left_this_frame -= (i64)diff;
    return 0;

}

void NDSJ_play(JSM)
{
}

void NDSJ_pause(JSM)
{
}

void NDSJ_stop(JSM)
{
}

void NDSJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.ppu.x;
    out->scanline = this->clock.ppu.y;
    out->master_cycle = this->clock.master_cycle_count7;
}

static void skip_BIOS(struct NDS *this)
{
    // Load 170h bytes of header into main RAM starting at 27FFE00
    u32 *hdr_start = (u32 *)this->cart.ROM.ptr;
    u32 *hdr = hdr_start;

    for (i32 i = 0; i < 0x170; i += 4) {
        NDS_mainbus_write9(this, 0x027FFE00+i, 4, 0, hdr[i>>2]);
    }

    // Read binary addresses
    u32 bin7_offset, bin9_offset, bin9_lo, bin7_lo, bin9_size, bin7_size, arm7_entry, arm9_entry, bin9_hi, bin7_hi;
    bin9_offset = cR[4](hdr, 0x20);
    arm9_entry = cR[4](hdr, 0x24);
    bin9_lo = cR[4](hdr, 0x28);
    bin9_size = cR[4](hdr, 0x2C);

    bin7_offset = cR[4](hdr, 0x30);
    arm7_entry = cR[4](hdr, 0x34);
    bin7_lo = cR[4](hdr, 0x38);
    bin7_size = cR[4](hdr, 0x3C);

    // Copy binaries into RAM
    for (u32 i = 0; i < bin7_size; i += 4) {
        NDS_mainbus_write7(this, bin7_lo+i, 4, 0, cR[4](this->cart.ROM.ptr, bin7_offset+i));
    }
    for (u32 i = 0; i < bin9_size; i += 4) {
        NDS_mainbus_write9(this, bin9_lo+i, 4, 0, cR[4](this->cart.ROM.ptr, bin9_offset+i));
    }

    this->arm9.regs.R[14] = 0x03002F7C;
    this->arm9.regs.R_irq[0] = 0x03003F80;
    this->arm9.regs.R_svc[0] = 0x03003FC0;
    this->arm9.regs.R[15] = arm9_entry;
    this->arm9.regs.CPSR.mode = ARM9_system;
    ARM946ES_fill_regmap(&this->arm9);
    ARM946ES_NDS_direct_boot(&this->arm9);
    ARM946ES_reload_pipeline(&this->arm9);

    this->arm7.regs.R[13] = 0x0380FD80;
    this->arm7.regs.R_irq[0] = 0x0380FF80;
    this->arm7.regs.R_svc[0] = 0x0380FFC0;
    this->arm7.regs.R[15] = arm7_entry;
    this->arm7.regs.CPSR.mode = ARM7_system;
    ARM7TDMI_fill_regmap(&this->arm7);
    ARM7TDMI_reload_pipeline(&this->arm7);

    NDS_mainbus_write9(this, R9_WRAMCNT, 1, 0, 3); // setup WRAM


    NDS_mainbus_write9(this, 0x027FF800, 4, 0, 0x1FC2); // chip id
    NDS_mainbus_write9(this, 0x027FF804, 4, 0, 0x1FC2); // chip id
    NDS_mainbus_write9(this, 0x027FF850u, 2, 0, 0x5835); // ARM7 BIOS CRC
    NDS_mainbus_write9(this, 0x027FF880u, 2, 0, 0x0007); // Message from ARM9 to ARM7
    NDS_mainbus_write9(this, 0x027FF884u, 2, 0, 0x0006); // ARM7 boot task
    NDS_mainbus_write9(this, 0x027FFC00u, 4, 0, 0x1FC2); // Copy of chip ID 1
    NDS_mainbus_write9(this, 0x027FFC04u, 4, 0, 0x1FC2); // Copy of chip ID 2
    NDS_mainbus_write9(this, 0x027FFC10u, 4, 0, 0x5835); // Copy of ARM7 BIOS CRC
    NDS_mainbus_write9(this, 0x027FFC40u, 4, 0, 0x0001); // Boot indicator
    // Now copy 112 bytes from firmware 0x03FE00  to RAM 0x027FFC80
    for (u32 i = 0; i < 112; i++) {
        NDS_mainbus_write9(this, 0x027FFC80 + i, 1, 0, this->mem.firmware[0x03FE00+i]);
    }
    // Now write to POSTFLG registers...
    NDS_mainbus_write9(this, 0x04000300, 1, 0, 1);
    NDS_mainbus_write7(this, 0x04000300, 1, 0, 1);

    // Do some more boot stuf...
    NDS_mainbus_write9(this, R9_POWCNT1, 2, 0, 0x0203);

    NDS_cart_direct_boot(this);
    printf("\ndirect boot done!");
 }

void NDSJ_reset(JSM)
{
    JTHIS;
    // Emu resets...
    scheduler_clear(&this->scheduler);
    this->clock.master_cycle_count7 = 0;
    this->waitstates.current_transaction = 0;

    ARM7TDMI_reset(&this->arm7);
    ARM946ES_reset(&this->arm9);
    NDS_CP_reset(&this->arm9);
    NDS_clock_reset(&this->clock);
    NDS_SPI_reset(this);
    NDS_PPU_reset(this);
    NDS_mainbus_write9(this, R9_VRAMCNT+0, 1, 0, 0);
    NDS_mainbus_write9(this, R9_VRAMCNT+1, 1, 0, 0);
    NDS_mainbus_write9(this, R9_VRAMCNT+2, 1, 0, 0);
    NDS_mainbus_write9(this, R9_VRAMCNT+3, 1, 0, 0);
    NDS_mainbus_write9(this, R9_VRAMCNT+4, 1, 0, 0);
    NDS_mainbus_write9(this, R9_VRAMCNT+6, 1, 0, 0);
    NDS_mainbus_write9(this, R9_WRAMCNT, 1, 0, 3); // at R9_VRAMCNT+7
    NDS_mainbus_write9(this, R9_VRAMCNT+8, 1, 0, 0);
    NDS_mainbus_write9(this, R9_VRAMCNT+9, 1, 0, 0);

    this->io.arm7.IME = this->io.arm7.IE = this->io.arm7.IF = 0;
    this->io.arm9.IME = this->io.arm9.IE = this->io.arm9.IF = 0;
    this->io.arm7.POSTFLG = this->io.arm9.POSTFLG = 0;
    // TODO: Reset DMA and timers...
    // IPC, SPi, APU

    // Components such as RTC...
    NDS_bus_reset(this);
    NDS_GE_reset(this);
    NDS_RE_reset(this);

    skip_BIOS(this);
    this->waitstates.current_transaction = 0;
    this->clock.master_cycle_count7 = 0;
    this->clock.master_cycle_count9 = 0;

    scheduler_clear(&this->scheduler);
    this->clock.cycles_left_this_frame = 0;
    schedule_frame(this, 0, 1); // Schedule first frame
    printf("\nNDS reset complete!");
}


static void sample_audio(struct NDS* this, u32 num_cycles)
{
/*    for (u64 i = 0; i < num_cycles; i++) {
        NDS_APU_cycle(this);
        u64 mc = this->clock.master_cycle_count + i;
        if (this->audio.buf && (mc >= (u64) this->audio.next_sample_cycle)) {
            this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
            if (this->audio.buf->upos < this->audio.buf->samples_len) {
                ((float *)(this->audio.buf->ptr))[this->audio.buf->upos] = NDS_APU_mix_sample(this, 0);
            }
            this->audio.buf->upos++;
        }

        struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = NDS_APU_mix_sample(this, 1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(this->dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(this->dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = NDS_APU_sample_channel(this, j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }*/
}

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

u32 NDSJ_finish_scanline(JSM)
{
    JTHIS;

    u64 scanline_cycle = NDS_clock_current7(this) - this->clock.ppu.scanline_start;
    assert(scanline_cycle < this->clock.timing.scanline.cycles_total);

    u64 old_clock = NDS_clock_current7(this);


    NDSJ_step_master(jsm, this->clock.timing.scanline.cycles_total - scanline_cycle);
    u64 diff = NDS_clock_current7(this) - old_clock;
    this->clock.cycles_left_this_frame -= diff;
    return this->ppu.display->last_written;
}

static u32 NDSJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    scheduler_run_for_cycles(&this->scheduler, howmany);
    return this->ppu.display->last_written;
}

static void NDSJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    // 7, 9, firmware
    memcpy(this->mem.bios7, mfs->files[0].buf.ptr, 16384);
    memcpy(this->mem.bios9, mfs->files[1].buf.ptr, 4096);
    memcpy(this->mem.firmware, mfs->files[2].buf.ptr, 256 * 1024);
}

static void NDSIO_unload_cart(JSM)
{
}

static void NDSIO_load_cart(JSM, struct multi_file_set *mfs, struct physical_io_device *pio) {
    JTHIS;
    struct buf* b = &mfs->files[0].buf;

    u32 r;
    NDS_cart_load_ROM_from_RAM(&this->cart, b->ptr, b->size, pio, &r);
    //NDSJ_reset(jsm);
}

static void setup_lcd(struct JSM_DISPLAY *d)
{
    d->standard = JSS_LCD;
    d->enabled = 1;

    d->fps = 59.8261;
    d->fps_override_hint = 60;
    // 256x192, but 355x263 with v and h blanks

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 256;
    d->pixelometry.cols.max_visible = 256;
    d->pixelometry.cols.right_hblank = 99;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 384;
    d->pixelometry.rows.max_visible = 384;
    d->pixelometry.rows.bottom_vblank = 71;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 6;

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


static void NDSJ_describe_io(JSM, struct cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *controller = cvec_push_back(this->jsm.IOs);
    NDS_controller_setup_pio(controller);
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
    d->cartridge_port.load_cart = &NDSIO_load_cart;
    d->cartridge_port.unload_cart = &NDSIO_unload_cart;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(256 * 384 * 2);
    d->display.output[1] = malloc(256 * 384 * 2);
    memset(d->display.output[0], 0, 256*384*2);
    memset(d->display.output[1], 0, 256*384*2);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_lcd(&d->display);
    this->ppu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    this->ppu.cur_output = (u16 *)(d->display.output[0]);

    struct physical_io_device *t = cvec_push_back(IOs);
    physical_io_device_init(t, HID_TOUCHSCREEN, 1, 1, 1, 0);
    t->touchscreen.params.width = 256;
    t->touchscreen.params.height = 192;
    t->touchscreen.params.x_offset = 0;
    t->touchscreen.params.y_offset = -192;
    this->spi.touchscr.pio = t;

    setup_audio(IOs);

    this->ppu.display = &((struct physical_io_device *)cpg(this->ppu.display_ptr))->display;
}
