//
// Created by . on 5/2/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/pack.h"

namespace ZXSpectrum {

enum td_states {
    td_stopped = 0,
    td_playing = 1
};

PACK_BEGIN
struct zxs_pulse {
    u64 start{};
    u32 duration{};
    u32 level{};
} PACK_END;

enum td_kinds {
    tdk_none = 0,
    tdk_binary = 1,
    tdk_pulses = 2
};
struct core;

struct TAPE_DECK {
    explicit TAPE_DECK(core *parent);
    void rewind();
    void remove();
    void load_pzx(multi_file_set& mfs);
    void play();
    void stop();
    void load(multi_file_set& mfs);

    core *bus;
    u32 tape_deck_index{};

    td_states state{td_stopped};
    td_kinds kind{tdk_none};
    buf TAPE_binary{};
    std::vector<zxs_pulse> TAPE_pulses{};

    u64 play_start_cycle{};
    u64 play_pause_cycle{};
    u32 pulse_block{};
    u32 head_pos{};
};

}