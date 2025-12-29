//
// Created by . on 12/4/24.
//
#include <cassert>
#include <cstdlib>

#include "nds.h"
#include "nds_bus.h"
#include "nds_debugger.h"
#include "nds_rtc.h"
#include "system/nds/3d/nds_ge.h"
#include "nds_regs.h"
#include "nds_apu.h"

#include "component/cpu/arm7tdmi/arm7tdmi.h"

#include "helpers/multisize_memaccess.cpp"

jsm_system *NDS_new()
{
    return new NDS::core();
}

void NDS_delete(jsm_system *jsm)
{
    delete jsm;
}

namespace NDS {
static u32 read_trace_cpu9(void *ptr, u32 addr, u8 sz) {
    return core::mainbus_read9(ptr, addr, sz, 0, false);
}

static u32 read_trace_cpu7(void *ptr, u32 addr, u8 sz) {
    return core::mainbus_read7(ptr, addr, sz, 0, false);
}

core::core() :
    clock{&waitstates.current_transaction},
    scheduler{&clock.master_cycle_count7},
    arm7{&clock.master_cycle_count7, &waitstates.current_transaction, &scheduler}, arm9{&scheduler, &clock.master_cycle_count9, &waitstates.current_transaction}, ppu{this}, ge{this, &scheduler},
    re{this}, apu{this, &scheduler},
    cart{this}
{
    has.load_BIOS = true;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = false;
    has.set_audiobuf = true;
    re.ge = &ge;
    ge.re = &re;
    for (u32 i = 0; i < 16; i++) {
        mem.rw[0].read[i] = &core::busrd7_invalid;
        mem.rw[0].write[i] = &core::buswr7_invalid;
        mem.rw[1].read[i] = &core::busrd9_invalid;
        mem.rw[1].write[i] = &core::buswr9_invalid;
    }
    memset(dbg_info.mgba.str, 0, sizeof(dbg_info.mgba.str));
#define BND9(page, func) { mem.rw[1].read[page] = &core::busrd9_##func; mem.rw[1].write[page] = &core::buswr9_##func; }
    BND9(0x2, main);
    BND9(0x3, shared);
    BND9(0x4, io);
    BND9(0x5, obj_and_palette);
    BND9(0x6, vram);
    BND9(0x7, oam);
    BND9(0x8, gba_cart);
    BND9(0x9, gba_cart);
    BND9(0xA, gba_sram);
#undef BND9

#define BND7(page, func) { mem.rw[0].read[page] = &core::busrd7_##func; mem.rw[0].write[page] = &core::buswr7_##func; }
    BND7(0x0, bios7);
    BND7(0x2, main);
    BND7(0x3, shared);
    BND7(0x4, io);
    BND7(0x6, vram);
    BND7(0x8, gba_cart);
    BND7(0x9, gba_cart);
    BND7(0xA, gba_sram);
#undef BND7

    scheduler.max_block_size = 25;

    scheduler.run.func = &run_block;
    scheduler.run.ptr = this;

    DMA_init();
    //ARM7TDMI_init(&arm7, &clock.master_cycle_count7, &waitstates.current_transaction, nullptr);
    arm7.read_data_ptr = this;
    arm7.write_data_ptr = this;
    arm7.read_data = &mainbus_read7;
    arm7.write_data = &mainbus_write7;
    arm7.read_ins_ptr = this;
    arm7.read_ins = &mainbus_fetchins7;

    //ARM946ES_init(&arm9, &clock.master_cycle_count9, &waitstates.current_transaction, nullptr);
    arm9.read_ptr = this;
    arm9.read_func = &mainbus_read9;

    arm9.write_ptr = this;
    arm9.write_func = &mainbus_write9;

    arm9.fetch_ptr = this;
    arm9.fetch_ins_func = &mainbus_fetchins9;

    snprintf(label, sizeof(label), "Nintendo DS");
    jsm_debug_read_trace dt7;
    dt7.read_trace_arm = &read_trace_cpu7;
    dt7.ptr = this;
    arm7.setup_tracing(&dt7, &clock.master_cycle_count7, 1);

    jsm_debug_read_trace dt9;
    dt9.read_trace_arm = &read_trace_cpu9;
    dt9.ptr = this;
    arm9.setup_tracing(&dt9, &clock.master_cycle_count9, 2);

    jsm.described_inputs = false;
    jsm.cycles_left = 0;

}


static void setup_debug_waveform(debug_waveform *dw)
{
    if (dw->samples_requested == 0) return;
    dw->samples_rendered = dw->samples_requested;
    dw->user.cycle_stride = (static_cast<float>(33000000 / 60) / static_cast<float>(dw->samples_requested));
    dw->user.buf_pos = 0;
}

void core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    /*if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = ((float)(MASTER_CYCLES_PER_FRAME / (float)ab->samples_len));
        audio.next_sample_cycle = 0;
        debug_waveform *wf = (debug_waveform *)cvec_get(dbg.waveforms.main.vec, dbg.waveforms.main.index);
        apu.ext_enable = wf->ch_output_enabled;
    }

    // PSG
    setup_debug_waveform(cvec_get(dbg.waveforms.main.vec, dbg.waveforms.main.index));
    for (u32 i = 0; i < 6; i++) {
        setup_debug_waveform(cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index));
        debug_waveform *wf = (debug_waveform *)cvec_get(dbg.waveforms.chan[i].vec, dbg.waveforms.chan[i].index);
        if (i < 4)
            apu.channels[i].ext_enable = wf->ch_output_enabled;
        else
            apu.fifo[i - 4].ext_enable = wf->ch_output_enabled;
    }*/

}

