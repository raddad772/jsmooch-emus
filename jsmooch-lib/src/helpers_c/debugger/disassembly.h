//
// Created by . on 8/11/24.
//

#ifndef JSMOOCH_EMUS_DISASSEMBLY_H
#define JSMOOCH_EMUS_DISASSEMBLY_H

void disassembly_view_dirty_mem(debugger_interface *dbgr, disassembly_view *dview, u32 mem_bus, u32 addr_start, u32 addr_end);
void disassembly_view_delete(disassembly_view *);
void cpu_reg_context_delete(cpu_reg_context *);

void disassembly_view_init(disassembly_view *);
void cpu_reg_context_init(cpu_reg_context *);
void disassembly_range_delete(disassembly_range *);
void disassembly_range_init(disassembly_range *);
void disassembly_entry_delete(disassembly_entry*);
void disassembly_entry_clear_for_reuse(disassembly_entry*);
void disassembly_entry_init(disassembly_entry *);



#endif //JSMOOCH_EMUS_DISASSEMBLY_H
