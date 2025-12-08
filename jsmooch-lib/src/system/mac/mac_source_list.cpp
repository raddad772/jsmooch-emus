//
// Created by . on 12/7/25.
//
#include <list>
#include <cstdio>
#include <cassert>

#include "helpers/inifile.h"

#include "mac_source_list.h"
#include "mac_bus.h"

namespace mac {


static char* seek_whitespace_end(char *buf) {
    char *ptr = buf;
    switch (*ptr) {
        case 0:
        case '\n': // newline
        case 0x0D: // linefeed
            return nullptr;
        case ' ':
        case '\t':
            break;
        default:
            return ptr;
    }
    for (;;) {
        char c = *ptr++;
        switch (c) {
            case ' ': // space
            case '\t': // tab
                continue;
            case 0:
            case '\n': // newline
            case 0x0D: // linefeed
                return nullptr;
        }
        return --ptr;
    }
}

bool str_contains_and_replace_with_end(char *s, char c) {
    while (*s != c && *s !=0) {
        s++;
    }
    if (*s == c) {
        *s = 0;
        return true;
    }
    return false;
}

static void get_vars(void *ptr, source_listing::view &lv, source_listing::realtime_vars &rv) {
    auto *th = static_cast<mac::core *>(ptr);
    u32 PC = th->cpu.debug.ins_PC;
    if ((PC >= lv.base_addr+lv.range_start) && (PC < lv.base_addr+lv.range_end)) {
        if (lv.addr_to_line.contains(PC-lv.base_addr)) {
            rv.line_of_executing_instruction = lv.addr_to_line[PC-lv.base_addr];
            rv.instruction_in_list = true;
            return;
        }
    }
    rv.instruction_in_list = false;
}

void core::load_symbols(debugger_interface &dif) {
    construct_path_with_home(dasm.path, sizeof(dasm.path), "Documents/512k disasm hex.a");
    printf("\nPATH: %s", dasm.path);

    char * line = nullptr;
    size_t len = 0;
    ssize_t read;

    dbg.source_listing.view = dif.make_view(dview_source_listing);
    auto &lv = dbg.source_listing.view.get().source_listing;
    lv.lines.reserve(40000);
    lv.symbols.reserve(40000);

    lv.get_realtime_vars.ptr = this;
    lv.get_realtime_vars.func = &get_vars;
    lv.base_addr = 0x400000;

    FILE *fp = fopen(dasm.path, "r");
    if (!fp) {
        assert(1==2);
        return;
    }
    u32 max_len = 0;
    u32 last_addr = 0;
    u32 num_lines = 0;
    std::list<source_listing::symbol> waiting_for_addr{};
    while ((read = getline(&line, &len, fp)) != -1) {
        if (read > 0 && line[read-1] == '\n') line[read-1] = 0;
        if (read > max_len) max_len = read;
        char *tstart = seek_whitespace_end(line);
        // TODO: figure out why this isn't working for some blank lines
        if (!tstart || *tstart == ';') {
            auto &dl = lv.add_line(last_addr, source_listing::LK_OTHER, line);
            num_lines++;
            continue;
        }
        // Test if label
        char *colon = strchr(line, ':');
        if (colon != nullptr) {
            // See if it's a comment...
            char *semicolon = strchr(line, ';');
            if (!semicolon || semicolon > colon) {
                // Create our own symbol
                auto &s = waiting_for_addr.emplace_back();
                s.line.make(lv.lines, lv.lines.size());
                s.frt.sprintf("%s", line);
                *colon = 0;
                s.name.sprintf("%s", tstart);
                num_lines++;
                continue;
            }
        }
        // Parse out address
        // Up to the first 6 characters of the line will be addr, left-aligned

        char *eptr;
        u32 new_addr = strtoul(line, &eptr, 16);
        u32 my_addr = last_addr;
        if ((eptr - line) < 7) {
            if (new_addr > my_addr) my_addr = new_addr;
        }
        last_addr = my_addr;
        // Add symbols
        for (auto &s : waiting_for_addr) {
            lv.add_line(my_addr, source_listing::LK_LABEL, s.frt.ptr);
            lv.add_symbol(my_addr, s.line.index, s.name.ptr);
        }
        waiting_for_addr.clear();

        // Add line
        last_addr = my_addr;
        auto &l = lv.add_line(my_addr, source_listing::LK_CODE, line+7);

        num_lines++;
    }
    //printf("\nLINES:%d  MAX SIZE:%d", num_lines, max_len);
    fclose(fp);

    dasm.loaded_symbols = true;
}
}