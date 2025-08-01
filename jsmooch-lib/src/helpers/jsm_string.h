//
// Created by Dave on 2/14/2024.
//

#ifndef JSMOOCH_EMUS_JSM_STRING_H
#define JSMOOCH_EMUS_JSM_STRING_H

#include <stdarg.h>

#include "helpers/int.h"

struct jsm_string {
    char *ptr; // Pointer
    char *cur; // Current location in string
    u32 allocated_len; // Maximum length
};


void jsm_string_init(struct jsm_string *str, u32 size);
void jsm_string_delete(struct jsm_string *str);
void jsm_string_seek(struct jsm_string *str, i32 pos);
int jsm_string_sprintf(struct jsm_string *str, const char* format, ...);
int jsm_string_vsprintf(struct jsm_string *str, const char* format, va_list va);
void jsm_string_empty(struct jsm_string *str);
void jsm_string_quickempty(struct jsm_string *);


// thanks https://stackoverflow.com/questions/744766/how-to-compare-ends-of-strings-in-c
u32 ends_with(const char *str, const char *suffix);


#endif //JSMOOCH_EMUS_JSM_STRING_H
