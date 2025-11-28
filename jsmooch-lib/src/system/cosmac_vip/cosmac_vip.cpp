//
// Created by . on 11/20/25.
//

#include "cosmac_vip.h"
#include "vip_bus.h"

#define TAG_SCANLINE 1
#define TAG_FRAME 2

#define JSM jsm_system* jsm

u32 read_trace_cdp1802(void *ptr, u32 addr) {
    return static_cast<VIP::core *>(ptr)->read_main_bus(addr, 0, 0);
}

jsm_system *VIP_new(jsm::systems in_kind)
{
    auto* th = new VIP::core(in_kind);

    th->scheduler.max_block_size = 8;

    snprintf(th->label, sizeof(th->label), "Cosmac VIP %dk", in_kind == jsm::systems::COSMAC_VIP_2k ? 2 : 4);

    jsm_debug_read_trace dt;
    dt.read_trace = &read_trace_cdp1802;
    dt.ptr = static_cast<void *>(th);
    th->cpu.setup_tracing(&dt, &th->clock.master_cycle_count);
    return th;
}

void VIP_delete(JSM) {
    auto *th = dynamic_cast<VIP::core *>(jsm);

    for (physical_io_device &pio : th->IOs) {
    }
    th->IOs.clear();

    delete th;
}

namespace VIP {
static void setup_debug_waveform(const core *th, debug_waveform &dw)
{
    if (dw.samples_requested == 0) return;
    dw.samples_rendered = dw.samples_requested;
    dw.user.cycle_stride = (static_cast<float>(th->clock.master_cycle_count) / static_cast<float>(dw.samples_requested));
    dw.user.buf_pos = 0;
}

void core::set_audiobuf(audiobuf *ab)
{
    audio.buf = ab;
    if (audio.master_cycles_per_audio_sample == 0) {
        audio.master_cycles_per_audio_sample = (static_cast<float>(clock.master_cycle_count) / static_cast<float>(ab->samples_len));
        audio.next_sample_cycle_max = 0;
        debug_waveform *wf = &dbg.waveforms.main.get();
        audio.master_cycles_per_max_sample = static_cast<float>(clock.master_cycle_count) / static_cast<float>(wf->samples_requested);
    }

    debug_waveform *wf = &dbg.waveforms.main.get();
    setup_debug_waveform(this, *wf);
}

static inline float i16_to_float(i16 val)
{
    return ((static_cast<float>(static_cast<i32>(val) + 32768) / 65535.0f) * 2.0f) - 1.0f;
}


static void sample_audio(void *ptr, u64 key, u64 clock, u32 jitter)
{
    core* th = static_cast<core *>(ptr);
    if (th->audio.buf) {
        th->audio.cycles++;
        if (th->audio.buf->upos < (th->audio.buf->samples_len << 1)) {
            static_cast<float *>(th->audio.buf->ptr)[th->audio.buf->upos] = i16_to_float(static_cast<i16>(0));
        }
        th->audio.buf->upos++;
    }
    th->audio.next_sample_cycle += th->audio.master_cycles_per_audio_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle), 0, th, &sample_audio, nullptr);
}

static void do_cycle(void *ptr, u64 key, u64 clock, u32 jitter) {
    core *th = static_cast<core *>(ptr);
    th->do_cycle();
    clock -= jitter;
    //printf("\nCYCLE AT %lld", cur);
    th->scheduler.only_add_abs(clock+8, 0, th, &do_cycle, nullptr);;
    //if (clock > 29456) printf("\nGOT PAST 29456!");
}

static void sample_audio_debug_max(void *ptr, u64 key, u64 clock, u32 jitter)
{
    core *th = static_cast<core *>(ptr);

    debug_waveform *dw = &th->dbg.waveforms.main.get();
    if (dw->user.buf_pos < dw->samples_requested) {
        const i32 smp = 0;
        static_cast<float *>(dw->buf.ptr)[dw->user.buf_pos] = i16_to_float(static_cast<i16>(smp));
        dw->user.buf_pos++;
    }

    th->audio.next_sample_cycle_max += th->audio.master_cycles_per_max_sample;
    th->scheduler.only_add_abs(static_cast<i64>(th->audio.next_sample_cycle_max), 0, th, &sample_audio_debug_max, nullptr);
}

static void new_scanline (void *ptr, u64 key, u64 cur, u32 jitter) {
}

static void schedule_frame(void *ptr, u64 key, u64 cur, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    cur -= jitter;
    printf("\nSCHEDULING FRAME!");
    for (u32 i = 0; i < 262; i++) {
        cur += 14*8;
        th->scheduler.only_add_abs_w_tag(static_cast<i64>(cur), 0, th, &new_scanline, nullptr, TAG_SCANLINE);
    }
    cur += 14*8;
    printf("\nFRAMESCHED AT %lld", cur);
    th->scheduler.only_add_abs_w_tag(static_cast<i64>(cur), 0, th, &schedule_frame, nullptr, TAG_FRAME);
}

void core::schedule_first()
{
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle_max), 0, this, &sample_audio_debug_max, nullptr);
    scheduler.only_add_abs(static_cast<i64>(audio.next_sample_cycle), 0, this, &sample_audio, nullptr);
    scheduler.only_add_abs(0, 0, this, &VIP::do_cycle, nullptr);;

    schedule_frame(this, 0, 0, 0);
}

