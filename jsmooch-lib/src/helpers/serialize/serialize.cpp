//
// Created by . on 11/10/24.
//
#include <cstring>

#include "serialize.h"


void serialized_state::new_section(const char *friendly_name, int kind, int version)
{
    serialized_state_section &sec = sections.emplace_back(serialized_state_section());
    sec.sz = 0;
    sec.offset = buf.size();
    sec.kind = kind;
    sec.version = version;
    cur_section = &sec;
    snprintf(sec.friendly_name, 50, "%s", friendly_name);
}

/*static inline void add_sz(serialized_state *this, u64 howmuch)
{
    cvec_alloc_atleast(&buf, buf.len+howmuch);
}*/

void Sload(serialized_state &th, void *ptr, u64 howmuch)
{
    if (howmuch == 0) return;
    memcpy(ptr, &th.buf[0] + th.iter.offset, howmuch);
    th.iter.offset += howmuch;
}

void serialized_state::write_to_file(FILE *f) const {
    u32 fout;
#define WV(x) fwrite(&(x), 1, sizeof(x), f);

    fout = 0xD34DB33F;
    WV(fout);

    WV(version);
    WV(kind);

    for (auto &sec : sections) {
        WV(sec.kind);
        WV(sec.version);
        WV(sec.sz);
        fwrite(sec.friendly_name, 1, sizeof(sec.friendly_name), f);

        fwrite(buf.data() + sec.offset, 1, sec.sz, f);
    }
    printf("\nSaved %ld sections", sections.size());

#undef WV
}

int serialized_state::read_from_file(FILE *f, size_t file_size)
{
#define RV(x) fread(&(x), 1, sizeof(x), f);

    u32 v32;

    RV(v32);
    if (v32 != 0xD34DB33F) {
        printf("\nBAD MAGIC NUMBER");
        return -1;
    }

    sections.clear();
    buf.clear();
    buf.resize(file_size);

    RV(version);
    RV(kind);

    u32 file_offset = 12;
    while(file_offset < file_size) {
        //printf("\nREAD SECTION %d of %lld", file_offset, (u64)file_size);
        serialized_state_section &sec = sections.emplace_back();
        cur_section = &sec;

        RV(sec.kind);
        RV(sec.version);
        RV(sec.sz);
        file_offset += 16;
        fread(sec.friendly_name, 1, sizeof(sec.friendly_name), f);
        file_offset += 50;

        sec.offset = buf.size();
        buf.resize(buf.size() + sec.sz);
        fread(&buf[0] + sec.offset, 1, sec.sz, f);

        file_offset += sec.sz;
    }
    printf("\nLoaded %ld sections", sections.size());
#undef RV
    return 0;
}

void Sadd(serialized_state &th, const void *ptr, u64 howmuch)
{
    if (howmuch == 0) return;
    u64 p = th.buf.size();
    th.buf.resize(p + howmuch);
    memcpy(th.buf.data() + p, ptr, howmuch);
    th.cur_section->sz += howmuch;
}
