//
// Created by RadDad772 on 3/13/24.
//

#include <string.h>
#include <stdio.h>
#include "helpers/physical_io.h"
#include "helpers/pack.h"
#include "controller.h"
#include "dreamcast.h"

PACK_BEGIN
struct controller_info {
    u32 func;
    u32 sub_func[3];
    u8 region;
    u8 direction;
    char name[30];
    char license[60];
    u16 standby_power;
    u16 max_power;
} PACK_END;


static u32 reply5[28] = {
        0x01000000, // capability - controller
        0x000f06fe, // sub-capability 00F060FE is basically all the buttons and triggers and sticks
        0, 0, // 2 empty sub-capabilities
        0xff004472,
        0x65616d63,
        0x61737420,
        0x436f6e74,
        0x726f6c6c,
        0x65722020,
        0x20202020,
        0x20202020,
        0x50726f64,
        0x75636564,
        0x20427920,
        0x6f722055,
        0x6e646572,
        0x204c6963,
        0x656e7365,
        0x2046726f,
        0x6d205345,
        0x47412045,
        0x4e544552,
        0x50524953,
        0x45532c4c,
        0x54442e20,
        0x20202020,
        0xae01f401
};

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
    printf("\n\nDC controller written data:%08x index:%d", data, this->cmd_index);
    if (this->state == DCC_tx) {
        printf("\nDC CONTROLLER GOT COMMAND DURING TRANSMIT SEQUENCE!");
        return;
    }
    this->cmd[this->cmd_index] = data;
    if (this->cmd_index == 0) {
        this->de_cmd = (data >> 0) & 0xFF;
        this->dest_AP = (data >> 16) & 0xFF;
        this->src_AP = (data >> 8) & 0xFF;
        this->data_size = (data >> 24) & 0xFF;
    }
    this->cmd_index++;
    if (this->cmd_index >= this->data_size) {
        this->state = DCC_tx;
        this->cmd_index = 0;
        switch(this->de_cmd) {
            case 1:
                printf("\nREPLY TO DECMD WITH 5");
                this->reply_cmd = 5;
                break;
            default:
                printf("\nUNKNOWN WAY TO REPLY! %d", this->de_cmd);
                this->reply_cmd = 0xFFFFFFFF;
                break;
        }
    }
}

// upper 8 bits command
// next 8 Destination AP
// next 8 source AP
// next 8 data size

#define SWAP32(x) (((x) & 0xFF) << 24) | (((x) & 0xFF00) << 8) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF000000) >> 24)

static void swap_32s(u32 *first, u32 num)
{
    for (u32 i = 0; i < num; i++) {
        *first = SWAP32(*first);
        first++;
    }
}


u32 DC_controller_read(void *ptr, u32* more)
{
    struct DC_controller* this = (struct DC_controller*)ptr;
    //printf("\nDC controller read");
    if ((this->state == DCC_rx) || (this->reply_cmd == 0xFFFFFFFF)) {
        printf("\nATTEMPT TO READ CONTROLLER WHEN WE EXPECED A WRITE");
        *more = 0;
        return 0xFFFFFFFF;
    }
    u32 ret = 0xFFFFFFFF;
    switch(this->reply_cmd) {
        case 5: // identify yourself!
            if (this->cmd_index == 0) {
                this->reply_len = 28;
                ret = (5 << 24); // CMD 5
                struct controller_info ci = (struct controller_info){
                        .func = 0x00000001,
                        .sub_func = { 0x000f06fe, 0, 0},
                        .region = 0xff,
                        .direction = 0,
                        .standby_power = 0x01ae,
                        .max_power = 0x01f4,
                };
                memset(ci.name, 0x20, 30);
                memset(ci.license, 0x20, 60);
                char *sptr = ci.name;
                u32 l = snprintf(sptr, 30, "Dreamcast Controller");
                sptr += l;
                *sptr = 0x20;
                sptr = ci.license;
                sptr += snprintf(ci.license, 60, "Produced By or Under License From SEGA ENTERPRISES,LTD.");
                *sptr = 0x20;
                memcpy(&this->reply_buf, &ci, sizeof(struct controller_info));
                swap_32s(this->reply_buf + 4, 28 - 4);
            }
            break;
        default:
            printf("\nUNKNOWN DC CONTROLLER COMMAND!!!!");
            break;
    }
    if (this->cmd_index == 0) {
        ret |= (this->dest_AP) << 16; // SEND ER (us). add bits to this to have sub-devices
        ret |= (this->src_AP | 1) << 8; // RECEIVER
        ret |= this->reply_len;
    }
    else {
        ret = this->reply_buf[this->cmd_index-1];
    }
    this->cmd_index++;
    if (this->cmd_index > this->reply_len) {
        this->state = DCC_rx;
        *more = 0;
        this->cmd_index = 0;
    }
    else
        *more = 1;
    return ret;
}
