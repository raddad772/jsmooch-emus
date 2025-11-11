
#ifndef JSMOOCH_EMUS_ELF_HELPERS_H
#define JSMOOCH_EMUS_ELF_HELPERS_H

#include "helpers/int.h"
#include "helpers/cvec.h"

enum elf_symbol32_kind {
    esk_unknown,
    esk_object,
    esk_file,
    esk_function,
    esk_variable,
    esk_common
};

struct elf_symbol32 {
    u32 offset;
    char name[256];
    char fname[256];
    enum elf_symbol32_kind kind;
};

struct elf_symbol_list32 {
    struct cvec symbols;
    u32 num;
};


void elf_symbol_list32_init(elf_symbol_list32*);
void elf_symbol_list32_delete(elf_symbol_list32*);
void elf_symbol_list32_add(elf_symbol_list32*, u32 offset, const char* fname, const char*name, enum elf_symbol32_kind kind);
struct elf_symbol32* elf_symbol_list32_find(elf_symbol_list32*, u32 addr, u32 mask);

#endif