//
// Created by . on 5/9/25.
//

#include "snes_controller_port.h"
#include "snes_bus.h"
#include "component/controller/snes/snes_joypad.h"

void SNES_controller_port::latch(u32 val)
{
    switch(kind) {
        case SNES_CK_none:
            return;
        case SNES_CK_standard: {
            SNES_joypad *jp = static_cast<SNES_joypad *>(ptr);
            return jp->latch(val); }
        default:
            printf("\nUNKNOWN CONTROLLER KIND! DEATH!");
            return;
        }
}

u32 SNES_controller_port::data()
{
    switch(kind) {
        case SNES_CK_none:
            return 0;
        case SNES_CK_standard: {
            SNES_joypad *jp = static_cast<SNES_joypad *>(ptr);
            return jp->data(); }
        default:
            printf("\nUNKNOWN CONTROLLER KINd2!");
            return 0;
    }
}

void SNES_controller_port::connect(SNES_controller_kinds in_kind, void *in_ptr)
{
    kind = in_kind;
    ptr = in_ptr;
}