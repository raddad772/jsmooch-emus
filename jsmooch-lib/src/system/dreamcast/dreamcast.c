//
// Created by Dave on 2/11/2024.
//

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "gdi.h"
#include "gdrom.h"
#include "helpers/scheduler.h"
#include "helpers/sys_interface.h"
#include "dreamcast.h"
#include "dc_mem.h"
#include "holly.h"
#include "controller.h"

#define IP_BIN

#define sched_printf(...) (void)0

#define JTHIS struct DC* this = (struct DC*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct NES* this

#define DC_CYCLES_PER_FRAME (DC_CYCLES_PER_SEC / 60)

void DCJ_play(JSM);
void DCJ_pause(JSM);
void DCJ_stop(JSM);
void DCJ_get_framevars(JSM, struct framevars* out);
void DCJ_reset(JSM);
void DCJ_killall(JSM);
u32 DCJ_finish_frame(JSM);
u32 DCJ_finish_scanline(JSM);
u32 DCJ_step_master(JSM, u32 howmany);
void DCJ_load_BIOS(JSM, struct multi_file_set* mfs);
void DCJ_enable_tracing(JSM);
void DCJ_disable_tracing(JSM);
static void DC_schedule_frame(struct DC* this);
static void new_frame(struct DC* this, u32 copy_buf);
void DCJ_describe_io(JSM, struct cvec* IOs);
static void DCJ_sideload(JSM, struct multi_file_set* mfs);

#define JITTER 448

void DC_new(JSM)
{
    fflush(stdout);
    do_sh4_decode();
    struct DC* this = (struct DC*)malloc(sizeof(struct DC));
    scheduler_init(&this->scheduler);
    this->scheduler.jitter = JITTER;

    dbg.dcptr = this;

    SH4_init(&this->sh4, &this->scheduler);
    this->described_inputs = 0;
    this->sh4.mptr = (void *)this;
    this->sh4.read = &DCread_noins;
    this->sh4.write = &DCwrite;
    this->sh4.fetch_ins = &DCfetch_ins;
    SH4_give_memaccess(&this->sh4, &this->sh4mem);
    DC_mem_init(this);
    holly_init(this);

    for (u32 i = 0; i < 4; i++) {
        MAPLE_port_init(&this->maple.ports[i]);
    }
    DC_controller_init(&this->c1);
    DC_controller_connect(this, 0, &this->c1);

    this->gdrom.gdi.num_tracks = 0;

    GDROM_init(this);
    GDROM_reset(this);

    this->clock.frame_cycle = 0;
    this->clock.cycles_per_frame = DC_CYCLES_PER_SEC / 60;

    this->sb.SB_FFST = this->sb.SB_FFST_rc = 0;

    /*NES_clock_init(&this->clock);
    //NES_bus_init(&this, &this->clock);
    r2A03_init(&this->cpu, this);
    NES_PPU_init(&this->ppu, this);
    NES_cart_init(&this->cart, this);
    NES_mapper_init(&this->bus, this);

    */
    holly_reset(this);
    this->holly.master_frame = -1;

    new_frame(this, 0);

    buf_init(&this->BIOS);
    buf_init(&this->ROM);
    buf_init(&this->flash.buf);
    GDROM_reset(this);

    this->settings.broadcast = 4; // NTSC
    this->settings.region = 0; //1; // EN
    this->settings.language = 6; //1; // EN

    jsm->ptr = (void*)this;
    jsm->kind = SYS_DREAMCAST;

    jsm->finish_frame = &DCJ_finish_frame;
    jsm->finish_scanline = &DCJ_finish_scanline;
    jsm->step_master = &DCJ_step_master;
    jsm->reset = &DCJ_reset;
    jsm->load_BIOS = &DCJ_load_BIOS;
    jsm->killall = &DCJ_killall;
    jsm->get_framevars = &DCJ_get_framevars;
    jsm->enable_tracing = &DCJ_enable_tracing;
    jsm->disable_tracing = &DCJ_disable_tracing;
    jsm->play = &DCJ_play;
    jsm->pause = &DCJ_pause;
    jsm->stop = &DCJ_stop;
    jsm->describe_io = &DCJ_describe_io;
    jsm->sideload = &DCJ_sideload;
}

