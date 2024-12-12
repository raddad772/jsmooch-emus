//
// Created by Dave on 2/4/2024.
//
#include "assert.h"
#include "stdlib.h"
#include <stdio.h>
#include <string.h>
#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"
#include "helpers/debugger/debugger.h"

#include "zxspectrum.h"

#define JTHIS struct ZXSpectrum* this = (struct ZXSpectrum*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct ZXSpectrum* this

static void ZXSpectrumJ_play(JSM);
static void ZXSpectrumJ_pause(JSM);
static void ZXSpectrumJ_stop(JSM);
static void ZXSpectrumJ_get_framevars(JSM, struct framevars* out);
static void ZXSpectrumJ_reset(JSM);
static void ZXSpectrumJ_killall(JSM);
static u32 ZXSpectrumJ_finish_frame(JSM);
static u32 ZXSpectrumJ_finish_scanline(JSM);
static u32 ZXSpectrumJ_step_master(JSM, u32 howmany);
static void ZXSpectrumJ_load_BIOS(JSM, struct multi_file_set* mfs);
static void ZXSpectrumJ_enable_tracing(JSM);
static void ZXSpectrumJ_disable_tracing(JSM);
static void ZXSpectrumJ_describe_io(JSM, struct cvec* IOs);

static u32 ZXSpectrum_CPU_read_trace(void *ptr, u32 addr);

static void ZXSpectrum_CPU_cycle(struct ZXSpectrum* this);
static void ZXSpectrumJ_setup_debugger_interface(JSM, struct debugger_interface *intf);

void ZXSpectrum_new(JSM, enum ZXSpectrum_variants variant)
{
    struct ZXSpectrum* this = (struct ZXSpectrum*)malloc(sizeof(struct ZXSpectrum));
    this->variant = variant;
    this->described_inputs = 0;

    switch(variant) {
        default:
            printf("\nUnknown ZXSpectrum, defaulting to 48k!");
            __attribute__ ((fallthrough));;
        case ZXS_spectrum48:
            this->ROM_size = 16 * 1024;
            this->RAM_size = 48 * 1024;
            break;
        case ZXS_spectrum128:
            this->ROM_size = 32 * 1024;
            this->RAM_size = 128 * 1024;
            break;
    }

    this->ROM = calloc(1, this->ROM_size);
    this->RAM = calloc(1, this->RAM_size);
    this->ROM_mask = this->ROM_size - 1;
    this->RAM_mask = this->RAM_size - 1;

    memset(&this->clock, 0, sizeof(this->clock));
    this->clock.flash.count = 16;
    /*
        this.master_clocks_per_line = 448;
        this.irq_ula_cycle = 113144
        this.irq_cpu_cycle = 56572
    */

    snprintf(jsm->label, sizeof(jsm->label), "ZX Spectrum");
    ZXSpectrum_ULA_init(this, variant);
    ZXSpectrum_tape_deck_init(this);
    Z80_init(&this->cpu, 0);

    // setup tracing reads
    struct jsm_debug_read_trace a;
    a.ptr = (void *)this;
    a.read_trace = &ZXSpectrum_CPU_read_trace;
    Z80_setup_tracing(&this->cpu, &a, &this->clock.master_cycles);

    Z80_reset(&this->cpu);

    this->cycles_left = 0;
    this->display_enabled = 1;
    jsm->ptr = (void*)this;

    jsm->finish_frame = &ZXSpectrumJ_finish_frame;
    jsm->finish_scanline = &ZXSpectrumJ_finish_scanline;
    jsm->step_master = &ZXSpectrumJ_step_master;
    jsm->reset = &ZXSpectrumJ_reset;
    jsm->load_BIOS = &ZXSpectrumJ_load_BIOS;
    jsm->get_framevars = &ZXSpectrumJ_get_framevars;
    jsm->play = &ZXSpectrumJ_play;
    jsm->pause = &ZXSpectrumJ_pause;
    jsm->stop = &ZXSpectrumJ_stop;
    jsm->describe_io = &ZXSpectrumJ_describe_io;
    jsm->sideload = NULL;
    jsm->setup_debugger_interface = &ZXSpectrumJ_setup_debugger_interface;
}

