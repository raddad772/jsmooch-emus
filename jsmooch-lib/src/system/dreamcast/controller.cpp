//
// Created by RadDad772 on 3/13/24.
//

#include <cstring>
#include <cstdio>
#include "helpers/physical_io.h"
#include "helpers/pack.h"
#include "controller.h"
#include "dc_bus.h"

namespace DC {
PACK_BEGIN
struct controller_info {
    u32 func{};
    u32 sub_func[3]{};
    u8 region{};
    u8 direction{};
    char name[30]{};
    char license[60]{};
    u16 standby_power{};
    u16 max_power{};
} PACK_END;


static constexpr u32 reply5[28] = {
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

void pcontroller_write(void *ptr, u32 data) {
    auto *th = static_cast<controller *>(ptr);
    th->write(data);
}

u32 pcontroller_read(void *ptr, u32* more) {
    auto *th = static_cast<controller *>(ptr);
    return th->read(more);
}

void core::connect_controller(int portnum, DC::controller* which)
{
    auto *p = &maple.ports[portnum];
    p->device_kind = MAPLE::DK_CONTROLLER;
    p->device_ptr = static_cast<void *>(which);
    p->write_device = &pcontroller_write;
    p->read_device = &pcontroller_read;
}

void controller::write(u32 data) {
    printf("\n\nDC controller written data:%08x index:%d", data, cmd_index);
    if (state == DCC_tx) {
        printf("\nDC CONTROLLER GOT COMMAND DURING TRANSMIT SEQUENCE!");
        return;
    }
    cmd[cmd_index] = data;
    if (cmd_index == 0) {
        de_cmd = (data >> 0) & 0xFF;
        dest_AP = (data >> 16) & 0xFF;
        src_AP = (data >> 8) & 0xFF;
        data_size = (data >> 24) & 0xFF;
    }
    cmd_index++;
    if (cmd_index >= data_size) {
        state = DCC_tx;
        cmd_index = 0;
        switch(de_cmd) {
            case 1:
                printf("\nREPLY TO DECMD WITH 5");
                reply_cmd = 5;
                break;
            default:
                printf("\nUNKNOWN WAY TO REPLY! %d", de_cmd);
                reply_cmd = 0xFFFFFFFF;
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

u32 controller::read(u32* more) {
    //printf("\nDC controller read");
    if ((state == DCC_rx) || (reply_cmd == 0xFFFFFFFF)) {
        printf("\nATTEMPT TO READ CONTROLLER WHEN WE EXPECED A WRITE");
        *more = 0;
        return 0xFFFFFFFF;
    }
    u32 ret = 0xFFFFFFFF;
    switch(reply_cmd) {
        case 5: // identify yourself!
            if (cmd_index == 0) {
                reply_len = 28;
                ret = (5 << 24); // CMD 5
                controller_info ci = {
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
                memcpy(&reply_buf, &ci, sizeof(controller_info));
                swap_32s(reply_buf + 4, 28 - 4);
            }
            break;
        default:
            printf("\nUNKNOWN DC CONTROLLER COMMAND!!!!");
            break;
    }
    if (cmd_index == 0) {
        ret |= (dest_AP) << 16; // SEND ER (us). add bits to this to have sub-devices
        ret |= (src_AP | 1) << 8; // RECEIVER
        ret |= reply_len;
    }
    else {
        ret = reply_buf[cmd_index-1];
    }
    cmd_index++;
    if (cmd_index > reply_len) {
        state = DCC_rx;
        *more = 0;
        cmd_index = 0;
    }
    else
        *more = 1;
    return ret;
}

void controller::setup_pio(physical_io_device *d, u32 num, const char*name, bool connected)
{
    d->init(HID_CONTROLLER, connected, connected, true, true);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;

    JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "a", DBCID_co_fire1);
    pio_new_button(cnt, "b", DBCID_co_fire2);
    pio_new_button(cnt, "start", DBCID_co_start);

    pio = d;
}

}