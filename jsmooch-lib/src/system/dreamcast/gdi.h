//
// Created by Dave on 2/19/2024.
//

#ifndef JSMOOCH_EMUS_GDI_H
#define JSMOOCH_EMUS_GDI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "helpers/buf.h"
#include "helpers/int.h"

enum GDI_track_types {
    GDI_audio = 0,
    GDI_data = 4
};

struct GDI_track {
    u32 num;
    u32 begin_LBA;
    enum GDI_track_types type;
    u32 sector_size;
    u32 offset;

    u8 *data;
    u32 data_sz;
};

struct GDI_image {
    char path[255];
    u32 num_tracks;
    struct GDI_track tracks[50];

    u32 primary_volume_descriptor;
};

enum ISO9660_VD {
    boot_record = 0,
    primary_volume_descriptor = 1,
    supplementary_volume_descriptor = 2,
    volume_partition_descriptor = 3,
    terminator = 255
};

struct ISO9660_directroy_dt {
    u32 year;
    u32 month;
    u32 day;
    u32 hour;
    u32 min;
    u32 sec;
    u32 tz_offset;
};


struct ISO9660_directory_record {
    u32 len;
    u32 extended_attr_record_len;
    u32 location_extant;
    u32 size_extant;
    struct ISO9660_directroy_dt dt;
    u32 file_flags;
    u32 interleave_file_unit_size;
    u32 interleave_gap_mode;
    u32 volume_sequence_num;
    u32 id_len;
    char id[255];
    struct buf data;
};

struct ISO9660_dt {
    u32 year;
    u32 month;
    u32 day;
    u32 hour;
    u32 min;
    u32 sec;
    u32 millisec;
    u32 tz_offset;
};

// Starting at 0x10
// primary volume descriptor points to stuff we care about
struct ISO9660_volume_descriptor {
    enum ISO9660_VD type;
    char data[2041];

    // for primary volume descriptor

    char system_id[33];
    char volume_id[33];
    u32 space_size; // Number of Logical blocks
    u32 set_size; // Size of set in this logical volume (number of disks)
    u32 sequence_number; // number of this disk in the set
    u32 logical_block_size; // Should be 2048. Size of a logical block
    u32 path_table_size; // Size in bytes of path table
    u32 type_l_loc; // Location of type-L little endian path table
    u32 optional_type_l_loc; // Location of optional table
    u32 type_m_loc; // Location of type-M big endian path table
    u32 optional_type_m_loc; // Location of optiona ltype-M table
    struct ISO9660_directory_record root_entry;
    char set_id[129];
    char publisher_id[129];
    char data_preparer_id[129];
    char app_id[129];
    char copyright_file_id[38];
    char abstract_file_id[38];
    char bibliographic_file_id[38];
    struct ISO9660_dt creation_dt;
    struct ISO9660_dt modification_dt;
    struct ISO9660_dt expiration_dt;
    struct ISO9660_dt effective_dt;
    u32 path_version;
};

void GDI_init(struct GDI_image *);
void GDI_delete(struct GDI_image *);
void GDI_load(char *folder, const char *filename, GDI_image *img);
void GDI_GetToc(struct GDI_image *, u32 *toc, u32 area);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_GDI_H
