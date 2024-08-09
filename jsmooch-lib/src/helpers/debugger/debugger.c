//
// Created by . on 8/7/24.
//

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "debugger.h"

static void mark_disassembly_range_invalid(struct disassembly_range *range)
{
    range->valid = 0;
    for (u32 j = 0; j < cvec_len(&range->entries); j++) {
        disassembly_entry_delete(cvec_get(&range->entries, j));
    }

    cvec_clear(&range->entries);
    range->addr_range_end = -1;
    range->addr_range_start = -1;
}


static void disassembly_view_dirty_mem(struct debugger_interface *dbgr, struct disassembly_view *dview, u32 mem_bus, u32 addr_start, u32 addr_end)
{
    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        struct disassembly_range *range = cvec_get(&dview->ranges, i);
        // If the addr range start or end are in ours...
        if ((range->valid && ((range->addr_range_start >= addr_start) && (range->addr_range_start <= addr_end)) ||
                ((range->addr_range_end >= addr_start) && (range->addr_range_end <= addr_end)))) {
            mark_disassembly_range_invalid(range);
        }
    }
}

void debugger_interface_dirty_mem(struct debugger_interface *dbgr, u32 mem_bus, u32 addr_start, u32 addr_end)
{
    if (dbgr == NULL) return;
    for (u32 i = 0; i < cvec_len(&dbgr->views); i++) {
        struct debugger_view *dv = cvec_get(&dbgr->views, i);
        switch(dv->kind) {
            case dview_null:
                break;
            case dview_events:
                assert(1==0);
                break;
            case dview_memory:
                assert(1==0);
                break;
            case dview_disassembly: {
                struct disassembly_view *dview = &dv->disassembly;
                disassembly_view_dirty_mem(dbgr, dview, mem_bus, addr_start, addr_end);
                break; }
        }
    }
}

void disassembly_view_init(struct disassembly_view *this)
{
    memset(this, 0, sizeof(*this));
    cvec_init(&this->ranges, sizeof(struct disassembly_range), 100);
    cvec_init(&this->cpu.regs, sizeof(struct cpu_reg_context), 32);
    jsm_string_init(&this->processor_name, 40);
}


#define DTOR_cvec_child(getfrom, kname) for (u32 i = 0; i < cvec_len(&this-> getfrom); i++) {\
                                    kname##_delete(cvec_get(&this-> getfrom, i));\
                                    }\
                                    cvec_delete(&this-> getfrom);

#define ZERO_SELF memset(this, 0, sizeof(*this))

void disassembly_entry_init(struct disassembly_entry *this)
{
    ZERO_SELF;
    jsm_string_init(&this->dasm, 100);
    jsm_string_init(&this->context, 100);
}

void disassembly_entry_delete(struct disassembly_entry* this)
{
    jsm_string_delete(&this->dasm);
    jsm_string_delete(&this->context);
}

void disassembly_range_init(struct disassembly_range *this)
{
    ZERO_SELF;
    cvec_init(&this->entries, sizeof(struct disassembly_range), 10);
}

void disassembly_range_delete(struct disassembly_range *this)
{
    DTOR_cvec_child(entries, disassembly_entry)
}

void cpu_reg_context_init(struct cpu_reg_context *this)
{
    ZERO_SELF;
}

void cpu_reg_context_delete(struct cpu_reg_context *this)
{
}

void disassembly_view_delete(struct disassembly_view *this)
{
    DTOR_cvec_child(ranges, disassembly_range);
    DTOR_cvec_child(cpu.regs, cpu_reg_context)
}

void debugger_view_init(struct debugger_view *this, enum debugger_view_kinds kind)
{
    ZERO_SELF;
    this->kind = kind;
    switch(this->kind) {
        case dview_null:
            assert(1==2);
            break;
        case dview_disassembly:
            disassembly_view_init(&this->disassembly);
            break;
        case dview_events:
            assert(1==2);
            break;
        case dview_memory:
            assert(1==2);
            break;
        default:
            assert(1==2);
    }
}

void debugger_view_delete(struct debugger_view *this)
{
    switch(this->kind) {
        case dview_null:
            assert(1==2);
            break;
        case dview_disassembly:
            disassembly_view_delete(&this->disassembly);
            break;
        case dview_events:
            assert(1==2);
            break;
        case dview_memory:
            assert(1==2);
            break;
        default:
            assert(1==2);
    }
}

void debugger_interface_init(struct debugger_interface *this)
{
    ZERO_SELF;
    cvec_init(&this->views, sizeof(struct debugger_view), 5);
}

void debugger_interface_delete(struct debugger_interface *this)
{
    DTOR_cvec_child(views, debugger_view);
}

static struct disassembly_range* find_intersecting_range(struct disassembly_view *dview, u32 what)
{
    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        struct disassembly_range *r = cvec_get(&dview->ranges, i);
        if (r->valid && (r->addr_range_start <= what) && (r->addr_range_end >= what)) return r;
    }
    return NULL;
}

static void w_entry_to_strs(struct disassembly_entry_strings *strs, struct disassembly_entry *entry)
{
    snprintf(strs->addr, 100, "%06x", entry->addr);
    snprintf(strs->dasm, 200, "%s", entry->dasm.ptr);
    snprintf(strs->context, 400, "%s", entry->context.ptr);
}