void ZXSpectrum_notify_IRQ(struct ZXSpectrum* this, u32 level)
{
    Z80_notify_IRQ(&this->cpu, level);
}

void ZXSpectrum_delete(JSM)
{
    JTHIS;
    while (cvec_len(this->IOs) > 0) {
        struct physical_io_device* pio = cvec_pop_back(this->IOs);
        physical_io_device_delete(pio);
    }

    ZXSpectrum_ULA_delete(&this->ula);
    ZXSpectrum_tape_deck_delete(this);
    if (this->RAM) {
        free(this->RAM);
        this->RAM = NULL;
    }
    if (this->ROM) {
        free(this->ROM);
        this->ROM = NULL;
    }
    free(jsm->ptr);
    jsm->ptr = NULL;

    jsm_clearfuncs(jsm);
}

static void ZXSpectrumJ_setup_debugger_interface(JSM, struct debugger_interface *intf)
{
    intf->supported_by_core = 0;
    printf("\nWARNING: debugger interface not supported on core: zxspectrum");
}

static void new_button(struct JSM_CONTROLLER* cnt, const char* name, enum JKEYS common_id)
{
    struct HID_digital_button *b = cvec_push_back(&cnt->digital_buttons);
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->state = 0;
    b->id = 0;
    b->kind = DBK_BUTTON;
    b->common_id = common_id;
}


static u32 ZXSpectrum_keyboard_keymap[40] = {
        JK_1, JK_2, JK_3, JK_4, JK_5,
        JK_0, JK_9, JK_8, JK_7, JK_6,
        JK_Q, JK_W, JK_E, JK_R, JK_T,
        JK_P, JK_O, JK_I, JK_U, JK_Y,
        JK_A, JK_S, JK_D, JK_F, JK_G,
        JK_ENTER, JK_L, JK_K, JK_J, JK_H,
        JK_CAPS, JK_Z, JK_X, JK_C, JK_V,
        JK_SPACE, JK_SHIFT, JK_M, JK_N, JK_B
};

static void setup_keyboard(struct ZXSpectrum* this)
{
    struct physical_io_device *d = cvec_push_back(this->IOs);
    physical_io_device_init(d, HID_KEYBOARD, 0, 0, 1, 1);

    d->id = 0;
    d->kind = HID_KEYBOARD;
    d->connected = 1;
    d->enabled = 1;

    struct JSM_KEYBOARD* kbd = &d->keyboard;
    memset(kbd, 0, sizeof(struct JSM_KEYBOARD));
    kbd->num_keys = 40;

    for (u32 i = 0; i < 40; i++) {
       kbd->key_defs[i] = ZXSpectrum_keyboard_keymap[i];
       if (ZXSpectrum_keyboard_keymap[i] == JK_SPACE) {
           printf("\nSPAE SETUP AT %d", i);
       }
    }
}

static u8 cpu_readmem(struct ZXSpectrum* this, u16 addr)
{
    if (addr < 0x4000) return this->bank.ROM[addr];
    return this->bank.RAM[(addr >> 14)][addr & 0x3FFF];
}

static void cpu_writemem(struct ZXSpectrum* this, u16 addr, u8 val)
{
    if (addr < 0x4000) return;
    this->bank.RAM[(addr >> 14)][addr & 0x3FFF] = (u8)val;
}

static u32 ZXSpectrum_CPU_read_trace(void *ptr, u32 addr)
{
    struct ZXSpectrum* this = (struct ZXSpectrum*)ptr;
    return cpu_readmem(this, addr);
}

