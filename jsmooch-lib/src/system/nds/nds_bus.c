//
// Created by . on 12/4/24.
//
#include <string.h>

#include "nds_bus.h"
#include "helpers/multisize_memaccess.c"

static u32 timer_reload_ticks(u32 reload)
{
    // So it overflows at 0x100
    // reload value is 0xFD
    // 0xFD ^ 0xFF = 2
    // How many ticks til 0x100? 256 - 253 = 3, correct!
    // 100. 256 - 100 = 156, correct!
    // Unfortunately if we set 0xFFFF, we need 0x1000 tiks...
    // ok but what about when we set 255? 256 - 255 = 1 which is wrong
    if (reload == 0xFFFF) return 0x10000;
    return 0x10000 - reload;
}


static u32 timer_enabled(struct NDS *this, u32 tn) {
    return NDS_clock_current(this) >= this->timer[tn].enable_at;
}

static u32 busrd_invalid7(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD7 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static u32 busrd_invalid9(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD7 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static void buswr_invalid7(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE7 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    this->waitstates.current_transaction++;
    dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

static void buswr_invalid9(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE7 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    this->waitstates.current_transaction++;
    dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

#define WAITCNT 0x04000204

void NDS_dma_start(struct NDS_DMA_ch *ch, u32 i, u32 is_sound)
{
    ch->op.started = 1;
    u32 mask = ch->io.transfer_size ? ~3 : ~1;
    mask &= 0x0FFFFFFF;
    //u32 mask = 0x0FFFFFFF;
    if (ch->op.first_run) {
        ch->op.dest_addr = ch->io.dest_addr & mask;
        ch->op.src_addr = ch->io.src_addr & mask;
    }
    else if (ch->io.dest_addr_ctrl == 3) {
        ch->op.dest_addr = ch->io.dest_addr & mask;
    }
    ch->op.word_count = ch->io.word_count;
    ch->op.sz = ch->io.transfer_size ? 4 : 2;
    ch->op.word_mask = i == 3 ? 0xFFFF : 0x3FFF;
    ch->op.dest_access = ARM7P_nonsequential | ARM7P_dma;
    ch->op.src_access = ARM7P_nonsequential | ARM7P_dma;
    ch->op.is_sound = is_sound;
    if (is_sound) {
        ch->op.sz = 4;
        ch->io.dest_addr_ctrl = 2;
        ch->op.word_count = 4;
    }
}


static u32 read_timer(struct NDS *this, u32 tn)
{
    struct NDS_TIMER *t = &this->timer[tn];
    u64 current_time = this->clock.master_cycle_count + this->waitstates.current_transaction;
    if (!timer_enabled(this, tn) || t->cascade) return t->val_at_stop;

    // Timer is enabled, so, check how many cycles we have had...
    u64 ticks_passed = (((current_time - 1) - t->enable_at) >> t->shift) % (timer_reload_ticks(t->reload));
    u32 v = t->reload + ticks_passed;
    return v;
}

void NDS_bus_init(struct NDS *this)
{
    for (u32 i = 0; i < 4; i++) {
        struct NDS_TIMER *t = &this->timer[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
    for (u32 i = 0; i < 16; i++) {
        this->mem.rw[0].read[i] = &busrd_invalid7;
        this->mem.rw[0].write[i] = &buswr_invalid7;
        this->mem.rw[1].read[i] = &busrd_invalid9;
        this->mem.rw[1].write[i] = &buswr_invalid9;
    }
    memset(this->dbg_info.mgba.str, 0, sizeof(this->dbg_info.mgba.str));
}

/*static void trace_read(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    struct trace_view *tv = this->cpu.dbg.tvptr;
    if (!tv) return;
    trace_view_startline(tv, 2);
    trace_view_printf(tv, 0, "BUSrd");
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}

static void trace_write(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    struct trace_view *tv = this->cpu.dbg.tvptr;
    if (!tv) return;
    trace_view_startline(tv, 2);
    trace_view_printf(tv, 0, "BUSwr");
    trace_view_printf(tv, 1, "%lld", this->clock.master_cycle_count + this->waitstates.current_transaction);
    trace_view_printf(tv, 2, "%08x", addr);
    trace_view_printf(tv, 3, "%08x", val);
    trace_view_endline(tv);
}*/


u32 NDS_mainbus_read7(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct NDS *this = (struct NDS *)ptr;
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[0].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else v = busrd_invalid7(this, addr, sz, access, has_effect);
    //if (dbg.trace_on) trace_read(this, addr, sz, v);
    return v;
}

u32 NDS_mainbus_read9(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct NDS *this = (struct NDS *)ptr;
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[1].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else v = busrd_invalid9(this, addr, sz, access, has_effect);
    //if (dbg.trace_on) trace_read(this, addr, sz, v);
    return v;
}

u32 NDS_mainbus_fetchins9(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct NDS *this = (struct NDS*)ptr;
    u32 v = NDS_mainbus_read9(ptr, addr, sz, access, 1);
    switch(sz) {
        case 4:
            this->io.open_bus.arm9 = v;
            break;
        case 2:
            this->io.open_bus.arm9 = (v << 16) | v;
            break;
    }
    return v;
}


u32 NDS_mainbus_fetchins7(void *ptr, u32 addr, u32 sz, u32 access)
{
    struct NDS *this = (struct NDS*)ptr;
    u32 v = NDS_mainbus_read7(ptr, addr, sz, access, 1);
    switch(sz) {
        case 4:
            this->io.open_bus.arm7 = v;
            break;
        case 2:
            this->io.open_bus.arm7 = (v << 16) | v;
            break;
    }
    return v;
}

void NDS_mainbus_write7(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    struct NDS *this = (struct NDS *)ptr;
    //if (dbg.trace_on) trace_write(this, addr, sz, val);
    if (addr < 0x10000000) {
        //printf("\nWRITE addr:%08x sz:%d val:%08x", addr, sz, val);
        return this->mem.rw[0].write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr_invalid7(this, addr, sz, access, val);
}

void NDS_mainbus_write9(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    struct NDS *this = (struct NDS *)ptr;
    //if (dbg.trace_on) trace_write(this, addr, sz, val);
    if (addr < 0x10000000) {
        return this->mem.rw[1].write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr_invalid9(this, addr, sz, access, val);
}


u64 NDS_clock_current(struct NDS *this)
{
    return this->clock.master_cycle_count + this->waitstates.current_transaction;
}
