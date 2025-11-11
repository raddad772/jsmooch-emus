//
// Created by . on 6/3/25.
//

#include "memory.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/cvec.h"
#include "helpers/ooc.h"
#include "helpers/physical_io.h"
#include "events.h"

void memory_view_init(struct memory_view *mv)
{
    memset(mv, 0, sizeof(*mv));
    cvec_init(&mv->modules, sizeof(struct memory_view_module), 50);
}

void memory_view_delete(struct memory_view *mv)
{
    cvec_delete(&mv->modules);
}

void memory_view_add_module(struct debugger_interface *dbgr, memory_view *mv, const char *name, u32 id, u32 addr_digits, u32 range_start, u32 range_end, void *readptr, void (*readmem16func)(void *ptr, u32 addr, void *dest))
{
    struct memory_view_module *mm = (struct memory_view_module *)cvec_push_back(&mv->modules);
    strcpy(mm->name, name);
    mm->id = id;
    mm->addr_digits = addr_digits;
    mm->addr_start = range_start;
    mm->addr_end = range_end;
    mm->read_mem16 = readmem16func;
    mm->read_mem16_ptr = readptr;
}

u32 memory_view_num_modules(struct memory_view *mv)
{
    return cvec_len(&mv->modules);
}

struct memory_view_module *memory_view_get_module(struct memory_view *mv, u32 id) {
    for (u32 i = 0; i < cvec_len(&mv->modules); i++) {
        struct memory_view_module *mm = cvec_get(&mv->modules, i);
        if (mm->id == id) return mm;
    }
    return NULL;
}

void memory_view_get_line(struct memory_view_module *mm, u32 addr, char *out)
{
    memset(out, 0, 16);
    if (!mm) return;
    addr &= 0xFFFFFFF0;
    mm->read_mem16(mm->read_mem16_ptr, addr, out);
}


