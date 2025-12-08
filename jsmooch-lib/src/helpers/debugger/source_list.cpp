//
// Created by . on 12/7/25.
//

#include "debugger.h"

namespace source_listing {
line &view::add_line(u32 memaddr, line_kinds kind, char *txt)
{
    if (memaddr < range_start) range_start = memaddr;
    if (memaddr > range_end) range_end = memaddr;
    if ((!addr_to_line.contains(memaddr)) && kind == LK_CODE) {
        addr_to_line[memaddr] = lines.size();
    }
    auto &l = lines.emplace_back();
    l.text.sprintf("%s", txt);
    l.kind = kind;
    l.addr = memaddr;
    //l.text.sprintf("%s", txt);
    l.text.trim_right();
    return l;
}

symbol &view::add_symbol(u32 memaddr, u32 line, char *txt) {
    if (!addr_to_symbol.contains(memaddr)) {
        addr_to_symbol[memaddr] = symbols.size();
    }
    auto &s = symbols.emplace_back();
    s.line.make(lines, line);
    s.addr = memaddr;
    s.name.sprintf("%s", txt);
    s.name.trim_right();
    return s;
}

void view::finalize_adds() {
    // Symbols can have multiple lines, but should point to start of area (where symbol defined)
    // Multiple lines can point to same symbol, and each line's memaddr should also point to that symbol

    // First fill in symbol with line it was defined, if that exists...
    for (auto &l : lines) {
        if (addr_to_symbol.contains(l.addr))
            l.symbol.make(symbols, addr_to_symbol[l.addr]);
    }
}
}