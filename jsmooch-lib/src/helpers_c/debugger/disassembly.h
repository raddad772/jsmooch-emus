//
// Created by . on 8/11/24.
//

#ifndef JSMOOCH_EMUS_DISASSEMBLY_H
#define JSMOOCH_EMUS_DISASSEMBLY_H

void disassembly_view_dirty_mem(struct debugger_interface *dbgr, struct disassembly_view *dview, u32 mem_bus, u32 addr_start, u32 addr_end);
void disassembly_view_delete(struct disassembly_view *);
void cpu_reg_context_delete(struct cpu_reg_context *);

void disassembly_view_init(struct disassembly_view *);
void cpu_reg_context_init(struct cpu_reg_context *);
void disassembly_range_delete(struct disassembly_range *);
void disassembly_range_init(struct disassembly_range *);
void disassembly_entry_delete(struct disassembly_entry*);
void disassembly_entry_clear_for_reuse(struct disassembly_entry*);
void disassembly_entry_init(struct disassembly_entry *);



#endif //JSMOOCH_EMUS_DISASSEMBLY_H
