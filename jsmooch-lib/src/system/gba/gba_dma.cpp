//
// Created by . on 5/5/25.
//

#include "gba_dma.h"
#include "gba_bus.h"

namespace GBA {

static void block_step_dma(void *ptr, u64 key, u64 clock, u32 jitter);
void block_step_cpu(void *ptr, u64 key, u64 clock, u32 jitter);

void DMA::eval_bit_masks()
{
    // TODO: this
    bit_mask.vblank = bit_mask.hblank = bit_mask.normal = 0;
    for (u32 i = 0; i < 4; i++) {
        const auto &ch = channel[i];
        if (ch.io.enable && ch.latch.started) // currently-running DMA
            bit_mask.normal |= 1 << i;
        if (ch.io.enable && ch.io.start_timing == 1)
            bit_mask.vblank |= 1 << i;
        if (ch.io.enable && ch.io.start_timing == 2)
            bit_mask.hblank |= 1 << i;
    }
    if (bit_mask.normal) bus->scheduler.run.func = &block_step_dma;
    else bus->scheduler.run.func = &block_step_cpu;
}

DMA::DMA(core *parent) :
    bus{parent},
    channel{DMA_ch(bus, 0), DMA_ch(bus, 1), DMA_ch(bus, 2), DMA_ch(bus, 3)}
{
    for (u32 i = 0; i < 4; i++) {
        channel[i].word_mask = i < 3 ? 0x3FFF : 0xFFFF;
    }
}

void DMA::raise_irq_for_dma(u32 num)
{
    bus->io.IF |= 1 << (8 + num);
    bus->eval_irqs();
}

bool DMA_ch::go() {
    if ((io.enable) && (latch.started)) {
        if (!io.transfer_size) {
            u16 value;
            if (latch.src_addr >= 0x02000000) {
                value = core::mainbus_read(bus, latch.src_addr, 2, latch.src_access, 1);
                io.open_bus = (value << 16) | value;
            } else {
                if (latch.dest_addr & 2) {
                    value = io.open_bus >> 16;
                } else {
                    value = io.open_bus & 0xFFFF;
                }
                bus->waitstates.current_transaction++;
            }

           core::mainbus_write(bus, latch.dest_addr, 2, latch.dest_access, value);
        }
        else {
            if (latch.src_addr >= 0x02000000)
                io.open_bus = core::mainbus_read(bus, latch.src_addr, 4, latch.src_access, 1);
            else
                bus->waitstates.current_transaction++;
            core::mainbus_write(bus, latch.dest_addr, 4, latch.dest_access, io.open_bus);
        }

        // TODO: fix this
        latch.src_access = ARM7P_sequential | ARM7P_dma;
        latch.dest_access = ARM7P_sequential | ARM7P_dma;

        latch.src_addr += src_add;
        latch.dest_addr += dest_add;
        latch.word_count = (latch.word_count - 1) & word_mask;
        if (latch.word_count == 0) {
            latch.started = 0; // Disable running

            if (io.irq_on_end) {
                if (!is_sound) printf("\nDMA IRQ %d", num);
                bus->dma.raise_irq_for_dma(num);
            }

            if (io.repeat && io.start_timing != 0) {
                if (is_sound)
                    latch.word_count = 4;
                else {
                    latch.word_count = io.word_count;
                }
                if ((io.dest_addr_ctrl == 3) && (!is_sound))
                    latch.dest_addr = io.dest_addr & (io.transfer_size ? ~3 : ~1);
            }
            else if (!io.repeat) {
                io.enable = 0;
            }
            bus->dma.eval_bit_masks();
        }
        return true;
    }
    return false;
}

static constexpr u32 hipri[16] = {
        0, // 0000
        0, // 0001
        1, // 0010
        0, // 0011
        2, // 0100
        0, // 0101
        1, // 0110
        0, // 0111
        3, // 1000
        0, // 1001
        1, // 1010
        0, // 1011
        2, // 1100
        0, // 1101
        1, // 1110
        0, // 1111
};


void block_step_cpu(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<GBA::core *>(ptr);
    th->waitstates.current_transaction = 0;
    th->cpu.IRQcheck(false);
    th->cpu.run_noIRQcheck();
    th->clock.master_cycle_count += th->waitstates.current_transaction;
#ifdef GBA_STATS
    timing.arm_cycles += waitstates.current_transaction;
#endif
    th->waitstates.current_transaction = 0;
}

void block_step_dma(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<GBA::core *>(ptr);
    u32 chn = hipri[th->dma.bit_mask.normal];
    th->dma.channel[chn].go();
}

void DMA_ch::start()
{
    latch.started = 1;
    on_modify_write();
    bus->dma.eval_bit_masks();
}

void DMA_ch::cnt_written(bool old_enable)
{
    if (io.enable) {
        if (!old_enable) { // 0->1
            latch.dest_addr = io.dest_addr;
            latch.src_addr = io.src_addr;

            if ((io.start_timing == 3) && ((num == 1) || (num == 2))) {
                io.transfer_size = 1;
                latch.word_count = 4;
                latch.src_addr &= ~3;
                latch.dest_addr &= ~3;
                is_sound = 1;
            }
            else {
                is_sound = 0;
                const u32 mask = io.transfer_size ? ~3 : ~1;
                latch.src_addr &= mask;
                latch.dest_addr &= mask;
                latch.word_count = io.word_count;
                if (io.start_timing == 0) start();
            }
        } // 1->1 really do nothing
    }
    else { // 1->0 or 0->0?
        latch.started = 0;
        bus->dma.eval_bit_masks();
    }
}

void DMA_ch::on_modify_write()
{
    static constexpr i32 src[2][4] = { { 2, -2, 0, 0}, {4, -4, 0, 0} };
    static constexpr i32 dst[2][4] = { { 2, -2, 0, 2}, {4, -4, 0, 4} };
    dest_add = is_sound ? 0 : dst[io.transfer_size][io.dest_addr_ctrl];
    src_add = src[io.transfer_size][io.src_addr_ctrl];
}
}