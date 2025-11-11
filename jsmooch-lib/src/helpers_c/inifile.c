//
// Created by . on 8/3/24.
//
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <pwd.h>
#endif
#include <stdlib.h>

#include "inifile.h"
#include "user.h"

char *construct_path_with_home(char *w, size_t w_sz, const char *who)
{
    const char *homeDir = get_user_dir();
    return w + snprintf(w, w_sz, "%s/%s", homeDir, who);
}

void inifile_init(inifile *this)
{
    cvec_init(&this->sections, sizeof(kv_section), 10);
}

struct kv_section *section_new(inifile *inifile, const char *name)
{
    struct kv_section *this = cvec_push_back(&inifile->sections);

    cvec_init(&this->kvs, sizeof(kv_pair), 10);
    snprintf(this->name, sizeof(this->name), "%s", name);

    return this;
}

static void section_delete(kv_section *this)
{
    cvec_delete(&this->kvs);
}

static void inifile_clear(inifile* this)
{
    u32 len = cvec_len(&this->sections);
    for (u32 i = 0; i < len; i++) {
        struct kv_section *sec = cvec_get(&this->sections, i);
        section_delete(sec);
    }
    cvec_clear(&this->sections);
}



struct kv_pair *pair_new(kv_section *sec, const char *name)
{
    struct kv_pair *this = cvec_push_back(&sec->kvs);

    *this = (kv_pair) { .kind=kvk_empty };
    snprintf(this->key, sizeof(this->key), "%s", name);

    return this;
}

void inifile_delete(inifile *this)
{
    u32 len = cvec_len(&this->sections);
    for (u32 i = 0; i < len; i++) {
        struct kv_section *sec = cvec_get(&this->sections, i);
        section_delete(sec);
    }
    cvec_delete(&this->sections);
}

static int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
    return 0;
}

static struct kv_section *find_section(inifile *this, const char* name)
{
    u32 len = cvec_len(&this->sections);
    for (u32 i = 0; i < len; i++) {
        struct kv_section *sec = cvec_get(&this->sections, i);
        if (strcicmp(sec->name, name) == 0)
            return sec;
    }
    return NULL;
}

static struct kv_pair *find_key(kv_section* this, const char* name)
{
    u32 len = cvec_len(&this->kvs);
    for (u32 i = 0; i < len; i++) {
        struct kv_pair *pair = cvec_get(&this->kvs, i);
        if (strcicmp(pair->key, name) == 0)
            return pair;
    }
    return NULL;
}


u32 inifile_has_key(inifile *this, const char* section, const char* key)
{
    struct kv_section *sec = find_section(this, section);
    if (sec == NULL) return 0;

    struct kv_pair *pair = find_key(sec, key);
    if (pair == NULL) return 0;

    return 1;
}

static struct kv_section* get_or_make_section(inifile *this, const char* section)
{
    struct kv_section *sec = find_section(this, section);
    if (sec == NULL) {
        sec = section_new(this, section);
    }
    return sec;
}

struct kv_pair* inifile_get_or_make_key(inifile *this, const char* section, const char* key)
{
    struct kv_section *sec = find_section(this, section);
    if (sec == NULL) {
        sec = section_new(this, section);
    }

    struct kv_pair *pair = find_key(sec, key);
    if (pair == NULL) {
        pair = pair_new(sec, key);
    }

    return pair;
}


enum line_kind {
    unknown,
    keyval,
    section_header,
    empty,
};

enum get_line_retval {
    empty_line = 0,
    good_line = 1,
    eof = 2
};

static enum get_line_retval get_line(read_file_buf *rfb, char *line, size_t line_size)
{
    snprintf(line, line_size, "");
    char *line_ptr = line;
    char *buf_ptr = ((char *)rfb->buf.ptr) + (rfb->pos - 1);
    char *end_ptr = (char *)rfb->buf.ptr + rfb->buf.size;

    u32 opening_spaces = 1;
    int line_good = 0;

    while (buf_ptr < end_ptr) {
        buf_ptr++;
        rfb->pos++;
        if (buf_ptr == end_ptr) break;

        // Trim leading spaces...
        if ((*buf_ptr == ' ') && opening_spaces) {
            continue;
        }
        else {
            opening_spaces = 0;
        }

        if (*buf_ptr == '\n') { // End of line
            break;
        }

        // At least one non-space and non-newline character
        line_good = 1;
        *line_ptr = *buf_ptr;
        line_ptr++;
        if ((line_ptr - line) > (line_size-1)) { // Max line length
            break;
        }
    }

    // Cap off our line
    *line_ptr = 0;

