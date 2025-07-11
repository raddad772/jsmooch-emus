//
// Created by . on 5/2/24.
//

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "tape_deck.h"
#include "zxspectrum.h"

#define BTHIS struct ZXSpectrum_tape_deck* this = &bus->tape_deck


void ZXSpectrum_tape_deck_rewind(struct ZXSpectrum *bus)
{
    BTHIS;
    this->head_pos = 0;
    this->state = td_stopped;
    this->play_pause_cycle = this->play_start_cycle = 0;
}

void ZXSpectrum_tape_deck_remove(struct ZXSpectrum *bus)
{
    BTHIS;
    this->kind = tdk_none;
    this->state = td_stopped;
    this->head_pos = 0;
    cvec_clear(&this->TAPE_pulses);
}

void ZXSpectrum_tape_deck_init(struct ZXSpectrum* bus)
{
    BTHIS;
    buf_init(&this->TAPE_binary);
    cvec_init(&this->TAPE_pulses, sizeof(struct zxs_pulse), 2000000);
    this->kind = tdk_none;
    this->state = td_stopped;
    this->head_pos = 0;
}

void ZXSpectrum_tape_deck_delete(struct ZXSpectrum* bus)
{
    BTHIS;
    buf_delete(&this->TAPE_binary);
    cvec_delete(&this->TAPE_pulses);
}

void ZXSpectrum_tape_deck_load_pzx(struct ZXSpectrum* bus, struct multi_file_set* mfs) {
    BTHIS;
    this->kind = tdk_none;
    this->state = td_stopped;
    this->play_pause_cycle = this->play_start_cycle = 0;
    cvec_clear(&this->TAPE_pulses);

    u32 pulses[2][256];

    u8 *ptr = (u8 *) mfs->files[0].buf.ptr;
    u8 *end = ptr + mfs->files[0].buf.size;
    u64 current_cycle = 0;
    while (ptr < end) {
#define G8 *ptr; ptr += 1
#define G16 *(u16 *)ptr; ptr += 2
#define G32 *(u32 *)ptr; ptr += 4
        // Process a block
        u32 pulse_level = 0;
        u32 block_kind = G32;
        u32 sz = G32;
        switch (block_kind) {
            case 0x54585a50: {// PZXT block
                u8 major = G8;
                u8 minor = G8;
                if (major != 1) {
                    printf("\nUnsupported PZX version %d", major);
                    return;
                }
                ptr += (sz - 2);
                break;
            }
            case 0x534c5550: {// PULS
                u8 *subptr = ptr;
                u8 *subend = subptr + sz;
                ptr += sz;
                pulse_level = 0;
                while (subptr < subend) {
#define G16sub *(u16 *)subptr; subptr += 2
                    u32 count = 1;
                    u32 duration = G16sub;
                    if (duration > 0x8000) {
                        count = duration & 0x7FFF;
                        duration = G16sub;
                    }
                    if (duration >= 0x8000) {
                        duration &= 0x7FFF;
                        duration <<= 16;
                        duration |= G16sub;
                    }
                    for (u32 i = 0; i < count; i++) {
                        if (duration == 0) {
                            pulse_level ^= 1;
                            continue;
                        }
                        struct zxs_pulse *p = cvec_push_back(&this->TAPE_pulses);
                        p->start = current_cycle;
                        p->level = pulse_level;
                        p->duration = duration;
                        current_cycle += duration;
                        pulse_level ^= 1;
                    }
                }
                break;
            }
            case 0x41544144: { // DATA
                u32 count = G32;
                pulse_level = (count >> 31) & 1;
                count &= 0x7FFFFFFF;
                u32 tail_len = G16;
                u32 num_bits[2];
                num_bits[0] = G8;
                num_bits[1] = G8;
                for (u32 i = 0; i < num_bits[0]; i++) {
                    pulses[0][i] = G16;
                }
                for (u32 i = 0; i < num_bits[1]; i++) {
                    pulses[1][i] = G16;
                }
                i32 shift = 7;
                u8 byte = G8;
                if (count < 8) {
                    shift = (i32) count - 1;
                }
                for (u32 i = 0; i < count; i++) {
                    if (shift == -1) {
                        byte = G8;
                        shift = 7;
                    }
                    u32 bit = (byte >> shift) & 1;
                    for (u32 j = 0; j < num_bits[bit]; j++) {
                        u32 duration = pulses[bit][j];
                        struct zxs_pulse *p = cvec_push_back(&this->TAPE_pulses);
                        p->start = current_cycle;
                        p->level = pulse_level;
                        p->duration = duration;
                        current_cycle += duration;
                        pulse_level ^= 1;
                    }

                    struct zxs_pulse *p = cvec_push_back(&this->TAPE_pulses);
                    p->start = current_cycle;
                    p->level = pulse_level;
                    p->duration = tail_len;
                    current_cycle += tail_len;
                    pulse_level ^= 1;
                }
                break;
            }
            case 0x53554150: { // PAUS
                struct zxs_pulse *p = cvec_push_back(&this->TAPE_pulses);
                u32 duration = G32;
                p->start = current_cycle;
                p->level = (duration >> 31) & 1;
                p->duration = duration & 0x7FFFFFFF;
                current_cycle += p->duration;
                pulse_level ^= 1;
                break;
            }
            case 0x53575242: {
                printf("\nIgnoring BRWS block...");
                ptr += sz;
                break;
            }
            case 0x504f5453: { // STOP
                printf("\nUnsupported STOP POINT. Add support...");
                ptr += sz;
                break;
            }
            default:
                printf("\nUnknown block type %04x", block_kind);
                ptr += sz;
                break;
        }
    }

#undef G32
#undef G16
#undef G8
    this->kind = tdk_pulses;
}