void DC_delete(struct jsm_system* jsm)
{
    JTHIS;

    if (this->gdrom.gdi.num_tracks > 0)
        GDI_delete(&this->gdrom.gdi);

    buf_delete(&this->BIOS);
    buf_delete(&this->ROM);
    buf_delete(&this->flash.buf);
    scheduler_delete(&this->scheduler);
    holly_delete(this);
    SH4_delete(&this->sh4);

    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm->finish_frame = NULL;
    jsm->finish_scanline = NULL;
    jsm->step_master = NULL;
    jsm->reset = NULL;
    jsm->load_BIOS = NULL;
    jsm->killall = NULL;
    jsm->get_framevars = NULL;
    jsm->play = NULL;
    jsm->pause = NULL;
    jsm->stop = NULL;
    jsm->enable_tracing = NULL;
    jsm->disable_tracing = NULL;
    jsm->describe_io = NULL;
}

void DC_copy_fb(struct DC* this, u32* where) {
    u32* ptr = (u32*)this->VRAM;
    ptr += this->holly.FB_R_SOF1.field;
    //ptr += 0x00020000;

    //printf("\nRENDER USING PTR %08llx", ptr - ((u32*)this->VRAM));

    u32* out = where;
    u8* rgb;
    for (u32 y = 0; y <= this->holly.FB_R_SIZE.fb_y_size; y++) {
        out = (where + (y * 640));
        for (u32 x = 0; x <= this->holly.FB_R_SIZE.fb_x_size; x++) {
            rgb = (u8*) ptr;
            u32 r = rgb[0];
            u32 g = rgb[1];
            u32 b = rgb[2];
            *out = (b << 16) | (g << 8) | (r) | 0xFF000000;
            ptr++;
            out++;
        }
        ptr += (this->holly.FB_R_SIZE.fb_modulus) - 1;
    }
}

void DCJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void DCJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

void DCJ_play(JSM)
{
    JTHIS;
    // FOr now we use this to copy framebuffer
    this->holly.cur_output = this->holly.display->device.display.output[0];
}

void DCJ_pause(JSM)
{

}

void DCJ_stop(JSM)
{
    JTHIS;
    DC_copy_fb(this, this->holly.cur_output);
    this->holly.master_frame++;
}

void DCJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_cycle = this->sh4.clock.trace_cycles;
    out->master_frame = this->holly.master_frame;
    out->last_used_buffer = this->holly.display->device.display.last_written;
}

void DCJ_reset(JSM)
{
    JTHIS;
    SH4_reset(&this->sh4);
}


void DCJ_killall(JSM)
{

}

static void new_frame(struct DC* this, u32 copy_buf)
{
    this->clock.frame_cycle = 0;
    this->clock.frame_start_cycle = (i64)this->sh4.clock.trace_cycles;
    this->holly.master_frame++;
    this->clock.interrupt.vblank_in_yet = this->clock.interrupt.vblank_out_yet = 0;
    DC_recalc_frame_timing(this);
    if (copy_buf) DC_copy_fb(this, this->holly.cur_output);
    this->holly.master_frame++;
    DC_schedule_frame(this);
}

enum DC_frame_events {
    evt_EMPTY=0,
    evt_FRAME_START,
    evt_VBLANK_IN,
    evt_VBLANK_OUT,
    evt_FRAME_END,
};

struct DCF_event {
    u32 cycle;
    enum DC_frame_events kind;
};

static void DC_schedule_frame(struct DC* this)
{
    // events
    // frame start @0.
    // vblank_in_start @?
    // vblank_out_start @?
    // frame end @200mil
    sched_printf("\nScheduling frame @ %lld", this->clock.frame_start_cycle);
    scheduler_add(&this->scheduler, this->clock.frame_start_cycle, SE_keyed_event, evt_FRAME_START, 0);
    scheduler_add(&this->scheduler, this->clock.frame_start_cycle + this->clock.interrupt.vblank_in_start, SE_keyed_event, evt_VBLANK_IN, 0);
    scheduler_add(&this->scheduler, this->clock.frame_start_cycle + this->clock.interrupt.vblank_out_start, SE_keyed_event, evt_VBLANK_OUT, 0);
    scheduler_add(&this->scheduler, this->clock.frame_start_cycle+DC_CYCLES_PER_FRAME, SE_keyed_event, evt_FRAME_END, 0);
}

u32 DCJ_finish_frame(JSM)
{
    JTHIS;
    DCJ_step_master(jsm, DC_CYCLES_PER_FRAME - this->clock.frame_cycle);
    return 0;
}

u32 DCJ_finish_scanline(JSM)
{
    JTHIS;
    assert(1==0);
    return 0;
}

