//
// Created by . on 2/15/25.
//

#include <cstdlib>

#include "ps1_bus.h"
#include "ps1_dma.h"
#include "ps1_debugger.h"

namespace PS1 {
void DMA_channel::do_linked_list()
{
    //printf("\nDo linked list CH%d", num);
    u32 addr = base_addr & 0x1FFFFC;
    dbgloglog_bus(PS1D_DMA_CH0+num, DBGLS_INFO, "CH:%d LINKED LIST  DEST:%08x  IE:%d", num, addr, (bus->dma.irq.IE >> num) & 1);
    if (direction == D_to_ram) {
        printf("\nInvalid DMA direction for linked list mode: to RAM");
        return;
    }

    if (num != DP_GPU) {
        printf("\nInvalid DMA dest for linked list mode %d", num);
        return;
    }
    i32 lnum = 65536;
    while(lnum-- > 0) {
        u32 header = bus->mainbus_read(addr, 4, true);
        u32 copies = (header >> 24) & 0xFF;

        while (copies > 0) {
            addr = (addr + 4) & 0x1FFFFC;
            u32 cmd = bus->mainbus_read(addr, 4, true);
            bus->gpu.write_gp0(cmd);

            copies--;
        }

        // Following Mednafen on this?
        if ((header & 0x800000) != 0)
            break;

        addr = header & 0x1FFFFC;
        if (lnum == 1) printf("\n(DMA) warning: infinite linked list terminating");
    }
    if (num == 3) printf("\nDMA Finish transfer of %d", 65536 - lnum);
    bus->dma.complete_transfer(num);
}

u32 DMA_channel::transfer_size() const
{
    u32 bs = block_size;
    u32 bc = block_count;
    switch(sync) {
        case D_manual:
            return bs;
        case D_request:
            return bc * bs;
        case D_linked_list:
            return -1;
        default:
    }
    NOGOHERE;
    return 0;
}

void DMA_channel::do_block()
{
    u32 mstep = (step == D_increment) ? 4 : -4;
    u32 addr = base_addr;
    u32 copies = transfer_size();;
    dbgloglog_bus(PS1D_DMA_CH0+num, DBGLS_INFO, "CH:%d BLOCK  DEST:%08x  LEN:%d bytes  IE:%d", num, addr, copies, (bus->dma.irq.IE >> num) & 1);
    //printf("\nDo block ch%d base_addr:%08x copies:%d", num, base_addr, copies);
    if (copies == -1) {
        printf("\nCouldn't decide DMA transfer size");
        return;
    }
    //printf("\nDMA ch:%d direction:%d copies:%d base_addr:%05x", num, direction, copies, base_addr);

    while (copies > 0) {
        u32 cur_addr = addr & 0x1FFFFC;
        u32 src_word = 0;
        switch(direction) {
            case D_from_ram:
                src_word = bus->mainbus_read(cur_addr, 4, true);
                switch(num) {
                    case DP_GPU:
                        bus->gpu.write_gp0(src_word);
                        break;
                    case DP_MDEC_in: {
                        static u32 e = 0;
                        if (!e) {
                            printf("\nWARN DMA write to MDEC in!");
                            e = 1;
                        }
                        //ps1!.MDEC.command(src_word);
                        break; }
                    case DP_SPU: {// Ignore SPU transfer for now
                        bus->spu.DMA_write(src_word);
                        break; }
                    default:
                        printf("\nUNHANDLED DMA PORT! %d", num);
                        return;
                }
                break;
            case D_to_ram:
                switch(num) {
                    case DP_OTC:
                        src_word = (copies == 1) ? 0xFFFFFF : ((addr - 4) & 0x1FFFFF);
                        break;
                    case DP_GPU:
                        src_word = bus->gpu.get_gpuread();
                        break;
                    case DP_cdrom: {
                        src_word = bus->cdrom.mainbus_read(0x1F801802, 4, true);
                        break; }
                    case DP_MDEC_out:
                        src_word = 0;
                        break;
                    case DP_SPU:
                        src_word = bus->spu.DMA_read();
                        break;
                    default:
                        printf("\nUNKNOWN DMA PORT %d", num);
                        src_word = 0;
                        break;
                }
                //printf("\nDMA WRITE %08x: %08x", cur_addr, src_word);
                bus->mainbus_write(cur_addr, 4, src_word);
                break;
            default:
                printf("\nUnsupported direction %d", direction);
                break;
        }
        addr = (addr + mstep) & 0xFFFFFFFF;
        copies--;
    }
    bus->dma.complete_transfer(num);
}

void DMA::complete_transfer(u32 num) {
    // TODO: schedule for future
    u32 bit = 1 << num;
    if ((irq.IE & bit) && !(irq.IF & bit)) {
        irq.IF |= bit;
        update_IRQs();
    }
}

void DMA_channel::do_dma()
{
    // Executes DMA for a channel
    // We'll just do an instant copy for now
    if (sync == D_linked_list)
        do_linked_list();
    else
        do_block();
    trigger = 0;
    enable = 0;
}

bool DMA_channel::active()
{
    u32 menable = (sync == D_manual) ? trigger : 1;
    return master_enable && enable && menable;
}

u32 DMA_channel::get_control() {

    u32 v = direction |
           (step << 1) |
           (chop << 8) |
           (sync << 9) |
           (chop_dma_size << 16) |
           (chop_cpu_size << 20) |
           (enable << 24) |
           (trigger << 28) |
           (unknown << 29);
    //printf("\nRETURN CTRL: %08x", v);
    return v;
}

void DMA_channel::set_control(u32 val)
{
    if (num == 6) {
        step = D_decrement;
        enable = (val >> 24) & 1;
        direction = D_to_ram;
        chop = 0;
        sync = D_manual;
        chop_dma_size = 0;
        chop_cpu_size = 0;
        trigger = (val >> 28) & 1;
        unknown = (val >> 29) & 2;
        return;
    }

    direction = (val & 1) ? D_from_ram : D_to_ram;
    step = ((val >> 1) & 1) ? D_decrement : D_increment;
    chop = (val >> 8) & 1;
    switch ((val >> 9) & 3) {
        case 0:
            sync = D_manual;
            break;
        case 1:
            sync = D_request;
            break;
        case 2:
            sync = D_linked_list;
            break;
        default:
            printf("\nUnknown DMA mode 3");
            break;
    }
    chop_dma_size = (val >> 16) & 7;
    chop_cpu_size = (val >> 20) & 7;
    enable = (val >> 24) & 1;
    trigger = (val >> 28) & 1;
    unknown = (val >> 29) & 3;
}

u32 DMA::read(u32 addr, u32 sz)
{
    // 1f8010F0
    if (sz < 4) sz = 4;
    u32 l3 = addr & 3;
    addr &= 0xFFFFFFFC;
    u32 ch_num = ((addr - 0x80) & 0x70) >> 4;
    u32 reg = (addr & 0x0F);
    i64 v = -1;
    switch(ch_num) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: {
            auto *ch = &channels[ch_num];
            switch(reg) {
                case 0:
                    v = ch->base_addr;
                    break;
                case 4:
                    v = (ch->block_count << 16) | ch->block_size;
                    break;
                case 8:
                    v = ch->get_control();
                    break;
                default:
                    printf("\nUnimplemented per-channel DMA register: %d %d %08x", ch_num, reg, addr);
                    return 0xFFFFFFFF;
            }
            break; }
        case 7:
            switch(reg) {
                case 0: // DPCR - DMA control 0x1F8010F0:
                    v = control;
                    break;
                case 4: // DICR - DMA interrupt control 0x1F8010F4:
                    v = irq.master_enable << 23;
                    v |= irq.IF << 24;
                    v |= irq.IE << 16;
                    v |= irq.level << 31;
                    break;
                default:
                    printf("\nUnimplemented per-channel DMA register read2 %d %d %08x", ch_num, reg, addr);
                    return 0xFFFFFFFF;
            }

        default:
    }
    if (v == -1) {
        printf("\nUnhandled DMA read %08x", addr);
    }
    return v >> (l3 * 8);
}

