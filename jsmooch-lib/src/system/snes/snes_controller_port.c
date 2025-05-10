//
// Created by . on 5/9/25.
//

#include "snes_controller_port.h"
#include "snes_bus.h"
#include "component/controller/snes/snes_joypad.h"

void SNES_controllerport_latch(struct SNES *snes, u32 num, u32 val)
{
    struct SNES_controller_port *p = &snes->r5a22.controller_port[num];
    if (p->ptr) {
        switch(p->kind) {
            case SNES_CK_none:
                return;
            case SNES_CK_standard:
                return SNES_joypad_latch(p->ptr, val);
            default:
                printf("\nUNKNOWN CONTROLLER KIND! DEATH!");
                return;
        }
    }
}

u32 SNES_controllerport_data(struct SNES *snes, u32 num)
{
    struct SNES_controller_port *p = &snes->r5a22.controller_port[num];
    if (p->ptr) {
        switch(p->kind) {
            case SNES_CK_none:
                return 0;
            case SNES_CK_standard:
                return SNES_joypad_data(p->ptr);
            default:
                printf("\nUNKNOWN CONTROLLER KINd2!");
                return 0;
        }
    }
    return 0;
}

void SNES_controllerport_connect(struct SNES_controller_port *this, enum SNES_controller_kinds kind, void *ptr)
{
    this->kind = kind;
    this->ptr = ptr;
}

void SNES_controllerport_delete(struct SNES_controller_port *this)
{
    this->kind = SNES_CK_none;
    this->ptr = NULL;
}
