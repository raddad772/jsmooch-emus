//
// Created by Dave on 2/4/2024.
//
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "helpers/physical_io.h"
#include "helpers/sys_interface.h"

#include "zxspectrum.h"
#include "zxspectrum_bus.h"

jsm_system *ZXSpectrum_new(ZXSpectrum::variants variant) {
    return new ZXSpectrum::core(variant);
}

namespace ZXSpectrum {

static u32 CPU_read_trace(void *ptr, u32 addr)
{
    auto *th = static_cast<core *>(ptr);
    return th->CPU_readmem(addr);
}

core::core(variants variant_in) :
    clock(variant_in),
    ula(variant_in, this),
    tape_deck(this),
    cpu(false),
    variant(variant_in) {

    has.load_BIOS = true;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = false;
    has.set_audiobuf = false;
    
    described_inputs = false;

    switch(variant) {
        default:
            printf("\nUnknown ZXSpectrum, defaulting to 48k!");
            FALLTHROUGH;
        case spectrum48:
            ROM_size = 16 * 1024;
            RAM_size = 48 * 1024;
            break;
        case spectrum128:
            ROM_size = 32 * 1024;
            RAM_size = 128 * 1024;
            break;
    }

    ROM = static_cast<u8 *>(calloc(1, ROM_size));
    RAM = static_cast<u8 *>(calloc(1, RAM_size));
    ROM_mask = ROM_size - 1;
    RAM_mask = RAM_size - 1;

    clock.flash.count = 16;
    /*
        this.master_clocks_per_line = 448;
        this.irq_ula_cycle = 113144
        this.irq_cpu_cycle = 56572
    */

    snprintf(label, sizeof(label), "ZX Spectrum");

    // setup tracing reads
    jsm_debug_read_trace a;
    a.ptr = this;
    a.read_trace = &CPU_read_trace;
    cpu.setup_tracing(&a, &clock.master_cycles);

    cpu.reset();

    cycles_left = 0;
    display_enabled = true;
}

void core::notify_IRQ(bool level)
{
    cpu.notify_IRQ(level);
}
core::~core() {
    if (RAM) {
        free(RAM);
        RAM = nullptr;
    }
    if (ROM) {
        free(ROM);
        ROM = nullptr;
    }
}

static constexpr u32 ZXSpectrum_keyboard_keymap[40] = {
        JK_1, JK_2, JK_3, JK_4, JK_5,
        JK_0, JK_9, JK_8, JK_7, JK_6,
        JK_Q, JK_W, JK_E, JK_R, JK_T,
        JK_P, JK_O, JK_I, JK_U, JK_Y,
        JK_A, JK_S, JK_D, JK_F, JK_G,
        JK_ENTER, JK_L, JK_K, JK_J, JK_H,
        JK_CAPS, JK_Z, JK_X, JK_C, JK_V,
        JK_SPACE, JK_SHIFT, JK_M, JK_N, JK_B
};

void core::setup_keyboard()
{
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_KEYBOARD, true, true, true, false);

    d->id = 0;

    JSM_KEYBOARD* kbd = &d->keyboard;
    kbd->num_keys = 40;

    for (u32 i = 0; i < 40; i++) {
       kbd->key_defs[i] = static_cast<JKEYS>(ZXSpectrum_keyboard_keymap[i]);
       if (ZXSpectrum_keyboard_keymap[i] == JK_SPACE) {
           printf("\nSPAE SETUP AT %d", i);
       }
    }
    ula.keyboard_ptr.make(IOs, IOs.size()-1);
}

u8 core::CPU_readmem(u16 addr)
{
    if (addr < 0x4000) return bank.ROM[addr];
    return bank.RAM[(addr >> 14)][addr & 0x3FFF];
}

void core::CPU_writemem(u16 addr, u8 val)
{
    if (addr < 0x4000) return;
    bank.RAM[(addr >> 14)][addr & 0x3FFF] = (u8)val;
}