static void ZXSpectrum_fast_load(struct ZXSpectrum* this)
{
    struct Z80* cpu = &this->cpu;
    struct ZXSpectrum_tape_deck *td = &this->tape_deck;
    // Replaces ROM code at 056B with instant-load kind.
    // Everything should be the same at exit, but without
    // actually doing all the waiting.
    u32 A = (cpu->regs.AF_ >> 8) & 0xFF;
    u32 F = cpu->regs.AF_ & 0xFF;
    u32 actually_load = (F & 1) == 1; // 0 = VERIFY
    cpu->regs.H = 0;
    cpu->regs.L = *(u8 *)&td->TAPE_binary.ptr[td->head_pos];
    cpu->regs.H ^= cpu->regs.L;
    td->head_pos++;
    if (td->head_pos >= td->TAPE_binary.size) td->head_pos = 0;
    if ((A ^ cpu->regs.H) != 0) { // Early-return
        return;
    }
    u32 DE = (cpu->regs.D << 8) | cpu->regs.E;
    u32 IX = cpu->regs.IX;
    while(DE > 0) {
        cpu->regs.L = *(u8*)&td->TAPE_binary.ptr[td->head_pos];
        td->head_pos++;
        if (td->head_pos >= td->TAPE_binary.size) td->head_pos = 0;
        cpu->regs.H ^= cpu->regs.L;
        if (actually_load) {
            cpu_writemem(this, IX, cpu->regs.L);
        }
        DE = (DE - 1) & 0xFFFF;
        IX = (IX + 1) & 0xFFFF;
    }
    // Last byte is not loaded into RAM
    cpu->regs.L = *(u8*)&td->TAPE_binary.ptr[td->head_pos];
    td->head_pos++;
    if (td->head_pos >= td->TAPE_binary.size) td->head_pos = 0;
    cpu->regs.H ^= cpu->regs.L;

    cpu->regs.D = cpu->regs.E = 0xFF;
    cpu->regs.IX = IX;
    cpu->regs.A = cpu->regs.H;
    Z80_regs_F_setbyte(&this->cpu.regs.F, 0x93);
    if (cpu->regs.A == 0) {
        // No error!
    } else {
        // Error!
        cpu->regs.A = cpu->regs.H = 0xFF;
        cpu->regs.F.C = +(cpu->regs.A == 0);
        printf("\nFAST LOAD ERROR!");
    }

    printf("\nFast load finish!");
}

#define SR(reg, pos) this->cpu.regs. reg = infil[pos]
#define SR2(reg, pos) this->cpu.regs. reg = infil[pos] | (infil[pos] << 8)

static void load_Z80_file(struct ZXSpectrum* this, struct multi_file_set* mfs) {
    struct buf *b = &mfs->files[0].buf;
    u8 *infil = b->ptr;

    SR(A, 0);
    Z80_regs_F_setbyte(&this->cpu.regs.F, infil[1]);
    SR(C, 2);
    SR(B, 3);
    SR(L, 4);
    SR(H, 5);
    SR2(PC, 6);
    SR2(SP, 8);
    SR(I, 10); // WAIT WHAT?
    this->cpu.regs.R = infil[10] & 0x7F | ((infil[11] & 1) << 7);
    this->ula.io.border_color = (infil[11] >> 1) & 7;
    if ((infil[11] >> 5) & 1) {
        printf("\nERROR COMPRESSED BLOCK");
        return;
    }
    SR(E, 13);
    SR(D, 14);
    SR2(BC_, 15);
    SR2(DE_, 17);
    SR2(HL_, 19);
    this->cpu.regs.AF_ = (infil[21] << 8) | infil[22];
    SR2(IY, 23);
    SR2(IX, 25);
    SR(EI, 27);
    SR(IFF2, 28);
    this->cpu.regs.IM = infil[29] & 3;
    printf("\nINTERRUPT MODE SHOULD BE 2: %d", this->cpu.regs.IM);
    u32 v = 1;
    if (infil[6] == infil[7] == 0) {
        // v2 or 3
        u32 hlen = (infil[31] << 8) | infil[30];
        if (hlen == 23) {
            v = 2;
        }
        else if (hlen == 54) {
            v = 3;
        }
        else if (hlen == 55) {
            v = 31;
        }
        else {
            printf("\nWATI WHAT? %d", hlen);
        }

    }
}