void core::do_next_scheduled_frame(void *bound_ptr, u64 key, u64 current_clock, u32 jitter)
{
    auto *th = static_cast<core *>(bound_ptr);
    th->schedule_frame(current_clock-jitter, false);
}

void core::schedule_frame(u64 start_clock, bool is_first)
{
    // Schedule out a frame!
    i64 cur_clock = start_clock;
    clock.cycles_left_this_frame += clock.timing.frame.cycles;

    //clock.cycles_left_this_frame += clock.timing.frame.cycles;

    // x lines of end hblank, start hblank
    // somewhere in there, start vblank
    for (u32 line = 0; line < 263; line++) {
        // vblank start
        if (line == clock.timing.frame.vblank_up_on) {
            scheduler.only_add_abs(cur_clock, 1, this, &PPU::core::vblank, nullptr);
        }
        // vblank end
        if (line == clock.timing.frame.vblank_down_on) {
            scheduler.only_add_abs(cur_clock, 0, this, &PPU::core::vblank, nullptr);
        }

        // hblank down...
        scheduler.only_add_abs(cur_clock, 0, this, &PPU::core::hblank, nullptr);
        // hblank up...
        scheduler.only_add_abs(cur_clock+clock.timing.scanline.cycle_of_hblank, 1, this, PPU::core::hblank, nullptr);

        // Advance clock
        cur_clock += clock.timing.scanline.cycles_total;
    }

    //printf("\nSCHEDULE NEW FRAME SET FOR CYCLE %lld", start_clock+clock.timing.frame.cycles);
    scheduler.only_add_abs_w_tag(start_clock+clock.timing.frame.cycles, 0, this, &do_next_scheduled_frame, nullptr, 1);
    if (is_first) scheduler.only_add_abs(static_cast<i64>(apu.next_sample), 0, &apu, &APU::core::master_sample_callback, nullptr);
}

