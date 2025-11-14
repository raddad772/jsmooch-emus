//
// Created by . on 5/8/24.
//

#include "elf_helpers.h"
#include <cstdio>

void elf_symbol_list32_init(elf_symbol_list32* this)
{
    cvec_init(&this->symbols, sizeof(elf_symbol32), 2000);
    this->num = 0;
}

void elf_symbol_list32_delete(elf_symbol_list32* this)
{
    cvec_delete(&this->symbols);
}


struct elf_symbol32* elf_symbol_list32_find(elf_symbol_list32* this, u32 addr, u32 mask)
{
    for (u32 i = 0; i < this->num; i++) {
        struct elf_symbol32* sym = cvec_get(&this->symbols, i);
        if ((sym->offset & mask) == (addr & mask))
            return sym;
    }
    return NULL;
}

void elf_symbol_list32_add(elf_symbol_list32* this, u32 offset, const char* fname, const char*name, enum elf_symbol32_kind kind)
{
    /*struct elf_symbol32* sym = elf_symbol_list32_find(this, offset, 0xFFFFFFFF);
    if (sym != NULL) {
        printf("\nDUPLICATE SYMBOL DROPPED: %s from %s. OLD ONE: %s from %s", name, fname, sym->name, sym->fname);
        return;
    }*/
    struct elf_symbol32* sym;
    sym = cvec_push_back(&this->symbols);
    snprintf(sym->name, sizeof(sym->name), "%s", name);
    snprintf(sym->fname, sizeof(sym->fname), "%s", fname);
    sym->offset = offset;
    sym->kind = kind;
    this->num++;
}