static void load_SNA_file(struct ZXSpectrum* this, struct multi_file_set* mfs)
{
    struct buf* b = &mfs->files[0].buf;
    u8 *infil = b->ptr;

    this->cpu.regs.I = infil[0];
    this->cpu.regs.HL_ = infil[1] + (infil[2] << 8);
    this->cpu.regs.DE_ = infil[3] + (infil[4] << 8);
    this->cpu.regs.BC_ = infil[5] + (infil[6] << 8);
    this->cpu.regs.AF_ = infil[7] + (infil[8] << 8);
    this->cpu.regs.L = infil[9];
    this->cpu.regs.H = infil[0x0A];
    this->cpu.regs.E = infil[0x0B];
    this->cpu.regs.D = infil[0x0C];
    this->cpu.regs.C = infil[0x0D];
    this->cpu.regs.B = infil[0x0E];
    this->cpu.regs.IY = infil[0x0F] + (infil[0x10] << 8);
    this->cpu.regs.IX = infil[0x11] + (infil[0x12] << 8);
    this->cpu.regs.IFF2 = (infil[0x13] >> 2) & 1;
    this->cpu.regs.IFF1 = infil[0x13] & 1;
    this->cpu.regs.R = infil[0x14];
    Z80_regs_F_setbyte(&this->cpu.regs.F, infil[0x15]);
    this->cpu.regs.A = infil[0x16];
    this->cpu.regs.SP = infil[0x17] + (infil[0x18] << 8);
    this->cpu.regs.IM = infil[0x19];
    this->ula.io.border_color = infil[0x1A];

    memcpy(this->RAM, infil+0x1B, 48*1024);

    /* RETN */
    this->cpu.regs.Q = 0;
    //console.log('SP!', hex4(this->cpu.regs.SP));
    this->cpu.regs.WZ = cpu_readmem(this, this->cpu.regs.SP);
    this->cpu.regs.SP = (this->cpu.regs.SP + 1) & 0xFFFF;
    this->cpu.regs.WZ |= cpu_readmem(this, this->cpu.regs.SP) << 8;
    this->cpu.regs.SP = (this->cpu.regs.SP + 1) & 0xFFFF;
    this->cpu.regs.PC = this->cpu.regs.WZ;
    this->cpu.regs.IFF1 = this->cpu.regs.IFF2;
    //console.log('PC!', hex4(this->cpu.regs.PC));

    /* Initialize CPU */
    this->cpu.regs.TCU = 0;
    this->cpu.pins.Addr = this->cpu.regs.PC;
    this->cpu.pins.D = cpu_readmem(this, this->cpu.regs.PC);
    this->cpu.regs.PC = (this->cpu.regs.PC + 1) & 0xFFFF;
    this->cpu.regs.EI = 0;
    this->cpu.regs.P = 0;
    this->cpu.regs.prefix = 0;
    this->cpu.regs.rprefix = Z80P_HL;
    this->cpu.regs.IR = Z80_S_DECODE;
    this->cpu.regs.poll_IRQ = true;
}

static void ZXSpectrumIO_insert_tape(JSM, struct physical_io_device *pio, struct multi_file_set *mfs, struct buf* sram)
{
    JTHIS;
    char *s = mfs->files[0].name;
    if (ends_with(s, ".pzx")) {
        ZXSpectrum_tape_deck_load_pzx(this, mfs);
    }
    if (ends_with(s, ".tap")) {
        ZXSpectrum_tape_deck_load(this, mfs);
    }
    else if (ends_with(s, ".sna")) {
        load_SNA_file(this, mfs);
    }
    else if (ends_with(s, ".z80")) {
        load_Z80_file(this, mfs);
    }
    else {
        printf("\nCould not detect file type by extension: %s", s);
    }
}

static void ZXSpectrumIO_play(JSM)
{
    JTHIS;
    ZXSpectrum_tape_deck_play(this);
}


static void ZXSpectrumIO_stop(JSM)
{
    JTHIS;
    ZXSpectrum_tape_deck_stop(this);
}

static void ZXSpectrumIO_rewind(JSM)
{
    JTHIS;
    ZXSpectrum_tape_deck_rewind(this);
}

static void ZXSpectrumIO_remove_tape(JSM)
{
    JTHIS;
    ZXSpectrum_tape_deck_remove(this);
}