u32 DCJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    i64 steps_done = 0;

    if (this->scheduler.first_event == NULL)
        DC_schedule_frame(this);
    u32 quit = 0;
    i64 cycles_left = (i64)howmany;

    i64 cycles_start = this->sh4.clock.trace_cycles;

    u64 key;
    while(cycles_left > 0) {
        // Clear scheduled events
        while((key = scheduler_next_event_if_any(&this->scheduler)) != -1) {
            // handle an event if any exist
            switch ((enum DC_frame_events)key) {
                case evt_FRAME_START:
                    sched_printf("\nFRAME START: %llu", this->sh4.clock.trace_cycles);
                    this->clock.frame_cycle = 0;
                    this->clock.frame_start_cycle = (i64)this->sh4.clock.trace_cycles;
                    this->clock.in_vblank = 1;
                    break;
                case evt_VBLANK_IN:
                    holly_vblank_in(this);
                    break;
                case evt_VBLANK_OUT:
                    holly_vblank_out(this);
                    break;
                case evt_EMPTY:
                    break;
                case evt_FRAME_END:
                    sched_printf("\nEVENT: FRAME END %llu", this->sh4.clock.trace_cycles);
                    new_frame(this, 1);
                    break;
                default:
                    printf("\nUNKNOWN FRAME EVENT %d", this->clock.frame_cycle);
                    break;
            }
            if (dbg.do_break) break;
        }
        if (quit || dbg.do_break) {
            break;
        }
        i64 til_next_event = scheduler_til_next_event(&this->scheduler, this->sh4.clock.trace_cycles);
        if (til_next_event > cycles_left) til_next_event = cycles_left;

        i64 timer_old_cycles = (i64)this->sh4.clock.timer_cycles;
        i64 old_cycles = (i64)this->sh4.clock.trace_cycles;
        u64 old_tcb = this->sh4.clock.trace_cycles_blocks;
        //this->sh4.clock.trace_cycles_blocks += til_next_event;
        SH4_run_cycles(&this->sh4, til_next_event);
        i64 timer_ran_cycles = (i64)this->sh4.clock.timer_cycles - timer_old_cycles;
        i64 ran_cycles = (i64)this->sh4.clock.trace_cycles - old_cycles;
        //this->sh4.clock.trace_cycles_blocks = old_tcb + ran_cycles;
        this->sh4.clock.trace_cycles_blocks += timer_ran_cycles;
        this->clock.frame_cycle += ran_cycles;
        scheduler_ran_cycles(&this->scheduler, ran_cycles);
        cycles_left -= ran_cycles;

        if (dbg.do_break) break;
    }
    sched_printf("\nSTEPS:%lli BRK:%d", this->sh4.clock.trace_cycles - cycles_start, dbg.do_break);

    printf("\n\nCONSOLE:\n---\n%s", this->sh4.console.ptr);
    return steps_done;
}

void DCJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    // We expect dc_boot.bin and dc_flash.bin
    u32 found = 0;
    for (u32 i = 0; i < mfs->num_files; i++) {
        struct read_file_buf* rfb = &mfs->files[i];
        if (!strcmp(rfb->name, "dc_boot.bin")) {
            buf_copy(&this->BIOS, &rfb->buf);
            found++;
        }
        else if (!strcmp(rfb->name, "dc_flash.bin")) {
            buf_copy(&this->flash.buf, &rfb->buf);
            found++;
        }
        else {
            printf("\n UNKNOWN FILE? %s", rfb->name);
        }
    }
    if (found != 2) {
        printf("\nHmmm what?!?!?! DC BIOS LOAD FAILURE!?!?!");
        fflush(stdout);
    }
}

static void DCIO_close_drive(JSM)
{

}

static void DCIO_open_drive(JSM)
{

}

static void DCIO_remove_disc(JSM)
{

}

static void DCIO_insert_disc(JSM, struct multi_file_set *mfs)
{
    JTHIS;
    GDI_load(mfs->files[0].path, mfs->files[0].name, &this->gdrom.gdi);
}

