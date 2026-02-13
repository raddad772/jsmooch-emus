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

void SPU_VOICE::cycle() {
    i16 step = io.sample_rate;
    // TODO: implement PMON
    if (step > 0x3FFF) step = 0x4000;
    pitch_counter = (pitch_counter + step) & 0xFFFF;
}

static void sch_FIFO_transfer(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<SPU *>(ptr);
    th->FIFO_transfer(timecode - jitter);
}

void SPU::DMA_write(u32 val) {
    if (!io.SPUSTAT.data_transfer_dma_write_req) {
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
    if (!io.SPUSTAT.data_transfer_dma_read_req) {
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

void SPU::write_control_regs(u32 addr, u8 sz, u32 val) {
    switch (addr) {
        case 0x1F801DA4: // IRQ9 addr
            io.IRQ_addr = val << 3;
            return;
        case 0x1F801DA6: // RAM transfer address
            io.RAM_transfer_addr = val;
            latch.RAM_transfer_addr = val << 3;
            return;
        case 0x1F801DA8: // FIFO write
            FIFO.push(val);
            // if we're in data transfer mode, schedule it if it's not
            if (io.SPUCNT.sound_ram_transfer_mode == 1 && !FIFO.still_sch) {
                schedule_FIFO_transfer(bus->clock.master_cycle_count);
                io.SPUSTAT.data_transfer_busy = 1;
            }
            return;
        case 0x1F801DAA: // SPUCNT
            write_SPUCNT(val);
            return;
        case 0x1F801DAC: // RAMCNT
            io.RAMCNT.u = val;
            return;
        case 0x1F801DAE: // SPUSTAT
            return;
    }
    printf("\n(SPU) Unhandled CTRL write %08x(%d): %08x", addr, sz, val);
}

void SPU::write_SPUCNT(u16 val) {
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
}

u32 SPU::read_control_regs(u32 addr, u8 sz) {
    switch (addr) {
        case 0x1F801DA4: // IRQ9 addr
            return io.IRQ_addr << 3;
        case 0x1F801DA6:
            return io.RAM_transfer_addr;
        case 0x1F801DA8:
            printf("\n(SPU) FIFO READ WHAT?!");
            return 0xFFFF;
        case 0x1F801DAA: // SPUCNT
            return io.SPUCNT.u;
        case 0x1F801DAC: // RAMCNT
            return io.RAMCNT.u;
        case 0x1F801DAE: // SPUSTAT
            return io.SPUSTAT.u;
    }
    printf("\n(SPU) Unhandled CTRL read %08x(%d)", addr, sz);
}

void SPU::mainbus_write(u32 addr, u8 sz, u32 val)
{
    u32 raddr = addr;
    if ((sz == 1) && (addr & 1)) {
        static int a = 1;
        if (a == 1) {
            a = 0;
            printf("\n(SPU) WARN Ignore SPU 8bit write to odd address");
        }
        return;
    }
    addr &= 0xFFFFFFFE;
    if (sz == 4) {
        mainbus_write(addr, 2, val & 0xFFFF);
        mainbus_write(addr + 2, 2, (val >> 16) & 0xFFFF);
        return;
    }
    if (sz == 1) sz = 2;
    val &= masksz[sz];

    if (addr >= 0x1F801C00 && addr <= 0x1F801D7F) {
        addr &= 0x17F;
        return voices[addr >> 4].write_reg((addr & 0xF) >> 1, val);
    }

    if (addr >= 0x1F801DA2 && addr <= 0x1F801DBF) {
        write_control_regs(addr, sz, val);
        return;
    }

    switch (addr) {
        case 0x1F801D90:
            for (u32 i = 1; i < 16; i++) {
                voices[i].io.PMON = (val >> i) & 1;
            }
            return;
        case 0x1F801D92:
            for (u32 i = 0; i < 8; i++) {
                voices[i+16].io.PMON = (val >> i) & 1;
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

void SPU_VOICE::reset(PS1::core *ps1, u32 num_in) {
    num = num_in;
    bus = ps1;
}

u16 SPU_VOICE::read_reg(u32 regnum) {
    switch (regnum) {
        case 2: return io.sample_rate;
        case 3: return io.adpcm_start_addr >> 3;
        case 7: return io.adpcm_repeat_addr >> 3;

    }
}

void SPU_VOICE::write_reg(u32 regnum, u16 val) {
    switch (regnum) {
        case 2: io.sample_rate = val; return;
        case 3: io.adpcm_start_addr = val << 3; return;
        case 7: io.adpcm_repeat_addr = val << 3; return;
    }
}

void SPU::reset() {
    for (u32 i = 0; i < 24; i++) voices[i].reset(bus, i);
}

u32 SPU::mainbus_read(u32 addr, u8 sz, bool has_effect)
{
    u32 raddr = addr;
    addr &= 0xFFFFFFFE;
    u32 v;
    if (sz == 4) {
        v = mainbus_read(addr, 2, has_effect);
        v |= mainbus_read(addr + 2, 2, has_effect) << 16;
        return v;
    }
    if (sz == 1) sz = 2;
    if (addr >= 0x1F801C00 && addr <= 0x1F801D7F) {
        addr -= 0x1F801C00;
        addr &= 0x17F; // 8 16-bit regs per voice
        return voices[addr >> 4].read_reg((addr & 0xF) >> 1);
    }
    if (addr >= 0x1F801DA2 && addr <= 0x1F801DBF) {
        return read_control_regs(addr, sz);
    }

    switch (addr) {
        case 0x1F801D90:
            v = 0;
            for (u32 i = 1; i < 16; i++) {
                v |= voices[i].io.PMON << i;
            }
            return v;
        case 0x1F801D92:
            v = 0;
            for (u32 i = 0; i < 8; i++) {
                v |= voices[i+16].io.PMON << i;
            }
            return v;


    }
    printf("\n(SPU) Unhandled read %08x (%d)", raddr, sz);
    return masksz[sz];
}
}
