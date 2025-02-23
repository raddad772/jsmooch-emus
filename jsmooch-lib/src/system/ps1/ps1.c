//
// Created by . on 2/11/25.
//

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ps1.h"
#include "ps1_bus.h"
#include "gpu/ps1_gpu.h"
#include "ps1_debugger.h"

#include "helpers/debugger/debugger.h"
#include "component/cpu/r3000/r3000.h"

#include "helpers/multisize_memaccess.c"
#include "helpers/physical_io.h"

#define JTHIS struct PS1* this = (struct PS1*)jsm->ptr
#define JSM struct jsm_system* jsm

static void PS1J_play(JSM);
static void PS1J_pause(JSM);
static void PS1J_stop(JSM);
static void PS1J_get_framevars(JSM, struct framevars* out);
static void PS1J_reset(JSM);
static u32 PS1J_finish_frame(JSM);
static u32 PS1J_finish_scanline(JSM);
static u32 PS1J_step_master(JSM, u32 howmany);
static void PS1J_load_BIOS(JSM, struct multi_file_set* mfs);
static void PS1J_describe_io(JSM, struct cvec* IOs);

// 240x160, but 308x228 with v and h blanks
#define PS1_CYCLES_PER_FRAME_NTSC 564480
#define PS1_CYCLES_PER_FRAME_PAL 677376


static void setup_debug_waveform(struct debug_waveform *dw)
{
    /*if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = ((float)MASTER_CYCLES_PER_FRAME / (float)dw->samples_requested);
    dw->user.buf_pos = 0;*/
}

void PS1J_set_audiobuf(struct jsm_system* jsm, struct audiobuf *ab)
{
    JTHIS;
    /*this->audio.buf = ab;
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
    */
}

static u32 read_trace_cpu(void *ptr, u32 addr, u32 sz)
{
    struct PS1 *this = (struct PS1 *)ptr;
    return PS1_mainbus_read(this, addr, sz, 0);
}

static void PS1_update_SR(void *ptr, struct R3000 *core, u32 val)
{
    struct PS1 *this = (struct PS1 *)ptr;
    this->mem.cache_isolated = (val & 0x10000) == 0x10000;
    printf("\nNew SR: %04x", core->regs.COP0[12] & 0xFFFF);
}

static void BIOS_patch_reset(struct PS1 *this)
{
    memcpy(this->mem.BIOS, this->mem.BIOS_unpatched, 512 * 1024);
}

static void BIOS_patch(struct PS1 *this, u32 addr, u32 val)
{
    cW32(this->mem.BIOS, addr, val);
}

static void PS1J_sideload(JSM, struct multi_file_set *fs) {
    JTHIS;
    buf_allocate(&this->sideloaded, fs->files[0].buf.size);
    memcpy(this->sideloaded.ptr, fs->files[0].buf.ptr, fs->files[0].buf.size);
}

static u32 snoop_read(void *ptr, u32 addr, u32 sz, u32 has_effect)
{
    u32 r = PS1_mainbus_read(ptr, addr, sz, has_effect);
    //printf("\nread %08x (%d): %08x", addr, sz, r);
    return r;
}

static void snoop_write(void *ptr, u32 addr, u32 sz, u32 val)
{
    //printf("\nwrite %08x (%d): %08x", addr, sz, val);
    PS1_mainbus_write(ptr, addr, sz, val);
}

