//
// Created by . on 1/20/25.
//

#include "helpers/debugger/debugger.h"
#include "nds_dma.h"
#include "nds_bus.h"
#include "nds_irq.h"
#include "nds_debugger.h"

// TODO
// 1. make gbatek DMA9 noted changes
// 3. make them execute during CPU execute parts
// 4. add registers
// 5. add interrupt registers
// 6. add timers/registers

static void raise_irq_for_dma7(struct NDS *this, u32 num)
{
    NDS_update_IF7(this, NDS_IRQ_DMA0 + num);
}

static void raise_irq_for_dma9(struct NDS *this, u32 num)
{
    NDS_update_IF9(this, NDS_IRQ_DMA0 + num);
}

static u32 dma7_go_ch(struct NDS *this, u32 num) {
    struct NDS_DMA_ch *ch = &this->dma7[num];
    if (ch->run_counter && ch->io.enable) {
        ch->run_counter--;
        if (ch->run_counter == 0) NDS_dma7_start(this, ch, num);
        else return 0;
    }
    if ((ch->io.enable) && (ch->op.started)) {
        if (ch->op.sz == 2) {
            u16 value;
            if (ch->op.src_addr >= 0x02000000) {
                value = NDS_mainbus_read7(this, ch->op.src_addr, 2, ch->op.src_access, 1);
                ch->io.open_bus = (value << 16) | value;
            } else {
                if (ch->op.dest_addr & 2) {
                    value = ch->io.open_bus >> 16;
                } else {
                    value = ch->io.open_bus & 0xFFFF;
                }
                this->waitstates.current_transaction++;
            }

            NDS_mainbus_write7(this, ch->op.dest_addr, 2, ch->op.dest_access, value);
        }
        else {
            if (ch->op.src_addr >= 0x02000000)
                ch->io.open_bus = NDS_mainbus_read7(this, ch->op.src_addr, 4, ch->op.src_access, 1);
            else
                this->waitstates.current_transaction++;
            NDS_mainbus_write7(this, ch->op.dest_addr, 4, ch->op.dest_access, ch->io.open_bus);
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
                raise_irq_for_dma7(this, num);
            }

            if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
        }
        return 1;
    }
    return 0;
}

u32 NDS_dma7_go(struct NDS *this) {
    for (u32 i = 0; i < 4; i++) {
        if (dma7_go_ch(this, i)) return 1;
    }
    return 0;
}

void NDS_dma7_start(struct NDS *this, struct NDS_DMA_ch *ch, u32 i)
{
    dbgloglog(NDS_CAT_DMA_START, DBGLS_INFO, "DMA7 %d start", i);
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
}

// -------------################

static u32 dma9_go_ch(struct NDS *this, u32 num) {
    struct NDS_DMA_ch *ch = &this->dma9[num];
    if (ch->run_counter && ch->io.enable) {
        ch->run_counter--;
        if (ch->run_counter == 0) NDS_dma9_start(this, ch, num);
        else return 0;
    }
    if ((ch->io.enable) && (ch->op.started)) {
        if (ch->op.sz == 2) {
            u16 value;
            if (ch->op.src_addr >= 0x02000000) {
                value = NDS_mainbus_read9(this, ch->op.src_addr, 2, ch->op.src_access, 1);
                ch->io.open_bus = (value << 16) | value;
            } else {
                if (ch->op.dest_addr & 2) {
                    value = ch->io.open_bus >> 16;
                } else {
                    value = ch->io.open_bus & 0xFFFF;
                }
                this->waitstates.current_transaction++;
            }

            NDS_mainbus_write9(this, ch->op.dest_addr, 2, ch->op.dest_access, value);
        }
        else {
            if (ch->op.src_addr >= 0x02000000)
                ch->io.open_bus = NDS_mainbus_read9(this, ch->op.src_addr, 4, ch->op.src_access, 1);
            else
                this->waitstates.current_transaction++;
            NDS_mainbus_write9(this, ch->op.dest_addr, 4, ch->op.dest_access, ch->io.open_bus);
        }

        ch->op.src_access = ARM9P_sequential | ARM9P_dma;
        ch->op.dest_access = ARM9P_sequential | ARM9P_dma;
        switch(ch->io.src_addr_ctrl) {
            case 0: // increment
                ch->op.src_addr = (ch->op.src_addr + ch->op.sz) & 0x0FFFFFFF;
                break;
            case 1: // decrement
                ch->op.src_addr = (ch->op.src_addr - ch->op.sz) & 0x0FFFFFFF;
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
                break;
            case 1: // decrement
                ch->op.dest_addr = (ch->op.dest_addr - ch->op.sz) & 0x0FFFFFFF;
                break;
            case 2: // constant
                break;
            case 3: // increment & reload on repeat
                ch->op.dest_addr = (ch->op.dest_addr + ch->op.sz) & 0x0FFFFFFF;
                break;
        }

        ch->op.word_count = (ch->op.word_count - 1) & ch->op.word_mask;
        if (ch->op.word_count == 0) {
            ch->op.started = 0; // Disable
            ch->op.first_run = 0;
            if (ch->io.irq_on_end) {
                raise_irq_for_dma9(this, num);
            }

            if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
        }
        return 1;
    }
    return 0;
}

u32 NDS_dma9_go(struct NDS *this) {
    for (u32 i = 0; i < 4; i++) {
        if (dma9_go_ch(this, i)) return 1;
    }
    return 0;
}

void NDS_dma9_start(struct NDS *this, struct NDS_DMA_ch *ch, u32 i)
{
    dbgloglog(NDS_CAT_DMA_START, DBGLS_INFO, "DMA9 %d start", i);
    ch->op.started = 1;
    u32 mask = ch->io.transfer_size ? ~3 : ~1;
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
    ch->op.word_mask = 0x1FFFFF;
    ch->op.dest_access = ARM9P_nonsequential | ARM9P_dma;
    ch->op.src_access = ARM9P_nonsequential | ARM9P_dma;
}

// #############################

void NDS_check_dma7_at_vblank(struct NDS *this)
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        struct NDS_DMA_ch *ch = &this->dma7[i];
        if ((ch->io.enable) && (!ch->op.started) && (ch->io.start_timing == 1)) {
            NDS_dma7_start(this, ch, i);
        }
    }
}

void NDS_check_dma9_at_vblank(struct NDS *this)
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        struct NDS_DMA_ch *ch = &this->dma9[i];
        if ((ch->io.enable) && (!ch->op.started) && (ch->io.start_timing == 1)) {
            NDS_dma9_start(this, ch, i);
        }
    }
}

void NDS_check_dma9_at_hblank(struct NDS *this)
{
    if (this->clock.ppu.y >= 192) return;
    for (u32 i = 0; i < 4; i++) {
        struct NDS_DMA_ch *ch = &this->dma9[i];
        if (ch->io.enable && !ch->op.started && (ch->io.start_timing == 2)) {
            NDS_dma9_start(this, ch, i);
        }
    }
}

void NDS_trigger_dma7_if(struct NDS *this, u32 start_timing)
{
    struct NDS_DMA_ch *ch;
    for (u32 i = 0; i < 4; i++) {
        ch = &this->dma7[i];
        if (ch->io.enable && ch->io.start_timing == start_timing)
            NDS_dma7_start(this, ch, i);
    }
}

void NDS_trigger_dma9_if(struct NDS *this, u32 start_timing)
{
    struct NDS_DMA_ch *ch;
    for (u32 i = 0; i < 4; i++) {
        ch = &this->dma9[i];
        if (ch->io.enable && ch->io.start_timing == start_timing)
            NDS_dma9_start(this, ch, i);
    }
}