void core::run_block(void *ptr, u64 num_cycles, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);

    th->waitstates.current_transaction = 0;
    th->clock.cycles7 += static_cast<i64>(num_cycles);
    th->clock.cycles9 += static_cast<i64>(num_cycles);


    if (th->io.arm7.halted) {
        th->io.arm7.halted &= ((!!(th->io.arm7.IF & th->io.arm7.IE)) ^ 1);
    }

    th->arm7_ins = true;
    while (static_cast<i64>(th->clock.master_cycle_count7) < th->clock.cycles7) {
        th->arm7.run_noIRQcheck();
        th->clock.master_cycle_count7 += th->waitstates.current_transaction;
        th->waitstates.current_transaction = 0;
        if (th->io.arm7.halted) {
            th->clock.master_cycle_count7 = th->clock.cycles7;
            break;
        }
    }
    th->arm7_ins = false;

    if (th->ge.fifo.pausing_cpu || th->arm9.halted) {
        th->clock.master_cycle_count9 = th->clock.cycles9;
    }
    else {
        th->arm9_ins = true;
        while(static_cast<i64>(th->clock.master_cycle_count9) < th->clock.cycles9) {
            th->arm9.run_noIRQcheck();
            th->clock.master_cycle_count9 += th->waitstates.current_transaction;
            th->waitstates.current_transaction = 0;
        }
        th->arm9_ins = false;
    }

    // TODO: let this be scheduled.
    // TODO Next: yes do it!
    //ARM7TDMI_IRQcheck(&arm7, 0);
    //ARM946ES_IRQcheck(&arm9, 0);
}

void core::sample_audio()
{
    // Take NDS samples into our buffer
    auto *outptr = static_cast<float *>(audio.buf->ptr);
    for (u32 i = 0; i < audio.buf->samples_len; i++) {
        if (apu.buffer.len < 1) {
            printf("\nUNDERRUN %d SAMPLES!", audio.buf->samples_len - i);
            return;
        }

        i32 smp = apu.buffer.samples2[apu.buffer.head];
        apu.buffer.head = (apu.buffer.head + 1) & (NDS_APU_MAX_SAMPLES - 1);
        // Current range is -512 to 511, 10 bits?
        float l = ((static_cast<float>(smp) + 512.0f) / 511.5f) - 1.0f;
        assert(l>=-1.0f && l<=1.0f);

        smp = apu.buffer.samples2[apu.buffer.head];
        apu.buffer.head = (apu.buffer.head + 1) & (NDS_APU_MAX_SAMPLES - 1);
        float r = ((static_cast<float>(smp) + 512.0f) / 511.5f) - 1.0f;

        *outptr = l;
        outptr++;
        *outptr = r;
        outptr++;
        apu.buffer.len--;
    }
}


u32 core::finish_frame()
{
    u64 total = 0;
    scheduler.run_til_tag(1);
    sample_audio();
    return 0;
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
    
    out.master_frame = clock.master_frame;
    out.x = clock.ppu.x;
    out.scanline = clock.ppu.y;
    out.master_cycle = clock.master_cycle_count7;
}