static void setup_crt48(struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;
    d->enabled = 1;

    d->fps=50.1;
    d->fps_override_hint = 50;

    // total PAL pixels is 448x312x50.1fps for ZX48, 456x311x50.8fps for ZX128
    // so. 352x304 "display" area, = 8-row blank at end
    // 256x192 are the standard draw area
    // 312 lines

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 352;
    d->pixelometry.cols.max_visible = 352;
    d->pixelometry.cols.right_hblank = 96;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 304;
    d->pixelometry.rows.max_visible = 304;
    d->pixelometry.rows.bottom_vblank = 8;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 5;
    d->geometry.physical_aspect_ratio.height = 4;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_crt128(struct JSM_DISPLAY *d)
{
    d->standard = JSS_NTSC;

    d->fps=50.8;
    d->fps_override_hint = 50;

    // total PAL pixels is 448x312x50.1fps for ZX48, 456x311x50.8fps for ZX128
    // so. 352x304 "display" area, = 8-row blank at end
    // 256x192 are the standard draw area
    // 312 lines

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 352;
    d->pixelometry.cols.right_hblank = 104;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 304;
    d->pixelometry.rows.bottom_vblank = 8;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 5;
    d->geometry.physical_aspect_ratio.height = 4;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

 void ZXSpectrumJ_describe_io(JSM, struct cvec *IOs)
{
    JTHIS;
    if (this->described_inputs) return;
    this->described_inputs = 1;

    this->IOs = IOs;

    // controllers
    setup_keyboard(this);
    this->ula.keyboard_devices = IOs;
    this->ula.keyboard_device_index = 0;

    // power and reset buttons
    struct physical_io_device* chassis = cvec_push_back(IOs);
    physical_io_device_init(chassis, HID_CHASSIS, 1, 1, 1, 1);
    struct HID_digital_button* b;
    b = cvec_push_back(&chassis->chassis.digital_buttons);
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // tape deck
    struct physical_io_device *d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_AUDIO_CASSETTE, 1, 1, 1, 0);
    d->audio_cassette.insert_tape = &ZXSpectrumIO_insert_tape;
    d->audio_cassette.remove_tape = &ZXSpectrumIO_remove_tape;
    d->audio_cassette.rewind = &ZXSpectrumIO_rewind;
    d->audio_cassette.play = &ZXSpectrumIO_play;
    d->audio_cassette.stop = &ZXSpectrumIO_stop;
    this->tape_deck.IOs = IOs;
    this->tape_deck.tape_deck_index = 2;

    // screen
    d = cvec_push_back(IOs);
    physical_io_device_init(d, HID_DISPLAY, 1, 1, 0, 1);
    this->ula.display_ptr = make_cvec_ptr(IOs, cvec_len(IOs)-1);
    d->display.output[0] = malloc(352 * 304);
    d->display.output[1] = malloc(352 * 304);
    d->display.output_debug_metadata[0] = NULL;
    d->display.output_debug_metadata[1] = NULL;
    switch(this->variant) {
        case ZXS_spectrum48:
            setup_crt48(&d->display);
            break;
        case ZXS_spectrum128:
            setup_crt128(&d->display);
            break;
        default:
            assert(1==0);
    }
    this->ula.cur_output = (u8 *)d->display.output[0];
    d->display.last_written = 1;

    this->ula.display = &((struct physical_io_device *)cpg(this->ula.display_ptr))->display;
}

static void ZXSpectrumJ_enable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

static void ZXSpectrumJ_disable_tracing(JSM)
{
    // TODO
    assert(1==0);
}

static void ZXSpectrumJ_play(JSM)
{
}

static void ZXSpectrumJ_pause(JSM)
{
}

static void ZXSpectrumJ_stop(JSM)
{
}

static void ZXSpectrumJ_get_framevars(JSM, struct framevars* out)
{
    JTHIS;
    out->master_frame = this->clock.frames_since_restart;
    out->x = this->ula.screen_x;
    out->scanline = this->ula.screen_y;
}

static void ZXSpectrumJ_reset(JSM)
{
    JTHIS;
    Z80_reset(&this->cpu);
    ZXSpectrum_ULA_reset(this);
    // RAM is weird.
    // 48k has contiguous 48k
    // 128k and 2+ have...
    // 0x4000 = bank 5
    // 0x8000 = bank 2
    // 0xc000 = bank 0 at startup
    // upper RAM address, shift right by 15, gets area # (0x4000, etc.)
    // then it's 0x4000 * that number

    // ROM is bank 0 or 1 of 16k
    this->bank.RAM[1] = this->RAM + (5 * 0x4000);
    this->bank.RAM[2] = this->RAM + (2 * 0x4000);
    this->bank.RAM[3] = this->RAM;
    this->bank.ROM = this->ROM;
    this->bank.display = this->RAM + (5 * 0x4000);
}


static void ZXSpectrumJ_killall(JSM)
{

}

static u32 ZXSpectrumJ_finish_frame(JSM)
{
    JTHIS;
    u32 current_frame = this->clock.frames_since_restart;
    u32 scanlines = 0;
    while(current_frame == this->clock.frames_since_restart) {
        scanlines++;
        ZXSpectrumJ_finish_scanline(jsm);
        if (dbg.do_break) break;
    }
    return this->ula.display->last_written;
}

static u32 ZXSpectrumJ_finish_scanline(JSM)
{
    JTHIS;
    u32 current_y = this->clock.ula_y;

    while(current_y == this->clock.ula_y) {
        ZXSpectrum_CPU_cycle(this);
        ZXSpectrum_ULA_cycle(this);
        ZXSpectrum_ULA_cycle(this);
        this->clock.master_cycles += 2;
        if (dbg.do_break) break;
    }
    return 0;
}

static void ZXSpectrum_CPU_cycle(struct ZXSpectrum* this)
{
    if (this->clock.contended && ((this->cpu.pins.Addr - 0x4000) < 0x4000)) return;
    Z80_cycle(&this->cpu);
    if (this->cpu.pins.RD) {
        if (this->cpu.pins.MRQ) {// read ROM/RAM
            this->cpu.pins.D = cpu_readmem(this, this->cpu.pins.Addr);

            if ((this->cpu.pins.Addr == 0x056B) && (this->cpu.regs.PC == 0x056C)) { // Fast tape load hack time!
                // QuickLOAD only for binary. We need to translate to binary in future...
                if (this->tape_deck.kind == tdk_binary) {
                    printf("\nquick LOAD trigger");
                    // return RET
                    this->cpu.pins.D = 0xC9;
                    // do quickload
                    ZXSpectrum_fast_load(this);
                }
            }

            printif(z80.mem, DBGC_Z80 "\nZXS(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
        } else if (this->cpu.pins.IO) { // read IO port
            this->cpu.pins.D = ZXSpectrum_ULA_reg_read(this, this->cpu.pins.Addr);
            printif(z80.io, DBGC_Z80"\nZXS(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
        }
    }
    if (this->cpu.pins.WR) {
        if (this->cpu.pins.MRQ) {// write ROM/RAM
            if (dbg.trace_on && (this->cpu.trace.last_cycle != *this->cpu.trace.cycles)) {
                this->cpu.trace.last_cycle = *this->cpu.trace.cycles;
                printif(z80.mem, DBGC_Z80 "\nZ80(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
            }
            cpu_writemem(this, this->cpu.pins.Addr, this->cpu.pins.D);
        }
        else if (this->cpu.pins.IO) {// write IO
            ZXSpectrum_ULA_reg_write(this, this->cpu.pins.Addr, this->cpu.pins.D);
            printif(z80.io, DBGC_Z80 "\nZ80(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST, *this->cpu.trace.cycles, this->cpu.pins.Addr, this->cpu.pins.D, this->cpu.regs.TCU);
        }
    }
}

static u32 ZXSpectrumJ_step_master(JSM, u32 howmany)
{
    JTHIS;
    i32 todo = (howmany >> 1);
    if (todo == 0) todo = 1;
    for (i32 i = 0; i < todo; i++) {
        ZXSpectrum_CPU_cycle(this);
        ZXSpectrum_ULA_cycle(this);
        ZXSpectrum_ULA_cycle(this);
        this->clock.master_cycles += 2;
        if (dbg.do_break) break;
    }
    return 0;
}

static void ZXSpectrumJ_load_BIOS(JSM, struct multi_file_set* mfs)
{
    JTHIS;
    struct buf* b = &mfs->files[0].buf;
    memcpy(this->ROM, b->ptr, 16384 <= b->size ? 16384 : b->size);
}

