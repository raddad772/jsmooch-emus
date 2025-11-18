//
// Created by . on 5/5/25.
//

#include "gba_dma.h"
#include "gba_bus.h"

static void eval_bit_masks(struct GBA *this)
{
    // TODO: this
    this->dma.bit_mask.vblank = this->dma.bit_mask.hblank = this->dma.bit_mask.normal = 0;
    for (u32 i = 0; i < 4; i++) {
        struct GBA_DMA_ch *ch = &this->dma.channel[i];
        if (ch->io.enable && ch->latch.started) // currently-running DMA
            this->dma.bit_mask.normal |= 1 << i;
        if (ch->io.enable && ch->io.start_timing == 1)
            this->dma.bit_mask.vblank |= 1 << i;
        if (ch->io.enable && ch->io.start_timing == 2)
            this->dma.bit_mask.hblank |= 1 << i;
    }
    if (this->dma.bit_mask.normal) this->scheduler.run.func = &GBA_block_step_dma;
    else this->scheduler.run.func = &GBA_block_step_cpu;
}

void GBA_DMA_init(struct GBA *this)
{
    for (u32 i = 0; i < 4; i++) {
        this->dma.channel[i].num = i;
        this->dma.channel[i].word_mask = i < 3 ? 0x3FFF : 0xFFFF;
    }
}

static void raise_irq_for_dma(struct GBA *this, u32 num)
{
    this->io.IF |= 1 << (8 + num);
    GBA_eval_irqs(this);
}

static u32 dma_go_ch(struct GBA *this, u32 num) {
    struct GBA_DMA_ch *ch = &this->dma.channel[num];
    if ((ch->io.enable) && (ch->latch.started)) {
        if (!ch->io.transfer_size) {
            u16 value;
            if (ch->latch.src_addr >= 0x02000000) {
                value = GBA_mainbus_read(this, ch->latch.src_addr, 2, ch->latch.src_access, 1);
                ch->io.open_bus = (value << 16) | value;
            } else {
                if (ch->latch.dest_addr & 2) {
                    value = ch->io.open_bus >> 16;
                } else {
                    value = ch->io.open_bus & 0xFFFF;
                }
                this->waitstates.current_transaction++;
            }

            GBA_mainbus_write(this, ch->latch.dest_addr, 2, ch->latch.dest_access, value);
        }
        else {
            if (ch->latch.src_addr >= 0x02000000)
                ch->io.open_bus = GBA_mainbus_read(this, ch->latch.src_addr, 4, ch->latch.src_access, 1);
            else
                this->waitstates.current_transaction++;
            GBA_mainbus_write(this, ch->latch.dest_addr, 4, ch->latch.dest_access, ch->io.open_bus);
        }

        // TODO: fix this
        ch->latch.src_access = ARM7P_sequential | ARM7P_dma;
        ch->latch.dest_access = ARM7P_sequential | ARM7P_dma;

        ch->latch.src_addr += ch->src_add;
        ch->latch.dest_addr += ch->dest_add;
        ch->latch.word_count = (ch->latch.word_count - 1) & ch->word_mask;
        if (ch->latch.word_count == 0) {
            ch->latch.started = 0; // Disable running

            if (ch->io.irq_on_end) {
                if (!ch->is_sound) printf("\nDMA IRQ %d", num);
                raise_irq_for_dma(this, num);
            }

            if (ch->io.repeat && ch->io.start_timing != 0) {
                if (ch->is_sound)
                    ch->latch.word_count = 4;
                else {
                    ch->latch.word_count = ch->io.word_count;
                }
                if ((ch->io.dest_addr_ctrl == 3) && (!ch->is_sound))
                    ch->latch.dest_addr = ch->io.dest_addr & (ch->io.transfer_size ? ~3 : ~1);
            }
            else if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
            eval_bit_masks(this);
        }
        return 1;
    }
    return 0;
}

static const u32 hipri[16] = {
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

void GBA_block_step_dma(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct GBA *this = (struct GBA *)ptr;
    u32 chn = hipri[this->dma.bit_mask.normal];
    dma_go_ch(this, chn);
}

void GBA_DMA_start(struct GBA *gba, struct GBA_DMA_ch *ch)
{
    ch->latch.started = 1;
    GBA_DMA_on_modify_write(ch);
    eval_bit_masks(gba);
}

void GBA_DMA_cnt_written(struct GBA *this, struct GBA_DMA_ch *ch, u32 old_enable)
{
    if (ch->io.enable) {
        if (!old_enable) { // 0->1
            ch->latch.dest_addr = ch->io.dest_addr;
            ch->latch.src_addr = ch->io.src_addr;

            if ((ch->io.start_timing == 3) && ((ch->num == 1) || (ch->num == 2))) {
                ch->io.transfer_size = 1;
                ch->latch.word_count = 4;
                ch->latch.src_addr &= ~3;
                ch->latch.dest_addr &= ~3;
                ch->is_sound = 1;
            }
            else {
                ch->is_sound = 0;
                u32 mask = ch->io.transfer_size ? ~3 : ~1;
                ch->latch.src_addr &= mask;
                ch->latch.dest_addr &= mask;
                ch->latch.word_count = ch->io.word_count;
                if (ch->io.start_timing == 0) GBA_DMA_start(this, ch);
            }
        } // 1->1 really do nothing
    }
    else { // 1->0 or 0->0?
        ch->latch.started = 0;
        eval_bit_masks(this);
    }
}

void GBA_DMA_on_modify_write(struct GBA_DMA_ch *ch)
{
    static const i32 src[2][4] = { { 2, -2, 0, 0}, {4, -4, 0, 0} };
    static const i32 dst[2][4] = { { 2, -2, 0, 2}, {4, -4, 0, 4} };
    ch->dest_add = ch->is_sound ? 0 : dst[ch->io.transfer_size][ch->io.dest_addr_ctrl];
    ch->src_add = src[ch->io.transfer_size][ch->io.src_addr_ctrl];
}
