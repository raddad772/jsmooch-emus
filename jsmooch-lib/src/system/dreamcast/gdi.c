//
// Created by Dave on 2/19/2024.
//

#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "gdi.h"

void GDI_init(GDI_image *this)
{
    this->num_tracks = 0;
    for (u32 i = 0; i < 50; i++) {
        this->tracks[i].data = NULL;
        this->tracks[i].data_sz = 0;
    }
}

void GDI_clear(GDI_image* this)
{
    for (u32 i = 0; i < 50; i++) {
        if (this->tracks[i].data != NULL) {
            free(this->tracks[i].data);
        }
        this->tracks[i].data_sz = 0;
    }
}

void GDI_delete(GDI_image *this)
{
    GDI_clear(this);
}

static u32 decode_du32(const u8* w)
{
    u64 v = *(u64*)w;
    return ((v >> 56) & 0x000000FF)
           | ((v >> 40) & 0x0000FF00)
           | ((v >> 24) & 0x00FF0000)
           | ((v >> 8) & 0xFF000000);
}

static void read_sector_to_buffer(u8 *dst, u8 *src, u32 sz)
{
    assert(sz==2352);
    if (sz == 2352) {
        memcpy(dst, src+16, 2048);
        return;
    }
}

static void read_sectors_to_buffer(u8* dst, GDI_track *track, u32 lba, u32 num)
{
    u8* dst_ptr = dst;
    for (u32 n = 0; n < num; n++) {
        u8 *ptr = track->data + ((lba + n) * track->sector_size);
        read_sector_to_buffer(dst_ptr, ptr, track->sector_size);
        dst_ptr += 2048;
    }
}

#define ru16(x) (*(u16*)x)
#define ru32(x) (*(u32*)x)

static u32 ru32be(const u8 *w)
{
    u32 in = *(u32*)w;
    u32 out = ((in&0xFF) << 24) | ((in & 0xFF00)<<8) | ((in & 0xFF0000)>>8) | ((in & 0xFF000000)>>24);
    return out;
}

static void read_datetime(ISO9660_dt* this, u8* whr)
{
    char str[5];
    char *where = (char *)whr;
    char *beef;
    memset(str, 0, 5);
#define doit(x,y,n) memcpy(str, where+x, n); this->y = strtol(str, &beef, 10); memset(str, 0, 5)
    doit(0, year, 4);
    doit(4, month, 2);
    doit(6, day, 2);
    doit(8, hour, 2);
    doit(10, min, 2);
    doit(12, sec, 2);
    doit(14, millisec, 2);
    this->tz_offset = (u32)where[16];
}

static void read_directory_datetime(ISO9660_directroy_dt* this, u8* whr)
{
    this->year = whr[0] + 1900;
    this->month = whr[1];
    this->day = whr[2];
    this->hour = whr[3];
    this->min = whr[4];
    this->sec = whr[5];
    this->tz_offset = whr[6];
}

static void read_directory_entry(ISO9660_directory_record* this, u8* w, u32 lba_start, GDI_track *track, u32 is_root)
{
    memset(this, 0, sizeof(ISO9660_directory_record));
    buf_init(&this->data);
    this->len = w[0];
    this->extended_attr_record_len = w[1];
    this->location_extant = decode_du32(w + 2) - lba_start;
    this->size_extant = decode_du32(w + 10);
    read_directory_datetime(&this->dt, w+18);
    this->file_flags = w[25];
    this->interleave_file_unit_size = w[26];
    this->interleave_gap_mode = w[27];
    this->volume_sequence_num = ru16(w+28);
    this->id_len = w[32];
    memcpy(this->id, w+33, this->id_len);
    // Now read out the sectors it says to
    u32 sz = ((this->size_extant + 2047) / 2048) * 2048;
    buf_allocate(&this->data, sz);
    printf("\nLE %d", this->location_extant);
    read_sectors_to_buffer(this->data.ptr, track, this->location_extant, sz / 2048);
}

void pprint_directory_entry(ISO9660_directory_record* this)
{
    printf("\nDirectory name: %s\nLBA: %d SIZE:%d", this->id, this->location_extant, this->size_extant);
}

struct ISO9660_path_table_entry{
    u32 len;
    u32 extanded_attribute_record_len;
    u32 location_of_extent;
    u32 directory_number_of_parents;
    char volume_id[500];
};

static void read_primary_volume_descriptor(ISO9660_volume_descriptor *vd, u8* sector, u32 lba_start, GDI_track *track)
{
    memset(vd, 0, sizeof(ISO9660_volume_descriptor));
    assert(sector[1] == 'C' && sector[2] == 'D' && sector[3] == '0' && sector[4] == '0' && sector[5] == '1' && sector[6] == 1);
    vd->type = 1;
    memcpy(vd->system_id, sector+8, 32);
    memcpy(vd->volume_id, sector+40, 32);
    vd->space_size = decode_du32(sector+80);
    vd->set_size = ru16(sector+120);
    vd->sequence_number = ru16(sector+124);
    vd->logical_block_size = ru16(sector+128);
    vd->path_table_size = decode_du32(sector+132);
    vd->type_l_loc = decode_du32(sector + 140); //ru32(sector+140) - lba_start
    vd->optional_type_l_loc = decode_du32(sector + 144) - lba_start;
    read_directory_entry(&vd->root_entry, sector+156, lba_start, track, 1);
    u8* ptr = vd->root_entry.data.ptr;
    return;
    struct ISO9660_directory_record dr;
    do {
        read_directory_entry(&dr, ptr, lba_start, track, 0);
        pprint_directory_entry(&dr);
        if ((ptr - (u8*)vd->root_entry.data.ptr) > 2048) break;
        printf("\n! %d", dr.len);
        ptr += dr.len;
    } while(true);
    memcpy(vd->set_id, sector+190, 128);
    memcpy(vd->publisher_id, sector+318, 128);
    memcpy(vd->data_preparer_id, sector+446, 128);
    memcpy(vd->app_id, sector+574, 128);
    memcpy(vd->copyright_file_id, sector+702, 37);
    memcpy(vd->abstract_file_id, sector+739, 37);
    memcpy(vd->bibliographic_file_id, sector+776, 37);
    read_datetime(&vd->creation_dt, sector+813);
    read_datetime(&vd->modification_dt, sector+830);
    read_datetime(&vd->expiration_dt, sector+847);
    read_datetime(&vd->effective_dt, sector+864);
    vd->path_version = (u32)sector[881];
}

