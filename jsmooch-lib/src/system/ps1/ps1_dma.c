//
// Created by . on 2/15/25.
//

#include <stdlib.h>

#include "ps1_bus.h"
#include "ps1_dma.h"

static void do_dma_linked_list(struct PS1 *ps1, struct PS1_DMA_channel *ch)
{
    u32 addr = ch->base_addr & 0x1FFFFC;
    if (ch->direction == PS1D_to_ram) {
        printf("\nInvalid DMA direction for linked list mode: to RAM");
        return;
    }

    if (ch->num != PS1DP_GPU) {
        printf("\nInvalid DMA dest for linked list mode %d", ch->num);
        return;
    }

    while(1) {
        u32 header = PS1_mainbus_read(ps1, addr, 4, 1);
        u32 copies = (header >> 24) & 0xFF;

        while (copies > 0) {
            addr = (addr + 4) & 0x1FFFFC;
            u32 cmd = PS1_mainbus_read(ps1, addr, 4, 1);
            PS1_GPU_write_gp0(&ps1->gpu, cmd);

            copies--;
        }

        // Following Mednafen on this?
        if ((header & 0x800000) != 0)
            break;

        addr = header & 0x1FFFFC;
    }
}

static u32 ch_transfer_size(struct PS1_DMA_channel *this)
{
    u32 bs = this->block_size;
    u32 bc = this->block_count;
    switch(this->sync) {
        case PS1D_manual:
            return bs;
        case PS1D_request:
            return bc * bs;
        case PS1D_linked_list:
            return -1;
    }
    NOGOHERE;
    return 0;
}

static void do_dma_block(struct PS1 *ps1, struct PS1_DMA_channel *ch)
{
    u32 step = (ch->step == PS1D_increment) ? 4 : -4;
    u32 addr = ch->base_addr;
    u32 copies = ch_transfer_size(ch);
    if (copies == -1) {
        printf("\nCouldn't decide DMA transfer size");
        return;
    }

    while (copies > 0) {
        u32 cur_addr = addr & 0x1FFFFC;
        u32 src_word = 0;
        switch(ch->direction) {
            case PS1D_from_ram:
                src_word = PS1_mainbus_read(ps1, cur_addr, 4, 1);
                switch(ch->num) {
                    case PS1DP_GPU:
                        PS1_GPU_write_gp0(&ps1->gpu, src_word);
                        break;
                    case PS1DP_MDEC_in: {
                        static u32 e = 0;
                        if (!e) {
                            printf("\nWARN DMA write to MDEC in!");
                            e = 1;
                        }
                        //this->ps1!.MDEC.command(src_word);
                        break; }
                    case PS1DP_SPU: {// Ignore SPU transfer for now
                        static u32 e = 0;
                        if (!e) {
                            printf("\nWARN DMA write to SPU!");
                            e = 1;
                        }
                        break; }
                    default:
                        printf("UNHANDLED DMA PORT! %d", ch->num);
                        return;
                }
                break;
            case PS1D_to_ram:
                switch(ch->num) {
                    case PS1DP_OTC:
                        src_word = (copies == 1) ? 0xFFFFFF : ((addr - 4) & 0x1FFFFF);
                        break;
                    case PS1DP_GPU:
                        printf("unimplemented DMA GPU read");
                        src_word = 0;
                        break;
                    case PS1DP_cdrom: {
                        static u32 e = 0;
                        if (!e) {
                            e = 1;
                            printf("\nWARN DMA read CDROM not implement!");
                        }
                        //src_word = this->cdrom.dma_read_word();
                        break; }
                    case PS1DP_MDEC_out:
                        src_word = 0;
                        break;
                    default:
                        printf("UNKNOWN DMA PORT %d", ch->num);
                        src_word = 0;
                        break;
                }
                PS1_mainbus_write(ps1, cur_addr, 4, src_word);
                break;
        }
        addr = (addr + step) & 0xFFFFFFFF;
        copies--;
    }
}

static void do_dma(struct PS1 *ps1, struct PS1_DMA_channel *ch)
{
    // Executes DMA for a channel
    // We'll just do an instant copy for now
    if (ch->sync == PS1D_linked_list)
        do_dma_linked_list(ps1, ch);
    else
        do_dma_block(ps1, ch);
    ch->trigger = 0;
    ch->enable = 0;
}

static u32 active(struct PS1_DMA_channel *this)
{
    u32 enable = (this->sync == PS1D_manual) ? this->trigger : 1;
    return enable && this->enable;
}

