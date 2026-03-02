//
// Created by RadDad772 on 3/18/24.
//

#include "helpers/debug.h"
//#include "g2.h"
#include "dc_bus.h"

namespace DC {
static void pvr_dma_init(u32 addr, u64 val)
{
    if (val & 1)
        printf("\nPVR DMA!?!?!?");
}


void core::update_dma_triggers(u32 addr, u64 val)
{
    if (val & 1) printf("\nG2 TRIGGER TO DMA %08x", addr);
}

void core::G2_write(u32 addr, u64 val, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g2_writes.cpp"
    }
    *success = false;
    printf("\nUnhandled G2 reg write %02x val %04llx bits %d", addr, val, sz);
}

u64 core::G2_read(u32 addr, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g2_reads.cpp"
    }
    *success = false;
    printf("\nUnhandled G2 reg read %02x sz %d", addr, sz);
    return 0;
}

void core::G2_write_ADST(u64 val)
{
    if (val & 1)
        printf("\nAICA transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", g2.SB_ADSTAR, g2.SB_ADSTAG, g2.SB_ADLEN.transfer_len, g2.SB_ADLEN.end_restart);
}

void core::G2_write_E1ST(u64 val)
{
    if (val & 1)
        printf("\nEXT1 transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", g2.SB_E1STAR, g2.SB_E1STAG, g2.SB_E1LEN.transfer_len, g2.SB_E1LEN.end_restart);
}

void core::G2_write_E2ST(u64 val)
{
    if (val & 1)
        printf("\nEXT2 transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", g2.SB_E2STAR, g2.SB_E2STAG, g2.SB_E2LEN.transfer_len, g2.SB_E2LEN.end_restart);
}

void core::G2_write_DDST(u64 val)
{
    if (val & 1)
        printf("\nAICA transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", g2.SB_ADSTAR, g2.SB_ADSTAG, g2.SB_ADLEN.transfer_len, g2.SB_ADLEN.end_restart);
}
}