    int a = line_good ? good_line : empty_line;
    if (buf_ptr == end_ptr) return eof | a;

    return a;
}

enum parse_line_ret {
    plr_none,
    plr_kvp,
    plr_section
};

static enum parse_line_ret parse_line(inifile *this, char *line, size_t line_sz, char *str1buf, size_t str1sz, char *str2buf, size_t str2sz)
{
    char *line_ptr = line - 1;
    char *line_end_ptr = line + line_sz;

    u32 opening_spaces = 1;

    u32 is_section = 0;
    u32 is_kvp = 0;
    u32 is_unknown = 1;
    u32 copy_buf = 0;

    char *buf_ptr[2] = {str1buf, str2buf};
    char *buf_end_ptr[2] = {str1buf+str1sz-1, str2buf+str2sz-1};
    char *last_non_space[2] = {buf_ptr[0], buf_ptr[1]};

    while(true) {
        line_ptr++;
        if (line_ptr >= line_end_ptr) {
            break;
        }

        if (*line_ptr == 0) break;
        if (*line_ptr == '\n') break;
        if ((*line_ptr == ' ') && opening_spaces)
            continue;
        else
            opening_spaces = 0;

        if (*line_ptr == '#') {
            break;
        }

        if ((*line_ptr == '[') && is_unknown) {
            is_section = 1;
            is_unknown = 0;
            opening_spaces = 1;
            continue;
        }

        if ((*line_ptr == ']') && is_section) {
            break;
        }

        if (*line_ptr == '=') {
            opening_spaces = 1;
            copy_buf = 1;
            is_kvp = 1;
            is_unknown = 0;
            continue;
        }

        // Copy character
        if (buf_ptr[copy_buf] < buf_end_ptr[copy_buf]) {
            *buf_ptr[copy_buf] = *line_ptr;
            buf_ptr[copy_buf]++;
        }
        *buf_ptr[copy_buf] = 0;
        if (*line_ptr != ' ')
            last_non_space[copy_buf] = buf_ptr[copy_buf];
    }
    *buf_ptr[0] = *buf_ptr[1] = 0;

    if (is_kvp) {
        *last_non_space[0] = 0;
        *last_non_space[1] = 0;
        return plr_kvp;
    }
    else if (is_section) {
        *last_non_space[0] = 0;
        return plr_section;
    }
    return plr_none;
}


static void parse_kvp(kv_section *sec, char *key, char *val)
{
    u32 is_int = 1; // finding a non-int character in value will set this to 0
    u32 is_float = 1; // finding a non-float character will set this to 0

    struct kv_pair *p = cvec_push_back(&sec->kvs);
    snprintf(p->key, sizeof(p->key), "%s", key);
    p->str_value[0] = 0;

    char *ptr = val;
    u32 len = 0;
    while(*ptr!=0) {
        len++;
        switch(*ptr) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                break;
            case '.':
                is_int = 0;
                break;
            default:
                is_int = 0;
                is_float = 0;
                break;
        }
        ptr++;
    }
    if (len == 0) {
        p->kind = kvk_empty;
    }
    else if (is_int) {
        p->kind = kvk_int;
        p->int_value = atoi(val);
    }
    else if (is_float) {
        p->kind = kvk_float;
        p->float_value = atof(val);
    }
    else {
        p->kind = kvk_string;
        snprintf(p->str_value, sizeof(p->str_value), "%s", val);
    }
}

int inifile_load(inifile *this, const char* path)
{
    struct kv_section *current_section = NULL;
    inifile_clear(this);

    struct read_file_buf rfb;
    rfb_init(&rfb);

    if (!rfb_read(NULL, path, &rfb)) {
        printf("\nFAILED TO READ!");
        goto exit;
    }

    char line[2000];

    u64 pos;
    char buf[2][2000];
    // Go line by line
    while(true) {
        enum get_line_retval glr = get_line(&rfb, line, sizeof(line));
        if ((glr & 1) == empty_line){
            if (glr & eof) {
                break;
            }
            continue;
        }

        // Parse the line
        enum parse_line_ret plr = parse_line(this, line, sizeof(line), buf[0], sizeof(buf[0]), buf[1], sizeof(buf[1]));
        switch(plr) {
            case plr_none:
                break;
            case plr_section:
                current_section = get_or_make_section(this, buf[0]);
                break;
            case plr_kvp:
                if (current_section == NULL) current_section = get_or_make_section(this, "default");
                parse_kvp(current_section, buf[0], buf[1]);
                break;
        }
        if (glr & eof) {
            break;
        }
    }

    exit:
    rfb_delete(&rfb);
    return 1;
}

void inifile_save(inifile *this)
{

}
