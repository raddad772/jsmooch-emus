//
// Created by . on 5/2/24.
//

#ifndef JSMOOCH_EMUS_TAPE_DECK_H
#define JSMOOCH_EMUS_TAPE_DECK_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

enum td_states {
    td_stopped = 0,
    td_playing = 1
};

struct zxs_pulse {
    u64 start;
    u32 duration;
    u32 level;
} __attribute__((packed));

enum td_kinds {
    tdk_none = 0,
    tdk_binary = 1,
    tdk_pulses = 2
};

struct ZXSpectrum_tape_deck {
    struct cvec* IOs;
    u32 tape_deck_index;

    enum td_states state;
    enum td_kinds kind;
    struct buf TAPE_binary;
    struct cvec TAPE_pulses;

    u64 play_start_cycle;
    u64 play_pause_cycle;
    u32 pulse_block;
    u32 head_pos;

};

struct ZXSpectrum;
void ZXSpectrum_tape_deck_init(struct ZXSpectrum* bus);
void ZXSpectrum_tape_deck_delete(struct ZXSpectrum* bus);
void ZXSpectrum_tape_deck_load(struct ZXSpectrum* bus, struct multi_file_set* mfs);
void ZXSpectrum_tape_deck_load_pzx(struct ZXSpectrum* bus, struct multi_file_set* mfs);
void ZXSpectrum_tape_deck_rewind(struct ZXSpectrum *bus);
void ZXSpectrum_tape_deck_remove(struct ZXSpectrum *bus);
void ZXSpectrum_tape_deck_play(struct ZXSpectrum* bus);
void ZXSpectrum_tape_deck_stop(struct ZXSpectrum* bus);
#endif //JSMOOCH_EMUS_TAPE_DECK_H
