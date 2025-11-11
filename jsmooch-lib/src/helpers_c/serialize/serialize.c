//
// Created by . on 11/10/24.
//
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "serialize.h"


void serialized_state_init(serialized_state *this)
{
    memset(this, 0, sizeof(*this));
    cvec_init(&this->buf, 1, 2048);
    cvec_init(&this->sections, sizeof(serialized_state_section), 5);
    jsimg_init(&this->screenshot);
    jsm_string_init(&this->fpath, 200);
    jsm_string_init(&this->fname, 200);
}

void serialized_state_delete(serialized_state *this)
{
    cvec_delete(&this->buf);
    cvec_delete(&this->sections);
    jsimg_delete(&this->screenshot);
    jsm_string_delete(&this->fpath);
    jsm_string_delete(&this->fname);
}

void serialized_state_new_section(serialized_state *this, const char *friendly_name, int kind, int version)
{
    struct serialized_state_section *sec = cvec_push_back(&this->sections);
    memset(sec, 0, sizeof(*sec));
    sec->sz = 0;
    sec->offset = this->buf.len;
    sec->kind = kind;
    sec->version = version;
    this->cur_section = sec;
    snprintf(sec->friendly_name, 50, "%s", friendly_name);
}

static inline void add_sz(serialized_state *this, u64 howmuch)
{
    cvec_alloc_atleast(&this->buf, this->buf.len+howmuch);
}

void Sload(serialized_state *this, void *ptr, u64 howmuch)
{
    if (howmuch == 0) return;
    memcpy(ptr, ((u8 *)this->buf.data) + this->iter.offset, howmuch);
    this->iter.offset += howmuch;
}

void serialized_state_write_to_file(serialized_state *state, FILE *f)
{
    u32 fout;
#define WV(x) fwrite(&(x), 1, sizeof(x), f);

    fout = 0xD34DB33F;
    WV(fout);

    WV(state->version);
    WV(state->kind);

    for (u32 i = 0; i < cvec_len(&state->sections); i++) {
        struct serialized_state_section *sec = cvec_get(&state->sections, i);
        WV(sec->kind);
        WV(sec->version);
        WV(sec->sz);
        fwrite(sec->friendly_name, 1, sizeof(sec->friendly_name), f);

        fwrite(((char *)state->buf.data) + sec->offset, 1, sec->sz, f);
    }
    printf("\nSaved %lld sections", cvec_len(&state->sections));

#undef WV
}

int serialized_state_read_from_file(serialized_state *state, FILE *f, size_t file_size)
{
#define RV(x) fread(&(x), 1, sizeof(x), f);
    state->buf.len = file_size;

    u32 v32;
    u64 v64;

    RV(v32);
    if (v32 != 0xD34DB33F) {
        printf("\nBAD MAGIC NUMBER");
        return -1;
    }

    cvec_clear(&state->sections);
    cvec_clear(&state->buf);

    RV(state->version);
    RV(state->kind);

    u32 file_offset = 12;
    while(file_offset < file_size) {
        //printf("\nREAD SECTION %d of %lld", file_offset, (u64)file_size);
        struct serialized_state_section *sec = cvec_push_back(&state->sections);
        state->cur_section = sec;

        RV(sec->kind);
        RV(sec->version);
        RV(sec->sz);
        file_offset += 16;
        fread(sec->friendly_name, 1, sizeof(sec->friendly_name), f);
        file_offset += 50;

        sec->offset = state->buf.len;
        cvec_alloc_atleast(&state->buf, state->buf.len + sec->sz);
        fread(((char *)state->buf.data) + state->buf.len, 1, sec->sz, f);

        state->buf.len += sec->sz;
        file_offset += sec->sz;
    }
    printf("\nLoaded %lld sections", cvec_len(&state->sections));
#undef RV
    return 0;
}

void Sadd(serialized_state *this, void *ptr, u64 howmuch)
{
    if (howmuch == 0) return;
    add_sz(this, howmuch);
    memcpy(((u8 *)this->buf.data) + this->buf.len, ptr, howmuch);
    this->buf.len += howmuch;
    this->cur_section->sz += howmuch;
}
