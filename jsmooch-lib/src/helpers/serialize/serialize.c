//
// Created by . on 11/10/24.
//
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "serialize.h"


void seralized_state_init(struct serialized_state *this)
{
    memset(this, 0, sizeof(*this));
    cvec_init(&this->buf, 1, 2048);
    cvec_init(&this->sections, sizeof(struct serialized_state_section), 5);
    jsimg_init(&this->screenshot);
    jsm_string_init(&this->fpath, 200);
    jsm_string_init(&this->fname, 200);
}

void serialized_state_delete(struct serialized_state *this)
{
    cvec_delete(&this->buf);
    cvec_delete(&this->sections);
    jsimg_delete(&this->screenshot);
    jsm_string_delete(&this->fpath);
    jsm_string_delete(&this->fname);
}

void serialized_state_new_section(struct serialized_state *this, const char *friendly_name, int kind, int version)
{
    struct serialized_state_section *sec = cvec_push_back(&this->sections);
    memset(sec, 0, sizeof(*sec));
    sec->sz = 0;
    sec->offset = this->buf.len;
    sec->kind = kind;
    sec->version = version;
    snprintf(sec->friendly_name, 50, "%s", friendly_name);
}

static inline void add_sz(struct serialized_state *this, u64 howmuch)
{
    cvec_alloc_atleast(&this->buf, this->buf.len+howmuch);
}

void Sload(struct serialized_state *this, void *ptr, u64 howmuch)
{
    memcpy(ptr, ((u8 *)this->buf.data) + this->iter.offset, howmuch);
    this->iter.offset += howmuch;
}


void Sadd(struct serialized_state *this, void *ptr, u64 howmuch)
{
    add_sz(this, howmuch);
    memcpy(((u8 *)this->buf.data) + this->buf.len, ptr, howmuch);
    this->buf.len += howmuch;
    this->cur_section->sz += howmuch;
}
