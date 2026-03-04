//
// Created by RadDad772 on 3/13/24.
//

#include <cassert>
#include "stdio.h"
#include <cstdlib> // abort()
#include "dreamcast.h"
#include "dc_bus.h"
#include "holly.h"
#include "maple.h"
#include "controller.h"
namespace DC::MAPLE {

core::core(DC::core *parent) :
    bus(parent),
    ports {MAPLE::PORT(parent), MAPLE::PORT(parent), MAPLE::PORT(parent), MAPLE::PORT(parent)}
    {}

PORT::PORT(DC::core *parent) : bus(parent) {
    device_kind = DK_NONE;
    device_ptr = nullptr;
    port = this;
    read_device = nullptr;
    write_device = nullptr;

}

void core::write(u32 addr, u64 val, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/maple_writes.cpp"
        case 0x005F6C88: // SB_MSHTCL
            if (val & 1) bus->maple.vblank_repeat_trigger = 0;
            return;
    }
    printf("\nYEAH GOT HERE SORRY DAWG3");
    *success = false;
}

u64 core::read(u32 addr, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/maple_reads.cpp"
    }
    printf("\nYEAH GOT HERE SORRY DAWG4");
    *success = false;
    return 0;
}

u32 PORT::read(u32* more)
{
    switch(device_kind) {
        case DK_NONE:
            *more = 0;
            return 0xFFFFFFFF;
        case DK_CONTROLLER:
            return read_device(device_ptr, more);
    }
    NOGOHERE;
    return 0;
}

void PORT::write(u32 data)
{
    switch(device_kind) {
        case DK_NONE:
            return;
        case DK_CONTROLLER:
            write_device(device_ptr, data);
            return;
    }
    NOGOHERE;
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
    u32 u{};
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


void core::dma_init()
{
    if (SB_MDEN == 0) {
        printf("\nCan't enable maple dma if disabled globally!");
        return;
    }
    if (vblank_repeat_trigger && SB_MDTSEL == 1) {
        printf("\nSkipping MapleDMA due to vblank repeat trigger");
        return;
    }
    if (SB_MDTSEL == 1) vblank_repeat_trigger = 1;
    printf("\nMAPLE DMA TRANSFER cycle:%llu", bus->trace_cycles);
    u32 caddr = SB_MDSTAR;
    for (u32 i = 0; i < 0xFFFF; i++) {
        MAPLE_CMD cmd;
        cmd.u = bus->mainbus_read(caddr, 4, false);
        caddr+=4;
        printf("\nMAPLE CMD %d (%08x): %08x (pattern:%03d transfer_len:%d port: %d)", i, caddr, cmd.u, cmd.pattern, cmd.transfer_len, cmd.port_select);
        assert(cmd.pattern == 0b000);

        u32 receieve_ptr = bus->mainbus_read(caddr, 4, 0);
        caddr += 4;

        for (u32 tx_index = 0; tx_index < (cmd.transfer_len+1); tx_index++) {
            u32 data = bus->mainbus_read(caddr, 4, 0);
            caddr += 4;
            ports[cmd.port_select].write(data);
        }

        // Now receive
        u32 more;
        for (u32 rx_ct = 0; rx_ct < 128; rx_ct++) {
            u32 data = ports[cmd.port_select].read(&more);
            //printf("\n%08x more:%d write to %08x", data, more, receieve_ptr);
            bus->mainbus_write(receieve_ptr, data, 4);
            receieve_ptr += 4;
            if ((rx_ct == 0) && (data == 0xFFFFFFFF)) break;
            if (!more) {
                break;
            }

        }
        if (cmd.end_flag) break;
    }
    bus->holly.raise_interrupt(HOLLY::hirq_maple_dma, -1);
    SB_MDST = 0;
}

}