static u32 CreateTrackInfo(u32 ctrl,u32 addr,u32 fad)
{
    u8 p[4];
    p[0]=(ctrl<<4)|(addr<<0);
    p[1]=fad>>16;
    p[2]=fad>>8;
    p[3]=fad>>0;

    return *(u32*)p;
}
static u32 CreateTrackInfo_se(u32 ctrl,u32 addr,u32 tracknum)
{
    u8 p[4];
    p[0]=(ctrl<<4)|(addr<<0);
    p[1]=tracknum;
    p[2]=0;
    p[3]=0;
    return *(u32*)p;
}


void GDI_GetToc(GDI_image *this, u32* to, u32 area)
{
    memset(to, 0xFFFFFFFF, 102*4);

    u32 first_track=1;
    u32 last_track=(u32)(this->num_tracks);
    if (area==1)
        first_track=3;
    //else if (disc->type==GdRom)
    //{
    else
        last_track=2;
    //
    //}

    //Generate the TOC info

    //-1 for 1..99 0 ..98
    //t.CTRL=track.mode==0?0:4;
    // u32 fmt = d->tracks[i].CTRL==4?2048:2352;
    // CTRL=4 if 2048 sectors
    // CTRL=0 if 2352 sectors
    //	to[99]=CreateTrackInfo_se(disc->tracks[first_track-1].CTRL,disc->tracks[first_track-1].ADDR,first_track);
    //	to[100]=CreateTrackInfo_se(disc->tracks[last_track-1].CTRL,disc->tracks[last_track-1].ADDR,last_track);
    to[99] = CreateTrackInfo_se(this->tracks[first_track-1].type,1,first_track);
    to[100]=CreateTrackInfo_se(this->tracks[last_track-1].type,1,last_track);

    //if (disc->type==GdRom)
    //{
        //use smaller LEADOUT
        if (area==0)
            to[101]=CreateTrackInfo(0,0,13085);
        //else {
            //printf("\nINVALID DENSITY");
        //}
    //}
    for (u32 i=first_track-1;i<last_track;i++)
    {
        to[i]=CreateTrackInfo(this->tracks[i].type,1,this->tracks[i].begin_LBA);
    }

}

void GDI_load(char *folder, const char *filename, GDI_image *img)
{
    struct GDI_image *this = img;
    snprintf(this->path, sizeof(this->path), "%s/%s", folder, filename);
    printf("\nPATH! %s", this->path);
    GDI_clear(this);

    FILE *f = fopen(this->path, "r");
    char tmp[5000];
    fgets(tmp, 5000, f);
    this->num_tracks = atoi(tmp);
    for (u32 i = 0; i < this->num_tracks; i++) {
        fgets(tmp, 5000, f);
        char *p = tmp;
        u32 track_num = strtol(p, &p, 10);
        this->tracks[i].begin_LBA = strtol(p, &p, 10)+150; // FADS
        this->tracks[i].type = strtol(p, &p, 10); // CTRL
        this->tracks[i].sector_size = strtol(p, &p, 10);
        char *fname = strtok(p, " ");
        char fn[500];
        strcpy(fn, fname);
        this->tracks[i].offset = strtol(p, &p, 10);
        assert(this->tracks[i].offset == 0);
        char fpath[500];
        snprintf(fpath, sizeof(fpath), "%s/%s", folder, fn);
        FILE *dt = fopen(fpath, "rb");
        fseek(dt, 0L, SEEK_END);
        this->tracks[i].data_sz = ftell(dt);
        fseek(dt, 0, SEEK_SET);
        this->tracks[i].data = malloc(this->tracks[i].data_sz);
        u32 data = fread(this->tracks[i].data, 1, this->tracks[i].data_sz, dt);
        fclose(dt);
    }
    fclose(f);

    // OK, we really mostly care about track 3 for new
    return;
    struct GDI_track* t = &this->tracks[2];

    // Now find the primary volume descriptor, starting at sector 10
    u32 num_sectors = t->data_sz / t->sector_size;
    u8 p[2048];
    u32 found = 0;
    printf("\nBeginning search");
    struct ISO9660_volume_descriptor vd;
    for (u32 s = 0x10; s < num_sectors; s++) {
        read_sector_to_buffer(p, t->data + (s * t->sector_size), t->sector_size);
        //printf("YO! %d", (u32)p[0]);
        if (p[0] == 1) {
            found = 1;
            printf("\nFOUND PRIMARY VOLUME DESCRIPTOR AT SECTOR %d\n", s);
            read_primary_volume_descriptor(&vd, p, 45000, t);
            fflush(stdout);
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("\nPRIMARY VOLUME DESCRIPTOR NOT FOUND!");
        return;
    }
    printf("\nLBA ADDR %d %d %d %d", vd.type_l_loc, vd.optional_type_l_loc, vd.path_table_size, vd.logical_block_size);
}

// when >2048 bytes, First 16 bytes of each sector will be the header, then 2048 bytes of sector data and then the error correction at the end

