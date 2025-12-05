// Created by . on 8/5/24.
//

#pragma once
#include "generic_floppy.h"
namespace floppy::mac {
struct DISC {

    generic::DISC<80,12,5181> disc{}; // struct generic_floppy_track
    bool write_protect{true};
    u32 num_heads{};

    void save();
    bool load(const char *fname, buf &b);
    void fill_tracks(u32 num_heads);

    u8 *dc42_load_gcr(buf &b, u32 head_count, u32 &check, u32 fmt);
    int dc42_load_tags(u8 *bptr, u32 tags_size, u32 &check);
    bool load_dc42(buf &b);
    bool load_plain(buf &b);
    void encode_track(generic::TRACK<12, 5181> &track);
    bool load_dave(buf &b);
};
}