void core::fast_load()
{
    // Replaces ROM code at 056B with instant-load kind.
    // Everything should be the same at exit, but without
    // actually doing all the waiting.
    u32 A = (cpu.regs.AF_ >> 8) & 0xFF;
    u32 F = cpu.regs.AF_ & 0xFF;
    u32 actually_load = (F & 1) == 1; // 0 = VERIFY
    cpu.regs.H = 0;
    cpu.regs.L = static_cast<u8 *>(tape_deck.TAPE_binary.ptr)[tape_deck.head_pos];
    cpu.regs.H ^= cpu.regs.L;
    tape_deck.head_pos++;
    if (tape_deck.head_pos >= tape_deck.TAPE_binary.size) tape_deck.head_pos = 0;
    if ((A ^ cpu.regs.H) != 0) { // Early-return
        return;
    }
    u32 DE = (cpu.regs.D << 8) | cpu.regs.E;
    u32 IX = cpu.regs.IX;
    while(DE > 0) {
        cpu.regs.L = ((u8*)tape_deck.TAPE_binary.ptr)[tape_deck.head_pos];
        tape_deck.head_pos++;
        if (tape_deck.head_pos >= tape_deck.TAPE_binary.size) tape_deck.head_pos = 0;
        cpu.regs.H ^= cpu.regs.L;
        if (actually_load) {
            CPU_writemem(IX, cpu.regs.L);
        }
        DE = (DE - 1) & 0xFFFF;
        IX = (IX + 1) & 0xFFFF;
    }
    // Last byte is not loaded into RAM
    cpu.regs.L = static_cast<u8 *>(tape_deck.TAPE_binary.ptr)[tape_deck.head_pos];
    tape_deck.head_pos++;
    if (tape_deck.head_pos >= tape_deck.TAPE_binary.size) tape_deck.head_pos = 0;
    cpu.regs.H ^= cpu.regs.L;

    cpu.regs.D = cpu.regs.E = 0xFF;
    cpu.regs.IX = IX;
    cpu.regs.A = cpu.regs.H;
    cpu.regs.F.u = 0x93;
    if (cpu.regs.A == 0) {
        // No error!
    } else {
        // Error!
        cpu.regs.A = cpu.regs.H = 0xFF;
        cpu.regs.F.C = +(cpu.regs.A == 0);
        printf("\nFAST LOAD ERROR!");
    }

    printf("\nFast load finish!");
}

#define SR(reg, pos) cpu.regs. reg = infil[pos]
#define SR2(reg, pos) cpu.regs. reg = infil[pos] | (infil[pos] << 8)

