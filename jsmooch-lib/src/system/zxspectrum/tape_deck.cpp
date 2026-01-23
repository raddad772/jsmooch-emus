//
// Created by . on 5/2/24.
//

#include <cstring>
#include <cassert>

#include "tape_deck.h"
#include "zxspectrum.h"
#include "zxspectrum_bus.h"

namespace ZXSpectrum {
void TAPE_DECK::rewind()
{
    
    head_pos = 0;
    state = td_stopped;
    play_pause_cycle = play_start_cycle = 0;
}

void TAPE_DECK::remove()
{
    kind = tdk_none;
    state = td_stopped;
    head_pos = 0;
    TAPE_pulses.clear();
}

TAPE_DECK::TAPE_DECK(core *parent) : bus(parent)
    {
}

void TAPE_DECK::load_pzx(multi_file_set& mfs) {
    
    kind = tdk_none;
    state = td_stopped;
    play_pause_cycle = play_start_cycle = 0;
    TAPE_pulses.clear();

    u32 pulses[2][256];

    u8 *ptr = static_cast<u8 *>(mfs.files[0].buf.ptr);
    u8 *end = ptr + mfs.files[0].buf.size;
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
                        zxs_pulse *p = &TAPE_pulses.emplace_back();
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
                    shift = static_cast<i32>(count) - 1;
                }
                for (u32 i = 0; i < count; i++) {
                    if (shift == -1) {
                        byte = G8;
                        shift = 7;
                    }
                    u32 bit = (byte >> shift) & 1;
                    for (u32 j = 0; j < num_bits[bit]; j++) {
                        u32 duration = pulses[bit][j];
                        zxs_pulse *p = &TAPE_pulses.emplace_back();
                        p->start = current_cycle;
                        p->level = pulse_level;
                        p->duration = duration;
                        current_cycle += duration;
                        pulse_level ^= 1;
                    }

                    zxs_pulse *p = &TAPE_pulses.emplace_back();
                    p->start = current_cycle;
                    p->level = pulse_level;
                    p->duration = tail_len;
                    current_cycle += tail_len;
                    pulse_level ^= 1;
                }
                break;
            }
            case 0x53554150: { // PAUS
                zxs_pulse *p = &TAPE_pulses.emplace_back();
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
    kind = tdk_pulses;
}

void TAPE_DECK::play()
{
    
    if (kind != tdk_pulses) {
        printf("\nError play not supported for this tape type");
        return;
    }
    printf("\nPlaying tape!");
    state = td_playing;
    // Play from stop without eject
    if ((play_pause_cycle > play_start_cycle) && (play_start_cycle > 0)) {
        u64 r = bus->clock.master_cycles - play_pause_cycle;
        play_pause_cycle = 0;
        play_start_cycle += r;
    }
    else { // Play from scratch
        play_pause_cycle = 0;
        play_start_cycle = bus->clock.master_cycles;
        pulse_block = 0;
    }
}

void TAPE_DECK::stop()
{
    
    if (kind != tdk_pulses) {
        printf("\nStop not supported for this tape type!");
        return;
    }
    printf("\nStopping tape...");
    // We were playing...we're pausing
    if ((play_start_cycle > 0) && (state == td_playing)) {
        play_pause_cycle = bus->clock.master_cycles;
    }
    else { // We're just stopped already
        play_start_cycle = 0;
        play_pause_cycle = 0;
    }
    state = td_stopped;
}


void TAPE_DECK::load(multi_file_set& mfs) {
    
    kind = tdk_binary;
    state = td_stopped;
    buf* b = &mfs.files[0].buf;

    u8* ubi = static_cast<u8 *>(b->ptr);
    u32 pos = 0;
    u32 total_size = 0;
    buf blocks[512];
    u32 blocks_len=0;
    while(pos<(b->size-1)) {
        // Fetch block size
        u32 block_size = ubi[pos] + (ubi[pos+1] << 8);
        pos += 2;
        // Allocate and copy block
        total_size += block_size;
        blocks[blocks_len].allocate(block_size);
        memcpy(blocks[blocks_len].ptr, ubi+pos, block_size);
        blocks_len++;
        pos += block_size;

        assert(blocks_len < 512);
    }

    printf("\nJust parsed %d blocks of total size %d", blocks_len, total_size);
    TAPE_binary.allocate(total_size);
    u32 tpos = 0;
    for (u32 i = 0; i < blocks_len; i++) {
        for (u32 j = 0; j < blocks[i].size; j++) {
            static_cast<u8 *>(TAPE_binary.ptr)[tpos] = *(static_cast<u8 *>(blocks[i].ptr) + j);
            tpos++;
        }
    }
}
}