void DMA::reset() {
     write(0x1F8010F0, 2, 0x07654321);
}

void DMA::write(u32 addr, u32 sz, u32 val)
{
    //printf("\nWR DMA addr:%04x sz:%d val:%04x", addr, sz, val);
    const u32 l3 = addr & 3;
    addr &= 0x1FFFFFFC; // 32-bit read/writes only, force-align (and shift after)
    u32 ch_num = ((addr - 0x80) & 0x70) >> 4;
    u32 reg = (addr & 0x0F);
    u32 ch_activated = 0;
    val <<= (l3 * 8);
    if (sz < 4) sz = 4;
    switch(ch_num) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: {
            auto *ch = &channels[ch_num];
            switch(reg) {
                case 0:
                    //printf("\nBASE ADDRESS WRITE CH:%d VAL:%08x", ch_num, val);
                    //fflush(stdout);
                    ch->base_addr = val & 0xFFFFFF;
                    //if (ch->base_addr == 0x58124) dbg_break("BAD DMA BASE ADDR", 0);
                    break;
                case 4:
                    ch->block_size = val & 0xFFFF;
                    if (ch->block_size == 0) ch->block_size = 0x10000;
                    ch->block_count = (val >> 16) & 0xFFFF;
                    break;
                case 8:
                    ch->set_control(val);
                    break;
                default:
                    printf("\nUnimplemented per-channel DMA register write: %d %d %08x: %08x", ch_num, reg, addr, val);
                    return;
            }
            if (ch->active()) ch->do_dma();
            break; }
        case 7: // common registers
            switch(reg) {
                case 0: {// DPCR - DMA Control register 0x1F8010F0:
                    control = val;
                    i32 bit = 3;
                    for (u32 ch = 0; ch < 7; ch++) {
                        channels[ch].master_enable = (val >> bit) & 1;
                        bit += 4;
                    }
                    return; }
            case 4: {// DICR - DMA Interrupt register case 0x1F8010F4:
                    // Low 5 bits are R/w, we don't know what
                    irq.master_enable = (val >> 23) & 1;
                    u32 to_ack = (val >> 24) & 0b1111111;
                    to_ack ^= 0b1111111; // Invert bits
                    irq.IF &= to_ack;
                    irq.IE = (val >> 16) & 0b1111111;
                    irq.mode = val & 0b1111111;
                    update_IRQs();
                    return; }
                default:
                    printf("\nUnhandled DMA write: %d %d %08x", ch_num, reg, addr);
                    return;
            }
    }
}

void DMA::update_IRQs() {
    u32 old = irq.level;
    irq.level = irq.master_enable && (irq.IF & irq.IE);
    if (old != irq.level) {
        bus->set_irq(IRQ_DMA, irq.level);
    }
}

}