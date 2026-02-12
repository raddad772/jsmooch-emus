//
// Created by . on 2/11/26.
//
#include <cassert>
#include "cdrom_formats.h"

static inline u32 msf_to_lba(u32 m, u32 s, u32 f) {
    return m * 60 * 75 + s * 75 + f;
}

struct cue_track {
    int track_no{};
    CDROM_TRACK_MODE mode{};
    int file_index{};

    u32 pregap_lba{};        // total pregap (INDEX 00 + PREGAP)
    u32 file_lba{};          // INDEX 01 file position

    u32 start_lba{};         // start of pregap on disc
    u32 data_lba{};          // start of INDEX 01 on disc
};

u8 *CDROM_DISC::ptr_to_data(u32 m, u32 s, u32 f) {
    u32 lba = msf_to_lba(m, s, f);
    u32 offset = lba * 2352;
    if (offset >= data.sz)
        return nullptr;
    return data.ptr + offset;
}

bool CDROM_DISC::parse_cue(multi_file_set &mfs) {
    valid = false;

    if (mfs.files.empty())
        return false;

    // Find .cue file
    int cue_index = -1;
    for (size_t i = 0; i < mfs.files.size(); i++) {
        const char *n = mfs.files[i].name;
        if (strstr(n, ".cue") || strstr(n, ".CUE")) {
            cue_index = static_cast<int>(i);
            break;
        }
    }
    if (cue_index < 0)
        return false;

    const char *cue = static_cast<const char *>(mfs.files[cue_index].buf.ptr);
    size_t cue_sz = mfs.files[cue_index].buf.size;

    std::vector<cue_track> tracks;

    int current_file = -1;
    cue_track *current_track = nullptr;

    const char *p = cue;
    const char *end = cue + cue_sz;

    auto next_line = [&](char *out, size_t out_sz) -> bool {
        if (p >= end)
            return false;

        // Skip line endings from previous read
        while (p < end && (*p == '\n' || *p == '\r'))
            p++;

        if (p >= end)
            return false;

        // Skip leading whitespace
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;

        size_t i = 0;
        while (p < end && *p != '\n' && *p != '\r') {
            if (i + 1 < out_sz)
                out[i++] = *p;
            p++;
        }
        out[i] = 0;

        return true;
    };

    char line[512];

    while (next_line(line, sizeof(line))) {
        printf("\nLINE (%ld): %s", strlen(line), line);
        if (!line[0]) continue;
        // FILE "foo.bin" BINARY
        if (!strncmp(line, "FILE", 4)) {
            char fname[256]{};
            if (sscanf(line, R"(FILE "%255[^"]")", fname) == 1) {
                for (size_t i = 0; i < mfs.files.size(); i++) {
                    if (!strcmp(mfs.files[i].name, fname)) {
                        current_file = static_cast<int>(i);
                        break;
                    }
                }
            }
        }

        // TRACK nn MODE
        else if (!strncmp(line, "TRACK", 5)) {
            int num{};
            char mode[64]{};

            if (sscanf(line, "TRACK %d %63s", &num, mode) == 2) {
                tracks.push_back({});
                current_track = &tracks.back();
                current_track->track_no = num;
                current_track->file_index = current_file;

                if (!strcmp(mode, "AUDIO"))
                    current_track->mode = CDMODE_AUDIO;
                else if (!strcmp(mode, "MODE1/2352"))
                    current_track->mode = CDMODE_MODE1;
                else if (!strcmp(mode, "MODE2/2352"))
                    current_track->mode = CDMODE_MODE2;
            }
        }

        // INDEX 00 mm:ss:ff
        else if (!strncmp(line, "INDEX 00", 8) && current_track) {
            u32 m{}, s{}, f{};
            if (sscanf(line, "INDEX 00 %u:%u:%u", &m, &s, &f) == 3) {
                current_track->pregap_lba += msf_to_lba(m, s, f);
            }
        }

        // INDEX 01 mm:ss:ff
        else if (!strncmp(line, "INDEX 01", 8) && current_track) {
            u32 m{}, s{}, f{};
            if (sscanf(line, "INDEX 01 %u:%u:%u", &m, &s, &f) == 3) {
                current_track->file_lba = msf_to_lba(m, s, f);
            }
        }
        // PREGAP mm:ss:ff
        else if (!strncmp(line, "PREGAP", 6) && current_track) {
            u32 m{}, s{}, f{};
            if (sscanf(line, "PREGAP %u:%u:%u", &m, &s, &f) == 3) {
                current_track->pregap_lba += msf_to_lba(m, s, f);
            }
        }
    }
    printf("\nNUM TRACKS %ld", tracks.size());
    if (tracks.empty())
        return false;

    // Assign absolute LBAs (with global 150 sector pregap)
    u32 cur_lba = 150; // lead-in

    for (size_t i = 0; i < tracks.size(); i++) {
        cue_track &t = tracks[i];

        t.start_lba = cur_lba;
        t.data_lba  = cur_lba + t.pregap_lba;

        read_file_buf &f = mfs.files[t.file_index];
        u32 file_sectors = (u32)(f.buf.size / 2352);

        u32 next_file_lba = file_sectors;
        if (i + 1 < tracks.size() &&
            tracks[i + 1].file_index == t.file_index) {
            next_file_lba = tracks[i + 1].file_lba;
            }

        u32 data_sectors = next_file_lba - t.file_lba;
        cur_lba = t.data_lba + data_sectors;
    }

    // Allocate disc buffer
    u32 total_lba = cur_lba;
    u32 total_size = total_lba * 2352;
    total_size = 74 * 60 * 75 * 2352; // 74min * 60sec * 75sector * 2352 bytes per
    printf("\nALLOCATE %d BYTES", total_size);
    data.allocate(total_size);
    memset(data.ptr, 0, total_size);

    // Copy track data
    for (auto &t : tracks) {
        read_file_buf &f = mfs.files[t.file_index];

        u8 *dst = data.ptr + t.data_lba * 2352;
        u8 *src = static_cast<u8 *>(f.buf.ptr) + t.file_lba * 2352;

        u32 file_sectors = static_cast<u32>(f.buf.size / 2352);
        u32 copy_lba = file_sectors - t.file_lba;

        memcpy(dst, src, copy_lba * 2352);
    }

    valid = true;
    return true;
}
