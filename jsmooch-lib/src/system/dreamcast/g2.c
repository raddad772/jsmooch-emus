//
// Created by RadDad772 on 3/18/24.
//

#include "stdio.h"

#include "helpers_c/debug.h"
#include "g2.h"


static void pvr_dma_init(struct DC* this, u32 addr, u64 val)
{
    if (val & 1)
        printf("\nPVR DMA!?!?!?");
}


static void update_dma_triggers(struct DC* this, u32 addr, u64 val)
{
    //if (val & 1) printf("\nG2 TRIGGER TO DMA %08x", addr);
}

void G2_write(struct DC* this, u32 addr, u64 val, u32 bits, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g2_writes.c"
    }
    *success = 0;
    printf("\nUnhandled G2 reg write %02x val %04llx bits %d", addr, val, bits);
}

u64 G2_read(struct DC* this, u32 addr, u32 sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g2_reads.c"
    }
    *success = 0;
    printf("\nUnhandled G2 reg read %02x sz %d", addr, sz);
    return 0;
}

void G2_write_ADST(struct DC* this, u64 val)
{
    if (val & 1)
        printf("\nAICA transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", this->g2.SB_ADSTAR, this->g2.SB_ADSTAG, this->g2.SB_ADLEN.transfer_len, this->g2.SB_ADLEN.end_restart);
}

void G2_write_E1ST(struct DC* this, u64 val)
{
    if (val & 1)
        printf("\nEXT1 transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", this->g2.SB_E1STAR, this->g2.SB_E1STAG, this->g2.SB_E1LEN.transfer_len, this->g2.SB_E1LEN.end_restart);
}

void G2_write_E2ST(struct DC* this, u64 val)
{
    if (val & 1)
        printf("\nEXT2 transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", this->g2.SB_E2STAR, this->g2.SB_E2STAG, this->g2.SB_E2LEN.transfer_len, this->g2.SB_E2LEN.end_restart);
}

void G2_write_DDST(struct DC* this, u64 val)
{
    if (val & 1)
        printf("\nAICA transfer sys_addr:%08x g2_addr:%08x allocated_len:%d endrestart:%d", this->g2.SB_ADSTAR, this->g2.SB_ADSTAG, this->g2.SB_ADLEN.transfer_len, this->g2.SB_ADLEN.end_restart);
}
