//
// Created by . on 5/5/25.
//

#include "gba_dma.h"
#include "gba_bus.h"

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

        ch->op.src_access = ARM7P_sequential | ARM7P_dma;
        ch->op.dest_access = ARM7P_sequential | ARM7P_dma;

        switch(ch->io.src_addr_ctrl) {
            case 0: // increment
                ch->op.src_addr = (ch->op.src_addr + ch->op.sz) & 0x0FFFFFFF;
                if (num == 0) ch->op.src_addr &= 0x07FFFFFF;
                break;
            case 1: // decrement
                ch->op.src_addr = (ch->op.src_addr - ch->op.sz) & 0x0FFFFFFF;
                if (num == 0) ch->op.src_addr &= 0x07FFFFFF;
                break;
            case 2: // constant
                break;
            case 3: // prohibited
                printf("\nPROHIBITED!");
                break;
        }

        switch(ch->io.dest_addr_ctrl) {
            case 0: // increment
                ch->op.dest_addr = (ch->op.dest_addr + ch->op.sz) & 0x0FFFFFFF;
                if (num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
            case 1: // decrement
                ch->op.dest_addr = (ch->op.dest_addr - ch->op.sz) & 0x0FFFFFFF;
                if (num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
            case 2: // constant
                break;
            case 3: // increment & reload on repeat
                ch->op.dest_addr = (ch->op.dest_addr + ch->op.sz) & 0x0FFFFFFF;
                if (num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
        }

        ch->op.word_count = (ch->op.word_count - 1) & ch->op.word_mask;
        if (ch->op.word_count == 0) {
            ch->op.started = 0; // Disable
            ch->op.first_run = 0;
            if (ch->io.irq_on_end) {
                raise_irq_for_dma(this, num);
            }

            if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
        }
        return 1;
    }
    return 0;
}

u32 GBA_dma_go(struct GBA *this) {
    for (u32 i = 0; i < 4; i++) {
        if (dma_go_ch(this, i)) return 1;
    }
    return 0;
}

void GBA_dma_start(struct GBA_DMA_ch *ch, u32 i, u32 is_sound)
{
    ch->op.started = 1;
    u32 mask = ch->io.transfer_size ? ~3 : ~1;
    if (is_sound) mask = ~3;
    mask &= 0x0FFFFFFF;
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
    if ((i == 1) || (i == 2)) {
        printf("\n%d | src:%07X | dst: %07X", i, ch->op.src_addr, ch->op.dest_addr);
    }
}
