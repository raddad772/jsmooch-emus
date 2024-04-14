//
// Created by RadDad772 on 3/13/24.
//

#include "assert.h"
#include "stdio.h"
#include "dreamcast.h"
#include "holly.h"
#include "maple.h"
#include "controller.h"

void MAPLE_port_init(struct MAPLE_port* this)
{
    this->device_kind = MAPLE_NONE;
    this->device_ptr = NULL;
    this->port = this;
    this->read_device = NULL;
    this->write_device = NULL;
}

void maple_write(struct DC* this, u32 addr, u64 val, u32 sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/maple_writes.c"
        case 0x005F6C88: // SB_MSHTCL
            if (val & 1) this->maple.vblank_repeat_trigger = 0;
            return;
    }
    printf("\nYEAH GOT HERE SORRY DAWG");
    *success = 0;
}

u64 maple_read(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/maple_reads.c"
    }
    printf("\nYEAH GOT HERE SORRY DAWG2");
    *success = 0;
    return 0;
}

static u32 maple_port_in(struct DC* this, u32 port, u32* more)
{
    struct MAPLE_port* p = &this->maple.ports[port];
    switch(p->device_kind) {
        case MAPLE_NONE:
            *more = 0;
            return 0xFFFFFFFF;
        case MAPLE_CONTROLLER:
            return p->read_device(p->device_ptr, more);
    }
}

static void maple_port_out(struct DC* this, u32 port, u32 data)
{
    struct MAPLE_port* p = &this->maple.ports[port];
    switch(p->device_kind) {
        case MAPLE_NONE:
            return;
        case MAPLE_CONTROLLER:
            p->write_device(p->device_ptr, data);
            return;
    }
}

union MAPLE_CMD {
    struct {
        u32 transfer_len: 8;
        u32 pattern: 3;
        u32 : 5;
        u32 port_select: 2;
        u32 : 13;
        u32 end_flag: 1;
    };
    u32 u;
};

u32 bin3[8] = {
        0,
        1,
        10,
        11,
        100,
        101,
        110,
        111
};


void maple_dma_init(struct DC* this)
{
    if (this->maple.SB_MDEN == 0) {
        printf("\nCan't enable maple dma if disabled globally!");
        return;
    }
    if (this->maple.vblank_repeat_trigger && this->maple.SB_MDTSEL == 1) {
        printf("\nSkipping MapleDMA due to vblank repeat trigger");
        return;
    }
    if (this->maple.SB_MDTSEL == 1) this->maple.vblank_repeat_trigger = 1;
    //printf("\nMAPLE DMA TRANSFER %08x", this->maple.SB_MDSTAR);
    u32 caddr = this->maple.SB_MDSTAR;
    for (u32 i = 0; i < 0xFFFF; i++) {
        union MAPLE_CMD cmd;
        cmd.u = DCread((void *)this, caddr, 4, 0);
        caddr+=4;
        //printf("\nMAPLE CMD %d (%08x): %08x (pattern:%03d transfer_len:%d port: %d)", i, caddr, cmd.u, cmd.pattern, cmd.transfer_len, cmd.port_select);
        assert(cmd.pattern == 0b000);

        u32 receieve_ptr = DCread((void *)this, caddr, 4, 0);
        caddr += 4;

        for (u32 tx_index = 0; tx_index < (cmd.transfer_len+1); tx_index++) {
            u32 data = DCread((void *)this, caddr, 4, 0);
            caddr += 4;
            maple_port_out(this, cmd.port_select, data);
        }

        // Now receive
        u32 more;
        for (u32 rx_ct = 0; rx_ct < 128; rx_ct++) {
            u32 data = maple_port_in(this, cmd.port_select, &more);
            //printf("\nMAPLE DATA RECV %08x", data);
            DCwrite((void*)this, receieve_ptr, data, 4);
            receieve_ptr += 4;
            if ((rx_ct == 0) && (data == 0xFFFFFFFF)) break;
            if (!more) {
                break;
            }

        }
        if (cmd.end_flag) break;
    }
    holly_raise_interrupt(this, hirq_maple_dma, -1);
    this->maple.SB_MDST = 0;
}

