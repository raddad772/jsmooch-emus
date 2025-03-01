//
// Created by . on 2/26/25.
//

#include <string.h>
#include "ps1_digital_pad.h"
#include "../ps1_bus.h"

enum cmd_kinds {
    PCMD_read,
    PCMD_unknown
};

static void latch_buttons(struct PS1_SIO_digital_gamepad *this)
{
    struct physical_io_device* p = this->pio;
    if (p->connected) {
        struct cvec* bl = &p->controller.digital_buttons;
        struct HID_digital_button* b;
        this->buttons[0] = 0;
        this->buttons[1] = 0;
        // buttons are 0=pressed
#define B_GET(button_num, byte_num, bit_num) { b = cvec_get(bl, button_num); this->buttons[byte_num] |= (b->state << bit_num); }
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

        this->buttons[0] ^= 0xFF;
        this->buttons[1] ^= 0xFF;
        //u32 r = this->buttons[1] | (this->buttons[0] << 8);
        //if (r != 0xFFFF) printf("\npad: READING BUTTONS...%04x", r);
    }
    else {
        this->buttons[0] = 0xFF;
        this->buttons[1] = 0xFF;
    }
}

static void set_CS(void *ptr, u32 level, u64 clock_cycle) {
    struct PS1_SIO_digital_gamepad *this = (struct PS1_SIO_digital_gamepad *)ptr;
    u32 old_CS = this->interface.CS;
    this->interface.CS = level;
    if (old_CS != this->interface.CS) {
        this->selected = 0;
        this->protocol_step = 0;
        if (this->interface.CS) {
            //printf("\npad: CS 0->1");
            latch_buttons(this);
        }
        else {
            //printf("\npad: CS 1->0");
            if (this->interface.ACK) {
                //if (this->still_sched && this->sch_id)
                //    scheduler_delete_if_exist(&this->bus->scheduler, this->sch_id);
                enum PS1_SIO0_port p = this->pio->id == 1 ? PS1S0_controller1 : PS1S0_controller2;
                PS1_SIO0_update_ACKs(this->bus, p, 0);
            }
        }
    }
}

static void scheduler_call(void *ptr, u64 key, u64 current_clock, u32 jitter);

static void schedule_ack(struct PS1_SIO_digital_gamepad *this, u64 clock_cycle, u64 time, u32 level)
{
    //printf("\ncycle:%lld Schedule ack: %d", clock_cycle, level);
    if (level == 1) {
        enum PS1_SIO0_port p = this->pio->id == 1 ? PS1S0_controller1 : PS1S0_controller2;
        PS1_SIO0_update_ACKs(this->bus, p, 1);
        level = 0;
    }

    this->sch_id = scheduler_add_or_run_abs(&this->bus->scheduler, clock_cycle + time, level, this, &scheduler_call, &this->still_sched);
}

static void scheduler_call(void *ptr, u64 key, u64 current_clock, u32 jitter)
{
    struct PS1_SIO_digital_gamepad *this = (struct PS1_SIO_digital_gamepad *)ptr;
    enum PS1_SIO0_port p = this->pio->id == 1 ? PS1S0_controller1 : PS1S0_controller2;
    //printf("\ncyc:%lld Callback execute ack: %lld", current_clock, key);
    PS1_SIO0_update_ACKs(this->bus, p, key);

    if (key) { // Also schedule to de-assert
        schedule_ack(this, current_clock-jitter, 75, 0);
    }
    else this->sch_id = 0;
}

static u8 exchange_byte(void *ptr, u8 byte, u64 clock_cycle) {
    struct PS1_SIO_digital_gamepad *this = (struct PS1_SIO_digital_gamepad *)ptr;
    if (!this->interface.CS) return 0;

    if (this->protocol_step == 0) {
        if (byte == 0x01) {
            this->selected = 1;
            this->protocol_step++;

            //printf("\nSELECT PAD, DO ACK.");
            schedule_ack(this, clock_cycle, 1023, 1);
            return 0;
        }
    }

    u8 r = 0;

    if (this->selected) {
        u32 do_ack = 0;
        if (this->protocol_step == 1) { // send ID lo, recv Read Command (42h)
            r = 0x41;
            this->cmd = (byte == 0x42) ? PCMD_read : PCMD_unknown;
            if (this->cmd == PCMD_unknown) printf("\nUnknown command %02x to controller %d", byte, this->pio->id);
            do_ack = 1;
        }
        else switch (this->protocol_step) {
                case 2: // send ID hi, recv TAP (5ah?)
                    r = 0x5A;
                    do_ack = 1;
                    break;
                case 3: // send bit0...7 of digital switches
                    if (this->cmd == PCMD_read) r = this->buttons[0];
                    do_ack = 1;
                    break;
                case 4: // send bit8...15 of digital switches
                    if (this->cmd == PCMD_read) r = this->buttons[1];
                    break;
            }
        if (do_ack) schedule_ack(this, clock_cycle, 1023, 1);
    }

    this->protocol_step++;
    return r;
}

void PS1_SIO_digital_gamepad_init(struct PS1_SIO_digital_gamepad *this, struct PS1 *bus)
{
    memset(this, 0, sizeof(*this));
    this->interface.device_ptr = this;
    this->interface.kind = PS1DK_digital_pad;
    this->interface.exchange_byte = &exchange_byte;
    this->interface.set_CS = &set_CS;
    this->bus = bus;
}

void PS1_SIO_gamepad_setup_pio(struct physical_io_device *d, u32 num, const char*name, u32 connected)
{
    physical_io_device_init(d, HID_CONTROLLER, 0, 0, 1, 1);

    snprintf(d->controller.name, sizeof(d->controller.name), "%s", name);
    d->id = num;
    d->kind = HID_CONTROLLER;
    d->connected = connected;
    d->enabled = connected;

    struct JSM_CONTROLLER* cnt = &d->controller;

    // up down left right a b start select. in that order
    pio_new_button(cnt, "select", DBCID_co_select);
    pio_new_button(cnt, "start", DBCID_co_start);
    pio_new_button(cnt, "up", DBCID_co_up);
    pio_new_button(cnt, "down", DBCID_co_down);
    pio_new_button(cnt, "left", DBCID_co_left);
    pio_new_button(cnt, "right", DBCID_co_right);
    pio_new_button(cnt, "L2", DBCID_co_shoulder_left2);
    pio_new_button(cnt, "R2", DBCID_co_shoulder_right2);
    pio_new_button(cnt, "L1", DBCID_co_shoulder_left);
    pio_new_button(cnt, "R1", DBCID_co_shoulder_right);
    pio_new_button(cnt, "triangle", DBCID_co_fire4); // upper
    pio_new_button(cnt, "circle", DBCID_co_fire5); // right
    pio_new_button(cnt, "cross", DBCID_co_fire2); // lower
    pio_new_button(cnt, "square", DBCID_co_fire1); // left
}
