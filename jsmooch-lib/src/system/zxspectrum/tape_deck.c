//
// Created by . on 5/2/24.
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "tape_deck.h"
#include "zxspectrum.h"

#define BTHIS struct ZXSpectrum_tape_deck* this = &bus->tape_deck

void ZXSpectrum_tape_deck_init(struct ZXSpectrum* bus)
{
    BTHIS;
    buf_init(&this->TAPE);
    this->head_pos = 0;
}

void ZXSpectrum_tape_deck_delete(struct ZXSpectrum* bus)
{
    BTHIS;
    buf_delete(&this->TAPE);
}

static void reset(struct ZXSpectrum* bus)
{
    BTHIS;
    this->head_pos = 0;
}

void ZXSpectrum_tape_deck_load(struct ZXSpectrum* bus, struct multi_file_set* mfs) {
    BTHIS;
    struct buf* b = &mfs->files[0].buf;

    u8* ubi = b->ptr;
    u32 pos = 0;
    u32 total_size = 0;
    struct buf blocks[512];
    u32 blocks_len=0;
    while(pos<(b->size-1)) {
        // Fetch block size
        u32 block_size = ubi[pos] + (ubi[pos+1] << 8);
        pos += 2;
        // Allocate and copy block
        total_size += block_size;
        buf_init(&blocks[blocks_len]);
        buf_allocate(&blocks[blocks_len], block_size);
        blocks_len++;
        memcpy(blocks[blocks_len].ptr, ubi+pos, block_size);
        pos += block_size;

        assert(blocks_len < 512);
    }

    printf("Just parsed %d blocks of total size %d", blocks_len, total_size);
    buf_allocate(&this->TAPE, total_size);
    u32 tpos = 0;
    for (u32 i = 0; i < blocks_len; i++) {
        for (u32 j = 0; j < blocks[i].size; j++) {
            *(u8 *)&this->TAPE.ptr[tpos] = *((u8*)blocks[i].ptr + j);
            tpos++;
        }
        buf_delete(&blocks[i]);
    }
}
