
#pragma once

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

struct jsm_debug_read_trace;

namespace CDP1802 {

struct core;
union ctxt {
    struct {
        u32 X : 1;
        u32 P : 1;
        u32 N : 1;
        u32 D : 1;
        u32 _blank : 12;
        u32 registers : 16;
    };
    u32 u{};
};

void disassemble_entry(core*, disassembly_entry& entry);
void disassemble(u16 &PC, jsm_debug_read_trace &trace, jsm_string &outstr, ctxt &ctx);
}
