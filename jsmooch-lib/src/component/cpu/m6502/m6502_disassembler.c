//
// Created by Dave on 2/4/2024.
//
#include "m6502.h"
#include "m6502_disassembler.h"


void M6502_disassemble(u32 *PC, struct jsm_debug_read_trace *trace, struct jsm_string *outstr)
{

}

void M6502_disassemble_entry(struct M6502 *this, struct disassembly_entry* entry)
{
    u16 IR = this->trace.strct.read_trace(this->trace.strct.ptr, entry->addr);
    u16 opcode = IR;
    jsm_string_quickempty(&entry->dasm);
    jsm_string_quickempty(&entry->context);
    u32 PC = entry->addr+1;
    M6502_disassemble(&PC, &this->trace.strct, &entry->dasm);
    entry->ins_size_bytes = PC - entry->addr;
}