void core::load_Z80_file(multi_file_set& mfs) {
    buf *b = &mfs.files[0].buf;
    u8 *infil = static_cast<u8 *>(b->ptr);

    SR(A, 0);
    cpu.regs.F.u = infil[1];
    SR(C, 2);
    SR(B, 3);
    SR(L, 4);
    SR(H, 5);
    SR2(PC, 6);
    SR2(SP, 8);
    SR(I, 10); // WAIT WHAT?
    cpu.regs.R = infil[10] & 0x7F | ((infil[11] & 1) << 7);
    ula.io.border_color = (infil[11] >> 1) & 7;
    if ((infil[11] >> 5) & 1) {
        printf("\nERROR COMPRESSED BLOCK");
        return;
    }
    SR(E, 13);
    SR(D, 14);
    SR2(BC_, 15);
    SR2(DE_, 17);
    SR2(HL_, 19);
    cpu.regs.AF_ = (infil[21] << 8) | infil[22];
    SR2(IY, 23);
    SR2(IX, 25);
    SR(EI, 27);
    SR(IFF2, 28);
    cpu.regs.IM = infil[29] & 3;
    printf("\nINTERRUPT MODE SHOULD BE 2: %d", cpu.regs.IM);
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

void core::load_SNA_file(multi_file_set& mfs)
{
    buf* b = &mfs.files[0].buf;
    u8 *infil = static_cast<u8 *>(b->ptr);

    cpu.regs.I = infil[0];
    cpu.regs.HL_ = infil[1] + (infil[2] << 8);
    cpu.regs.DE_ = infil[3] + (infil[4] << 8);
    cpu.regs.BC_ = infil[5] + (infil[6] << 8);
    cpu.regs.AF_ = infil[7] + (infil[8] << 8);
    cpu.regs.L = infil[9];
    cpu.regs.H = infil[0x0A];
    cpu.regs.E = infil[0x0B];
    cpu.regs.D = infil[0x0C];
    cpu.regs.C = infil[0x0D];
    cpu.regs.B = infil[0x0E];
    cpu.regs.IY = infil[0x0F] + (infil[0x10] << 8);
    cpu.regs.IX = infil[0x11] + (infil[0x12] << 8);
    cpu.regs.IFF2 = (infil[0x13] >> 2) & 1;
    cpu.regs.IFF1 = infil[0x13] & 1;
    cpu.regs.R = infil[0x14];
    cpu.regs.F.u = infil[0x15];
    cpu.regs.A = infil[0x16];
    cpu.regs.SP = infil[0x17] + (infil[0x18] << 8);
    cpu.regs.IM = infil[0x19];
    ula.io.border_color = infil[0x1A];

    memcpy(RAM, infil+0x1B, 48*1024);

    /* RETN */
    cpu.regs.Q = 0;
    //console.log('SP!', hex4(cpu.regs.SP));
    cpu.regs.WZ = CPU_readmem(cpu.regs.SP);
    cpu.regs.SP = (cpu.regs.SP + 1) & 0xFFFF;
    cpu.regs.WZ |= CPU_readmem(cpu.regs.SP) << 8;
    cpu.regs.SP = (cpu.regs.SP + 1) & 0xFFFF;
    cpu.regs.PC = cpu.regs.WZ;
    cpu.regs.IFF1 = cpu.regs.IFF2;
    //console.log('PC!', hex4(cpu.regs.PC));

    /* Initialize CPU */
    cpu.regs.TCU = 0;
    cpu.pins.Addr = cpu.regs.PC;
    cpu.pins.D = CPU_readmem(cpu.regs.PC);
    cpu.regs.PC = (cpu.regs.PC + 1) & 0xFFFF;
    cpu.regs.EI = 0;
    cpu.regs.P = 0;
    cpu.regs.prefix = 0;
    cpu.regs.rprefix = Z80::regs::P_HL;
    cpu.regs.IR = Z80::S_DECODE;
    cpu.regs.poll_IRQ = true;
}

void ZXSpectrumIO_insert_tape(jsm_system *ptr, physical_io_device &pio, multi_file_set &mfs, buf* sram) {
    auto *th = dynamic_cast<ZXSpectrum::core *>(ptr);
    char *s = mfs.files[0].name;
    if (ends_with(s, ".pzx")) {
        th->tape_deck.load_pzx(mfs);
    }
    if (ends_with(s, ".tap")) {
        th->tape_deck.load(mfs);
    }
    else if (ends_with(s, ".sna")) {
        th->load_SNA_file(mfs);
    }
    else if (ends_with(s, ".z80")) {
        th->load_Z80_file(mfs);
    }
    else {
        printf("\nCould not detect file type by extension: %s", s);
    }
}

static void ZXSpectrumIO_play(jsm_system *sys)
{
    auto *th = dynamic_cast<ZXSpectrum::core *>(sys);
    th->tape_deck.play();
}


static void ZXSpectrumIO_stop(jsm_system *sys)
{
    auto *th = dynamic_cast<ZXSpectrum::core *>(sys);
    th->tape_deck.stop();
}

static void ZXSpectrumIO_rewind(jsm_system *sys)
{
    auto *th = dynamic_cast<ZXSpectrum::core *>(sys);
    th->tape_deck.rewind();
}

static void ZXSpectrumIO_remove_tape(jsm_system *sys)
{
    auto *th = dynamic_cast<ZXSpectrum::core *>(sys);
    th->tape_deck.remove();
}

static void setup_crt48(JSM_DISPLAY *d)
{
    d->kind = jsm::CRT;
    d->enabled = true;

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

void setup_crt128(JSM_DISPLAY *d)
{
    d->kind = jsm::CRT;

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

 void core::describe_io()
{
    if (described_inputs) return;
    described_inputs = true;
    IOs.reserve(15);

    // controllers
    setup_keyboard();

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
    HID_digital_button* b;
    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // tape deck
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_AUDIO_CASSETTE, true, true, true, true);
    d->audio_cassette.insert_tape = &ZXSpectrumIO_insert_tape;
    d->audio_cassette.remove_tape = &ZXSpectrumIO_remove_tape;
    d->audio_cassette.rewind = &ZXSpectrumIO_rewind;
    d->audio_cassette.play = &ZXSpectrumIO_play;
    d->audio_cassette.stop = &ZXSpectrumIO_stop;
    tape_deck.tape_deck_index = 2;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, true, true, false, true);
    d->display.output[0] = malloc(352 * 304);
    d->display.output[1] = malloc(352 * 304);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    switch(variant) {
        case spectrum48:
            setup_crt48(&d->display);
            break;
        case spectrum128:
            setup_crt128(&d->display);
            break;
        default:
            assert(1==0);
    }
    ula.cur_output = static_cast<u8 *>(d->display.output[0]);
    d->display.last_written = 1;
    ula.display_ptr.make(IOs, IOs.size() - 1);

    ula.display = &ula.display_ptr.get().display;
}

void core::enable_tracing()
{
    // TODO
    assert(1==0);
}

void core::disable_tracing()
{
    // TODO
    assert(1==0);
}

void core::play()
{
}

void core::pause()
{
}

void core::stop()
{
}

void core::get_framevars(framevars& out)
{
    out.master_frame = clock.frames_since_restart;
    out.x = ula.screen_x;
    out.scanline = ula.screen_y;
}

void core::reset() {
    cpu.reset();
    ula.reset();
    // RAM is weird.
    // 48k has contiguous 48k
    // 128k and 2+ have...
    // 0x4000 = bank 5
    // 0x8000 = bank 2
    // 0xc000 = bank 0 at startup
    // upper RAM address, shift right by 15, gets area # (0x4000, etc.)
    // then it's 0x4000 * that number

    // ROM is bank 0 or 1 of 16k
    bank.RAM[1] = RAM + (5 * 0x4000);
    bank.RAM[2] = RAM + (2 * 0x4000);
    bank.RAM[3] = RAM;
    bank.ROM = ROM;
    bank.display = RAM + (5 * 0x4000);
}


u32 core::finish_frame()
{
    u32 current_frame = clock.frames_since_restart;
    u32 scanlines = 0;
    while(current_frame == clock.frames_since_restart) {
        scanlines++;
        finish_scanline();
        if (dbg.do_break) break;
    }
    return ula.display->last_written;
}

u32 core::finish_scanline()
{
    u32 current_y = clock.ula_y;

    while(current_y == clock.ula_y) {
        CPU_cycle();
        ula.cycle();
        ula.cycle();
        clock.master_cycles += 2;
        if (dbg.do_break) break;
    }
    return 0;
}

void core::CPU_cycle()
{
    //if (clock.contended && ((cpu.pins.Addr - 0x4000) < 0x4000)) return;
    cpu.cycle();
    if (cpu.pins.RD) {
        if (cpu.pins.MRQ) {// read ROM/RAM
            cpu.pins.D = CPU_readmem(cpu.pins.Addr);

            if ((cpu.pins.Addr == 0x056B) && (cpu.regs.PC == 0x056C)) { // Fast tape load hack time!
                // QuickLOAD only for binary. We need to translate to binary in future...
                if (tape_deck.kind == tdk_binary) {
                    printf("\nquick LOAD trigger");
                    // return RET
                    cpu.pins.D = 0xC9;
                    // do quickload
                    fast_load();
                }
            }

            printif(z80.mem, DBGC_Z80 "\nZXS(%06llu)r   %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
        } else if (cpu.pins.IO && (cpu.pins.M1 == 0)) { // read IO port
            cpu.pins.D = ula.reg_read(cpu.pins.Addr);
            printif(z80.io, DBGC_Z80"\nZXS(%06llu)in  %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
        }
    }
    if (cpu.pins.WR) {
        if (cpu.pins.MRQ) {// write ROM/RAM
            if (dbg.trace_on && (cpu.trace.last_cycle != *cpu.trace.cycles)) {
                cpu.trace.last_cycle = *cpu.trace.cycles;
                printif(z80.mem, DBGC_Z80 "\nZ80(%06llu)wr  %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
            }
            CPU_writemem(cpu.pins.Addr, cpu.pins.D);
        }
        else if (cpu.pins.IO) {// write IO
            ula.reg_write(cpu.pins.Addr, cpu.pins.D);
            printif(z80.io, DBGC_Z80 "\nZ80(%06llu)out %04x   $%02x         TCU:%d" DBGC_RST, *cpu.trace.cycles, cpu.pins.Addr, cpu.pins.D, cpu.regs.TCU);
        }
    }
}

u32 core::step_master(u32 howmany)
{
    i32 todo = (howmany >> 1);
    if (todo == 0) todo = 1;
    for (i32 i = 0; i < todo; i++) {
        CPU_cycle();;
        ula.cycle();
        ula.cycle();;
        clock.master_cycles += 2;
        if (dbg.do_break) break;
    }
    return 0;
}

void core::load_BIOS(multi_file_set& mfs)
{
    buf* b = &mfs.files[0].buf;
    memcpy(ROM, b->ptr, 16384 <= b->size ? 16384 : b->size);
}

}