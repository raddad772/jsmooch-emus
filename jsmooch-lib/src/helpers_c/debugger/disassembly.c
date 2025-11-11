//
// Created by . on 8/11/24.
//
#include <string.h>
#include <stdio.h>

#include "helpers/ooc.h"

#include "debugger.h"
#include "disassembly.h"

/* The problem: represent all of RAM with pre-disassembled sections
 * Only assemble as we come across them, add on to some sections
 * Mark sections as dirty (such as due to memory banking)
 *
 * So we get a request for x-20 to x+80.
 * We fill it in.
 * Now we get a request for x-19 to x+81
 * Extend the previous one or create a new 1-instruction block?
 * Perhaps a max block size, and do it up to there?
 */

#define is_range_dirty(r) ((r)->addr_range_start == -1)
#define DBG_DISASSEMBLE_MAX_BLOCK_SIZE 50


static void mark_disassembly_range_invalid(struct disassembly_view *dview, disassembly_range *range, u32 index)
{
    range->valid = 0;

    for (u32 j = 0; j < cvec_len(&range->entries); j++) {
        disassembly_entry_clear_for_reuse(cvec_get(&range->entries, j));
    }
    cvec_clear(&range->entries);
    range->addr_range_end = -1;
    range->addr_range_start = -1;
    range->addr_of_next_ins = -1;
    u32 *dp = cvec_push_back(&dview->dirty_range_indices);
    *dp = index;
}


static int range_collides(struct disassembly_range *range, u32 addr_start, u32 addr_end)
{
    // So we want to check...
    if (is_range_dirty(range)) return 0; // If it's dirty, it doesn't collide

    // Whole of range is BEFORE check
    // WHole of range is AFTER check
    // If neither of those is true, it collides!

    // #1                5, 10    before?    after?   intersect?
    // compare...        1, 4     yes        no       no
    //                   1, 7     no         no       yes
    //                   4, 6     no         no       yes
    //                   5, 6     no         no       yes
    //                   4, 15    no         no       yes
    //                  10, 12    no         no       yes
    //                  11, 15    no         yes      no

    u32 whole_of_range_before = ((range->addr_range_start < addr_start) && (range->addr_range_end < addr_start));
    u32 whole_of_range_after = ((range->addr_range_start > addr_end) && (range->addr_range_end > addr_end));

    return !((whole_of_range_before) || (whole_of_range_after));
}

void disassembly_view_dirty_mem(struct debugger_interface *dbgr, disassembly_view *dview, u32 mem_bus, u32 addr_start, u32 addr_end)
{
    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        struct disassembly_range *range = cvec_get(&dview->ranges, i);
        // If the addr range start or end are in ours...
        if ((!is_range_dirty(range)) && range_collides(range, addr_start, addr_end)) {// TODO: add all colision cases
            mark_disassembly_range_invalid(dview, range, i);
        }
    }
}

void disassembly_view_init(struct disassembly_view *this)
{
    memset(this, 0, sizeof(*this));
    cvec_init(&this->ranges, sizeof(struct disassembly_range), 100);
    cvec_init(&this->cpu.regs, sizeof(struct cpu_reg_context), 32);
    cvec_init(&this->dirty_range_indices, sizeof(u32), 100);
    jsm_string_init(&this->processor_name, 40);
}

void disassembly_entry::clear_for_reuse()
{
    assert(dasm.ptr);
    *dasm.ptr = 0;
    dasm.cur = this->dasm.ptr;

    *context.ptr = 0;
    context.cur = this->context.ptr;

    addr = -1;
    ins_size_bytes = 0;
}

void disassembly_entry_delete(struct disassembly_entry* this)
{
    jsm_string_delete(&this->dasm);
    jsm_string_delete(&this->context);
    this->addr = 0;
}

void disassembly_range_init(struct disassembly_range *this)
{
    CTOR_ZERO_SELF;
    cvec_init(&this->entries, sizeof(struct disassembly_range), 20);
}

void disassembly_range_delete(struct disassembly_range *this)
{
    DTOR_child_cvec(entries, disassembly_entry)
}

void cpu_reg_context_init(struct cpu_reg_context *this)
{
    CTOR_ZERO_SELF;
}

void cpu_reg_context_delete(struct cpu_reg_context *this)
{
}

