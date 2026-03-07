//
// Created by . on 2/26/25.
//

#include <cstring>
#include "ps1_memcard.h"
#include "../ps1_bus.h"
#include "helpers/debug.h"

namespace PS1::SIO {
enum cmd_kinds {
    PCMD_read,
    PCMD_unknown
};

static void set_CS(void *ptr, u32 level, u64 clock_cycle) {
    auto *th = static_cast<memcard *>(ptr);
    //printf("\npad: set CS new:%d  old:%d", level, th->interface.CS);
    u32 old_CS = th->interface.CS;
    th->interface.CS = level;
    if (old_CS != th->interface.CS) {
        th->selected = 0;
        th->protocol_step = 0;
        if (th->interface.CS) {
            printif(ps1.pad, "\npad: CS 0->1, latch buttons");
        }
        else {
            printif(ps1.pad, "\npad: CS 1->0");
            if (th->interface.ACK) {
                printif(ps1.pad, "\npad: ACK asserted");
                //if (still_sched && sch_id)
                //    scheduler_delete_if_exist(&bus->scheduler, sch_id);
                SIO0_device p = th->pio->id == 1 ? SIO0_mem1 : SIO0_mem2;
                th->bus->sio0.update_ACKs(p, 0);
            }
        }
    }
}

static void scheduler_call(void *ptr, u64 key, u64 current_clock, u32 jitter);

void memcard::schedule_ack(u64 clock_cycle, u64 time, u32 level)
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
    memcard *th = static_cast<memcard *>(ptr);
    auto p = th->pio->id == 1 ? SIO0_controller1 : SIO0_controller2;
    //printf("\ncyc:%lld Callback execute ack: %lld", current_clock, key);
    th->bus->sio0.update_ACKs(p, key);

    if (key) { // Also schedule to de-assert
        th->schedule_ack(current_clock-jitter, 96, 0);
    }
    else th->sch_id = 0;
}

static u8 sch_exchange_byte(void *ptr, u8 byte, u64 clock_cycle) {
    memcard *th = static_cast<memcard *>(ptr);
    return th->exchange_byte(byte, clock_cycle);
}

u8 memcard::exchange_byte(u8 byte, u64 clock_cycle) {
    if (!interface.CS) return 0xFF;

    if (protocol_step == 0) {
        if (byte == 0x81) {
            selected = 1;
            printf("\nSELECT MEMCARD!");
            protocol_step++;

            //printf("\nSELECT PAD, DO ACK.");
            schedule_ack(clock_cycle, 96, 1);
            return 0xFF;
        }
    }

    u8 r = 0xFF;

    if (selected) {
        u32 do_ack = 0;
        if (protocol_step == 1) { // send ID lo, recv Read Command (42h)
            r = 0x41;
            switch (byte) {
                case 0x42:
                    cmd = PCMD_read;
                    break;
                default:
                    cmd = PCMD_unknown;
                    break;
            }
            cmd = (byte == 0x42) ? PCMD_read : PCMD_unknown;
            if (cmd == PCMD_unknown && byte != 0x43) printf("\nUnknown command %02x to memcard %d", byte, pio->id);
            if (cmd == PCMD_read) do_ack = 1;
        }
        else switch (protocol_step) {
                case 2: // send ID hi, recv TAP (5ah?)
                    r = 0x5A;
                    do_ack = 1;
                    break;
                case 3: // send bit0...7 of digital switches
                    if (cmd == PCMD_read) r = buttons[0];
                    do_ack = 1;
                    break;
                case 4: // send bit8...15 of digital switches
                    if (cmd == PCMD_read) r = buttons[1];
                    break;
                default:
            }
        if (do_ack) schedule_ack(clock_cycle, 100, 1);
    }

    protocol_step++;
    return r;
}

memcard::memcard(PS1::core *parent) : bus(parent)
{
    interface.device_ptr = this;
    interface.kind = DK_digital_pad;
    interface.exchange_byte = &sch_exchange_byte;
    interface.set_CS = &set_CS;
}


}