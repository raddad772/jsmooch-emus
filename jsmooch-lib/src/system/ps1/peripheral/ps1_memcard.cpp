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
    PCMD_write,
    PCMD_get_card_id,
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
            printif(ps1.pad, "\npad: CS 0->1");
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
    auto p = th->pio->id == 1 ? SIO0_mem1 : SIO0_mem2;
    //printf("\ncyc:%lld Callback execute ack: %lld", current_clock, key);
    th->interface.ACK = key;
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

u8 memcard::do_read(u8 byte) {
    u8 last_byte = io.last_byte;
    io.last_byte = byte;
    do_ack = true;
    if ((protocol_step >= 10) && (protocol_step <= 137)) {
        u32 addr = (io.addr << 10) + (protocol_step - 10);
        u8 v = static_cast<u8 *>(pio->memcard.store.data)[addr];
        io.CKSUM ^= v;
        return v;
    }
    switch (protocol_step) {
        case 2:
            return 0x5A;
        case 3:
            return 0x5D;
        case 4: // MSB
            io.addr = byte << 8;
            io.CKSUM = byte;
            return 0;
        case 5: // LSB
            io.addr |= byte;
            io.CKSUM ^= byte;
            printf("\nREAD ADDR %03x", io.addr);
            return last_byte;
        case 6:
            return 0x5C;
        case 7:
            return 0x5D;
        case 8:
            return io.addr >> 8;
        case 9:
            return io.addr & 0xFF;
        case 138:
            return io.CKSUM;
        case 139:
            printf("\nFINISH READ");
            do_ack = false;
            return 0x47;
    }
    printf("\nUHOH READ GOT HERE");
    return 0;
}

u8 memcard::do_write(u8 byte) {
    u8 last_byte = io.last_byte;
    io.last_byte = byte;
    do_ack = true;
    FLAG = 0;
    //printf("\nWRITE STEP %d", protocol_step);
    //if ((protocol_step >= 10) && (protocol_step <= 137)) {
    if ((protocol_step >= 6) && (protocol_step <= 133)) {
        u32 addr = (io.addr << 10) + (protocol_step - 6);
        static_cast<u8 *>(pio->memcard.store.data)[addr] = byte;
        io.CKSUM ^= byte;
        return last_byte;
    }
    switch (protocol_step) {
        case 2:
            return 0x5A;
        case 3:
            return 0x5D;
        case 4:
            io.addr = byte << 8;
            io.CKSUM = byte;
            return 0x00;
        case 5:
            io.addr |= byte;
            printf("\nWRITE ADDR %03x", io.addr);
            io.CKSUM ^= byte;
            return last_byte;
        case 134:
            return last_byte;
        case 135:
            return 0x5C;
        case 136:
            return 0x5D;
        case 137:
            do_ack = false;
            printf("\nFINISH WRITE");
            return 0x47;
    }
    printf("\nUHOH WRITE GOT HERE");
    return 0;

}


u8 memcard::exchange_byte(u8 byte, u64 clock_cycle) {
    if (!interface.CS) return 0xFF;

    if (protocol_step == 0) {
        if (byte == 0x81) {
            selected = 1;
            protocol_step++;

            //printf("\nSELECT PAD, DO ACK.");
            schedule_ack(clock_cycle, 96, 1);
            return 0xFF;
        }
    }

    u8 r = 0xFF;

    if (selected) {
        do_ack = false;
        if (protocol_step == 1) { // send ID lo, recv Read Command (42h)
            r = FLAG;
            switch (byte) {
                case 0x52:
                    cmd = PCMD_read;
                    break;
                case 0x57:
                    cmd = PCMD_write;
                    break;
                case 0x53:
                    //cmd = PCMD_get_card_id;
                    //break;
                default:
                    cmd = PCMD_unknown;
                    break;
            }
            if (cmd == PCMD_unknown) printf("\nUnknown command %02x to memcard %d", byte, pio->id);
            else do_ack = true;
        }
        else {
            switch (cmd) {
                case PCMD_read:
                    r = do_read(byte);
                    break;
                case PCMD_write:
                    r = do_write(byte);
                    break;
                case PCMD_unknown:
                    printf("\nHUH?");
                    break;
                case PCMD_get_card_id:
                    printf("\nUHHH");
                    break;
            }
        }
        if (do_ack) schedule_ack(clock_cycle, 100, 1);
        do_ack = false;

    }

    protocol_step++;
    return r;
}

memcard::memcard(PS1::core *parent) : bus(parent)
{
    interface.device_ptr = this;
    interface.kind = DK_mem_card;
    interface.exchange_byte = &sch_exchange_byte;
    interface.set_CS = &set_CS;
}


}