static u32 get_control(struct PS1_DMA_channel *this) {
    return this->direction |
           (this->step << 1) |
           (this->chop << 8) |
           (this->sync << 9) |
           (this->chop_dma_size << 16) |
           (this->chop_cpu_size << 20) |
           (this->enable << 24) |
           (this->trigger << 28) |
           (this->unknown << 29);
}

static void set_control(struct PS1_DMA_channel *this, u32 val)
{
    this->direction = (val & 1) ? PS1D_from_ram : PS1D_to_ram;
    this->step = ((val >> 1) & 1) ? PS1D_decrement : PS1D_increment;
    this->chop = (val >> 8) & 1;
    switch ((val >> 9) & 3) {
        case 0:
            this->sync = PS1D_manual;
        break;
        case 1:
            this->sync = PS1D_request;
        break;
        case 2:
            this->sync = PS1D_linked_list;
        break;
        default:
            printf("\nUnknown DMA mode 3");
        break;
    }
    this->chop_dma_size = (val >> 16) & 7;
    this->chop_cpu_size = (val >> 20) & 7;
    this->enable = (val >> 24) & 1;
    this->trigger = (val >> 28) & 1;
    this->unknown = (val >> 29) & 3;
}

static u32 irq_status(struct PS1 *this)
{
    return +(this->dma.irq_force || (this->dma.irq_enable && (this->dma.irq_flags_ch & this->dma.irq_enable_ch)));
}

u32 PS1_DMA_read(struct PS1 *this, u32 addr, u32 sz)
{
    u32 ch_num = ((addr - 0x80) & 0x70) >> 4;
    u32 reg = (addr & 0x0F);
    switch(ch_num) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: {
            struct PS1_DMA_channel *ch = &this->dma.channels[ch_num];
            switch(reg) {
                case 0:
                    return ch->base_addr;
                case 4:
                    return (ch->block_count << 16) | ch->block_size;
                case 8:
                    return get_control(ch);
                default:
                    printf("Unimplemented per-channel DMA register: %d %d %08x", ch_num, reg, addr);
                    return 0xFFFFFFFF;
            }
            break; }
        case 7:
            switch(reg) {
                case 0: // DPCR - DMA control 0x1F8010F0:
                    return this->dma.control;
                case 4: // DPIR - DMA interrupt control 0x1F8010F4:
                    return this->dma.unknown1 | (this->dma.irq_force << 15) | (this->dma.irq_enable_ch << 16) |
                           (this->dma.irq_enable << 23) | (this->dma.irq_flags_ch << 24) | (irq_status(this) << 31);
                default:
                    printf("Unimplemented per-channel DMA register read2 %d %d %08x", ch_num, reg, addr);
                    return 0xFFFFFFFF;
            }

        default:
            printf("Unhandled DMA read %08x", addr);
    }
    return 0xFFFFFFFF;
}

void PS1_DMA_write(struct PS1 *this, u32 addr, u32 sz, u32 val)
{
    u32 ch_num = ((addr - 0x80) & 0x70) >> 4;
    u32 reg = (addr & 0x0F);
    u32 ch_activated = 0;
    struct PS1_DMA_channel *ch;
    switch(ch_num) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            ch = &this->dma.channels[ch_num];
            switch(reg) {
                case 0:
                    ch->base_addr = val & 0xFFFFF;
                    break;
                case 4:
                    ch->block_size = val & 0xFFFF;
                    ch->block_count = (val >> 16) & 0xFFFF;
                    break;
                case 8:
                    set_control(ch, val);
                    break;
                default:
                    printf("Unimplemented per-channel DMA register write: %d %d %08x: %08x", ch_num, reg, addr, val);
                    return;
            }
            if (active(ch)) do_dma(this, ch);
            break;
        case 7: // common registers
            switch(reg) {
                case 0: // DPCR - DMA Control register 0x1F8010F0:
                    this->dma.control = val;
                    return;
                case 4: // DICR - DMA Interrupt register case 0x1F8010F4:
                    // Low 5 bits are R/w, we don't know what
                    this->dma.unknown1 = val & 31;
                    this->dma.irq_force = (val >> 15) & 1;
                    this->dma.irq_enable_ch = (val >> 16) & 0x7F;
                    this->dma.irq_enable = (val >> 23) & 1;
                    u32 to_ack = (val >> 24) & 0x3F;
                    this->dma.irq_flags_ch &= (to_ack ^ 0x3F);
                    return;
                default:
                    printf("Unhandled DMA write: %d %d %08x", ch_num, reg, addr);
                    return;
            }
    }
}