void disassembly_view_delete(struct disassembly_view *this)
{
    DTOR_child_cvec(ranges, disassembly_range);
    DTOR_child_cvec(cpu.regs, cpu_reg_context)
}

static void w_entry_to_strs(struct disassembly_entry_strings *strs, disassembly_entry *entry, int col_size)
{
    switch(col_size) {
        case 4:
            snprintf(strs->addr, 100, "%04x", entry->addr);
            break;
        case 6:
            snprintf(strs->addr, 100, "%06x", entry->addr);
            break;
        default:
            assert(1==2);
    }

    snprintf(strs->dasm, 200, "%s", entry->dasm.ptr);
    snprintf(strs->context, 400, "%s", entry->context.ptr);
}

static void mark_block_dirty(struct disassembly_range *r)
{
    r->addr_range_start = r->addr_range_end = -1;
    r->valid = 0;
    struct disassembly_range *this = r;
    DTOR_child_cvec(entries, disassembly_entry);
}

/*static int is_range_dirty(struct disassembly_range *r)
{
    return r->addr_range_start == -1;
}*/

static struct disassembly_range *find_range_including(struct disassembly_view *dview, u32 instruction_addr)
{
    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        struct disassembly_range *r = (struct disassembly_range *) cvec_get(&dview->ranges, i);
        if (!is_range_dirty(r) && (r->addr_range_start <= instruction_addr) &&
            (r->addr_range_end >= instruction_addr)) {
            for (u32 j = 0; j < cvec_len(&r->entries); j++) {
                struct disassembly_entry *e = (struct disassembly_entry *) cvec_get(&r->entries, j);
                if (e->addr == instruction_addr) return r;
            }
            mark_block_dirty(r);
            return NULL;
        }
    }
    return NULL;
}

static struct disassembly_range *find_next_range(struct disassembly_view *dview, u32 what_addr) {
    i64 lowest_addr = -1;
    struct disassembly_range *lowest_r = NULL;
    // Only called if there is no CURRENT range
    for (u32 i = 0; i < cvec_len(&dview->ranges); i++) {
        struct disassembly_range *r = cvec_get(&dview->ranges, i);

        if ((!is_range_dirty(r)) && (r->addr_range_start > what_addr)) {
            if (r->addr_range_start < lowest_addr) {
                lowest_addr = r->addr_range_start;
                lowest_r = r;
            }
        }
    }
    return lowest_r; // returns NULL if none found
}

#define CVEC_FOREACH(iterval, cvec, struct_type, iterator) for (u32 iterval = 0; iterval < cvec_len(&cvec); iterval++) {\
    struct struct_type * iterator = (struct struct_type *)cvec_get(&cvec, iterval)

#define CVEC_FOREACH_END }

static struct disassembly_range *get_range(struct disassembly_view *dview)
{
    // Get either a dirty range to re-use, or a new range
    struct disassembly_range *r = NULL;

    u32 *dp = cvec_pop_back(&dview->dirty_range_indices);
    if (dp) {
        r = cvec_get(&dview->ranges, *dp);
        assert(r->valid == 0);
        r->valid = 1;
    }
    else {
        r = cvec_push_back(&dview->ranges);
        disassembly_range_init(r);
    }
    return r;
}

static struct disassembly_range *create_diassembly_block(struct debugger_interface *di, disassembly_view *dview, u32 range_start, u32 range_end)
{
    assert(range_start<(dview->mem_end+1));
    u32 cur_addr = range_start;
    struct disassembly_range *r = get_range(dview);
    r->addr_range_start = range_start;
    r->addr_range_end = range_end;
    r->valid = 1;
    struct disassembly_entry *last_entry = NULL;
    while(true) {
        struct disassembly_entry *entry = cvec_push_back(&r->entries);
        if (cvec_len(&r->entries) > r->num_entries_previously_made) { // If we've added an entry that has not been init yet...
            r->num_entries_previously_made++;
            disassembly_entry_init(entry); // Init it!
        }
        entry->addr = cur_addr;
        //printf("\nDISASSEMBLE %04x", entry->addr);
        dview->get_disassembly.func(dview->get_disassembly.ptr, di, dview, entry);
        cur_addr += entry->ins_size_bytes;
        assert(cur_addr>range_start);
        if (cur_addr > range_end) break;
    }
    r->addr_of_next_ins = cur_addr;
    return r;
}

