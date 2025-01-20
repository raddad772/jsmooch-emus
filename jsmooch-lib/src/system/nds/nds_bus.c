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

static u32 busrd7_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD7 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static u32 busrd9_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    printf("\nREAD9 UNKNOWN ADDR:%08x sz:%d", addr, sz);
    this->waitstates.current_transaction++;
    //dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad reads", this->clock.master_cycle_count);
    return 0;
}

static void buswr7_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE7 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
    this->waitstates.current_transaction++;
    dbg.var++;
    //if (dbg.var > 15) dbg_break("too many bad writes", this->clock.master_cycle_count);
}

static void buswr9_invalid(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    printf("\nWRITE9 UNKNOWN ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
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

static u32 busrd9_main(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    return cR[sz](this->mem.RAM, addr & 0x3FFFFF);
}

static void buswr9_main(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    cW[sz](this->mem.RAM, addr & 0x3FFFFF, val);
}


static void buswr9_gba_cart(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    return;
}

static u32 busrd9_gba_cart(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->mem.io.gba_cart.enabled9) return (addr & 0x1FFFF) >> 1;
    return 0;
}

static void buswr9_gba_sram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    return;
}

static u32 masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};

static u32 busrd9_gba_sram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->mem.io.gba_cart.enabled9) return 0xFFFFFFFF & masksz[sz];
    return 0;
}

static void buswr9_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (!this->mem.io.RAM9.disabled) cW[sz](this->mem.RAM, (addr & this->mem.io.RAM9.mask) + this->mem.io.RAM9.base, val);
}

static u32 busrd9_shared(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (this->mem.io.RAM9.disabled) return 0; // undefined
    return cR[sz](this->mem.RAM, (addr & this->mem.io.RAM9.mask) + this->mem.io.RAM9.base);
}

static void buswr9_obj_and_palette(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x05000000) return;
    if (addr < 0x050000200) NDS_PPU_write_2d_bg_palette(this, 0, addr & 0x1FF, sz, val);
    if (addr < 0x050000400) NDS_PPU_write_2d_obj_palette(this, 0, addr & 0x1FF, sz, val);
    if (addr < 0x050000600) NDS_PPU_write_2d_bg_palette(this, 1, addr & 0x1FF, sz, val);
    if (addr < 0x050000800) NDS_PPU_write_2d_obj_palette(this, 1, addr & 0x1FF, sz, val);
    buswr9_invalid(this, addr, sz, access, val);
}

static u32 busrd9_obj_and_palette(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x05000000) return busrd9_invalid(this, addr, sz, access, has_effect);
    if (addr < 0x050000200) return NDS_PPU_read_2d_bg_palette(this, 0, addr & 0x1FF, sz);
    if (addr < 0x050000400) return NDS_PPU_read_2d_obj_palette(this, 0, addr & 0x1FF, sz);
    if (addr < 0x050000600) return NDS_PPU_read_2d_bg_palette(this, 1, addr & 0x1FF, sz);
    if (addr < 0x050000800) return NDS_PPU_read_2d_obj_palette(this, 1, addr & 0x1FF, sz);
    return busrd9_invalid(this, addr, sz, access, has_effect);
}

static void buswr9_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    u8 *ptr = this->mem.vram.map.arm9[(addr >> 14) & 0x3FF];
    if (ptr) cW[sz](ptr, addr & 0x3FFF, val);

    printf("\nInvalid VRAM write unmapped addr:%08x sz:%d val:%08x", addr, sz, val);
}

static u32 busrd9_vram(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u8 *ptr = this->mem.vram.map.arm9[(addr >> 14) & 0x3FF];
    if (ptr) return cR[sz](ptr, addr & 0x3FFF);

    printf("\nInvalid VRAM read unmapped addr:%08x sz:%d", addr, sz);
    return 0;
}

static void buswr9_oam(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x07000000) return;
    if (addr < 0x07000400) return cW[sz](this->ppu.eng2d[0].mem.oam, addr, val);
    if (addr < 0x07000800) return cW[sz](this->ppu.eng2d[0].mem.oam, addr, val);
    buswr9_invalid(this, addr, sz, access, val);
}

static u32 busrd9_oam(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x07000000) return busrd9_invalid(this, addr, sz, access, has_effect);
    if (addr < 0x07000400) return cR[sz](this->ppu.eng2d[0].mem.oam, addr);
    if (addr < 0x07000800) return cR[sz](this->ppu.eng2d[0].mem.oam, addr);
    return busrd9_invalid(this, addr, sz, access, has_effect);
}

#define R7_WRAMSTAT 0x04000241
#define R9_WRAMCNT  0x04000247

static u32 busrd7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    switch(addr) {
        case R7_WRAMSTAT:
            return this->mem.io.RAM9.val;
    }
    printf("\nUnhandled BUSRDIO78 addr:%08x", addr);
    return 0;
}

static u32 busrd9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    switch(addr) {
        case R9_WRAMCNT:
            return this->mem.io.RAM9.val;
    }
    printf("\nUnhandled BUSRDIO98 addr:%08x", addr);
    return 0;
}

static void buswr7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nUnhandled BUSWRIO78 addr:%08x val:%08x", addr, val);
}

