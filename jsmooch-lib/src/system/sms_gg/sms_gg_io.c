//
// Created by Dave on 2/9/2024.
//

#include "assert.h"
#include "stdio.h"
#include "sms_gg_io.h"
#include "sms_gg.h"
#include "component/controller/sms/sms_gamepad.h"

void SMSGG_controller_port_init(struct SMSGG_controller_port* this, enum jsm_systems variant, u32 which)
{
    this->attached_device = NULL;
    this->TR_level = this->TH_level = this->TR_direction = this->TH_direction = 1;

    this->variant = variant;
    this->which = which;
}

u32 SMSGG_controller_port_read(struct SMSGG_controller_port* this)
{
    if (this->attached_device == NULL) return 0x7F;
    return SMSGG_gamepad_read((struct SMSGG_gamepad*)this->attached_device);
}

void SMSGG_controller_port_reset(struct SMSGG_controller_port* this)
{
    this->TR_level = this->TH_level = 1;
    this->TR_direction = this->TH_direction = 1;
}

static u32 read_reg_ioport1(struct SMSGG *this, u32 val)
{
    /*
    D7 : Port B DOWN pin input
     D6 : Port B UP pin input
     D5 : Port A TR pin input
     D4 : Port A TL pin input
     D3 : Port A RIGHT pin input
     D2 : Port A LEFT pin input
     D1 : Port A DOWN pin input
     D0 : Port A UP pin input
     */
    u32 pinsA = SMSGG_controller_port_read(&this->io.portA);
    u32 pinsB;
    if (this->variant != SYS_GG)
        pinsB = SMSGG_controller_port_read(&this->io.portB);
    else
        pinsB = 0x7F;
    u32 r = (pinsA & 0x3F);
    r |= (pinsB & 3) << 6;
    if (this->io.portA.TR_direction == 0) {
        r = (r & 0xDF) | (this->io.portA.TR_level << 5);
    }
    return r;
}

static u32 read_reg_ioport2(struct SMSGG *this, u32 val)
{
    /*
     D7 : Port B TH pin input
     D6 : Port A TH pin input
     D5 : Unused
     D4 : RESET button (1= not pressed, 0= pressed)
     D3 : Port B TR pin input
     D2 : Port B TL pin input
     D1 : Port B RIGHT pin input
     D0 : Port B LEFT pin input
     */
    u32 pinsA = SMSGG_controller_port_read(&this->io.portA);
    u32 pinsB;
    if (this->variant != SYS_GG)
        pinsB = SMSGG_controller_port_read(&this->io.portB);
    else
        pinsB = 0x7F;

    //this->reset_button.latch();
    u32 r = (pinsB >> 2) & 0x0F;
    //r |= this->reset_button.value << 4;
    r |= 0x20 | 0x10;
    r |= (pinsA & 0x40);
    r |= (pinsB & 0x40) << 1;
    if (this->io.portB.TR_direction == 0) r = (r & 0xF7) | (this->io.portB.TR_level << 3);
    if (this->io.portA.TH_direction == 0) r = (r & 0xBF) | (this->io.portA.TH_level << 6);
    if (this->io.portB.TH_direction == 0) r = (r & 0x7F) | (this->io.portB.TH_level << 7);
    return r;
}

void write_reg_memory_ctrl(struct SMSGG* this, u32 val) {
    if (this->variant != SYS_GG) {
        SMSGG_mapper_sega_set_BIOS(&this->mapper, ((val & 8) >> 3) ^ 1); // 1 = disabled, 0 = enabled

        this->mapper.enable_cart = ((val & 0x40) >> 6) ^ 1;
        //assert(1==0);
        fflush(stdout);

        //if (!this->mapper.enable_cart) dbg.break();
    }
    this->io.disable = (val & 4) >> 2;
}

void write_reg_io_ctrl(struct SMSGG* this, u32 val) {
        u32 thl1 = this->io.portA.TH_level;
        u32 thl2 = this->io.portB.TH_level;

        this->io.portA.TR_direction = val & 1;
        this->io.portA.TH_direction = (val & 2) >> 1;
        this->io.portB.TR_direction = (val & 4) >> 2;
        this->io.portB.TH_direction = (val & 8) >> 3;

        if (this->region == REGION_JAPAN) {
            // Japanese sets level to direction
            this->io.portA.TH_level = this->io.portA.TH_direction;
            this->io.portB.TH_level = this->io.portB.TH_direction;
        } else {
            // others allow us to just set stuff directly I guess
            this->io.portA.TR_level = (val & 0x10) >> 4;
            this->io.portA.TH_level = (val & 0x20) >> 5;
            this->io.portB.TR_level = (val & 0x40) >> 6;
            this->io.portB.TH_level = (val & 0x80) >> 7;
        }
}


u32 SMSGG_bus_cpu_in_sms1(struct SMSGG* bus, u32 addr, u32 val, u32 has_effect) {
    addr &= 0xFF;
    if (addr <= 0x3F) {
        // reads return last byte of the instruction kind read the port
        return val;
    }
    if (addr <= 0x7F) {
        if (addr & 1) return SMSGG_VDP_read_hcounter(&bus->vdp);
        return SMSGG_VDP_read_vcounter(&bus->vdp);
    }
    if (addr <= 0xBF) {
        if (addr & 1) return SMSGG_VDP_read_status(&bus->vdp);
        return SMSGG_VDP_read_data(&bus->vdp);
    }
    // C0-FF, even is /O port A/B reg
    // Odd is I/O port B/misc.
    if (addr & 1) return read_reg_ioport2(bus, val);
    return read_reg_ioport1(bus, val);
}

void SMSGG_bus_cpu_out_sms1(struct SMSGG* this, u32 addr, u32 val) {
    addr &= 0xFF;
    if (addr == 2) {
        this->io.GGreg = val;
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
        SN76489_write_data(&this->sn76489, val);
        return;
    }
    if (addr <= 0xBF) {
        //printf("\nWRITE %02x %02x", addr, val);
        // even goes to VDP data
        // odd goes to VDP control

        if (addr & 1) SMSGG_VDP_write_control(&this->vdp, val);
        else SMSGG_VDP_write_data(&this->vdp, val);
        return;
    }
    // C0-FF, no effect
}

u32 SMSGG_bus_cpu_in_gg(struct SMSGG* bus, u32 addr, u32 val, u32 has_effect)
{
    addr &= 0xFF;
    //console.log('IN', hex2(addr));
    switch(addr) {
        case 0: // Various stuff
            // TODO: make this more complete
            if (bus->variant == SYS_GG)
                return 0x40 | (bus->io.pause_button->state ? 0x80 : 0);
            else
                return 0x40;
        case 2:
            return bus->io.GGreg;
        case 1:
        case 3:
        case 4:
        case 5:
            return 0;
        case 6:
        case 7:
            return 0;
    }
    if (addr <= 0x3F) {
        // reads return last byte of the instruction kind read the port
        return 0xFF;
    }
    if (addr <= 0x7F) {
        if (addr & 1) return SMSGG_VDP_read_hcounter(&bus->vdp);
        return SMSGG_VDP_read_vcounter(&bus->vdp);
    }
    if (addr <= 0xBF) {
        if (addr & 1) return SMSGG_VDP_read_status(&bus->vdp);
        return SMSGG_VDP_read_data(&bus->vdp);
    }
    // C0-FF, even is /O port A/B reg
    // Odd is I/O port B/misc.
    if (addr & 1) return read_reg_ioport2(bus, val);
    return read_reg_ioport1(bus, val);
}