void ZXSpectrum_tape_deck_play(struct ZXSpectrum* bus)
{
    BTHIS;
    if (this->kind != tdk_pulses) {
        printf("\nError play not supported for this tape type");
        return;
    }
    printf("\nPlaying tape!");
    this->state = td_playing;
    // Play from stop without eject
    if ((this->play_pause_cycle > this->play_start_cycle) && (this->play_start_cycle > 0)) {
        u64 r = bus->clock.master_cycles - this->play_pause_cycle;
        this->play_pause_cycle = 0;
        this->play_start_cycle += r;
    }
    else { // Play from scratch
        this->play_pause_cycle = 0;
        this->play_start_cycle = bus->clock.master_cycles;
        this->pulse_block = 0;
    }
}

void ZXSpectrum_tape_deck_stop(struct ZXSpectrum* bus)
{
    BTHIS;
    if (this->kind != tdk_pulses) {
        printf("\nStop not supported for this tape type!");
        return;
    }
    printf("\nStopping tape...");
    // We were playing...we're pausing
    if ((this->play_start_cycle > 0) && (this->state == td_playing)) {
        this->play_pause_cycle = bus->clock.master_cycles;
    }
    else { // We're just stopped already
        this->play_start_cycle = 0;
        this->play_pause_cycle = 0;
    }
    this->state = td_stopped;
}


void ZXSpectrum_tape_deck_load(struct ZXSpectrum* bus, struct multi_file_set* mfs) {
    BTHIS;
    this->kind = tdk_binary;
    this->state = td_stopped;
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

    printf("\nJust parsed %d blocks of total size %d", blocks_len, total_size);
    buf_allocate(&this->TAPE_binary, total_size);
    u32 tpos = 0;
    for (u32 i = 0; i < blocks_len; i++) {
        for (u32 j = 0; j < blocks[i].size; j++) {
            ((u8 *)this->TAPE_binary.ptr)[tpos] = *((u8*)blocks[i].ptr + j);
            tpos++;
        }
        buf_delete(&blocks[i]);
    }
}