void core::skip_BIOS()
{
    // Load 170h bytes of header into main RAM starting at 27FFE00
    u32 *hdr_start = static_cast<u32 *>(cart.ROM.ptr);
    u32 *hdr = hdr_start;

    for (i32 i = 0; i < 0x170; i += 4) {
        mainbus_write9(this, 0x027FFE00+i, 4, 0, hdr[i>>2]);
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
        mainbus_write7(this, bin7_lo+i, 4, 0, cR[4](cart.ROM.ptr, bin7_offset+i));
    }
    for (u32 i = 0; i < bin9_size; i += 4) {
        mainbus_write9(this, bin9_lo+i, 4, 0, cR[4](cart.ROM.ptr, bin9_offset+i));
    }

    arm9.regs.R[14] = 0x03002F7C;
    arm9.regs.R_irq[0] = 0x03003F80;
    arm9.regs.R_svc[0] = 0x03003FC0;
    arm9.regs.R[15] = arm9_entry;
    arm9.regs.CPSR.mode = ARM946ES::modes::M_system;
    arm9.fill_regmap();
    arm9.NDS_direct_boot();
    arm9.reload_pipeline();

    arm7.regs.R[13] = 0x0380FD80;
    arm7.regs.R_irq[0] = 0x0380FF80;
    arm7.regs.R_svc[0] = 0x0380FFC0;
    arm7.regs.R[15] = arm7_entry;
    arm7.regs.CPSR.mode = ARM7TDMI::modes::M_system;
    arm7.fill_regmap();
    arm7.reload_pipeline();

    mainbus_write9(this, R9_WRAMCNT, 1, 0, 3); // setup WRAM


    mainbus_write9(this, 0x027FF800, 4, 0, 0x1FC2); // chip id
    mainbus_write9(this, 0x027FF804, 4, 0, 0x1FC2); // chip id
    mainbus_write9(this, 0x027FF850u, 2, 0, 0x5835); // ARM7 BIOS CRC
    mainbus_write9(this, 0x027FF880u, 2, 0, 0x0007); // Message from ARM9 to ARM7
    mainbus_write9(this, 0x027FF884u, 2, 0, 0x0006); // ARM7 boot task
    mainbus_write9(this, 0x027FFC00u, 4, 0, 0x1FC2); // Copy of chip ID 1
    mainbus_write9(this, 0x027FFC04u, 4, 0, 0x1FC2); // Copy of chip ID 2
    mainbus_write9(this, 0x027FFC10u, 4, 0, 0x5835); // Copy of ARM7 BIOS CRC
    mainbus_write9(this, 0x027FFC40u, 4, 0, 0x0001); // Boot indicator
    // Now copy 112 bytes from firmware 0x03FE00  to RAM 0x027FFC80
    for (u32 i = 0; i < 112; i++) {
        mainbus_write9(this, 0x027FFC80 + i, 1, 0, mem.firmware[0x03FE00+i]);
    }
    // Now write to POSTFLG registers...
    mainbus_write9(this, 0x04000300, 1, 0, 1);
    mainbus_write7(this, 0x04000300, 1, 0, 1);

    // Do some more boot stuf...
    mainbus_write9(this, R9_POWCNT1, 2, 0, 0x0203);

    cart.direct_boot();
    printf("\ndirect boot done!");
 }