void PS1_new(struct jsm_system *jsm)
{
    struct PS1* this = (struct PS1*)malloc(sizeof(struct PS1));
    memset(this, 0, sizeof(*this));
    R3000_init(&this->cpu, &this->clock.master_cycle_count, &this->clock.waitstates);
    buf_init(&this->sideloaded);
    this->cpu.read_ptr = this;
    this->cpu.write_ptr = this;
    this->cpu.read = &snoop_read;
    //this->cpu.read = &PS1_mainbus_read;
    this->cpu.write = &snoop_write;
    //this->cpu.write = &PS1_mainbus_write;
    this->cpu.fetch_ptr = this;
    this->cpu.fetch_ins = &PS1_mainbus_fetchins;
    this->cpu.update_sr_ptr = this;
    this->cpu.update_sr = &PS1_update_SR;

    PS1_bus_init(this);
    PS1_GPU_init(this);
    PS1_SPU_init(this);
    /*PS1_clock_init(&this->clock);
    PS1_cart_init(&this->cart);
    PS1_PPU_init(this);
    PS1_APU_init(this);*/

    snprintf(jsm->label, sizeof(jsm->label), "PlayStation");
    struct jsm_debug_read_trace dt;
    dt.read_trace_arm = &read_trace_cpu;
    dt.ptr = this;
    R3000_setup_tracing(&this->cpu, &dt, &this->clock.master_cycle_count, 1);

    this->jsm.described_inputs = 0;
    this->jsm.IOs = NULL;
    this->jsm.cycles_left = 0;

    jsm->ptr = (void*)this;

    jsm->finish_frame = &PS1J_finish_frame;
    jsm->finish_scanline = &PS1J_finish_scanline;
    jsm->step_master = &PS1J_step_master;
    jsm->reset = &PS1J_reset;
    jsm->load_BIOS = &PS1J_load_BIOS;
    jsm->get_framevars = &PS1J_get_framevars;
    jsm->play = &PS1J_play;
    jsm->pause = &PS1J_pause;
    jsm->stop = &PS1J_stop;
    jsm->describe_io = &PS1J_describe_io;
    jsm->set_audiobuf = &PS1J_set_audiobuf;
    jsm->sideload = &PS1J_sideload;
    jsm->setup_debugger_interface = &PS1J_setup_debugger_interface;
    jsm->save_state = NULL;
    jsm->load_state = NULL;

}

void PS1_delete(struct jsm_system *jsm)
{
    JTHIS;

    R3000_delete(&this->cpu);
    buf_delete(&this->sideloaded);
    while (cvec_len(this->jsm.IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->jsm.IOs);
        if (pio->kind == HID_DISC_DRIVE) {
            if (pio->disc_drive.remove_disc) pio->disc_drive.remove_disc(jsm);
        }
        physical_io_device_delete(pio);
    }

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

static void set_irq(struct PS1 *this, enum PS1_IRQ from, u32 level)
{
    IRQ_multiplexer_set_level(&this->cpu.io.I_STAT, level, from);
    R3000_update_I_STAT(&this->cpu);
}

static void run_cycles(struct PS1 *this, i64 num)
{
    this->cycles_left += (i64)num;
    i64 block = 20;
    while (this->cycles_left > 0) {
        i64 clock_at_loop_start = this->clock.master_cycle_count;
        if (block > this->cycles_left) block = this->cycles_left;
        R3000_cycle(&this->cpu, block);
        i64 diff = (i64)this->clock.master_cycle_count - clock_at_loop_start;
        this->clock.cycles_left_this_frame -= diff;

        if (this->clock.cycles_left_this_frame <= 0) {
            this->clock.cycles_left_this_frame += PS1_CYCLES_PER_FRAME_NTSC;
            set_irq(this, PS1IRQ_VBlank, 1);
        }

        this->clock.master_cycle_count += block;
        this->cycles_left -= block;
        if (dbg.do_break) break;
    }
}

static void copy_vram(struct PS1 *this)
{
    memcpy(this->gpu.cur_output, this->gpu.VRAM, 1024*1024);
    this->gpu.cur_output = ((u16 *)this->gpu.display->output[this->gpu.display->last_written ^ 1]);
    this->gpu.display->last_written ^= 1;
    this->clock.master_frame++;
}

u32 PS1J_finish_frame(JSM)
{
    JTHIS;
#ifdef LYCODER
    dbg_enable_trace();
#endif
    run_cycles(this, PS1_CYCLES_PER_FRAME_NTSC);
    copy_vram(this);
    if (dbg.do_break) {
        printf("\nDUMP!");
        dbg.trace_on += 1;
        dbg_flush();
        dbg.trace_on -= 1;
    }
    dbg_flush();
    return 0;
}

void PS1J_play(JSM)
{
}

void PS1J_pause(JSM)
{
}

void PS1J_stop(JSM)
{
}

void PS1J_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.master_frame;
    out->x = this->clock.crt.x;
    out->scanline = this->clock.crt.y;
    out->master_cycle = this->clock.master_cycle_count;
}

