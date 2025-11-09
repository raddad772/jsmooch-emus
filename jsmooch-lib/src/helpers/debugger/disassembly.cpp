//
// Created by . on 8/11/24.
//
#include <cstring>
#include <cstdio>
#include <cassert>

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

#define is_range_dirty(r) (r.addr_range_start == -1)
#define is_srange_dirty (addr_range_start == -1)
#define DBG_DISASSEMBLE_MAX_BLOCK_SIZE 50


void disassembly_view::mark_range_invalid(disassembly_range &range, u32 index)
{
    range.valid = 0;

    for (auto &entry : range.entries) {
        entry.clear_for_reuse();
    }
    range.entries.clear();
    range.addr_range_end = -1;
    range.addr_range_start = -1;
    range.addr_of_next_ins = -1;
    dirty_range_indices.emplace_back(index);
}


int disassembly_range::collides(u32 addr_start, u32 addr_end)
{
    // So we want to check...
    if (is_srange_dirty) return 0; // If it's dirty, it doesn't collide

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

    u32 whole_of_range_before = ((addr_range_start < addr_start) && (addr_range_end < addr_start));
    u32 whole_of_range_after = ((addr_range_start > addr_end) && (addr_range_end > addr_end));

    return !((whole_of_range_before) || (whole_of_range_after));
}

void disassembly_view::dirty_mem(u32 mem_bus, u32 addr_start, u32 addr_end)
{
    for (size_t i = 0; i < ranges.size(); i++) {
        auto &range = ranges[i];
        if (!is_range_dirty(range) && range.collides(addr_start, addr_end)) {
            mark_range_invalid(range, i);
        }
    }}

disassembly_view::disassembly_view()
{
    ranges.reserve(100);
    cpu.regs.reserve(32);
    dirty_range_indices.reserve(100);
}

void disassembly_entry::clear_for_reuse()
{
    assert(dasm.ptr);
    *dasm.ptr = 0;
    dasm.cur = dasm.ptr;

    *context.ptr = 0;
    context.cur = context.ptr;

    addr = -1;
    ins_size_bytes = 0;
}

static void w_entry_to_strs(disassembly_entry_strings *strs, disassembly_entry *entry, int col_size)
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

void disassembly_range::mark_block_dirty()
{
    addr_range_start = addr_range_end = -1;
    valid = 0;
    entries.clear();
}

disassembly_range *disassembly_view::find_range_including(u32 instruction_addr)
{
    for (auto &r : ranges) {
        if (!is_range_dirty(r) && (r.addr_range_start <= instruction_addr) &&
            (r.addr_range_end >= instruction_addr)) {
            for (auto &e : r.entries) {
                if (e.addr == instruction_addr) return &r;
            }
            r.mark_block_dirty();
            return nullptr;
        }
    }
    return nullptr;
}

disassembly_range *disassembly_view::find_next_range(u32 what_addr) {
    i64 lowest_addr = -1;
    disassembly_range *lowest_r = nullptr;
    // Only called if there is no CURRENT range
    for (auto &r : ranges) {
        if ((!is_range_dirty(r)) && (r.addr_range_start > what_addr)) {
            if (r.addr_range_start < lowest_addr) {
                lowest_addr = r.addr_range_start;
                lowest_r = &r;
            }
        }
    }
    return lowest_r; // returns nullptr if none found
}

#define CVEC_FOREACH(iterval, cvec, struct_type, iterator) for (u32 iterval = 0; iterval < cvec_len(&cvec); iterval++) {\
    struct_type * iterator = (struct_type *)cvec_get(&cvec, iterval)

#define CVEC_FOREACH_END }

cvec_ptr<disassembly_range> disassembly_view::get_range()
{
    // Get either a dirty range to re-use, or a new range
    cvec_ptr<disassembly_range> a;
    u32 *dp = nullptr;
    if (dirty_range_indices.size() > 0)
        dp = &dirty_range_indices.back();
    if (dp) {
        a.make(ranges, *dp);
        disassembly_range &r = ranges.at(*dp);
        assert(r.valid == 0);
        r.valid = 1;
    }
    else {
        a.make(ranges, ranges.size());
    }
    return a;
}

disassembly_range *disassembly_view::create_diassembly_block(u32 range_start, u32 range_end)
{
    return nullptr;
}

int disassembly_view::get_rows(u32 instruction_addr, u32 bytes_before, u32 total_lines, std::vector<disassembly_entry_strings> &out_lines) {
    for (auto &[addr, dasm, context] : out_lines) {
        addr[0] = dasm[0] = context[0] = 0;
    }
    return 0;
}


void cpu_reg_context::render(char* outbuf, size_t outbuf_sz) {
    if (custom_render) {
        custom_render(this, outbuf, outbuf_sz);
    }
    else {
        switch(kind) {
            case RK_bitflags:
                snprintf(outbuf, outbuf_sz, "OOPS!");
                break;
            case RK_bool:
                snprintf(outbuf, outbuf_sz, "%s", bool_data ? "true" : "false");
                break;
            case RK_int8:
                snprintf(outbuf, outbuf_sz, "%02x", static_cast<u32>(int8_data));
                break;
            case RK_int16:
                snprintf(outbuf, outbuf_sz, "%04x", static_cast<u32>(int16_data));
                break;
            case RK_int32:
                snprintf(outbuf, outbuf_sz, "%08x", static_cast<u32>(int32_data));
                break;
            case RK_float:
                snprintf(outbuf, outbuf_sz, "%f", float_data);
                break;
            case RK_double:
                snprintf(outbuf, outbuf_sz, "%f", double_data);
                break;
        }
    }

}