void core::reset()
{

    // Emu resets...
    scheduler.clear();
    clock.master_cycle_count7 = 0;
    waitstates.current_transaction = 0;

    arm7.reset();
    arm9.reset();
    arm9.NDS_CP_reset();
    RTC_reset();
    spi.irq_id = 0;

    for (u32 i = 0; i < 4; i++) {
        timer7_t *t = &timer7[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
        timer9_t *p = &timer9[i];
        p->overflow_at = 0xFFFFFFFFFFFFFFFF;
        p->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
    //clock.reset();
    SPI_reset();
    ppu.reset();
    mainbus_write9(this, R9_VRAMCNT+0, 1, 0, 0);
    mainbus_write9(this, R9_VRAMCNT+1, 1, 0, 0);
    mainbus_write9(this, R9_VRAMCNT+2, 1, 0, 0);
    mainbus_write9(this, R9_VRAMCNT+3, 1, 0, 0);
    mainbus_write9(this, R9_VRAMCNT+4, 1, 0, 0);
    mainbus_write9(this, R9_VRAMCNT+6, 1, 0, 0);
    mainbus_write9(this, R9_WRAMCNT, 1, 0, 3); // at R9_VRAMCNT+7
    mainbus_write9(this, R9_VRAMCNT+8, 1, 0, 0);
    mainbus_write9(this, R9_VRAMCNT+9, 1, 0, 0);

    io.arm7.IME = io.arm7.IE = io.arm7.IF = 0;
    io.arm9.IME = io.arm9.IE = io.arm9.IF = 0;
    io.arm7.POSTFLG = io.arm9.POSTFLG = 0;
    // TODO: Reset DMA and timers...
    // IPC, SPi, APU

    // Components such as RTC...
    ge.reset();
    re.reset();

    skip_BIOS();
    waitstates.current_transaction = 0;
    clock.master_cycle_count7 = 0;
    clock.master_cycle_count9 = 0;

    scheduler.clear();
    clock.cycles_left_this_frame = 0;
    schedule_frame(0, true); // Schedule first frame
    printf("\nNDS reset complete!");
}
#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

u32 core::finish_scanline()
{
    

    u64 scanline_cycle = clock.current7() - clock.ppu.scanline_start;
    assert(scanline_cycle < clock.timing.scanline.cycles_total);

    u64 old_clock = clock.current7();


    core::step_master(clock.timing.scanline.cycles_total - scanline_cycle);
    u64 diff = clock.current7() - old_clock;
    clock.cycles_left_this_frame -= diff;
    return ppu.display->last_written;
}

u32 core::step_master(u32 howmany)
{
    
    scheduler.run_for_cycles(howmany);
    return ppu.display->last_written;
}

void core::load_BIOS(multi_file_set& mfs)
{
    
    // 7, 9, firmware
    memcpy(mem.bios7, mfs.files[0].buf.ptr, 16384);
    memcpy(mem.bios9, mfs.files[1].buf.ptr, 4096);
    memcpy(mem.firmware, mfs.files[2].buf.ptr, 256 * 1024);
}

static void NDSIO_unload_cart(jsm_system *ptr)
{
}

static void NDSIO_load_cart(jsm_system *ptr, multi_file_set &mfs, physical_io_device &pio) {
    
    buf* b = &mfs.files[0].buf;
    auto *th = dynamic_cast<core *>(ptr);

    u32 r;
    th->cart.load_ROM_from_RAM(static_cast<char *>(b->ptr), b->size, &pio, &r);
    //core::reset(jsm);
}

void core::setup_lcd(JSM_DISPLAY &d)
{
    d.kind = jsm::LCD;
    d.enabled = true;

    d.fps = 59.8261;
    d.fps_override_hint = 60;
    // 256x192, but 355x263 with v and h blanks

    d.pixelometry.cols.left_hblank = 0;
    d.pixelometry.cols.visible = 256;
    d.pixelometry.cols.max_visible = 256;
    d.pixelometry.cols.right_hblank = 99;
    d.pixelometry.offset.x = 0;

    d.pixelometry.rows.top_vblank = 0;
    d.pixelometry.rows.visible = 384;
    d.pixelometry.rows.max_visible = 384;
    d.pixelometry.rows.bottom_vblank = 71;
    d.pixelometry.offset.y = 0;

    d.geometry.physical_aspect_ratio.width = 4;
    d.geometry.physical_aspect_ratio.height = 6;

    d.pixelometry.overscan.left = d.pixelometry.overscan.right = 0;
    d.pixelometry.overscan.top = d.pixelometry.overscan.bottom = 0;
}

void core::setup_audio()
{
    physical_io_device *pio = &IOs.emplace_back();
    pio->kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio->audio_channel;
    chan->num = 2;
    chan->sample_rate = 32760; // uhhhh....yeah...lol
    chan->low_pass_filter = 16380;
}


void core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = true;

    IOs.reserve(15);

    // controllers
    physical_io_device *cntroller = &IOs.emplace_back();
    controller.setup_pio(cntroller);

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, true, true, true, true);
    HID_digital_button* b;

    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    // cartridge port
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_CART_PORT, true, true, true, true);
    d->cartridge_port.load_cart = &NDSIO_load_cart;
    d->cartridge_port.unload_cart = &NDSIO_unload_cart;

    // screen
    d = &IOs.emplace_back();
    d->init(HID_DISPLAY, true, true, false, true);
    d->display.output[0] = malloc(256 * 384 * 4);
    d->display.output[1] = malloc(256 * 384 * 4);
    memset(d->display.output[0], 0, 256*384*4);
    memset(d->display.output[1], 0, 256*384*4);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_lcd(d->display);
    ppu.display_ptr.make(IOs, IOs.size()-1);
    d->display.last_written = 1;
    ppu.cur_output = static_cast<u32 *>(d->display.output[0]);

    physical_io_device *t = &IOs.emplace_back();
    t->init(HID_TOUCHSCREEN, true, true, true, false);
    t->touchscreen.params.width = 256;
    t->touchscreen.params.height = 192;
    t->touchscreen.params.x_offset = 0;
    t->touchscreen.params.y_offset = -192;
    spi.touchscr.pio = t;

    setup_audio();

    ppu.display = &ppu.display_ptr.get().display;
}
}