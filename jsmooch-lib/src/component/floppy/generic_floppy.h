//
// Created by . on 8/5/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/buf.h"
#include "helpers/bitbuffer.h"

namespace floppy {
namespace generic {

struct SECTOR {
    u8 track{}, head{}, sector{}, info{};
    u8 *tag{}, *data{};
};

template<int sectors_per_track, int bits_per_sector>
struct TRACK {
    u32 id{}, head{};
    double radius_mm{}, length_mm{}, rpm{};

    std::array<SECTOR, sectors_per_track> sectors{};


    buf unencoded_data{};
    static_bitbuf<bits_per_sector*sectors_per_track,true> encoded_data{};

    buf tags_data{};
};

template<int num_tracks, int sectors_per_track, int bits_per_sector>
struct DISC {
    std::array<TRACK<sectors_per_track, bits_per_sector>, num_tracks> tracks{}; // struct generic_floppy_track
    struct {
        float size_mm{};
        float track_inner_mm{};
        float track_outer_mm{};
    } geometry{};
};

}
}