//
// Created by . on 2/11/26.
//
#include <cassert>
#include "cdrom_formats.h"
u8 *CDROM_DISC::ptr_to_data(u32 minutes, u32 seconds, u32 sectors) {
    u32 offset =  ((((minutes * 60) + seconds) * 75) + sectors) * 2352;
    u32 end = offset + 2351;
    assert(end < data.sz);
    return data.ptr + offset;
}
bool CDROM_DISC::parse_cue(multi_file_set &mfs) {
    valid = true;
    u32 total_size = 0x930 * 75 * 60 * 74;
    data.allocate(total_size);
    u8 *ptr = data.ptr;
    memset(ptr, 0, total_size);
    // 2 seconds added to start of CUE/BIn
    ptr += 0x930 * 150;

    // Data track
    memcpy(ptr, mfs.files[1].buf.ptr, mfs.files[1].buf.size);
    ptr += mfs.files[1].buf.size;

    // Audio track
    memcpy(ptr, mfs.files[2].buf.ptr, mfs.files[2].buf.size);

    return true;
}
