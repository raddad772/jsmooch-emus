//
// Created by . on 8/5/24.
//
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#define _USE_MATH_DEFINES // for M_PI
#endif
#include <math.h>

#include "helpers/multisize_memaccess.c"
#include "helpers/jsm_string.h"

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

static void mac_floppy_save(struct mac_floppy *mflpy)
{
    char PATH[500];
    snprintf(PATH, 500, "/Users/dave/dev/system11.bin");
    remove(PATH);

    printf("\nWRITE FILE %s", PATH);
    FILE *f = fopen(PATH, "wb");
    assert(f);

    u8 buf[1024*1024];
    u8 *bufptr;
    for (u32 trackn = 0; trackn < 80; trackn++) {
        struct generic_floppy_track *track = cvec_get(&mflpy->disc.tracks, trackn);
        u32 buflen = 0;
        bufptr = buf;
        u32 bitlen = track->encoded_data.num_bits;
        //u32 bytelen = (((bitlen & 7) != 0) ? 1 : 0) + (bitlen / 8);
        u32 bytelen = bitlen;
        cW32(bufptr, 0, bytelen);
        cW32(bufptr, 4, bitlen);
        bufptr += 8;
        buflen += 8;
        memcpy(bufptr, track->encoded_data.buf.data, bytelen);
        buflen += bytelen;
        fwrite(buf, 1, buflen, f);
    }
    fclose(f);
}


int mac_floppy_load(struct mac_floppy *mflpy, const char* fname, struct buf *b)
{
    int r;
    if (ends_with(fname, ".image")) {
        r = mac_floppy_dc42_load(mflpy, b);
    }
    else {
        r = mac_floppy_plain_load(mflpy, b);
    }
    if (r) mac_floppy_save(mflpy);
    return r;
}

void fill_mac_floppy_tracks(struct mac_floppy *mflpy, u32 num_heads) {
    mflpy->num_heads = num_heads;
    assert(num_heads==1);
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
        track_radius -= 1875;
        track->id = i;
        track->head = 0;
        track->radius_mm = ((double) track_radius) / 1000.0;
        track->length_mm = track->radius_mm * track->radius_mm * M_PI;
        track->rpm = track_RPM[speed_group];

        cvec_grow_by(&track->sectors, num_sectors);
        for (u32 s = 0; s < num_sectors; s++) {
            struct generic_floppy_sector *sector = cvec_get(&track->sectors, s);
            sector->track = track->id;
            sector->head = track->head;
            sector->sector = i;
            sector->info = 0x22;
            sector->tag = NULL;
            sector->data = NULL;
        }
    }
}

u32 dc42_calc_checksum(const u8 *buf, u32 count, u32 chk)
{
    u32 val;
    count >>= 1;
    while(count > 0) {
        val = buf[0] & 0xFF;
        val = (val << 8) | (buf[1] & 0xFF);
        chk = (chk + val) & 0xFFFFFFFF;
        chk = ((chk >> 1) | (chk << 31)) & 0xFFFFFFFF;
        count -= 1;
        buf += 2;
    }

    return chk;
}

void *dc42_load_gcr(struct mac_floppy *mflpy, struct buf *b, u32 head_count, u32 *check, u32 fmt)
{
    u32 tracki, headi, sectori;
    u32 sector_count;
    u8 *bptr = (char *)b->ptr + 84;

    printf("\nSECTOR FORMAT? %d", fmt);
    *check = 0;
    for (tracki = 0; tracki < 80; tracki++) {
        struct generic_floppy_track *track = cvec_get(&mflpy->disc.tracks, tracki);
        sector_count = cvec_len(&track->sectors);
        u8 *sector_ptr = track->unencoded_data.ptr;

        for (headi = 0; headi < head_count; headi++) {
            u32 sector_num = 0;
            for (sectori = 0; sectori < sector_count; sectori++) {
                struct generic_floppy_sector *sector = cvec_get(&track->sectors, sector_num + (headi * 800));
                memcpy(sector_ptr, bptr, 512);
                sector->data = sector_ptr;
                sector_ptr += 512;
                bptr += 512;

                *check = dc42_calc_checksum(sector->data, 512, *check);
                // Interleave sector numbers
                sector_num = (sector_num + 2) % sector_count;
                if(sector_num == 0)
                    sector_num++;
            }
        }
    }
    return bptr;
}

