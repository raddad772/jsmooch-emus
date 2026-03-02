//
// Created by . on 5/8/24.
//

#include "elf_helpers.h"
#include <cstdio>

elf_symbol_list32::elf_symbol_list32() {
    num = 0;
    symbols.reserve(2000);
}

elf_symbol32* elf_symbol_list32::find(u32 addr, u32 mask)
{
    for (u32 i = 0; i < num; i++) {
        elf_symbol32* sym = &symbols[i];
        if ((sym->offset & mask) == (addr & mask))
            return sym;
    }
    return nullptr;
}

void elf_symbol_list32::add(u32 offset, const char* fname, const char*name, elf_symbol32_kind kind)
{
    /*elf_symbol32* sym = elf_symbol_list32_find(this, offset, 0xFFFFFFFF);
    if (sym != NULL) {
        printf("\nDUPLICATE SYMBOL DROPPED: %s from %s. OLD ONE: %s from %s", name, fname, sym->name, sym->fname);
        return;
    }*/
    elf_symbol32* sym;
    sym = &symbols.emplace_back();
    snprintf(sym->name, sizeof(sym->name), "%s", name);
    if (fname == nullptr) {
        snprintf(sym->fname, sizeof(sym->fname), "");
    }
    else {
        snprintf(sym->fname, sizeof(sym->fname), "%s", fname);
    }

    sym->offset = offset;
    sym->kind = kind;
    num++;
}

