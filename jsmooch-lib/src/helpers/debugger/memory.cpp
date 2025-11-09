//
// Created by . on 6/3/25.
//

#include "memory.h"

#include "helpers/cvec.h"
#include "events.h"


void memory_view::add_module(const char *name, u32 inid, u32 addr_digits, u32 range_start, u32 range_end, void *readptr, void (*readmem16func)(void *ptr, u32 addr, void *dest))
{
    memory_view_module &mm = modules.push_back;
    strcpy(mm.name, name);
    mm.id = inid;
    mm.addr_digits = addr_digits;
    mm.addr_start = range_start;
    mm.addr_end = range_end;
    mm.read_mem16 = readmem16func;
    mm.read_mem16_ptr = readptr;
}

u32 memory_view::num_modules() {
    return modules.size();
}

memory_view_module *memory_view::get_module(u32 id) {
    for (auto &mm : modules) {
        if (mm.id == id) return &mm;
    }
    return nullptr;
}

void memory_view_get_line(memory_view_module *mm, u32 addr, char *out)
{
    memset(out, 0, 16);
    if (!mm) return;
    addr &= 0xFFFFFFF0;
    mm->read_mem16(mm->read_mem16_ptr, addr, out);
}


