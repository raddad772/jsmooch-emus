//
// Created by . on 5/5/25.
//

#include "gba_dma.h"
#include "gba_bus.h"

void GBA_DMA_init(struct GBA *this)
{
    for (u32 i = 0; i < 4; i++) {
        this->dma[i].num = i;
        this->dma[i].op.word_mask = i < 3 ? 0x3FFF : 0xFFFF;
    }
}

static void raise_irq_for_dma(struct GBA *this, u32 num)
{
    this->io.IF |= 1 << (8 + num);
    GBA_eval_irqs(this);
}

static u32 dma_go_ch(struct GBA *this, u32 num) {
    struct GBA_DMA_ch *ch = &this->dma[num];
    if ((ch->io.enable) && (ch->op.started)) {
        if (ch->op.sz == 2) {
            u16 value;
            if (ch->op.src_addr >= 0x02000000) {
                value = GBA_mainbus_read(this, ch->op.src_addr, 2, ch->op.src_access, 1);
                ch->io.open_bus = (value << 16) | value;
            } else {
                if (ch->op.dest_addr & 2) {
                    value = ch->io.open_bus >> 16;
                } else {
                    value = ch->io.open_bus & 0xFFFF;
                }
                this->waitstates.current_transaction++;
            }

            GBA_mainbus_write(this, ch->op.dest_addr, 2, ch->op.dest_access, value);
        }
        else {
            if (ch->op.src_addr >= 0x02000000)
                ch->io.open_bus = GBA_mainbus_read(this, ch->op.src_addr, 4, ch->op.src_access, 1);
            else
                this->waitstates.current_transaction++;
            GBA_mainbus_write(this, ch->op.dest_addr, 4, ch->op.dest_access, ch->io.open_bus);
        }

        // TODO: fix this
        ch->op.src_access = ARM7P_sequential | ARM7P_dma;
        ch->op.dest_access = ARM7P_sequential | ARM7P_dma;

        ch->op.src_addr += ch->src_add;
        ch->op.dest_addr += ch->dest_add;
        ch->op.word_count = (ch->op.word_count - 1) & ch->op.word_mask;
        if (ch->op.word_count == 0) {
            ch->op.started = 0; // Disable running
            this->dma_bit_mask &= ~(1 << ch->num);

            if (ch->io.irq_on_end) {
                raise_irq_for_dma(this, num);
            }

            if (ch->io.repeat && ch->io.start_timing != 0) {
                if (ch->op.is_sound)
                    ch->op.word_count = 4;
                else {
                    ch->op.word_count = ch->io.word_count & ch->op.word_mask;
                    if (ch->op.word_count == 0)
                        ch->op.word_count = ch->op.word_mask + 1;
                }
                if ((ch->io.dest_addr_ctrl == 3) && (!ch->op.is_sound))
                    ch->op.dest_addr = ch->io.dest_addr & (ch->io.transfer_size ? ~3 : ~1);
            }
            else {
                ch->io.enable = 0;
            }
        }
        return 1;
    }
    return 0;
}

u32 GBA_DMA_go(struct GBA *this) {
    for (u32 i = 0; i < 4; i++) {
        if (dma_go_ch(this, i)) return 1;
    }
    return 0;
}

void GBA_DMA_start(struct GBA *gba, struct GBA_DMA_ch *ch)
{
    ch->op.started = 1;
    gba->dma_bit_mask |= 1 << ch->num;
}

void GBA_DMA_cnt_written(struct GBA *this, struct GBA_DMA_ch *ch, u32 old_enable)
{
    if (ch->io.enable) {
        if (!old_enable) { // 0->1
            ch->op.dest_addr = ch->io.dest_addr;
            ch->op.src_addr = ch->io.src_addr;

            if ((ch->io.start_timing == 3) && ((ch->num == 1) || (ch->num == 2))) {
                ch->op.sz = 4;
                ch->op.word_count = 4;
                ch->op.src_addr &= ~3;
                ch->op.dest_addr &= ~3;
                ch->op.is_sound = 1;
            }
            else {
                ch->op.is_sound = 0;
                u32 mask = ch->io.transfer_size ? ~3 : ~1;
                ch->op.src_addr &= mask;
                ch->op.dest_addr &= mask;
                ch->op.word_count = ch->io.word_count & ch->op.word_mask;
                if (ch->op.word_count == 0)
                    ch->op.word_count = ch->op.word_mask + 1;
                if (ch->io.start_timing == 0) GBA_DMA_start(this, ch);
            }
        } // 1->1 really do nothing
    }
    else { // 1->0 or 0->0?
        ch->op.started = 0;
        this->dma_bit_mask &= ~(1 << ch->num);
    }
}

void GBA_DMA_on_modify_write(struct GBA_DMA_ch *ch)
{
    static const i32 src[2][4] = { { 2, -2, 0, 0}, {4, -4, 0, 0} };
    static const i32 dst[2][4] = { { 2, -2, 0, 2}, {4, -4, 0, 4} };
    ch->dest_add = ch->op.is_sound ? 0 : dst[ch->io.transfer_size][ch->io.dest_addr_ctrl];
    ch->src_add = src[ch->io.transfer_size][ch->io.src_addr_ctrl];
}
