//
// Created by . on 5/2/24.
//

#ifndef JSMOOCH_EMUS_TAPE_DECK_H
#define JSMOOCH_EMUS_TAPE_DECK_H

#include "helpers/int.h"
#include "helpers/physical_io.h"

struct ZXSpectrum_tape_deck {
    struct cvec* IOs;
    u32 tape_deck_index;

    struct buf TAPE;
    u32 head_pos;
};

struct ZXSpectrum;
void ZXSpectrum_tape_deck_init(struct ZXSpectrum* bus);
void ZXSpectrum_tape_deck_delete(struct ZXSpectrum* bus);
void ZXSpectrum_tape_deck_load(struct ZXSpectrum* bus, struct multi_file_set* mfs);

#endif //JSMOOCH_EMUS_TAPE_DECK_H
