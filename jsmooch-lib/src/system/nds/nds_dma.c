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

void NDS_DMA_init(struct NDS *this)
{
    for (u32 i = 0; i < 4; i++) {
        struct NDS_DMA_ch *ch = &this->dma7[i];
        ch->num = i;
        ch = &this->dma9[i];
        ch->num = i;
    }
}

static void dma7_irq(void *ptr, u64 key, u64 cur_time, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    NDS_update_IF7(this, NDS_IRQ_DMA0 + key);
}

static void dma7_go_ch(struct NDS *this, struct NDS_DMA_ch *ch) {
    u32 num_transfer = 0;
    u32 ct = this->waitstates.current_transaction;
    while((ch->io.enable) && (ch->op.started)) {
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
            num_transfer++;
        }
        else {
            if (ch->op.src_addr >= 0x02000000)
                ch->io.open_bus = NDS_mainbus_read7(this, ch->op.src_addr, 4, ch->op.src_access, 1);
            else
                this->waitstates.current_transaction++;
            NDS_mainbus_write7(this, ch->op.dest_addr, 4, ch->op.dest_access, ch->io.open_bus);
            num_transfer++;
        }

        ch->op.src_access = ARM7P_sequential | ARM7P_dma;
        ch->op.dest_access = ARM7P_sequential | ARM7P_dma;

        switch(ch->io.src_addr_ctrl) {
            case 0: // increment
                ch->op.src_addr = (ch->op.src_addr + ch->op.sz) & 0x0FFFFFFF;
                if (ch->num == 0) ch->op.src_addr &= 0x07FFFFFF;
                break;
            case 1: // decrement
                ch->op.src_addr = (ch->op.src_addr - ch->op.sz) & 0x0FFFFFFF;
                if (ch->num == 0) ch->op.src_addr &= 0x07FFFFFF;
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
                if (ch->num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
            case 1: // decrement
                ch->op.dest_addr = (ch->op.dest_addr - ch->op.sz) & 0x0FFFFFFF;
                if (ch->num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
            case 2: // constant
                break;
            case 3: // increment & reload on repeat
                ch->op.dest_addr = (ch->op.dest_addr + ch->op.sz) & 0x0FFFFFFF;
                if (ch->num < 3) ch->op.dest_addr &= 0x07FFFFFF;
                break;
        }

        ch->op.word_count = (ch->op.word_count - 1) & ch->op.word_mask;
        if (ch->op.word_count == 0) {
            ch->op.started = 0; // Disable
            ch->op.first_run = 0;
            if (ch->io.irq_on_end) {
                scheduler_add_or_run_abs(&this->scheduler, NDS_clock_current7(this) + num_transfer, ch->num, this, &dma7_irq, NULL);
            }

            if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
        }
    }
    this->waitstates.current_transaction = ct;
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
    dma7_go_ch(this, ch);
}

// -------------################
static void dma9_irq(void *ptr, u64 key, u64 cur_time, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    NDS_update_IF9(this, NDS_IRQ_DMA0 + key);
}


static void dma9_go_ch(struct NDS *this, struct NDS_DMA_ch *ch) {
    u32 num_transfer = 0;
    u32 ct = this->waitstates.current_transaction;
    while ((ch->io.enable) && (ch->op.started)) {
        if (ch->op.sz == 2) {
            u16 value;
            if (ch->op.src_addr >= 0x02000000) {
                value = NDS_mainbus_read9(this, ch->op.src_addr, 2, ch->op.src_access, 0);
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
            num_transfer++;
        }
        else {
            if (ch->op.src_addr >= 0x02000000)
                ch->io.open_bus = NDS_mainbus_read9(this, ch->op.src_addr, 4, ch->op.src_access, 0);
            else
                this->waitstates.current_transaction++;
            NDS_mainbus_write9(this, ch->op.dest_addr, 4, ch->op.dest_access, ch->io.open_bus);
            num_transfer++;
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
        ch->op.chunks = (ch->op.chunks - 1);
        if ((ch->io.start_timing == NDS_DMA_GE_FIFO) && (ch->op.chunks <= 0) && (ch->op.word_count != 0)) {
            ch->op.started = 0;
        }
        if (ch->op.word_count == 0) {
            ch->op.started = 0; // Disable
            ch->op.first_run = 0;
            if (ch->io.irq_on_end)
                scheduler_add_or_run_abs(&this->scheduler, NDS_clock_current9(this) + num_transfer, ch->num, this, &dma9_irq, NULL);

            if (!ch->io.repeat) {
                ch->io.enable = 0;
            }
        }
    }
    this->waitstates.current_transaction = ct;
}

void NDS_dma9_start(struct NDS *this, struct NDS_DMA_ch *ch, u32 i)
{
    dbgloglog(NDS_CAT_DMA_START, DBGLS_INFO, "DMA9 %d start", i);
    if (ch->io.start_timing == NDS_DMA_GE_FIFO) {
        ch->op.started = 1;
        if (ch->op.first_run) {
            ch->op.first_run = 0;
            u32 mask = ch->io.transfer_size ? ~3 : ~1;
            mask &= 0x0FFFFFFF;
            ch->op.dest_addr = ch->io.dest_addr & mask;
            ch->op.src_addr = ch->io.src_addr & mask;
            ch->op.word_count = ch->io.word_count;
            ch->op.sz = 4;
            ch->op.word_mask = 0x1FFFFF;
        }
        if (ch->op.word_count == 0) {
            ch->op.started = 0;
            printf("\nABT GE FIFO!");
            return;
        }
        ch->op.dest_access = ARM9P_nonsequential | ARM9P_dma;
        ch->op.src_access = ARM9P_nonsequential | ARM9P_dma;

        ch->op.chunks = 112;
        return;
    }

    ch->op.started = 1;
    u32 mask = ch->io.transfer_size ? ~3 : ~1;
    mask &= 0x0FFFFFFF;
    if (ch->op.first_run) {
        ch->op.dest_addr = ch->io.dest_addr & mask;
        ch->op.src_addr = ch->io.src_addr & mask;
    }
    else if (ch->io.dest_addr_ctrl == NDS_DMA_HBLANK) {
        ch->op.dest_addr = ch->io.dest_addr & mask;
    }
    ch->op.word_count = ch->io.word_count;
    ch->op.sz = ch->io.transfer_size ? 4 : 2;
    if ((ch->io.start_timing == NDS_DMA_START_OF_DISPLAY) && this->clock.ppu.y == 194) {
        ch->io.enable = 0;
        return;
    }
    ch->op.word_mask = 0x1FFFFF;
    ch->op.dest_access = ARM9P_nonsequential | ARM9P_dma;
    ch->op.src_access = ARM9P_nonsequential | ARM9P_dma;
    dma9_go_ch(this, ch);
}

// #############################

void NDS_check_dma7_at_vblank(struct NDS *this)
{
    // Check if any DMA channels are at enabled=1, started=0, time=hblank
    for (u32 i = 0; i < 4; i++) {
        struct NDS_DMA_ch *ch = &this->dma7[i];
        if ((ch->io.enable) && (!ch->op.started) && (ch->io.start_timing == NDS_DMA_VBLANK)) {
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
        if (ch->io.enable && !ch->op.started && (ch->io.start_timing == NDS_DMA_HBLANK)) {
            NDS_dma9_start(this, ch, i);
        }
    }
}

void NDS_trigger_dma7_if(struct NDS *this, u32 start_timing)
{
    struct NDS_DMA_ch *ch;
    assert(start_timing <= 3);
    if (start_timing >= 3) {
        static int a = 1;
        if (a) {
            a = 0;
            printf("\nWARN DMA MODE %d for ARM7 NOT IMPLEMENTED", start_timing);
        }
        return;
    }
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