static void new_button(struct JSM_CONTROLLER* cnt, const char* name, enum HID_digital_button_common_id common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    sprintf(b->name, "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}

static void setup_controller(struct DC* this, u32 num, const char*name, u32 connected)
{
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    sprintf(d->device.controller.name, "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;

    struct JSM_CONTROLLER* cnt = &d->device.controller;

    // up down left right a b start select. in that order
    new_button(cnt, "up", DBCID_co_up);
    new_button(cnt, "down", DBCID_co_down);
    new_button(cnt, "left", DBCID_co_left);
    new_button(cnt, "right", DBCID_co_right);
    new_button(cnt, "a", DBCID_co_fire1);
    new_button(cnt, "b", DBCID_co_fire2);
    new_button(cnt, "start", DBCID_co_start);
}

void DCJ_describe_io(JSM, struct cvec* IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    setup_controller(this, 0, "Player 1", 1);

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->device.chassis.digital_buttons);
    sprintf(b->name, "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = cvec_push_back(&chassis->device.chassis.digital_buttons);
    b->common_id = DBCID_ch_reset;
    sprintf(b->name, "Reset");
    b->state = 0;

    // GDROM
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISC_DRIVE, 1, 1, 1, 0);
    d->device.disc_drive.insert_disc = &DCIO_insert_disc;
    d->device.disc_drive.remove_disc = &DCIO_remove_disc;
    d->device.disc_drive.open_drive = &DCIO_open_drive;
    d->device.disc_drive.close_drive = &DCIO_close_drive;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    d->device.display.fps = 60;
    d->device.display.output[0] = malloc(640*480*4);
    d->device.display.output[1] = malloc(640*480*4);
    this->holly.display = d;
    this->holly.cur_output = (u32 *)d->device.display.output[0];
    d->device.display.last_written = 1;
    d->device.display.last_displayed = 1;

    this->c1.devices = IOs;
    this->c1.device_index = 0;

}


void DCJ_old_load_ROM(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    buf_copy(&this->ROM, b);

    u32 offset = 0x8000;

    for (u32 i = 0; i < this->ROM.size; i++) {
        this->RAM[offset+i] = ((u8 *)this->ROM.ptr)[i];
    }

    //this->sh4.regs.PC = 0xAC010000; // for like a demo
    this->sh4.regs.PC = 0xAC008300; // for IP.BIN
    for (u32 i = 0; i < 15; i++) {
        this->sh4.regs.R[i] = 0;
        if (i < 8) this->sh4.regs.R_[i] = 0;
    }
    this->sh4.regs.R[15] = 0x8C00D400;

    // Thanks Senryoku!
    // RTE - Some interrupts jump there instead of having their own RTE, I have NO idea why.
    this->sh4.write(this->sh4.mptr, 0x8C000010, 0x00090009, DC32); // nop nop
    this->sh4.write(this->sh4.mptr, 0x8C000014, 0x0009002B, DC32); // rte nop
    // RTS
    this->sh4.write(this->sh4.mptr, 0x8C000018, 0x00090009, DC32);
    this->sh4.write(this->sh4.mptr, 0x8C00001C, 0x0009000B, DC32);

    // Write some values to HOLLY...
    this->sh4.write(this->sh4.mptr, 0x005F8048, 6, DC32);          // FB_W_CTRL
    this->sh4.write(this->sh4.mptr, 0x005F8060, 0x00600000, DC32); // FB_W_SOF1
    this->sh4.write(this->sh4.mptr, 0x005F8064, 0x00600000, DC32); // FB_W_SOF2
    this->sh4.write(this->sh4.mptr, 0x005F8044, 0x0080000D, DC32); // FB_R_CTRL
    this->sh4.write(this->sh4.mptr, 0x005F8050, 0x00200000, DC32); // FB_R_SOF1
    this->sh4.write(this->sh4.mptr, 0x005F8054, 0x00200000, DC32); // FB_R_SOF2

    DC_schedule_frame(this);
}

// Thank you for this Deecey
static void DC_CPU_state_after_boot_rom(struct DC* this)
{
    struct SH4* sh4 = &this->sh4;
    SH4_SR_set(sh4, 0x400000F1);
    SH4_regs_FPSCR_set(&sh4->regs, 0x00040001);

    sh4->regs.R[0] = 0xAC0005D8;
    sh4->regs.R[1] = 0x00000009;
    sh4->regs.R[2] = 0xAC00940C;
    sh4->regs.R[3] = 0;
    sh4->regs.R[4] = 0xAC008300;
    sh4->regs.R[5] = 0xF4000000;
    sh4->regs.R[6] = 0xF4002000;
    sh4->regs.R[7] = 0x00000044;
    sh4->regs.R[8] = 0;
    sh4->regs.R[9] = 0;
    sh4->regs.R[10] = 0;
    sh4->regs.R[11] = 0;
    sh4->regs.R[12] = 0;
    sh4->regs.R[13] = 0;
    sh4->regs.R[14] = 0;
    sh4->regs.R[15] = 0;

    sh4->regs.R_[0] = 0xDFFFFFFF;
    sh4->regs.R_[1] = 0x500000F1;
    sh4->regs.R_[2] = 0;
    sh4->regs.R_[3] = 0;
    sh4->regs.R_[4] = 0;
    sh4->regs.R_[5] = 0;
    sh4->regs.R_[6] = 0;
    sh4->regs.R_[7] = 0;

    sh4->regs.GBR = 0x8C000000;
    sh4->regs.SSR = 0x40000001;
    sh4->regs.SPC = 0x8C000776;
    sh4->regs.SGR = 0x8D000000;
    sh4->regs.DBR = 0x8C000010;
    sh4->regs.VBR = 0x8C000000;
    sh4->regs.PR = 0x0C00043C;
    sh4->regs.FPUL.u = 0;

    sh4->regs.PC = 0xAC008300; // IP.bin start address
}

static void DC_RAM_state_after_boot_rom(struct DC* this, struct read_file_buf *IPBIN)
{
    memset(&this->RAM[0x00200000], 0, 0x1000000);

    for (u32 i = 0; i < 16; i++) {
        this->sh4.write(this, 0x8C0000E0 + 2 * i, this->sh4.read(this, 0x800000FE - 2 * i, DC16), DC16);
    }
    this->sh4.write(this, 0xA05F74E4, 0x001FFFFF, DC32);

    memcpy(&this->RAM[0x100], ((u8 *)this->BIOS.ptr) + 0x100, 0x3F00);
    memcpy(&this->RAM[0x8000], ((u8 *)this->BIOS.ptr) + 0x8000, 0x1F800);

    this->sh4.write(this, 0x8C0000B0, 0x8C003C00, DC32);
    this->sh4.write(this, 0x8C0000B4, 0x8C003D80, DC32);
    this->sh4.write(this, 0x8C0000B8, 0x8C003D00, DC32);
    this->sh4.write(this, 0x8C0000BC, 0x8C001000, DC32);
    this->sh4.write(this, 0x8C0000C0, 0x8C0010F0, DC32);
    this->sh4.write(this, 0x8C0000E0, 0x8C000800, DC32);

    this->sh4.write(this, 0x8C0000AC, 0xA05F7000, DC32);
    this->sh4.write(this, 0x8C0000A8, 0xA0200000, DC32);
    this->sh4.write(this, 0x8C0000A4, 0xA0100000, DC32);
    this->sh4.write(this, 0x8C0000A0, 0, DC32);
    this->sh4.write(this, 0x8C00002C, 0, DC32);
    this->sh4.write(this, 0x8CFFFFF8, 0x8C000128, DC32);

    //         // Load IP.bin from disk (16 first sectors of the last track)
    //        // FIXME: Here we assume the last track is the 3rd.
    // TODO
    if (IPBIN) {
        printf("\nLoading IP.BIN...");
        memcpy(&this->RAM[0x8000], IPBIN->buf.ptr, 0x8000);
    }


    // IP.bin patches
    this->sh4.write(this, 0xAC0090Db, 0x5113, DC16);
    this->sh4.write(this, 0xAC00940A, 0xB, DC16);
    this->sh4.write(this, 0xAC00940C, 0x9, DC16);

    this->sh4.write(this, 0x8C000000, 0x00090009, DC32);
    this->sh4.write(this, 0x8C000004, 0x001B0009, DC32);
    this->sh4.write(this, 0x8C000008, 0x0009AFFD, DC32);

    this->sh4.write(this, 0x8C00000C, 0, DC16);
    this->sh4.write(this, 0x8C00000E, 0, DC16);

    this->sh4.write(this, 0x8C000010, 0x00090009, DC32);
    this->sh4.write(this, 0x8C000014, 0x0009002B, DC32);

    this->sh4.write(this, 0x8C000018, 0x00090009, DC32);
    this->sh4.write(this, 0x8C00001C, 0x0009000B, DC32);

    this->sh4.write(this, 0x8C00002C, 0x16, DC8);
    this->sh4.write(this, 0x8C000064, 0x8C008100, DC32);
    this->sh4.write(this, 0x8C000090, 0, DC16);
    this->sh4.write(this, 0x8C000092, -128, DC16);

    this->maple.SB_MDST = 0;
    this->g2.SB_DDST = 0;
}


// Thanks to Deecey for values to write
static void DCJ_sideload(JSM, struct multi_file_set* mfs) {
    JTHIS;
    DC_CPU_state_after_boot_rom(this);
    DC_RAM_state_after_boot_rom(this, &mfs->files[1]);

    memcpy(&this->RAM[0x10000], mfs->files[0].buf.ptr, mfs->files[0].buf.size);
    this->sh4.regs.PC = 0xAC010000;
    // 0xac010000 and start at 0xac010000
}