static void skip_BIOS(struct PS1* this)
{
}

static void sideload_EXE(struct PS1 *this, struct buf *w)
{
    u8 *r = w->ptr;
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
        BIOS_patch_reset(this);

        if (file_size >= 4) {
            u32 address_read = 0x800;
            u32 address_write = load_addr & 0x1FFFFF;
            printf("\nWrite %d bytes from %08x to %08x", file_size, address_write, address_write+file_size);
            for (u32 i = 0; i < file_size; i+=4) {
                u32 data = cR32(r, address_read);
                cW32(this->mem.MRAM, address_write, data);
                address_read += 4;
                address_write += 4;
            }
        }

        // PC has to  e done first because we can't load it in thedelay slot?
        BIOS_patch(this, 0x6FF0, 0x3C080000 | (initial_pc >> 16));    // lui $t0, (r_pc >> 16)
        BIOS_patch(this, 0x6FF4, 0x35080000 | (initial_pc & 0xFFFF));  // ori $t0, $t0, (r_pc & 0xFFFF)
        printf("\nInitial PC: %08x", initial_pc);
        BIOS_patch(this, 0x6FF8, 0x3C1C0000 | (initial_gp >> 16));    // lui $gp, (r_gp >> 16)
        BIOS_patch(this, 0x6FFC, 0x379C0000 | (initial_gp & 0xFFFF));  // ori $gp, $gp, (r_gp & 0xFFFF)

        u32 r_sp = initial_sp_base + initial_sp_offset;
        if (r_sp != 0) {
            BIOS_patch(this, 0x7000, 0x3C1D0000 | (r_sp >> 16));   // lui $sp, (r_sp >> 16)
            BIOS_patch(this, 0x7004, 0x37BD0000 | (r_sp & 0xFFFF)); // ori $sp, $sp, (r_sp & 0xFFFF)
        } else {
            BIOS_patch(this, 0x7000, 0); // NOP
            BIOS_patch(this, 0x7004, 0); // NOP
        }

        u32 r_fp = r_sp;
        if (r_fp != 0) {
            BIOS_patch(this, 0x7008, 0x3C1E0000 | (r_fp >> 16));   // lui $fp, (r_fp >> 16)
            BIOS_patch(this, 0x700C, 0x01000008);                   // jr $t0
            BIOS_patch(this, 0x7010, 0x37DE0000 | (r_fp & 0xFFFF)); // // ori $fp, $fp, (r_fp & 0xFFFF)
        } else {
            BIOS_patch(this, 0x7008, 0);   // nop
            BIOS_patch(this, 0x700C, 0x01000008);                   // jr $t0
            BIOS_patch(this, 0x7010, 0); // // nop
        }
        printf("\nBIOS patched!");
    }
    else {
        printf("\nNOT OK EXE!");
    }
}

void PS1J_reset(JSM)
{
    JTHIS;
    R3000_reset(&this->cpu);
    PS1_bus_init(this);
    this->mem.cache_isolated = 0;
    this->clock.cycles_left_this_frame = PS1_CYCLES_PER_FRAME_NTSC;

    printf("\nPS1 reset!");
    if (this->sideloaded.size > 0) {
        return;
        sideload_EXE(this, &this->sideloaded);
    }
}

