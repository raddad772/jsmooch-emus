//
// Created by . on 2/11/25.
//

#include <cstdio>

#include "ps1_bus.h"
#include "ps1_dma.h"
#include "peripheral/ps1_sio.h"
#include "peripheral/ps1_digital_pad.h"
#include "helpers/multisize_memaccess.cpp"

#define deKSEG(addr) ((addr) & 0x1FFFFFFF)
#define DEFAULT_WAITSTATES 1
namespace PS1 {
static u32 read_trace_cpu(void *ptr, u32 addr, u8 sz)
{
    auto *th = static_cast<core *>(ptr);
    return th->mainbus_read(addr, sz, false);
}

static u32 mainbus_fetchins(void *ptr, u32 addr, u8 sz)
{
    auto *th = static_cast<core *>(ptr);
    return th->mainbus_read(addr, sz, true);
}

static void run_block(void *bound_ptr, u64 num, u64 current_clock, u32 jitter)
{
    auto *th = static_cast<core *>(bound_ptr);
    th->cycles_left += static_cast<i64>(num);
    th->cpu.check_IRQ();
    th->cpu.cycle(num);
}

static u32 snoop_read(void *ptr, u32 addr, u8 sz, u32 has_effect)
{
    auto *th = static_cast<core *>(ptr);
    u32 r = th->mainbus_read(addr, sz, has_effect);
    //printf("\nread %08x (%d): %08x", addr, sz, r);
    return r;
}

static void snoop_write(void *ptr, u32 addr, u8 sz, u32 val)
{
    auto *th = static_cast<core *>(ptr);
    //printf("\nwrite %08x (%d): %08x", addr, sz, val);
    th->mainbus_write(addr, sz, val);
}

static void update_SR(void *ptr, R3000::core *cpucore, u32 val)
{
    auto *th = static_cast<PS1::core *>(ptr);
    th->mem.cache_isolated = (val & 0x10000) == 0x10000;
    //printf("\nNew SR: %04x", core->regs.COP0[12] & 0xFFFF);
}

static void set_cdrom_irq_level(void *ptr, u32 lvl) {
    auto *th = static_cast<PS1::core *>(ptr);
    th->set_irq(IRQ_CDROM, lvl);
}

core::core() :
    IRQ_multiplexer(15),
    clock(true),
    scheduler(&clock.master_cycle_count),
    cpu(&clock.master_cycle_count, &clock.waitstates, &scheduler, &IRQ_multiplexer),
    sio0(this),
    cdrom(&this->scheduler),
    gpu(this),
    dma(this),
    io{ false, SIO::digital_gamepad(this) }
    {
    for (u32 i = 0; i < 3; i++) {
        timers[i].num = i;
        timers[i].bus = this;
    }
    dma.control = 0x7654321;
    for (u32 i = 0; i < 7; i++) {
        dma.channels[i].num = i;
        dma.channels[i].step = D_increment;
        dma.channels[i].sync = D_manual;
        dma.channels[i].direction = D_to_ram;
    }
    has.load_BIOS = true;
    has.max_loaded_files = 0;
    has.max_loaded_folders = 0;
    has.save_state = false;
    has.set_audiobuf = false;

    scheduler.max_block_size = 2;

    scheduler.run.func = &run_block;
    scheduler.run.ptr = this;

    setup_IRQs();

    cpu.read_ptr = this;
    cpu.write_ptr = this;
    cpu.read = &snoop_read;
    //cpu.read = &PS1_mainbus_read;
    cpu.write = &snoop_write;
    //cpu.write = &PS1_mainbus_write;
    cpu.fetch_ptr = this;
    cpu.fetch_ins = &mainbus_fetchins;
    cpu.update_sr_ptr = this;
    cpu.update_sr = &update_SR;

    snprintf(label, sizeof(label), "PlayStation");
    jsm_debug_read_trace dt;
    dt.read_trace_arm = &read_trace_cpu;
    dt.ptr = this;
    cpu.setup_tracing(dt, &clock.master_cycle_count, 1);

    jsm.described_inputs = false;
    jsm.cycles_left = 0;

    cdrom.set_irq_lvl = &set_cdrom_irq_level;
    cdrom.set_irq_ptr = this;
    IRQ_multiplexer.clock = &clock.master_cycle_count;
    //::dbg.trace_on = 1;
    ::dbg.traces.better_irq_multiplexer = 1;

    //
    ::dbg.traces.r3000.instruction = 1;
}

static constexpr u32 alignmask[5] = { 0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC };

u32 core::mainbus_read(u32 addr, u8 sz, bool has_effect)
{
    clock.waitstates += DEFAULT_WAITSTATES;
    u32 raddr = addr;

    addr = deKSEG(addr) & alignmask[sz];
    // 2MB MRAM mirrored 4 times
    if (raddr == 0x801FFD5C) {
        int a = 1;
        a++;
    }
    if (addr < 0x00800000) {
        return cR[sz](mem.MRAM, addr & 0x1FFFFF);
    }
    // 1F800000 1024kb of scratchpad
    if ((addr >= 0x1F800000) && (addr < 0x1F800400)) {
        return cR[sz](mem.scratchpad, addr & 0x3FF);
    }
    if ((addr >= 0x1F801800) && (addr < 0x1F801804)) {
        return cdrom.mainbus_read(addr, sz, has_effect);
    }
    /*if ((addr >= 0x1F800000) && (addr < 0x1F803000)) {
        // TODO: stub: IO area
        return 0;
    }*/
    // 1FC00000h 512kb BIOS
    if ((addr >= 0x1FC00000) && (addr < 0x1FC80000)) {
        return cR[sz](mem.BIOS, addr & 0x7FFFF);
    }

    if ((addr >= 0x1F801070) && (addr <= 0x1F801077)) {
        return cpu.read_reg(addr, sz);
    }

    if ((addr >= 0x1F801080) && (addr <= 0x1F8010FF)) {
        return dma.read(addr, sz);
    }

    if ((addr >= 0x1F801040) && (addr < 0x1F801050)) {
        return sio0.read(addr, sz);
    }
    if ((addr >= 0x1F801050) && (addr < 0x1F801060)) {
        return sio1.read(addr, sz);
    }

    if ((addr >= 0x1F801C00) && (addr < 0x1F801E00)) {
        return spu.read(addr, sz, has_effect);
    }

    switch(addr) {
        case 0x1f801110:
            // expansion area poll
            return 0;
        case 0x00FF1F00: // Invalid addresses?
        case 0x00FF1F04:
        case 0x00FF1F08:
        case 0x00FF1F0C:
        case 0x00FF1F50:
            return 0;
        case 0x1F80101C: // Expansion 2 Delay/size
            return 0x00070777;
        case 0x1F8010A8: // DMA2 GPU thing
        case 0x1F801810: // GP0/GPUREAD
            return gpu.get_gpuread();
        case 0x1F801814: // GPUSTAT Read GPU Status Register
            return gpu.get_gpustat();
        case 0x1F000084: // PIO
            //console.log('PIO READ!');
            return 0;
    }

    if ((addr >= 0x1F801100) && (addr < 0x1F801130)) return timers_read(addr, sz);

    static u32 e = 0;
    printf("\nUNHANDLED MAINBUS READ sz %d addr %08x", sz, raddr);
    e++;
    if (e > 10) dbg_break("TOO MANY BAD READ", clock.master_cycle_count+clock.waitstates);
    //return 0;
    switch(sz) {
        case 1:
            return 0xFF;
        case 2:
            return 0xFFFF;
        case 4:
            return 0xFFFFFFFF;
    }
    return 0;
}

void core::mainbus_write(u32 addr, u8 sz, u32 val)
{
    clock.waitstates += DEFAULT_WAITSTATES;
    if (mem.cache_isolated) return;
    //if (addr == 0x801FFD5C) {
        //printf("\nWrite 0x801FFD5C: %08x (%d) from PC %08x", val, sz, cpu.pipe.current.addr);
    //}
    addr = deKSEG(addr) & alignmask[sz];
    /*if (addr == 0) {
        printf("\nWRITE %08x:%08x ON cycle %lld", addr, val, ((PS1 *)ptr)->clock.master_cycle_count);
        dbg_break("GHAHA", 0);
    }*/
    if ((addr < 0x00800000) && !mem.cache_isolated) {
        cW[sz](mem.MRAM, addr & 0x1FFFFF, val);
        return;
    }
    if (((addr >= 0x1F800000) && (addr <= 0x1F800400)) && !mem.cache_isolated) {
        cW[sz](mem.scratchpad, addr & 0x3FF, val);
        return;
    }
    if ((addr >= 0x1F801070) && (addr <= 0x1F801077)) {
        cpu.write_reg(addr, sz, val);
        return;
    }

    if ((addr >= 0x1F801080) && (addr <= 0x1F8010FF)) {
        dma.write(addr, sz, val);
        return;
    }

    if ((addr >= 0x1F801040) && (addr < 0x1F801050)) {
        sio0.write(addr, sz, val);
        return;
    }
    if ((addr >= 0x1F801050) && (addr < 0x1F801060)) {
        sio1.write(addr, sz, val);
        return;
    }
    if ((addr >= 0x1F801800) && (addr < 0x1F801804)) {
        cdrom.mainbus_write(addr, val, sz);
        return;
    }

    if ((addr >= 0x1F801C00) && (addr < 0x1F801E00)) {
        spu.write(addr, sz, val);
        return;
    }

    switch(addr) {
        case 0x00FF1F00: // Invalid addresses
        case 0x00FF1F04:
        case 0x00FF1F08:
        case 0x00FF1F0C:
        case 0x00FF1F2C:
        case 0x00FF1F34:
        case 0x00FF1F3C:
        case 0x00FF1F40:
        case 0x00FF1F4C:
        case 0x00FF1F50:
        case 0x00FF1F60:
        case 0x00FF1F64:
        case 0x00FF1F7C:
            return;
        case 0x1F802041: // F802041h 1 PSX: POST (external 7 segment display, indicate BIOS boot status
            //printf("\nWRITE POST STATUS! %d", val);
            return;
        case 0x1F801810: // GP0 Send GP0 Commands/Packets (Rendering and VRAM Access)
            gpu.write_gp0(val);
            return;
        case 0x1F801814: // GP1
            gpu.write_gp1(val);
            return;
        case 0x1F801000: // Expansion 1 base addr
        case 0x1F801004: // Expansion 2 base addr
        case 0x1F801008: // Expansion 1 delay/size
        case 0x1F80100C: // Expansion 3 delay/size
        case 0x1F801010: // BIOS ROM delay/size
        case 0x1F801014: // SPU_DELAY delay/size
        case 0x1F801018: // CDROM_DELAY delay/size
        case 0x1F80101C: // Expansion 2 delay/size
        case 0x1F801020: // COM_DELAY /size
        case 0x1F801060: // RAM SIZE, 2mb mirrored in first 8mb
        case 0x1F801D80: // SPU main vol L
        case 0x1F801D82: // ...R
        case 0x1F801D84: // Reverb output L
        case 0x1F801D86: // ... R
        case 0x1FFE0130: // Cache control
            return;
    }

    if ((addr >= 0x1F801100) && (addr < 0x1F801130)) {
        timers_write(addr, sz, val);
        return;
    }

    printf("\nUNHANDLED MAINBUS WRITE %08x: %08x", addr, val);
}

void core::set_irq(IRQ from, u32 level)
{
    IRQ_multiplexer.set_level(from, level);
    cpu.update_I_STAT();
}

u64 core::clock_current() const
{
    return clock.master_cycle_count + clock.waitstates;
}
}