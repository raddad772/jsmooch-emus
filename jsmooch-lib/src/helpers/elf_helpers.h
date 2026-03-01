
#pragma once

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
    u32 offset{};
    char name[256]{};
    char fname[256]{};
    elf_symbol32_kind kind{};
};

struct elf_symbol_list32 {
    elf_symbol_list32();
    void add(u32 offset, const char* fname, const char*name, elf_symbol32_kind kind);
    elf_symbol32* find(u32 addr, u32 mask);
    std::vector<elf_symbol32> symbols{};
    u32 num{};
};
