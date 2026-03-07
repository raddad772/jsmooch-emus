//
// Created by . on 2/26/25.
//

#include <cstring>
#include "ps1_digital_pad.h"
#include "../ps1_bus.h"
#include "helpers/debug.h"

namespace PS1::SIO {
enum cmd_kinds {
    PCMD_read,
    PCMD_unknown
};

void digital_gamepad::latch_buttons()
{
    physical_io_device* p = pio;
    if (p->connected) {
        HID_digital_button* b;
        buttons[0] = 0;
        buttons[1] = 0;
        auto& bl = pio->controller.digital_buttons;
        // buttons are 0=pressed
#define B_GET(button_num, byte_num, bit_num) { b = &bl.at(button_num); buttons[byte_num] |= (b->state << bit_num); }
        B_GET(0, 0, 0); // select
        B_GET(1, 0, 3); // start
        B_GET(2, 0, 4); // up
        B_GET(3, 0, 5); // down
        B_GET(4, 0, 6); // left
        B_GET(5, 0, 7); // right
        B_GET(6, 1, 0); // L2
        B_GET(7, 1, 1); // R2
        B_GET(8, 1, 2); // L1
        B_GET(9, 1, 3); // R1
        B_GET(10, 1, 4); // triangle
        B_GET(11, 1, 5); // circle
        B_GET(12, 1, 6); // cross
        B_GET(13, 1, 7); // square
#undef B_GET
        //printf("\nBUTTONS: %02x", buttons[0]);
        buttons[0] ^= 0xFF;
        buttons[1] ^= 0xFF;
    }
    else {
        buttons[0] = 0xFF;
        buttons[1] = 0xFF;
    }
}

static void set_CS(void *ptr, u32 level, u64 clock_cycle) {
    auto *th = static_cast<digital_gamepad *>(ptr);
    //printf("\npad: set CS new:%d  old:%d", level, th->interface.CS);
    u32 old_CS = th->interface.CS;
    th->interface.CS = level;
    if (old_CS != th->interface.CS) {
        th->selected = 0;
        th->protocol_step = 0;
        printf("\nGAMEPAD SELECT DOWN!");
        if (th->interface.CS) {
            printif(ps1.pad, "\npad: CS 0->1, latch buttons");
            th->latch_buttons();
        }
        else {
            printif(ps1.pad, "\npad: CS 1->0");
            if (th->interface.ACK) {
                printif(ps1.pad, "\npad: ACK asserted");
                //if (still_sched && sch_id)
                //    scheduler_delete_if_exist(&bus->scheduler, sch_id);
                SIO0_device p = th->pio->id == 1 ? SIO0_controller1 : SIO0_controller2;
                th->bus->sio0.update_ACKs(p, 0);
            }
        }
    }
}

static void scheduler_call(void *ptr, u64 key, u64 current_clock, u32 jitter);

void digital_gamepad::schedule_ack(u64 clock_cycle, u64 time, u32 level)
{
    /*if (level == 1) {
        auto p = pio->id == 1 ? SIO0_controller1 : SIO0_controller2;
        bus->sio0.update_ACKs(p, 1);
        level = 0;
    }*/

    sch_id = bus->scheduler.add_or_run_abs(clock_cycle + time, level, this, &scheduler_call, &still_sched);
}

static void scheduler_call(void *ptr, u64 key, u64 current_clock, u32 jitter)
{
    digital_gamepad *th = static_cast<digital_gamepad *>(ptr);
    auto p = th->pio->id == 1 ? SIO0_controller1 : SIO0_controller2;
    //printf("\ncyc:%lld Callback execute ack: %lld", current_clock, key);
    th->interface.ACK = key;
    th->bus->sio0.update_ACKs(p, key);

    if (key) { // Also schedule to de-assert
        th->schedule_ack(current_clock-jitter, 96, 0);
    }
    else th->sch_id = 0;
}

static u8 sch_exchange_byte(void *ptr, u8 byte, u64 clock_cycle) {
    digital_gamepad *th = static_cast<digital_gamepad *>(ptr);
    return th->exchange_byte(byte, clock_cycle);
}

u8 digital_gamepad::exchange_byte(u8 byte, u64 clock_cycle) {
    if (!interface.CS) return 0xFF;

    if (protocol_step == 0) {
        if (byte == 0x01) {
            selected = 1;
            printf("\nSELECT GAMEPAD!");
            protocol_step++;

            //printf("\nSELECT PAD, DO ACK.");
            schedule_ack(clock_cycle, 96, 1);
            return 0xFF;
        }
    }

    u8 r = 0xFF;

    if (selected) {
        u32 do_ack = 0;
        printf("\nGP CHECK...");
        if (protocol_step == 1) { // send ID lo, recv Read Command (42h)
            switch (byte) {
                case 0x42:
                    r = 0x41;
                    cmd = PCMD_read;
                    break;
                default:
                    cmd = PCMD_unknown;
                    break;
            }
            cmd = (byte == 0x42) ? PCMD_read : PCMD_unknown;
            if (cmd == PCMD_unknown && byte != 0x43) printf("\nUnknown command %02x to controller %d", byte, pio->id);
            if (cmd == PCMD_read) do_ack = 1;
        }
        else {
            switch (protocol_step) {
                case 2: // send ID hi, recv TAP (5ah?)
                    r = 0x5A;
                    do_ack = 1;
                    break;
                case 3: // send bit0...7 of digital switches
                    if (cmd == PCMD_read) r = buttons[0];
                    do_ack = 1;
                    break;
                case 4: // send bit8...15 of digital switches
                    if (cmd == PCMD_read) {
                        r = buttons[1];
                        printf("\nFINISH GP READ!");
                    }
                    break;
                default:
            }
        }
        if (do_ack) schedule_ack(clock_cycle, 100, 1);
    }

    protocol_step++;
    return r;
}

digital_gamepad::digital_gamepad(PS1::core *parent) : bus(parent)
{
    interface.device_ptr = this;
    interface.kind = DK_digital_pad;
    interface.exchange_byte = &sch_exchange_byte;
    interface.set_CS = &set_CS;
}

void SIO0::memcard_setup_pio(physical_io_device *d, u32 num, const char*name, bool connected) {
    d->init(HID_MEM_CARD, connected, connected, true, true);
    snprintf(d->memcard.name, sizeof(d->memcard.name), "%s", name);
    d->id = num;
    d->kind = HID_MEM_CARD;
    d->connected = connected;
    d->enabled = connected;

    JSM_MEMCARD *m = &d->memcard;
    m->store.persistent = true;
    m->store.requested_size = 128 * 1024;
    m->store.fill_value = 0;
    m->store.dirty = true;
}

void SIO0::gamepad_setup_pio(physical_io_device *d, u32 num, const char*name, bool connected)
{
    d->init(HID_CONTROLLER, connected, connected, true, false);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;
    d->enabled = connected;

    JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    pio_new_button(cnt, "select", DBCID_co_select);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "L2", DBCID_co_shoulder_left2);
    pio_new_button(cnt, "R2", DBCID_co_shoulder_right2);
    pio_new_button(cnt, "L1", DBCID_co_shoulder_left);
    pio_new_button(cnt, "R1", DBCID_co_shoulder_right);
    pio_new_button(cnt, "triangle", DBCID_co_fire4); // upper
    pio_new_button(cnt, "circle", DBCID_co_fire5); // right
    pio_new_button(cnt, "cross", DBCID_co_fire2); // lower
    pio_new_button(cnt, "square", DBCID_co_fire1); // left
}
}