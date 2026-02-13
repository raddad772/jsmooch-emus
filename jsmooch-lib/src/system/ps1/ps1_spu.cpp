//
// Created by . on 2/20/25.
//
#include <cassert>
#include "ps1_spu.h"
#include "ps1_bus.h"

namespace PS1 {

void SPU_FIFO::push(u16 item) {
    if (len >= 32) {
        printf("\n(SPU) FIFO OVERFLOW!");
        return;
    }
    items[tail] = item;
    tail = (tail + 1) & 31;
    len++;
}

u16 SPU_FIFO::pop() {
    if (len < 1) {
        printf("\n(SPU) FIFO UNDERFLOW!");
        return 0xFFFF;
    }
    u16 v = items[head];
    head = (head + 1) & 31;
    len--;
    return v;
}

void SPU_FIFO::clear() {
    len = head = tail = 0;
}

void SPU::update_IRQs() {
    u32 old = io.IRQ_level;
    io.IRQ_level = io.SPUCNT.irq9_enable && io.SPUCNT.master_enable && io.SPUSTAT.irq9;
    if (old != io.IRQ_level) bus->set_irq(IRQ_SPU, io.IRQ_level);
}

static void sch_FIFO_transfer(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<SPU *>(ptr);
    th->FIFO_transfer(timecode - jitter);
}

void SPU::DMA_write(u32 val) {
    if (io.SPUCNT.sound_ram_transfer_mode != 2) {
        printf("\n(SPU) WARN DMA write ignored wrong mode %d", io.SPUCNT.sound_ram_transfer_mode);
        return;
    }
    latch.RAM_transfer_addr &= 0x7FFFF;
    write_RAM(latch.RAM_transfer_addr, val & 0xFFFF, true);
    latch.RAM_transfer_addr = (latch.RAM_transfer_addr + 2) & 0x7FFFF;
    write_RAM(latch.RAM_transfer_addr, val >> 16, true);
    latch.RAM_transfer_addr += 2;
}

u32 SPU::DMA_read() {
    if (io.SPUCNT.sound_ram_transfer_mode != 3) {
        printf("\n(SPU) WARN DMA read ignored wrong mode %d", io.SPUCNT.sound_ram_transfer_mode);
        return 0xFFFFFFFF;
    }

    latch.RAM_transfer_addr &= 0x7FFFF;
    u32 v = read_RAM(latch.RAM_transfer_addr, true, true);
    latch.RAM_transfer_addr = (latch.RAM_transfer_addr + 2) & 0x7FFFF;
    v |= read_RAM(latch.RAM_transfer_addr, true, true) << 16;
    latch.RAM_transfer_addr += 2;
    return v;
}

void SPU::FIFO_transfer(u64 clock) {
    if (FIFO.len > 0) {
        if (io.RAMCNT.mode != 1) printf("\n(SPU) WARN MANUAL FIFO WRITE MODE!=2!");
        latch.RAM_transfer_addr &= 0x7FFFF;
        u16 v = FIFO.pop();
        write_RAM(latch.RAM_transfer_addr, v, true);
        latch.RAM_transfer_addr += 2;
    }
    if (FIFO.len != 0) {
        schedule_FIFO_transfer(clock);
    }
}

void SPU::schedule_FIFO_transfer(u64 clock) {
    // i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched
    FIFO.sch_id = bus->scheduler.only_add_abs(clock + 24, 0, this, &sch_FIFO_transfer, &FIFO.still_sch);
}

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
void SPU::mainbus_write(u32 addr, u8 sz, u32 val)
{
    u32 raddr = addr;
    addr &= 0xFFFFFFFE;
    if (sz == 4) {
        mainbus_write(addr, 2, val & 0xFFFF);
        mainbus_write(addr + 2, 2, (val >> 16) & 0xFFFF);
        return;
    }
    if (sz == 1) sz = 2;
    val &= masksz[sz];
    switch (addr) {
        case 0x1F801DAC: // RAMCNT
            io.RAMCNT.u = val;
            return;
        case 0x1F801DAA: // SPUCNT
            io.SPUCNT.u = val & 0xFFFF;
            io.SPUSTAT.u = (io.SPUSTAT.u & ~0b111111) | (val & 0b111111);
            io.SPUSTAT.data_transfer_dma_rw_req = (val >> 5) & 1;
            io.SPUSTAT.data_transfer_dma_read_req = 0;
            io.SPUSTAT.data_transfer_dma_write_req = 0;
            if (io.SPUCNT.sound_ram_transfer_mode != 1 && FIFO.still_sch) {
                printf("\n(SPU) warn FIFO commit cancel!");
                bus->scheduler.delete_if_exist(FIFO.sch_id);
                io.SPUSTAT.data_transfer_busy = 0;
            }

            switch (io.SPUCNT.sound_ram_transfer_mode) {
                case 0: // OFF
                    break;
                case 1: // Manual Write/commit FIFO
                    if (FIFO.len > 0) {
                        schedule_FIFO_transfer(bus->clock.master_cycle_count);
                        io.SPUSTAT.data_transfer_busy = 1;
                    }
                    break;
                case 2: // DMAWrite
                    io.SPUSTAT.data_transfer_dma_write_req = 1;
                    break;
                case 3: // DMARead
                    io.SPUSTAT.data_transfer_dma_read_req = 1;
                    break;
            }
            if (!io.SPUCNT.irq9_enable) io.SPUSTAT.irq9 = 0;
            update_IRQs();
            return;
        case 0x1F801DAE: // SPUSTAT
            return;
        case 0x1F801DA6: // RAM transfer address
            io.RAM_transfer_addr = val;
            latch.RAM_transfer_addr = val << 3;
            return;
        case 0x1F801DA4: // IRQ9 addr
            io.IRQ_addr = val << 3;
            return;
        case 0x1F801DA8: // FIFO write
            FIFO.push(val);
            // if we're in data transfer mode, schedule it if it's not
            if (io.SPUCNT.sound_ram_transfer_mode == 1 && !FIFO.still_sch) {
                schedule_FIFO_transfer(bus->clock.master_cycle_count);
                io.SPUSTAT.data_transfer_busy = 1;
            }
            return;

    }
    printf("\n(SPU) Unhandled write %08x (%d): %08x", raddr, sz, val);
}

u16 SPU::read_RAM(u32 addr, bool has_effect, bool triggers_irq) {
    u16 v = RAM[(addr >> 1) & 0x3FFFF];
    if (triggers_irq && has_effect) check_irq_addr(addr);
    return v;
}

void SPU::write_RAM(u32 addr, u16 val, bool triggers_irq) {
    RAM[(addr >> 1) & 0x3FFFF] = val;
    if (triggers_irq) check_irq_addr(addr);
}

void SPU::check_irq_addr(u32 addr) {
    if (addr == io.IRQ_addr && ((io.RAMCNT.mode & 6) != 0)) {
        io.SPUSTAT.irq9 = 1;
        update_IRQs();
    }
}

u32 SPU::mainbus_read(u32 addr, u8 sz, bool has_effect)
{
    u32 raddr = addr;
    addr &= 0xFFFFFFFE;
    if (sz == 4) {
        u32 v = mainbus_read(addr, 2, has_effect);
        v |= mainbus_read(addr + 2, 2, has_effect) << 16;
        return v;
    }
    if (sz == 1) sz = 2;
    switch (addr) {
        case 0x1F801DAA: // SPUCNT
            return io.SPUCNT.u;
        case 0x1F801DAE: // SPUSTAT
            return io.SPUSTAT.u;
        case 0x1F801DA6:
            return io.RAM_transfer_addr;
        case 0x1F801DA4: // IRQ9 addr
            return io.IRQ_addr << 3;
        case 0x1F801DA8:
            printf("\n(SPU) FIFO WHAT?!");
            return 0xFFFF;
        case 0x1F801DAC: // RAMCNT
            return io.RAMCNT.u;
    }
    printf("\n(SPU) Unhandled read %08x (%d)", raddr, sz);
    return masksz[sz];
}
}