static void setup_crt(JSM_DISPLAY *d)
{
    d->kind = jsm::display_kinds::CRT;
    d->enabled = 1;

    d->fps = 60.0;
    d->fps_override_hint = 60;

    d->pixelometry.cols.left_hblank = 0;
    d->pixelometry.cols.visible = 112;
    d->pixelometry.cols.max_visible = 112;
    d->pixelometry.cols.right_hblank = 0;
    d->pixelometry.offset.x = 0;

    d->pixelometry.rows.top_vblank = 0;
    d->pixelometry.rows.visible = 262;
    d->pixelometry.rows.max_visible = 262;
    d->pixelometry.rows.bottom_vblank = 0;
    d->pixelometry.offset.y = 0;

    d->geometry.physical_aspect_ratio.width = 4;
    d->geometry.physical_aspect_ratio.height = 3;

    d->pixelometry.overscan.left = d->pixelometry.overscan.right = 0;
    d->pixelometry.overscan.top = d->pixelometry.overscan.bottom = 0;
}

static void setup_audio(std::vector<physical_io_device> &inIOs)
{
    physical_io_device &pio = inIOs.emplace_back();
    pio.kind = HID_AUDIO_CHANNEL;
    JSM_AUDIO_CHANNEL *chan = &pio.audio_channel;
    chan->num = 1;
    chan->sample_rate = 1760640;
    //chan->sample_rate = 64000;
    chan->low_pass_filter = 16000;
}

void core::describe_io()
{
    if (jsm.described_inputs) return;
    jsm.described_inputs = 1;

    IOs.reserve(15);

    // controllers
    physical_io_device &kp = IOs.emplace_back();
    kp.init(HID_HEX_KEYPAD, 1, 1, 1, 0);
    hex_keypad.pio_ptr.make(IOs, IOs.size()-1);

    // power and reset buttons
    physical_io_device* chassis = &IOs.emplace_back();
    chassis->init(HID_CHASSIS, 1, 1, 1, 1);

    HID_digital_button* b;
    b = &chassis->chassis.digital_buttons.emplace_back();
    snprintf(b->name, sizeof(b->name), "Power");
    b->state = 1;
    b->common_id = DBCID_ch_power;

    b = &chassis->chassis.digital_buttons.emplace_back();
    b->common_id = DBCID_ch_reset;
    snprintf(b->name, sizeof(b->name), "Reset");
    b->state = 0;

    // screen
    physical_io_device *d = &IOs.emplace_back();
    d->init(HID_DISPLAY, 1, 1, 0, 1);
    d->display.output[0] = malloc(112 * 262);
    d->display.output[1] = malloc(112 * 262);
    d->display.output_debug_metadata[0] = nullptr;
    d->display.output_debug_metadata[1] = nullptr;
    setup_crt(&d->display);
    pixie.display_ptr.make(IOs, IOs.size()-1);
    d->display.last_written = 1;
    pixie.cur_output = static_cast<u8 *>(d->display.output[0]);

    setup_audio(IOs);

    pixie.display = &pixie.display_ptr.get().display;
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

    out.master_frame = pixie.master_frame;
    out.x = static_cast<i32>(pixie.x);
    out.scanline = pixie.y;
    out.master_cycle = clock.master_cycle_count;
}

//#define DO_STATS

void core::update_hex_keypad() {
    auto& kp = hex_keypad.pio_ptr.get().hex_keypad;
    for (u32 i = 0; i < 16; i++) {
        hex_keypad.keys[i] = kp.key_states[i];
    }
}


u32 core::finish_frame()
{
    update_hex_keypad();

#ifdef DO_STATS
    u64 spc_start = apu.cpu.int_clock;
    u64 wdc_start = r5a22.cpu.int_clock;
#endif
    //scheduler.run_til_tag_tg16(TAG_FRAME);
    u64 start_frame = pixie.master_frame;
    while (pixie.master_frame == start_frame) {
        finish_scanline();
        if (::dbg.do_break) break;
    }

#ifdef DO_STATS
    u64 spc_num_cycles = (apu.cpu.int_clock - spc_start) * 60;
    u64 wdc_num_cycles = (r5a22.cpu.int_clock - wdc_start) * 60;
    double spc_div = (double)clock.master_cycle_count / (double)spc_num_cycles;
    double wdc_div = (double)clock.master_cycle_count / (double)wdc_num_cycles;
    printf("\nSCANLINE:%d FRAME:%lld", clock.ppu.y, clock.master_frame);
    printf("\n\nEFFECTIVE 65816 FREQ IS %lld. RUNNING AT SPEED",wdc_num_cycles);
    printf("\nEFFECTIVE SPC700 FREQ IS %lld. RUNNING AT SPEED", spc_num_cycles);

#endif
    return pixie.display->last_written;
}

u32 core::finish_scanline()
{
    //read_opts(jsm, th);
    i32 start_y = pixie.y;
    while (pixie.y == start_y) {
        do_cycle();
        this->clock.master_cycle_count += 8;
        if (::dbg.do_break) break;
    }

    return pixie.display->last_written;
}

u32 core::step_master(u32 howmany)
{
    //read_opts(jsm, th);
    //printf("\nRUN FOR %d CYCLES:", howmany);
    //u64 cur = clock.master_cycle_count;
    //scheduler.run_for_cycles_tg16(howmany);
    //u64 dif = clock.master_cycle_count - cur;
    //printf("\nRAN %lld CYCLES", dif);
    cycles_deficit += howmany;
    while (cycles_deficit > 0) {
        do_cycle();
        this->clock.master_cycle_count += 8;
        cycles_deficit -= 8;
        if (::dbg.do_break) break;
    }
    return 0;
}

};