int disassembly_view_get_rows(struct debugger_interface *di, disassembly_view *dview, u32 instruction_addr, u32 bytes_before, u32 total_lines, cvec *out_lines) {
    for (u32 i = 0; i < cvec_len(out_lines); i++) {
        struct disassembly_entry_strings *strs = cvec_get(out_lines, i);
        strs->addr[0] = strs->dasm[0] = strs->context[0] = 0;
    }

    // So we get an address,
    // dasm_rows comes out as struct disassembly_entry_strings
    //
    u32 line_of_current_instruction = 0;
    u32 num_rows = 0;

    u32 cur_search_addr = instruction_addr - bytes_before;
    if (bytes_before > instruction_addr) cur_search_addr = 0;

    // 0. set addr to place - start
    // A. Attempt to find block with addr
    // B. If found, copy relavent entries and repeat 1) with next address outside the block
    // C. If not found, find next closest block:
    //  1a) If closest block buts up against it, use that as a boundary
    //  1b) Otherwise, use MAX_BLOCK_SIZE
    //  2) Now, search for next address outside block
    struct disassembly_range *next_loop_r = NULL;
    u32 none_found = 0;

    while(true) {
        struct disassembly_range *fr = NULL;

        if (next_loop_r) fr = next_loop_r;
        else fr = none_found ? NULL : find_range_including(dview, cur_search_addr);

        next_loop_r = NULL;
        // If not found...
        if (fr == NULL) { // Attempt to start diassembly
            // Find next
            struct disassembly_range *nextr = none_found ? NULL : find_next_range(dview, cur_search_addr);
            none_found = nextr == NULL;
            u32 range_start = cur_search_addr;
            u32 range_end = 0;

            if (!nextr) { // If next range isn't found...
                none_found = 1;
                // Check if we're before the instruction addr. IF we are, we must skip to it and start over
                if (cur_search_addr < instruction_addr) {
                    cur_search_addr = instruction_addr;
                    continue;
                }
                // If we're not, we can start disassembly here.
                range_end = range_start + DBG_DISASSEMBLE_MAX_BLOCK_SIZE;
            }
            else { // We have a next range
                // Check if WE are < our start range...
                if (cur_search_addr < instruction_addr) {
                    // Just jump and restart, we can't start here
                    cur_search_addr = nextr->addr_range_start;
                    next_loop_r = nextr;
                    continue;
                }
                else { // We are >= our start range, so we can start where we are
                    range_end = nextr->addr_range_start - 1;
                }
            }

            // Check if it's > MAX_BLOCK_SIZE away, set new block size to that or MAX_BLOCK_SIZE
            u32 range_len = range_end - range_start;
            if (range_len > DBG_DISASSEMBLE_MAX_BLOCK_SIZE) range_len = DBG_DISASSEMBLE_MAX_BLOCK_SIZE;
            else {
                if (nextr) next_loop_r = nextr; // SUSPECT???
            }
            if (range_end > dview->mem_end) range_end = dview->mem_end;

            // Disassemble and create block!
            fr = create_diassembly_block(di, dview, range_start, range_end);
        }

        // At this point, we have a block, so add it in until we hit limit of address
        for (u32 i = 0; i < cvec_len(&fr->entries); i++) {
            struct disassembly_entry *e = cvec_get(&fr->entries, i);
            if (e->addr < cur_search_addr) continue; // Skip if we're in the block before current address

            struct disassembly_entry_strings *es = cvec_push_back(out_lines);

            if (dview->print_addr.func) {
                dview->print_addr.func(dview->print_addr.ptr, e->addr, es->addr, sizeof(es->addr));
            }
            else {
                snprintf(es->addr, sizeof(es->addr), "%04x", e->addr);
            }
            snprintf(es->dasm, sizeof(es->dasm), "%s", e->dasm.ptr);
            snprintf(es->context, sizeof(es->context), "%s", e->context.ptr);

            if (e->addr == instruction_addr) line_of_current_instruction = num_rows;

            num_rows++;
            if (num_rows >= total_lines) break;

            cur_search_addr = e->addr + e->ins_size_bytes;
            if (cur_search_addr > dview->mem_end) break;
        }
        if (num_rows >= total_lines) break;
        if (cur_search_addr > dview->mem_end) break;
    }

    return line_of_current_instruction;
}


void cpu_reg_context_render(struct cpu_reg_context *ctx, char* outbuf, size_t outbuf_sz) {
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
