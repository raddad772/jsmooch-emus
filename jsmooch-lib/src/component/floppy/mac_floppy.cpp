//
// Created by . on 8/5/24.
//
#include <cassert>
#include <cstring>
#include <cstdio>

#include <cmath>

#include "helpers/multisize_memaccess.cpp"
#include "helpers/jsm_string.h"

#include "mac_floppy.h"

namespace floppy::mac {

// MAME!
static constexpr u8 gcr6fw_tb[0x40] =
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

static constexpr double track_RPM[5] = {
        401.9241, 438.4626, 482.3089, 535.8988, 602.8861
};

void DISC::save()
{
    char PATH[500];
    snprintf(PATH, 500, "/Users/dave/dev/system11.bin");
    remove(PATH);

    printf("\nWRITE FILE %s", PATH);
    FILE *f = fopen(PATH, "wb");
    assert(f);

    u8 buf[1024*1024];
    u8 *bufptr{};
    for (u32 trackn = 0; trackn < 80; trackn++) {
        auto &track = disc.tracks[trackn];
        u32 buflen = 0;
        bufptr = buf;
        u32 bitlen = track.encoded_data.num_bits;
        //u32 bytelen = (((bitlen & 7) != 0) ? 1 : 0) + (bitlen / 8);
        u32 bytelen = bitlen;
        cW32(bufptr, 0, bytelen);
        cW32(bufptr, 4, bitlen);
        bufptr += 8;
        buflen += 8;
        memcpy(bufptr, &track.encoded_data.data[0], bytelen);
        buflen += bytelen;
        fwrite(buf, 1, buflen, f);
    }
    fclose(f);
}

bool DISC::load(const char* fname, buf &b)
{
    int r;
    if (ends_with(fname, ".image")) {
        r = load_dc42(b);
    }
    else if (ends_with(fname, ".bin")) {
        r = load_dave(b);
    }
    else {
        r = load_plain(b);
    }
    if (r) save();
    return r;
}

void DISC::fill_tracks(u32 num_heads_in) {
    num_heads=num_heads_in;
    assert(num_heads==1);
    u64 track_radius = 395000 + 1875;
    i32 speed_group = 0;
    u32 num_sectors = 12;
    for (u32 i = 0; i < 80; i++) {
        if (((i & 15) == 0) && (i > 0)) {
            speed_group++;
            num_sectors--;
        }
        auto &track = disc.tracks[i];
        track.unencoded_data.allocate(num_sectors * 512);
        track_radius -= 1875;
        track.id = i;
        track.head = 0;
        track.radius_mm = static_cast<double>(track_radius) / 1000.0;
        track.length_mm = track.radius_mm * track.radius_mm * M_PI;
        track.rpm = track_RPM[speed_group];

        for (u32 s = 0; s < num_sectors; s++) {
            auto &sector = track.sectors[s];
            sector.track = track.id;
            sector.head = track.head;
            sector.sector = i;
            sector.info = 0x22;
            sector.tag = nullptr;
            sector.data = nullptr;
        }
    }
}

u32 dc42_calc_checksum(const u8 *buf, u32 count, u32 chk)
{
    count >>= 1;
    while(count > 0) {
        u32 val = buf[0] & 0xFF;
        val = (val << 8) | (buf[1] & 0xFF);
        chk = (chk + val) & 0xFFFFFFFF;
        chk = ((chk >> 1) | (chk << 31)) & 0xFFFFFFFF;
        count -= 1;
        buf += 2;
    }

    return chk;
}

u8 *DISC::dc42_load_gcr(buf &b, u32 head_count, u32 &check, u32 fmt)
{
    u8 *bptr = static_cast<u8 *>(b.ptr) + 84;

    printf("\nSECTOR FORMAT? %d", fmt);
    check = 0;
    for (u32 tracki = 0; tracki < 80; tracki++) {
        auto &track = disc.tracks[tracki];
        u32 sector_count = track.sectors.size();
        u8 *sector_ptr = static_cast<u8 *>(track.unencoded_data.ptr);

        for (u32 headi = 0; headi < head_count; headi++) {
            u32 sector_num = 0;
            for (u32 sectori = 0; sectori < sector_count; sectori++) {
                auto &sector = track.sectors[sector_num];
                memcpy(sector_ptr, bptr, 512);
                sector.data = sector_ptr;
                sector_ptr += 512;
                bptr += 512;

                check = dc42_calc_checksum(sector.data, 512, check);
                // Interleave sector numbers
                sector_num = (sector_num + 2) % sector_count;
                if(sector_num == 0)
                    sector_num++;
            }
        }
    }
    return bptr;
}

int DISC::dc42_load_tags(u8 *bptr, u32 tags_size, u32 &check)
{
    check = 0;
    for (u32 headi = 0; headi < num_heads; headi++) {
        u32 sector_add = 800 * headi;
        for (u32 tracki = 0; tracki < disc.tracks.size(); tracki++) {
            auto &track = disc.tracks[tracki];
            track.tags_data.allocate(12 * track.sectors.size());
            u8 *td_buf = static_cast<u8 *>(track.tags_data.ptr);

            for (u32 sectori = 0; sectori < track.sectors.size(); sectori++) {
                auto &sector = track.sectors[sectori];
                memcpy(td_buf, bptr, 12);
                sector.tag = td_buf;
                td_buf += 12;
                bptr += 12;

                if ((headi > 0) || (tracki > 0) || (sectori > 0)) {
                    check = dc42_calc_checksum(sector.tag, 12, check);
                }
            }
        }
    }
    return true;
}

bool DISC::load_dc42(buf &b)
{
    u32 check = 0;
    fill_tracks(1);
    if (b.size < 84) return false;

    auto *bufptr = static_cast<u8 *>(b.ptr);
    u32 mn = cR16_be(bufptr, 82);
    printf("\nAttempting DC42 load. Filesize: %ld", b.size);
    if (mn != 0x0100) {
        printf("\nbad magic number! %04x", mn);
        return false;
    }
    if (bufptr[0] > 63) {
        printf("\nBad buf0 %d", bufptr[0]);
        return false;
    }

    u32 data_size = cR32_be(bufptr, 64);
    u32 tags_size = cR32_be(bufptr, 68);
    printf("\nDATA SIZE: %d", data_size);
    printf("\nTAGS SIZE: %d", tags_size);
    u32 data_check = cR32_be(bufptr, 72);
    u32 tags_check = cR32_be(bufptr, 76);

    u32 fmt = bufptr[81];

    u8 *bptr = nullptr;
    switch(data_size) {
        case 400UL * 1024UL:
            bptr = dc42_load_gcr(b, 1, check, fmt);
            break;
        case 800UL*1024UL:
            bptr = dc42_load_gcr(b, 2, check, fmt);
            break;
        default:
            assert(1==2);
    }

    if (!bptr) return false;

    if (data_check != check) {
        printf("\nData checksum error");
        return false;
    }

    if (tags_size > 0) {
        if (!dc42_load_tags(bptr, tags_size, check)) {
            printf("\nLoad tags fail!");
            return false;
        };
        if (tags_check != check) {
            printf("\ntag checksum error!");
            return false;
        }
    }
    printf("\nLoad success!");
    printf("\nGenerating bits...");
    for (u32 tracki = 0; tracki < 80; tracki++) {
        auto &track = disc.tracks[tracki];
        encode_track(track);
    }



    return true;
}

bool DISC::load_dave(buf &b) {
    printf("\nDISC SIZE: %ldb %ldKb", b.size, b.size >> 10);
    auto *buf_ptr = static_cast<u8 *>(b.ptr);

    fill_tracks(1);
    for (u32 i = 0; i < 80; i++) {
        auto &track = disc.tracks[i];
        memcpy(&track.encoded_data.data[0], buf_ptr, 62172);
        buf_ptr += 62172;
    }
    return true;
}

bool DISC::load_plain(buf &b)
{
    printf("\nDISC SIZE: %ldb %ldKb", b.size, b.size >> 10);
    auto *buf_ptr = static_cast<u8 *>(b.ptr);

    fill_tracks(1);

    for (u32 i = 0; i < 80; i++) {
        auto &track = disc.tracks[i];
        u32 num_sectors = track.sectors.size();
        memcpy(track.unencoded_data.ptr, buf_ptr, 512*num_sectors);
        buf_ptr += 512 * num_sectors;

        u32 sector_num = 0;

        u8 *out_ptr = static_cast<u8 *>(track.unencoded_data.ptr);
        for (u32 s = 0; s < track.sectors.size(); s++) {
            auto &sector = track.sectors[sector_num];
            sector.data = out_ptr;
            out_ptr += 512;

            // Interleave sector numbers
            sector_num = (sector_num + 2) % num_sectors;
            if(sector_num == 0)
                sector_num++;
        }

        encode_track(track);
    }
    return true;
}



// Thanks, MAME, again!
u32 gcr6_encode(const u8 va, const u8 vb, const u8 vc)
{
    u32 r = gcr6fw_tb[((va >> 2) & 0x30) | ((vb >> 4) & 0x0c) | ((vc >> 6) & 0x03)] << 24;
    r |= gcr6fw_tb[va & 0x3f] << 16;
    r |= gcr6fw_tb[vb & 0x3f] << 8;
    r |= gcr6fw_tb[vc & 0x3f];
    return r;
}


void DISC::encode_track(generic::TRACK<12, 5181> &track)
{
    // Thank-you to MAME for this!
    // 30318342 = 60.0 / 1.979e-6
    static constexpr u32 cells_per_speed_zone[5] = {
            30318342 / 394,
            30318342 / 429,
            30318342 / 472,
            30318342 / 525,
            30318342 / 590
    };

    u8 notag[12] = {};

    u32 speed_zone = track.id/16;
    if(speed_zone > 4)
        speed_zone = 4;

    u32 sectors = 12 - speed_zone;
    u32 pregap = cells_per_speed_zone[speed_zone] - 6208 * sectors;

    u32 prepregap = pregap % 48;
    track.encoded_data.write.pos = 0;
    if(prepregap >= 24) {
        track.encoded_data.write_bits(prepregap - 24, 0xff3fcf);
        track.encoded_data.write_bits(24, 0xf3fcff);
    } else
        track.encoded_data.write_bits(prepregap, 0xf3fcff);

    for(u32 i = 0; i != pregap / 48; i++) {
        track.encoded_data.write_bits(24, 0xff3fcf);
        track.encoded_data.write_bits(24, 0xf3fcff);
    }

    for(u32 s = 0; s != sectors; s++) {
        auto &sector = track.sectors[s];
        for(u32 i=0; i != 8; i++) {
            track.encoded_data.write_bits(24, 0xff3fcf);
            track.encoded_data.write_bits(24, 0xf3fcff);
        }

        track.encoded_data.write_bits(24, 0xd5aa96);
        track.encoded_data.write_bits(8, gcr6fw_tb[sector.track & 0x3f]);
        track.encoded_data.write_bits(8, gcr6fw_tb[sector.sector & 0x3f]);
        track.encoded_data.write_bits(8, gcr6fw_tb[(sector.track & 0x40 ? 1 : 0) | (sector.head ? 0x20 : 0)]);
        track.encoded_data.write_bits(8, gcr6fw_tb[sector.info & 0x3f]);
        u8 check = sector.track ^ sector.sector ^ ((sector.track & 0x40 ? 1 : 0) | (sector.head ? 0x20 : 0)) ^ sector.info;
        track.encoded_data.write_bits(8, gcr6fw_tb[check & 0x3f]);
        track.encoded_data.write_bits(24, 0xdeaaff);

        track.encoded_data.write_bits(24, 0xff3fcf);
        track.encoded_data.write_bits(24, 0xf3fcff);

        track.encoded_data.write_bits(24, 0xd5aaad);
        track.encoded_data.write_bits(8, gcr6fw_tb[sector.sector & 0x3f]);

        const u8 *data = sector.tag;
        if(!data)
            data = notag;

        u8 ca = 0, cb = 0, cc = 0;
        for(int i=0; i < 175; i ++) {
            if(i == 4)
                data = sector.data - 3*4;

            u8 va = data[3*i];
            u8 vb = data[3*i+1];
            u8 vc = i != 174 ? data[3*i+2] : 0;

            cc = (cc << 1) | (cc >> 7);
            const u16 suma = ca + va + (cc & 1);
            ca = suma;
            va = va ^ cc;
            const u16 sumb = cb + vb + (suma >> 8);
            cb = sumb;
            vb = vb ^ ca;
            if(i != 174)
                cc = cc + vc + (sumb >> 8);
            vc = vc ^ cb;

            const u32 nb = i != 174 ? 32 : 24;
            track.encoded_data.write_bits(nb, gcr6_encode(va, vb, vc) >> (32-nb));
        }

        track.encoded_data.write_bits(32, gcr6_encode(ca, cb, cc));
        track.encoded_data.write_bits(32, 0xdeaaffff);
    }
    track.encoded_data.write_final();
}

}