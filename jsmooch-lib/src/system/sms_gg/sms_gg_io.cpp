//
// Created by Dave on 2/9/2024.
//

#include <cassert>
#include <cstdio>
#include "sms_gg_io.h"
#include "sms_gg.h"
#include "sms_gg_bus.h"
#include "component/controller/sms/sms_gamepad.h"

namespace SMSGG {

void controller_port::init(jsm::systems in_variant, u32 in_which)
{
    attached_device = nullptr;
    TR_level = TH_level = TR_direction = TH_direction = 1;

    variant = in_variant;
    which = in_which;
}

u32 controller_port::read()
{
    if (attached_device == nullptr) return 0x7F;
    return attached_device->read();
}

void controller_port::reset()
{
    TR_level = TH_level = 1;
    TR_direction = TH_direction = 1;
}

u32 core::read_reg_ioport1(u32 val)
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
    u32 pinsA = io.portA.read();
    u32 pinsB;
    if (variant != jsm::systems::GG)
        pinsB = io.portB.read();
    else
        pinsB = 0x7F;
    u32 r = (pinsA & 0x3F);
    r |= (pinsB & 3) << 6;
    if (io.portA.TR_direction == 0) {
        r = (r & 0xDF) | (io.portA.TR_level << 5);
    }
    return r;
}

u32 core::read_reg_ioport2(u32 val)
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
    u32 pinsA = io.portA.read();
    u32 pinsB;
    if (variant != jsm::systems::GG)
        pinsB = io.portB.read();
    else
        pinsB = 0x7F;

    //reset_button.latch();
    u32 r = (pinsB >> 2) & 0x0F;
    //r |= reset_button.value << 4;
    r |= 0x20 | 0x10;
    r |= (pinsA & 0x40);
    r |= (pinsB & 0x40) << 1;
    if (io.portB.TR_direction == 0) r = (r & 0xF7) | (io.portB.TR_level << 3);
    if (io.portA.TH_direction == 0) r = (r & 0xBF) | (io.portA.TH_level << 6);
    if (io.portB.TH_direction == 0) r = (r & 0x7F) | (io.portB.TH_level << 7);
    return r;
}

void core::write_reg_memory_ctrl(u32 val) {
    if (variant != jsm::systems::GG) {
        //mapper_sega_set_BIOS(&mapper, ((val & 8) >> 3) ^ 1); // 1 = disabled, 0 = enabled

        //mapper.enable_cart = ((val & 0x40) >> 6) ^ 1;
        //assert(1==0);
        fflush(stdout);

        //if (!mapper.enable_cart) dbg.break();
    }
    io.disable = (val & 4) >> 2;
}

void core::write_reg_io_ctrl(u32 val) {
        u32 thl1 = io.portA.TH_level;
        u32 thl2 = io.portB.TH_level;

        io.portA.TR_direction = val & 1;
        io.portA.TH_direction = (val & 2) >> 1;
        io.portB.TR_direction = (val & 4) >> 2;
        io.portB.TH_direction = (val & 8) >> 3;

        if (region == jsm::regions::JAPAN) {
            // Japanese sets level to direction
            io.portA.TH_level = io.portA.TH_direction;
            io.portB.TH_level = io.portB.TH_direction;
        } else {
            // others allow us to just set stuff directly I guess
            io.portA.TR_level = (val & 0x10) >> 4;
            io.portA.TH_level = (val & 0x20) >> 5;
            io.portB.TR_level = (val & 0x40) >> 6;
            io.portB.TH_level = (val & 0x80) >> 7;
        }
}

}