//
// Created by . on 11/23/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/sys_interface.h"
#include "component/cpu/z80/z80.h"
#include "helpers/physical_io.h"

#include "component/controller/sms/sms_gamepad.h"
#include "sms_gg_clock.h"
#include "sms_gg_vdp.h"
#include "component/audio/sn76489/sn76489.h"
#include "sms_gg_mapper_sega.h"
#include "sms_gg_io.h"

#define SMSGG_DISPLAY_DRAW_SZ (256 * 240 * 2)
namespace SMSGG {
enum SS_kinds {
    console,
    debug,
    vdp_k,
    sn76489_k,
    z80,
    clock_k,
    mapper_k
};

struct core : jsm_system{
    explicit core(jsm::systems in_variant, jsm::regions in_region) : clock(in_variant, in_region), variant(in_variant), region(in_region), cpu(false), mapper(in_variant), vdp(this, in_variant), io(this) {
        has.save_state = true;
        has.load_BIOS = true;
        has.set_audiobuf = true;
    }
    clock clock;
    u32 read_reg_ioport1(u32 val);
    u32 read_reg_ioport2(u32 val);
    void write_reg_memory_ctrl(u32 val);
    void write_reg_io_ctrl(u32 val);
    jsm::systems variant;
    jsm::regions region;
    Z80::core cpu;
    mapper_sega mapper;
    VDP vdp;
    SN76489 sn76489;
    void cpu_out_sms1(u32 addr, u32 val);
    u32 cpu_in_gg(u32 addr, u32 val, u32 has_effect);

private:
    void serialize_core(serialized_state &state);
    void serialize_debug(serialized_state &state);
    void serialize_z80(serialized_state &state);
    void serialize_clock(serialized_state &state);
    void serialize_vdp(serialized_state &state);
    void serialize_mapper(serialized_state &state);
    void serialize_sn76489(serialized_state &state);
    void deserialize_core(serialized_state &state);
    void deserialize_debug(serialized_state &state);
    void deserialize_z80(serialized_state &state);
    void deserialize_clock(serialized_state &state);
    void deserialize_vdp(serialized_state &state);
    void deserialize_mapper(serialized_state &state);
    void deserialize_sn76489(serialized_state &state);
    void cpu_cycle();
    void poll_pause();
    void sample_audio();
    void debug_audio();
public:
    void notify_NMI(bool level);
    void notify_IRQ(bool level);
    u8 main_bus_read(u16 addr, u32 has_effect);
    void main_bus_write(u16 addr, u8 val);
    u32 cpu_in_sms1(u32 addr, u32 val, u32 has_effect);

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        audiobuf *buf;
    } audio{};

    u32 described_inputs{};
    u32 last_frame{};

    u32 (core::*cpu_in)(u32, u32, u32){};
    void (core::*cpu_out)(u32, u32){};

    struct SMSGGIO {
        explicit SMSGGIO(core *parent) : portA(parent), portB(parent) {}
        SMSGG_gamepad controllerA{};
        SMSGG_gamepad controllerB{};
        controller_port portA;
        controller_port portB;
        HID_digital_button *pause_button{};
        u32 disable{};
        u32 gg_start{};
        u32 GGreg{};
    } io;

    DBG_START
        DBG_CPU_REG_START1 *A, *B, *C, *D, *E, *HL, *F, *AF_, *BC_, *DE_, *HL_, *PC, *SP, *IX, *IY, *EI, *HALT, *CE DBG_CPU_REG_END1
        DBG_EVENT_VIEW
        DBG_IMAGE_VIEW(nametables)
        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(4)
        DBG_WAVEFORM_END1
        DBG_LOG_VIEW
    DBG_END

    struct SMSDBGDATA {
        struct DBGSMSGGROW {
            struct {
                u32 hscroll, vscroll;
                u32 bg_name_table_address;
                u32 bg_color_table_address;
                u32 bg_pattern_table_address;
                u32 bg_color;
                u32 bg_hscroll_lock;
                u32 bg_vscroll_lock;
                u32 num_lines;
                u32 left_clip;
            } io;
        } rows[240];

    } dbg_data{};


    void play() final {};
    void pause() final {};
    void stop() final {};
    void get_framevars(framevars& out) final;
    void reset() final;
    void killall();
    u32 finish_frame() final;
    u32 finish_scanline() final;
    u32 step_master(u32 howmany) final;
    void load_BIOS(multi_file_set& mfs) final;
    void enable_tracing() {};
    void disable_tracing() {};
    void describe_io() final;
    void save_state(serialized_state &state) final;
    void load_state(serialized_state &state, deserialize_ret &ret) final;
    void set_audiobuf(audiobuf *ab) final;
    void setup_debugger_interface(debugger_interface &intf) final;
};

}