static void buswr9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    switch(addr) {
        case R9_WRAMCNT: {
            switch (val & 3) {
                this->mem.io.RAM9.val = val;
                case 0: // 0 = 32k/0K, open-bus
                    this->mem.io.RAM9.base = 0;
                    this->mem.io.RAM9.mask = 0x7FFF;
                    this->mem.io.RAM9.disabled = 0;
                    this->mem.io.RAM7.base = 0;
                    this->mem.io.RAM7.mask = 0;
                    this->mem.io.RAM7.disabled = 1;
                    break;
                case 1: // 1 = hi 16K/ lo16K,
                    this->mem.io.RAM9.base = 0x4000;
                    this->mem.io.RAM9.mask = 0x3FFF;
                    this->mem.io.RAM9.disabled = 0;
                    this->mem.io.RAM7.base = 0;
                    this->mem.io.RAM7.mask = 0x3FFF;
                    this->mem.io.RAM7.disabled = 0;
                    break;
                case 2: // 2 = lo 16k/ hi16k,
                    this->mem.io.RAM9.base = 0;
                    this->mem.io.RAM9.mask = 0x3FFF;
                    this->mem.io.RAM9.disabled = 0;
                    this->mem.io.RAM7.base = 0x4000;
                    this->mem.io.RAM7.mask = 0x3FFF;
                    this->mem.io.RAM7.disabled = 0;
                    break;
                case 3: // 3 = 0k / 32k
                    this->mem.io.RAM9.base = 0;
                    this->mem.io.RAM9.mask = 0;
                    this->mem.io.RAM9.disabled = 1;
                    this->mem.io.RAM7.base = 0;
                    this->mem.io.RAM7.mask = 0x7FFF;
                    this->mem.io.RAM7.disabled = 0;
            }
            return;
        }
    }
    printf("\nUnhandled BUSWRIO98 addr:%08x val:%08x", addr, val);
}

static u32 busrd9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = busrd9_io8(this, addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd9_io8(this, addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd9_io8(this, addr+2, 1, access, has_effect) << 16;
        v |= busrd9_io8(this, addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static void buswr9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    buswr9_io8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr9_io8(this, addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr9_io8(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr9_io8(this, addr+3, 1, access, (val >> 24) & 0xFF);
    }
}

static u32 busrd7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = busrd7_io8(this, addr, 1, access, has_effect);
    if (sz >= 2) v |= busrd7_io8(this, addr+1, 1, access, has_effect) << 8;
    if (sz == 4) {
        v |= busrd7_io8(this, addr+2, 1, access, has_effect) << 16;
        v |= busrd7_io8(this, addr+3, 1, access, has_effect) << 24;
    }
    return v;
}

static void buswr7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    buswr7_io8(this, addr, 1, access, val & 0xFF);
    if (sz >= 2) buswr7_io8(this, addr+1, 1, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        buswr7_io8(this, addr+2, 1, access, (val >> 16) & 0xFF);
        buswr7_io8(this, addr+3, 1, access, (val >> 24) & 0xFF);
    }
}


void NDS_bus_init(struct NDS *this)
{
    for (u32 i = 0; i < 4; i++) {
        struct NDS_TIMER *t = &this->timer[i];
        t->overflow_at = 0xFFFFFFFFFFFFFFFF;
        t->enable_at = 0xFFFFFFFFFFFFFFFF;
    }
    for (u32 i = 0; i < 16; i++) {
        this->mem.rw[0].read[i] = &busrd7_invalid;
        this->mem.rw[0].write[i] = &buswr7_invalid;
        this->mem.rw[1].read[i] = &busrd9_invalid;
        this->mem.rw[1].write[i] = &buswr9_invalid;
    }
    memset(this->dbg_info.mgba.str, 0, sizeof(this->dbg_info.mgba.str));
#define BND9(page, func) { this->mem.rw[1].read[page] = &busrd9_##func; this->mem.rw[1].write[page] = &buswr9_##func; }
    BND9(0x2, main);
    BND9(0x3, shared);
    BND9(0x4, io);
    BND9(0x5, obj_and_palette);
    BND9(0x6, vram);
    BND9(0x7, oam);
    BND9(0x8, gba_cart);
    BND9(0x9, gba_cart);
    BND9(0xA, gba_sram);
#undef BND9

#define BND7(page, func) { this->mem.rw[0].read[page] = &busrd7_##func; this->mem.rw[0].write[page] = &buswr7_##func; }
    BND7(0x4, io);
#undef BND7
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
    else v = busrd7_invalid(this, addr, sz, access, has_effect);
    //if (dbg.trace_on) trace_read(this, addr, sz, v);
    return v;
}

u32 NDS_mainbus_read9(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct NDS *this = (struct NDS *)ptr;
    u32 v;

    if (addr < 0x10000000) v = this->mem.rw[1].read[(addr >> 24) & 15](this, addr, sz, access, has_effect);
    else v = busrd9_invalid(this, addr, sz, access, has_effect);
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

    buswr7_invalid(this, addr, sz, access, val);
}

void NDS_mainbus_write9(void *ptr, u32 addr, u32 sz, u32 access, u32 val)
{
    struct NDS *this = (struct NDS *)ptr;
    //if (dbg.trace_on) trace_write(this, addr, sz, val);
    if (addr < 0x10000000) {
        return this->mem.rw[1].write[(addr >> 24) & 15](this, addr, sz, access, val);
    }

    buswr9_invalid(this, addr, sz, access, val);
}


u64 NDS_clock_current(struct NDS *this)
{
    return this->clock.master_cycle_count + this->waitstates.current_transaction;
}
