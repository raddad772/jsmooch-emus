//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_ARM7TDMI_H
#define JSMOOCH_EMUS_ARM7TDMI_H

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "helpers/int.h"
#include "helpers/cvec.h"

struct ARM7TDMI_memaccess_t {
    void *ptr;
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 *success);
    void (*write)(void *ptr, u32 addr, u32 val, u32 sz, u32 *success);
};

struct ARM7TDMI {

    u32 pipeline[2];


    struct {
        struct jsm_debug_read_trace strct;
        struct jsm_string str;
        u32 ok;
        u64 *cycles;
        u32 ins_PC;
    } trace;

};


void ARM7TDMI_init(struct ARM7TDMI *);
void ARM7TDMI_delete(struct ARM7TDMI *);

void ARM7TDMI_reset(struct ARM7TDMI *);
void ARM7TDMI_disassemble_entry(struct ARM7TDMI*, struct disassembly_entry* entry);
void ARM7TDMI_setup_tracing(struct ARM7TDMI*, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer);
void ARM7TDMI_cycle(struct ARM7TDMI*, u32 num);
#endif //JSMOOCH_EMUS_ARM7TDMI_H
