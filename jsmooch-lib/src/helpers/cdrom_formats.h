//
// Created by . on 2/11/26.
//

#pragma once
#include <vector>

#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/simplebuf.h"

/*
tracks    99 tracks per disk     (01h..99h) (usually only 01h on Data Disks)
index     99 indices per track   (01h..99h) (rarely used, usually always 01h)
minutes   74 minutes per disk    (00h..73h) (or more, with some restrictions)
seconds   60 seconds per minute  (00h..59h)
sectors   75 sectors per second  (00h..74h)
frames    98 frames per sector
bytes     33 bytes per frame (24+1+8 = data + subchannel + error correction)
*/

enum CDROM_TRACK_MODE {
    CDMODE_AUDIO,
    CDMODE_MODE1,
    CDMODE_MODE2,
};

struct CDROM_DISC {
    simplebuf8 data{};
    bool valid;

    bool parse_cue(multi_file_set &mfs);
    u8 *ptr_to_data(u32 minutes, u32 seconds, u32 sectors);

    struct {
        u32 track_num=0;

    } parse{};
};
