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

/*
 So, something requests disassembly
*/

disassembly_view::disassembly_view() {

}


void disassembly_view::dirty_mem(u32 mem_bus, u32 addr_start, u32 addr_end)
{
    // Current version this does nothing!
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


int disassembly_view::get_rows(u32 instruction_addr, u32 bytes_before, u32 total_lines, std::vector<disassembly_entry_strings> &out_lines) {
    for (auto &[addr, dasm, context] : out_lines) {
        addr[0] = dasm[0] = context[0] = 0;
    }
    assert(mem_end!=0);
    if (!adv_get_rows.func) {
        // Just do the disassembly pretty much, no fancy block basis now!
        u32 num_rows = 0;
        u32 cur_addr = instruction_addr;
        u32 idx_in_rows = 0;
        disassembly_entry e;
        while (num_rows < total_lines) {
            if (idx_in_rows >= out_lines.size()) {
                out_lines.emplace_back();
            }
            auto &entrystr = out_lines.at(idx_in_rows);
            idx_in_rows++;
            num_rows++;
            e.clear_for_reuse();
            e.addr = cur_addr;
            get_disassembly.func(get_disassembly.ptr, *this, e);
            print_addr.func(print_addr.ptr, cur_addr, entrystr.addr, sizeof(entrystr.addr));
            snprintf(entrystr.dasm, sizeof(entrystr.dasm), "%s", e.dasm.ptr);
            if (has_context) snprintf(entrystr.context, sizeof(entrystr.context), "%s", e.context.ptr);
            cur_addr += e.ins_size_bytes;
            if (cur_addr > mem_end) cur_addr = 0;
        }
    }
    else {
        printf("\nIMPLEMENT ADVANCED DASM!");
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
            case RK_int4:
                snprintf(outbuf, outbuf_sz, "%01x", static_cast<u32>(int4_data));
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
