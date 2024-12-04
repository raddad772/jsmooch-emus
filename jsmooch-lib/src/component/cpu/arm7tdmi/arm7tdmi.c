//
// Created by . on 12/4/24.
//

#include "arm7tdmi.h"

void ARM7TDMI_init(struct ARM7TDMI *this)
{

}

void ARM7TDMI_delete(struct ARM7TDMI *this)
{

}

void ARM7TDMI_reset(struct ARM7TDMI *this)
{

}

void ARM7TDMI_setup_tracing(struct ARM7TDMI* this, struct jsm_debug_read_trace *strct, u64 *trace_cycle_pointer)
{
    this->trace.strct.read_trace_m68k = strct->read_trace_m68k;
    this->trace.strct.ptr = strct->ptr;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
    this->trace.cycles = trace_cycle_pointer;
}

void ARM7TDMI_disassemble_entry(struct ARM7TDMI *this, struct disassembly_entry* entry)
{
    u16 IR = this->trace.strct.read_trace_m68k(this->trace.strct.ptr, entry->addr, 1, 1);
    u16 opcode = IR;
    jsm_string_quickempty(&entry->dasm);
    jsm_string_quickempty(&entry->context);
    /*struct M68k_ins_t *ins = &M68k_decoded[opcode];
    u32 mPC = entry->addr+2;
    ins->disasm(ins, &mPC, &this->trace.strct, &entry->dasm);
    entry->ins_size_bytes = mPC - entry->addr;
    struct M68k_ins_t *t =  &M68k_decoded[IR & 0xFFFF];
    pprint_ea(this, t, 0, &entry->context);
    pprint_ea(this, t, 1, &entry->context);*/
}

void ARM7TDMI_cycle(struct ARM7TDMI*, u32 num)
{

}