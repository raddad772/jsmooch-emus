//
// Created by . on 8/3/24.
//
#include <cstring>
#include <ctype.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <pwd.h>
#endif

#include "inifile.h"
#include "user.h"
#include "helpers/buf.h"

char *construct_path_with_home(char *w, size_t w_sz, const char *who)
{
    const char *homeDir = get_user_dir();
    return w + snprintf(w, w_sz, "%s/%s", homeDir, who);
}


kv_section &inifile::new_section(const char *name)
{
    kv_section &th = sections.emplace_back();

    th.kvs.reserve(10);
    snprintf(th.name, sizeof(th.name), "%s", name);

    return th;
}

void inifile::clear()
{
    sections.clear();
}


kv_pair &kv_section::new_pair(const char *fname)
{
    kv_pair &th = kvs.emplace_back();
    th.kind = kvk_empty;
    snprintf(th.key, sizeof(th.key), "%s", fname);

    return th;
}

static int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower(static_cast<unsigned char>(*a)) - tolower(static_cast<unsigned char>(*b));
        if (d != 0 || !*a)
            return d;
    }
}

kv_section *inifile::find_section(const char* name)
{
    for (auto &sec : sections) {
        if (strcicmp(sec.name, name) == 0)
            return &sec;
    }
    return nullptr;
}

kv_pair *kv_section::find_key(const char* fname)
{
    for (auto& pair : kvs) {
        if (strcicmp(pair.key, fname) == 0)
            return &pair;
    }
    return nullptr;
}


u32 inifile::has_key(const char* section, const char* key)
{
    kv_section *sec = find_section(section);
    if (!sec) return 0;

    kv_pair *pair = sec->find_key(key);
    if (!pair) return 0;

    return 1;
}

kv_section* inifile::get_or_make_section(const char* section)
{
    kv_section *sec = find_section(section);
    if (sec == nullptr) {
        sec = &new_section(section);
    }
    return sec;
}

kv_pair* inifile::get_or_make_key(const char* section, const char* key)
{
    kv_section *sec = find_section(section);
    if (sec == nullptr) {
        sec = &new_section(section);
    }

    kv_pair *pair = sec->find_key(key);
    if (pair == nullptr) {
        pair = &sec->new_pair(key);
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

static get_line_retval get_line(read_file_buf *rfb, char *line, size_t line_size)
{
    snprintf(line, line_size, "");
    char *line_ptr = line;
    char *buf_ptr = static_cast<char *>(rfb->buf.ptr) + (rfb->pos - 1);
    char *end_ptr = static_cast<char *>(rfb->buf.ptr) + rfb->buf.size;

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

    get_line_retval a = line_good ? good_line : empty_line;
    if (buf_ptr == end_ptr) return static_cast<get_line_retval>(eof | a);

    return a;
}

enum parse_line_ret {
    plr_none,
    plr_kvp,
    plr_section
};

static parse_line_ret parse_line(inifile *th, char *line, size_t line_sz, char *str1buf, size_t str1sz, char *str2buf, size_t str2sz)
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


void kv_section::parse_pair(char *key, char *val)
{
    u32 is_int = 1; // finding a non-int character in value will set this to 0
    u32 is_float = 1; // finding a non-float character will set this to 0

    auto &p = kvs.emplace_back();
    snprintf(p.key, sizeof(p.key), "%s", key);
    p.str_value[0] = 0;

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
        p.kind = kvk_empty;
    }
    else if (is_int) {
        p.kind = kvk_int;
        p.int_value = atoi(val);
    }
    else if (is_float) {
        p.kind = kvk_float;
        p.float_value = atof(val);
    }
    else {
        p.kind = kvk_string;
        snprintf(p.str_value, sizeof(p.str_value), "%s", val);
    }
}

int inifile::load(const char* mpath)
{
    kv_section *current_section = nullptr;
    clear();

    read_file_buf rfb;

    if (!rfb.read(nullptr, mpath)) {
        printf("\nFAILED TO READ!");
        goto exit;
    }
    snprintf(path, sizeof(path), "%s", mpath);;

    char line[2000];

    char buf[2][2000];
    // Go line by line
    while(true) {
        get_line_retval glr = get_line(&rfb, line, sizeof(line));
        if ((glr & 1) == empty_line){
            if (glr & eof) {
                break;
            }
            continue;
        }

        // Parse the line
        parse_line_ret plr = parse_line(this, line, sizeof(line), buf[0], sizeof(buf[0]), buf[1], sizeof(buf[1]));
        switch(plr) {
            case plr_none:
                break;
            case plr_section:
                current_section = get_or_make_section(buf[0]);
                break;
            case plr_kvp:
                if (current_section == nullptr) current_section = get_or_make_section("default");
                current_section->parse_pair(buf[0], buf[1]);
                break;
        }
        if (glr & eof) {
            break;
        }
    }

    exit:
    return 1;
}

void inifile_save(inifile *th)
{

}
