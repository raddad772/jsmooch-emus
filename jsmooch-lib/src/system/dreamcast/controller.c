//
// Created by RadDad772 on 3/13/24.
//

#include <string.h>
#include <stdio.h>
#include "helpers/physical_io.h"
#include "controller.h"
#include "dreamcast.h"

void DC_controller_init(struct DC_controller* this)
{
    for (u32 i = 0; i < 16; i++)
        this->cmd[i] = 0;
    this->state = DCC_rx;
    this->cmd_index = 0;
    memset(&this->input_waiting, 0, sizeof(this->input_waiting));
}

void DC_controller_connect(struct DC* console, int portnum, struct DC_controller* which)
{
    struct MAPLE_port* p = &console->maple.ports[portnum];
    p->device_kind = MAPLE_CONTROLLER;
    p->device_ptr = (void *)which;
    p->write_device = &DC_controller_write;
    p->read_device = &DC_controller_read;
}

void DC_controller_write(void *ptr, u32 data)
{
    struct DC_controller* this = (struct DC_controller*)ptr;
    //printf("\nDC controller written %08x", data);
    if (this->state == DCC_tx) {
        printf("\nDC CONTROLLER GOT COMMAND DURING TRANSMIT SEQUENCE!");
        return;
    }
    this->cmd[this->cmd_index] = data;
    if (this->cmd_index == 0) {
        this->de_cmd = (data >> 24) & 0xFF;
        this->dest_AP = (data >> 16) & 0xFF;
        this->src_AP = (data >> 8) & 0xFF;
        this->data_size = data & 0xFF;
    }
    this->cmd_index++;
    if (this->cmd_index >= this->data_size) {
        this->state = DCC_tx;
        this->cmd_index = 0;
    }
}

static const u8 reply0[] = {
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x0F, 0x06, 0xFF,
        0, 0, 0, 0,
        0, 0, 0, 0
};
const u32* reply0_32 = (u32*)reply0;

// upper 8 bits command
// next 8 Destination AP
// next 8 source AP
// next 8 data size
u32 DC_controller_read(void *ptr, u32* more)
{
    struct DC_controller* this = (struct DC_controller*)ptr;
    //printf("\nDC controller read");
    if (this->state == DCC_rx) {
        printf("\nATTEMPT TO READ CONTROLLER WHEN WE EXPECED A WRITE");
        return 0xFFFFFFFF;
    }
    u32 ret = 0xFFFFFFFF;
    switch(this->de_cmd) {
        case 0: // identify yourself!
            if (this->cmd_index == 0)
                this->reply_len = 4;
            ret = reply0_32[this->cmd_index];
            break;
        default:
            printf("\nUNKNOWN DC CONTROLLER COMMAND!!!!");
            break;
    }
    this->cmd_index++;
    if (this->cmd_index >= this->reply_len) {
        this->state = DCC_rx;
        *more = 0;
        this->cmd_index = 0;
    }
    else
        *more = 1;
    return ret;
}