static struct disassembly_range* find_next_range(struct disassembly_view *dview, u32 search_addr)
{
    struct disassembly_range* closest=NULL;
    u32 current_distance = 0xFFFFFFFF;

    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        struct disassembly_range *range = cvec_get(&dview->ranges, i);
        // Check intersection
        if (!range->valid) continue;

        if ((range->addr_range_start >= search_addr) && (range->addr_range_end <= search_addr)) {
            return range;
        }

        // It must be either before or after now. So check for before, which we don't want...
        if (range->addr_range_end < search_addr) continue;

        // It's after, so determine distance...
        u32 distance = range->addr_range_start - search_addr;
        if (distance < current_distance) {
            current_distance = distance;
            closest = range;
        }
    }
    return closest;
}

static struct disassembly_range *create_disassembly_range(struct debugger_interface *di, struct disassembly_view *dview, u32 start_addr, u32 assemble_til)
{
    // First check for empty ranges...

    struct disassembly_range *range = NULL;

    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        range = cvec_get(&dview->ranges, i);
        if (range->valid)
            break;
        range = NULL;
    }

    if (range == NULL) {
        range = cvec_push_back(&dview->ranges);
        disassembly_range_init(range);
    }
    range->valid = 1;

    // Disassemble, and push entries back
    u32 asm_addr = start_addr;
    range->addr_range_start = start_addr;

    while(asm_addr < assemble_til) {
        struct disassembly_entry* entry = cvec_push_back(&range->entries);
        disassembly_entry_init(entry);
        entry->addr = asm_addr;
        // DO DISASSEMBLY
        dview->get_disassembly.func(dview->get_disassembly.ptr, di, dview, entry);
        // FINISH UP
        asm_addr += entry->ins_size_bytes;
        range->addr_range_end = asm_addr-1;
    }
    assert(range->addr_range_end > 0);
    return range;
}

int disassembly_view_get_rows(struct debugger_interface *di, struct disassembly_view *dview, u32 instruction_addr, u32 bytes_before, u32 bytes_after, struct cvec *out_lines)
{
    // ex. situation.
    // we have current instr = 0x100
    // we have a range, 70...90
    // another 110...120 and 120...135
    // 70-89, 110-120, 120-135
    // we then need, 90-109 and 136-139, plus the stuff we have

    // first find earliest intersecting range
    // then find one that intersects iwth current. if it doesn't exist, create it by finding next intersect and going to it
    // etc. until done
    int line_of_current_instruction = 0;

    for (u32 i = 0; i < cvec_len(out_lines); i++) {
        struct disassembly_entry_strings *strs = cvec_get(out_lines, i);
        strs->addr[0] = strs->dasm[0] = strs->context[0] = 0;
    }
    u32 line = 0;

    // except we CANNOT CREATE if we're before the current instruction.
    u32 asm_addr = instruction_addr - bytes_before;
    u32 end_addr = instruction_addr + bytes_after;


    struct disassembly_range *intersecting = NULL;
    while(asm_addr < end_addr) {
        if (intersecting == NULL)
            intersecting = find_intersecting_range(dview, asm_addr);
        if (intersecting != NULL) {
            // We got some lines!
            u32 found_any = 0;
            for (u32 i = 0; i < cvec_len(&intersecting->entries); i++) {
                struct disassembly_entry *entry = cvec_get(&intersecting->entries, i);
                if (entry->addr == instruction_addr)
                    line_of_current_instruction = (int)line;
                if (entry->addr >= asm_addr) {
                    found_any = 1;
                    struct disassembly_entry_strings *strs = cvec_get(out_lines, line++);
                    asm_addr = entry->addr + entry->ins_size_bytes;
                    w_entry_to_strs(strs, entry);
                }
            }
            if (!found_any) mark_disassembly_range_invalid(intersecting);
            intersecting = NULL;
            continue;
        }
        if (asm_addr < instruction_addr) {
            asm_addr = instruction_addr;
            intersecting = NULL;
            continue;
        }
        // If we're >= instruction_addr, we should be able to create a new range and disassemble.
        // First, find our next range
        struct disassembly_range *next_range = find_next_range(dview, asm_addr);
        u32 assemble_til = end_addr;
        if (next_range != NULL)
            assemble_til = next_range->addr_range_start - 1;

        intersecting = create_disassembly_range(di, dview, asm_addr, assemble_til);
        // Now loop around and we'll find the intersecting range already and fill up them strs!!
    }

    return line_of_current_instruction;
}


void cpu_reg_context_render(struct cpu_reg_context*ctx, char* outbuf, size_t outbuf_sz) {
    if (ctx->custom_render) {
        ctx->custom_render(ctx, outbuf, outbuf_sz);
    }
    else {
        switch(ctx->kind) {
            case RK_bitflags:
                snprintf(outbuf, outbuf_sz, "OOPS!");
                break;
            case RK_bool:
                snprintf(outbuf, outbuf_sz, "%s", ctx->bool_data ? "true" : "false");
                break;
            case RK_int8:
                snprintf(outbuf, outbuf_sz, "%02x", (u32)ctx->int8_data);
                break;
            case RK_int16:
                snprintf(outbuf, outbuf_sz, "%04x", (u32)ctx->int16_data);
                break;
            case RK_int32:
                snprintf(outbuf, outbuf_sz, "%08x", (u32)ctx->int32_data);
                break;
            case RK_float:
                snprintf(outbuf, outbuf_sz, "%f", ctx->float_data);
                break;
            case RK_double:
                snprintf(outbuf, outbuf_sz, "%f", ctx->double_data);
                break;
        }
    }

}