int dc42_load_tags(struct mac_floppy *mflpy, u8 *bptr, u32 tags_size, u32 *check)
{
    *check = 0;
    for (u32 headi = 0; headi < mflpy->num_heads; headi++) {
        u32 sector_add = 800 * headi;
        for (u32 tracki = 0; tracki < cvec_len(&mflpy->disc.tracks); tracki++) {
            struct generic_floppy_track *track = cvec_get(&mflpy->disc.tracks, tracki);
            buf_allocate(&track->tags_data, 12 * cvec_len(&track->sectors));
            u8 *td_buf = track->tags_data.ptr;

            for (u32 sectori = 0; sectori < cvec_len(&track->sectors); sectori++) {
                struct generic_floppy_sector *sector = cvec_get(&track->sectors, sectori);
                memcpy(td_buf, bptr, 12);
                sector->tag = td_buf;
                td_buf += 12;
                bptr += 12;

                if ((headi > 0) || (tracki > 0) || (sectori > 0)) {
                    *check = dc42_calc_checksum(sector->tag, 12, *check);
                }
            }
        }
    }
    return 1;
}

int mac_floppy_dc42_load(struct mac_floppy *mflpy, struct buf *b)
{
    u32 check = 0;
    fill_mac_floppy_tracks(mflpy, 1);
    if (b->size < 84) return 0;

    u8 *bufptr = b->ptr;
    u32 mn = cR16_be(bufptr, 82);
    printf("\nAttempting DC42 load. Filesize: %lld", b->size);
    if (mn != 0x0100) {
        printf("\nbad magic number! %04x", mn);
        return 0;
    }
    if (bufptr[0] > 63) {
        printf("\nBad buf0 %d", bufptr[0]);
        return 0;
    }

    u32 data_size = cR32_be(bufptr, 64);
    u32 tags_size = cR32_be(bufptr, 68);
    printf("\nDATA SIZE: %d", data_size);
    printf("\nTAGS SIZE: %d", tags_size);
    u32 data_check = cR32_be(bufptr, 72);
    u32 tags_check = cR32_be(bufptr, 76);

    u32 fmt = bufptr[81];

    u8 *bptr = NULL;
    switch(data_size) {
        case 400UL * 1024UL:
            bptr = dc42_load_gcr(mflpy, b, 1, &check, fmt);
            break;
        case 800UL*1024UL:
            bptr = dc42_load_gcr(mflpy, b, 2, &check, fmt);
            break;
        default:
            assert(1==2);
    }

    if (!bptr) return 0;

    if (data_check != check) {
        printf("\nData checksum error");
        return 0;
    }

    if (tags_size > 0) {
        if (!dc42_load_tags(mflpy, bptr, tags_size, &check)) {
            printf("\nLoad tags fail!");
            return 0;
        };
        if (tags_check != check) {
            printf("\ntag checksum error!");
            return 0;
        }
    }
    printf("\nLoad success!");
    printf("\nGenerating bits...");
    for (u32 tracki = 0; tracki < 80; tracki++) {
        struct generic_floppy_track *track = cvec_get(&mflpy->disc.tracks, tracki);
        mac_floppy_encode_track(track);
    }



    return 1;
}


int mac_floppy_plain_load(struct mac_floppy *mflpy, struct buf *b)
{
    printf("\nDISC SIZE: %lldb %lldKb", b->size, b->size >> 10);
    u8 *buf_ptr = (u8 *)b->ptr;

    fill_mac_floppy_tracks(mflpy, 1);

    for (u32 i = 0; i < 80; i++) {
        struct generic_floppy_track *track = cvec_get(&mflpy->disc.tracks, i);
        u32 num_sectors = cvec_len(&track->sectors);
        memcpy(track->unencoded_data.ptr, buf_ptr, 512*num_sectors);
        buf_ptr += 512 * num_sectors;

        u32 sector_num = 0;

        u8 *out_ptr = track->unencoded_data.ptr;
        for (u32 s = 0; s < cvec_len(&track->sectors); s++) {
            struct generic_floppy_sector *sector = cvec_get(&track->sectors, sector_num);
            sector->data = out_ptr;
            out_ptr += 512;

            // Interleave sector numbers
            sector_num = (sector_num + 2) % num_sectors;
            if(sector_num == 0)
                sector_num++;
        }

        mac_floppy_encode_track(track);
    }
    return 1;
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

