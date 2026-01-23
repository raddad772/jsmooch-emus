//
// Created by . on 1/21/26.
//

#pragma once

#include "helpers/sys_interface.h"
#include "component/cpu/z80/z80.h"
#include "ula.h"
#include "tape_deck.h"
#include "clock.h"

namespace ZXSpectrum {

struct core : jsm_system {
    CLOCK clock;
    ULA ula;
    TAPE_DECK tape_deck;
    Z80::core cpu;

    explicit core(variants variant_in);
    ~core();
    u8 ULA_readmem(u16 addr) const;
    void notify_IRQ(bool level);
    void CPU_cycle();
    u8 CPU_readmem(u16 addr);
    void CPU_writemem(u16 addr, u8 val);

    variants variant{};

    /*u8 ROM[16*1024]{};
    u8 RAM[48*1024]{};*/
    u8 *ROM{};
    u32 ROM_size{};
    u32 ROM_mask{};

    u8 *RAM{};
    u32 RAM_size{};
    u32 RAM_mask{};

    struct {
        u8 *display{};
        u8 *ROM{};
        u8 *RAM[4]{};
        u8 disable{};
    } bank{};

    bool described_inputs{};
    u32 cycles_left{};
    bool display_enabled{};
    void setup_keyboard();
    void fast_load();
    void load_Z80_file(multi_file_set& mfs);
    void load_SNA_file(multi_file_set& mfs);

public:
    void play() final;
    void pause() final;
    void stop() final;
    void get_framevars(framevars& out) final;
    void reset() final;
    void killall(){};
    u32 finish_frame() final;
    u32 finish_scanline() final;
    u32 step_master(u32 howmany) final;
    void load_BIOS(multi_file_set& mfs) final;
    void enable_tracing();
    void disable_tracing();
    void describe_io() final;
    void save_state(serialized_state &state) final{};
    void load_state(serialized_state &state, deserialize_ret &ret) final{};
    void set_audiobuf(audiobuf *ab) final{};
    void setup_debugger_interface(debugger_interface &intf) final;
    //void sideload(multi_file_set& mfs) final;
};

}