//
// Created by Dave on 2/14/2024.
//

#ifndef JSMOOCH_EMUS_JSM_STRING_H
#define JSMOOCH_EMUS_JSM_STRING_H

#include "helpers/int.h"

struct jsm_string {
    char *ptr; // Pointer
    char *cur; // Current location in string
    u32 allocated_len; // Maximum length
};


void jsm_string_init(struct jsm_string *str, u32 size);
void jsm_string_delete(struct jsm_string *str);
void jsm_string_seek(struct jsm_string *str, i32 pos);
void jsm_string_sprintf(struct jsm_string *str, const char* format, ...);
void jsm_string_empty(struct jsm_string *str);
void jsm_string_quickempty(struct jsm_string *this);

#endif //JSMOOCH_EMUS_JSM_STRING_H