static void sample_audio(struct PS1* this, u32 num_cycles)
{
    /*
    for (u64 i = 0; i < num_cycles; i++) {
        PS1_APU_cycle(this);
        u64 mc = this->clock.master_cycle_count + i;
        if (this->audio.buf && (mc >= (u64) this->audio.next_sample_cycle)) {
            this->audio.next_sample_cycle += this->audio.master_cycles_per_audio_sample;
            if (this->audio.buf->upos < this->audio.buf->samples_len) {
                ((float *)(this->audio.buf->ptr))[this->audio.buf->upos] = PS1_APU_mix_sample(this, 0);
            }
            this->audio.buf->upos++;
        }

        struct debug_waveform *dw = cpg(this->dbg.waveforms.main);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            if (dw->user.buf_pos < dw->samples_requested) {
                dw->user.next_sample_cycle += dw->user.cycle_stride;
                ((float *) dw->buf.ptr)[dw->user.buf_pos] = PS1_APU_mix_sample(this, 1);
                dw->user.buf_pos++;
            }
        }

        dw = cpg(this->dbg.waveforms.chan[0]);
        if (mc >= (u64)dw->user.next_sample_cycle) {
            for (int j = 0; j < 6; j++) {
                dw = cpg(this->dbg.waveforms.chan[j]);
                if (dw->user.buf_pos < dw->samples_requested) {
                    dw->user.next_sample_cycle += dw->user.cycle_stride;
                    float sv = PS1_APU_sample_channel(this, j);
                    ((float *) dw->buf.ptr)[dw->user.buf_pos] = sv;
                    dw->user.buf_pos++;
                    assert(dw->user.buf_pos < 410);
                }
            }
        }
    }
     */
}

u64 PS1_clock_current(struct PS1 *this)
{
    return this->clock.master_cycle_count + this->clock.waitstates;
}

u32 PS1J_finish_scanline(JSM)
{
    JTHIS;
    printf("\nNOT SUPPORT SCANLINE ADVANCE");
    return 0;
}


static u32 PS1J_step_master(JSM, u32 howmany)
{
    JTHIS;
    this->cycles_left = 0;
    run_cycles(this, howmany);
    return 0;
}

static void PS1J_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    if (mfs->files[0].buf.size != (512*1024)) {
        printf("\nbad BIOS!");
        return;
    }
    memcpy(this->mem.BIOS, mfs->files[0].buf.ptr, 512*1024);
    memcpy(this->mem.BIOS_unpatched, mfs->files[0].buf.ptr, 512*1024);
    printf("\nLOADED BIOS!");
}

static void setup_crt(struct JSM_DISPLAY *d)
{
    d->standard = JSS_CRT;
    d->enabled = 1;

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

static void setup_audio(struct cvec* IOs)
{
    struct physical_io_device *pio = cvec_push_back(IOs);
    pio->kind = HID_AUDIO_CHANNEL;
    struct JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->sample_rate = 22050;
    chan->low_pass_filter = 22050;
}


static void PS1J_describe_io(JSM, struct cvec* IOs)
{
    cvec_lock_reallocs(IOs);
    JTHIS;
    if (this->jsm.described_inputs) return;
    this->jsm.described_inputs = 1;

    this->jsm.IOs = IOs;

    // controllers
    struct physical_io_device *controller = cvec_push_back(this->jsm.IOs);
    //PS1_controller_setup_pio(controller);
    //this->controller.pio = controller;

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
    physical_io_device_init(d, HID_DISC_DRIVE, 1, 1, 1, 0);
    // TODO: this
    d->disc_drive.open_drive = NULL;
    d->disc_drive.remove_disc = NULL;
    d->disc_drive.insert_disc = NULL;
    d->disc_drive.close_drive = NULL;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(1024*1024);
    d->display.output[1] = malloc(1024*1024);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    setup_crt(&d->display);
    this->gpu.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.last_written = 1;
    //d->display.last_displayed = 1;
    this->gpu.cur_output = (u16 *)(d->display.output[0]);

    setup_audio(IOs);

    this->gpu.display = &((struct physical_io_device *)cpg(this->gpu.display_ptr))->display;
}
