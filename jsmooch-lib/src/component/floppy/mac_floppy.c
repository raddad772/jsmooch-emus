//
// Created by . on 8/5/24.
//
#include <string.h>
#include <stdio.h>
//#define _USE_MATH_DEFINES
#include <math.h>

#include "mac_floppy.h"

void mac_floppy_init(struct mac_floppy *this)
{
    generic_floppy_init(&this->disc);
}

void mac_floppy_delete(struct mac_floppy *this)
{
    generic_floppy_delete(&this->disc);
}


// MAME!
const u8 gcr6fw_tb[0x40] =
        {
                0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
                0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
                0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
                0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
                0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
                0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
                0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
                0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
        };

static double track_RPM[5] = {
        401.9241, 438.4626, 482.3089, 535.8988, 602.8861
};

void mac_floppy_load(struct mac_floppy *mflpy, struct buf *b)
{
    printf("\nDISC SIZE: %lldb %lldKb", b->size, b->size >> 10);
    u8 *buf_ptr = (u8 *)b->ptr;
    u64 track_radius = 395000 + 1875;
    i32 speed_group = 0;
    u32 num_sectors = 12;

    for (u32 i = 0; i < 80; i++) {
        if (((i & 15) == 0) && (i > 0)) {
            speed_group++;
            num_sectors--;
        }
        struct generic_floppy_track *track = cvec_push_back(&mflpy->disc.tracks);
        generic_floppy_track_init(track);
        buf_allocate(&track->unencoded_data, num_sectors * 512);
        memcpy(track->unencoded_data.ptr, buf_ptr, 512*num_sectors);
        buf_ptr += 512 * num_sectors;

        track_radius -= 1875;
        track->id = i;
        track->head = 0;
        track->radius_mm = ((double)track_radius) / 1000.0;
        track->length_mm = track->radius_mm * track->radius_mm * M_PI;
        track->rpm = track_RPM[speed_group];
        u32 sector_num = 0;

        cvec_grow(&track->sectors, num_sectors);
        u8 *out_ptr = track->unencoded_data.ptr;
        for (u32 s = 0; s < num_sectors; s++) {
            struct generic_floppy_sector *sector = cvec_get(&track->sectors, sector_num);
            sector->track = track->id;
            sector->head = track->head;
            sector->sector = i;
            sector->info = 0x22;
            sector->tag = NULL;

            sector->data = out_ptr;
            out_ptr += 512;

            // Interleave sector numbers
            sector_num = (sector_num + 2) % num_sectors;
            if(sector_num == 0)
                sector_num++;
        }

        mac_floppy_encode_track(track);
    }
}

// Thanks, MAME, again!
u32 gcr6_encode(u8 va, u8 vb, u8 vc)
{
    u32 r;
    r = gcr6fw_tb[((va >> 2) & 0x30) | ((vb >> 4) & 0x0c) | ((vc >> 6) & 0x03)] << 24;
    r |= gcr6fw_tb[va & 0x3f] << 16;
    r |= gcr6fw_tb[vb & 0x3f] << 8;
    r |= gcr6fw_tb[vc & 0x3f];
    return r;
}


void mac_floppy_encode_track(struct generic_floppy_track *track)
{
    // Thank-you to MAME for this!
    // 30318342 = 60.0 / 1.979e-6
    static const u32 cells_per_speed_zone[5] = {
            30318342 / 394,
            30318342 / 429,
            30318342 / 472,
            30318342 / 525,
            30318342 / 590
    };

    u8 notag[12];
    memset(notag, 0, 12);

    u32 speed_zone = track->id/16;
    if(speed_zone > 4)
        speed_zone = 4;

    u32 sectors = 12 - speed_zone;
    u32 pregap = cells_per_speed_zone[speed_zone] - 6208 * sectors;

    u32 prepregap = pregap % 48;
    if(prepregap >= 24) {
        bitbuf_write_bits(&track->encoded_data, prepregap - 24, 0xff3fcf);
        bitbuf_write_bits(&track->encoded_data, 24, 0xf3fcff);
    } else
        bitbuf_write_bits(&track->encoded_data, prepregap, 0xf3fcff);

    for(u32 i = 0; i != pregap / 48; i++) {
        bitbuf_write_bits(&track->encoded_data, 24, 0xff3fcf);
        bitbuf_write_bits(&track->encoded_data, 24, 0xf3fcff);
    }

    for(u32 s = 0; s != sectors; s++) {
        struct generic_floppy_sector *sector = cvec_get(&track->sectors, s);
        for(u32 i=0; i != 8; i++) {
            bitbuf_write_bits(&track->encoded_data, 24, 0xff3fcf);
            bitbuf_write_bits(&track->encoded_data, 24, 0xf3fcff);
        }

        bitbuf_write_bits(&track->encoded_data, 24, 0xd5aa96);
        bitbuf_write_bits(&track->encoded_data, 8, gcr6fw_tb[sector->track & 0x3f]);
        bitbuf_write_bits(&track->encoded_data, 8, gcr6fw_tb[sector->sector & 0x3f]);
        bitbuf_write_bits(&track->encoded_data, 8, gcr6fw_tb[(sector->track & 0x40 ? 1 : 0) | (sector->head ? 0x20 : 0)]);
        bitbuf_write_bits(&track->encoded_data, 8, gcr6fw_tb[sector->info & 0x3f]);
        u8 check = sector->track ^ sector->sector ^ ((sector->track & 0x40 ? 1 : 0) | (sector->head ? 0x20 : 0)) ^ sector->info;
        bitbuf_write_bits(&track->encoded_data, 8, gcr6fw_tb[check & 0x3f]);
        bitbuf_write_bits(&track->encoded_data, 24, 0xdeaaff);

        bitbuf_write_bits(&track->encoded_data, 24, 0xff3fcf);
        bitbuf_write_bits(&track->encoded_data, 24, 0xf3fcff);

        bitbuf_write_bits(&track->encoded_data, 24, 0xd5aaad);
        bitbuf_write_bits(&track->encoded_data, 8, gcr6fw_tb[sector->sector & 0x3f]);

        const u8 *data = sector->tag;
        if(!data)
            data = notag;

        u8 ca = 0, cb = 0, cc = 0;
        for(int i=0; i < 175; i ++) {
            if(i == 4)
                data = sector->data - 3*4;

            u8 va = data[3*i];
            u8 vb = data[3*i+1];
            u8 vc = i != 174 ? data[3*i+2] : 0;

            cc = (cc << 1) | (cc >> 7);
            u16 suma = ca + va + (cc & 1);
            ca = suma;
            va = va ^ cc;
            u16 sumb = cb + vb + (suma >> 8);
            cb = sumb;
            vb = vb ^ ca;
            if(i != 174)
                cc = cc + vc + (sumb >> 8);
            vc = vc ^ cb;

            u32 nb = i != 174 ? 32 : 24;
            bitbuf_write_bits(&track->encoded_data, nb, gcr6_encode(va, vb, vc) >> (32-nb));
        }

        bitbuf_write_bits(&track->encoded_data, 32, gcr6_encode(ca, cb, cc));
        bitbuf_write_bits(&track->encoded_data, 32, 0xdeaaffff);
    }
    bitbuf_write_final(&track->encoded_data);
}

