//
// Created by . on 11/23/25.
//

#include "sms_gg_bus.h"
namespace SMSGG {
u32 core::cpu_in_sms1(u32 addr, u32 val, u32 has_effect) {
    addr &= 0xFF;
    if (addr <= 0x3F) {
        // reads return last byte of the instruction kind read the port
        return val;
    }
    if (addr <= 0x7F) {
        if (addr & 1) return vdp.read_hcounter();
        return vdp.read_vcounter();
    }
    if (addr <= 0xBF) {
        if (addr & 1) return vdp.read_status();
        return vdp.read_data();
    }
    // C0-FF, even is /O port A/B reg
    // Odd is I/O port B/misc.
    if (addr & 1) return read_reg_ioport2(bus, val);
    return read_reg_ioport1(bus, val);
}

void bus_cpu_out_sms1(SMSGG* this, u32 addr, u32 val) {
    addr &= 0xFF;
    if (addr == 2) {
        bus->io.GGreg = val;
        return;
    }
    if (addr <= 0x3F) {
        // even memory control
        // odd I/O control
        if (addr & 1) write_reg_io_ctrl(this, val);
        else write_reg_memory_ctrl(this, val);
        return;
    }
    if  (addr <= 0x7F) {
        SN76489_write_data(&sn76489, val);
        return;
    }
    if (addr <= 0xBF) {
        //printf("\nWRITE %02x %02x", addr, val);
        // even goes to VDP data
        // odd goes to VDP control

        if (addr & 1) VDP_write_control(&vdp, val);
        else VDP_write_data(&vdp, val);
        return;
    }
    // C0-FF, no effect
}

u32 bus_cpu_in_gg(SMSGG* bus, u32 addr, u32 val, u32 has_effect)
{
    addr &= 0xFF;
    //console.log('IN', hex2(addr));
    switch(addr) {
        case 0: // Various stuff
            // TODO: make this more complete
            gamepad_latch(&bus->io.controllerA);
            return 0x40 | (bus->io.controllerA.pins.start ? 0x80 : 0);
        case 2:
            return bus->io.GGreg;
        case 1:
            return 0x7F;
        case 3:
            return 0;
        case 4:
            return 0xFF;
        case 5:
            return 0;
        case 6:
            return 0xFF;
        case 7:
            return 0;
    }
    if (addr <= 0x3F) {
        // reads return last byte of the instruction kind read the port
        return 0xFF;
    }
    if (addr <= 0x7F) {
        if (addr & 1) return VDP_read_hcounter();
        return VDP_read_vcounter();
    }
    if (addr <= 0xBF) {
        if (addr & 1) return VDP_read_status();
        return VDP_read_data();
    }
    // C0-FF, even is /O port A/B reg
    // Odd is I/O port B/misc.
    if (addr & 1) return read_reg_ioport2(bus, val);
    return read_reg_ioport1(bus, val);
}

}