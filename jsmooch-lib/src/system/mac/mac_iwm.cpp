//
// Created by . on 8/5/24.
//

#include <cstdlib>
#include <cassert>

#include "mac_iwm.h"
#include "mac_bus.h"

namespace mac {
iwm::iwm(core* parent, variants variant_in) : bus(parent), drive{FDD(parent, 0), FDD(parent, 1)}, variant(variant_in)
{
    floppy::mac::DISC &f = my_disks.emplace_back();
    f.write_protect = false;
    drive[0].disc = &f;
    my_disks.reserve(10);
    switch(variant) {
        case mac128k:
        case mac512k:
            num_drives = 1;
            break;
        case macplus_1mb:
            num_drives = 2;
            break;
        default:
            NOGOHERE;
    }
}

void iwm::update_pwm(u8 val) {
    static constexpr u8 VALUE_TO_LEN[64] = {
        0, 1, 59, 2, 60, 40, 54, 3, 61, 32, 49, 41, 55, 19, 35, 4, 62, 52, 30, 33, 50, 12, 14,
        42, 56, 16, 27, 20, 36, 23, 44, 5, 63, 58, 39, 53, 31, 48, 18, 34, 51, 29, 11, 13, 15,
        26, 22, 43, 57, 38, 47, 17, 28, 10, 25, 21, 37, 46, 9, 24, 45, 8, 7, 6
    };
    //if (!drive->has_pwm) return;
    for (auto &drv : drive) {
        drv.pwm.avg_sum += static_cast<i64>(VALUE_TO_LEN[val]) & 63;
        drv.pwm.avg_count++;
        if (drv.pwm.avg_count >= 100) {
            i64 idx = drv.pwm.avg_sum / (drv.pwm.avg_count / 10) - 11;
            if (idx < 0) idx = 0;
            if (idx > 399) idx = 399;
            drv.set_pwm_dutycycle((idx * 100) / 419);
            drv.pwm.avg_sum = 0;
            drv.pwm.avg_count = 0;
        }
    }
}

void iwm::reset() {
    for (auto & d : drive) {
        d.head.track_num = 0;
        d.RPM = 0;
    }
    if (drive[0].connected)
        drive[0].device = &drive[0].ptr.get();
    if (drive[1].connected)
        drive[1].device = &drive[1].ptr.get();
}

u16 iwm::do_read(u32 addr, u16 mask, u16 old, bool has_effect) {
    if (!has_effect) return 0xFFFF & mask;
    access(addr);
    u8 q67 = (lines.Q6 << 1) | lines.Q7;
    switch (q67) {
        case 0b00: { // read data
            if (!lines.ENABLE) return 0xFF;
            u8 v = regs.data;
            regs.data = 0;
            if (v != 0) printf("\nREAD DATA! MOTORON:%d %02x", selected_drive->motor_on, v);
            return v; }
        case 0b10: // read status
            regs.status.sense = selected_drive->read_reg(get_drive_reg());
            regs.status.mode = regs.mode.u & 0x1F;
            regs.status.enable = lines.ENABLE;

            regs.shifter = 0;
            //printf("\nREAD STATUS! %02x", regs.status.u);
            return regs.status.u;
        case 0b01: // read handshake
            return ((!(write.pos == 0 && write.buffer == -1) << 6) |
                ((write.buffer == -1) << 7));
                    // nothing for 0b11
        default:
    }
    printf("\nIWM unknown read! %04x q6:%d q7:%d", addr, lines.Q6, lines.Q7);
    return 0;
}

void iwm::clock() {
    if (selected_drive->clock() && regs.mode.latch && lines.HEADSEL == 0) {
        regs.shifter = (regs.shifter << 1) | selected_drive->head.flux;
        if (regs.shifter & 0x80) {
            regs.data = regs.shifter;
            regs.shifter = 0;
        }
    };

}

u8 iwm::get_drive_reg() const {
    return lines.HEADSEL | (lines.CA0 << 1) | (lines.CA1 << 2) | (lines.CA2 << 3);
}

void iwm::access(u32 addr) {
    switch ((addr >> 9) & 15) {
        case 0: lines.CA0 = 0; break;
        case 1: lines.CA0 = 1; break;
        case 2: lines.CA1 = 0; break;
        case 3: lines.CA1 = 1; break;
        case 4: lines.CA2 = 0; break;
        case 5: lines.CA2 = 1; break;
        case 6: lines.LSTRB = 0; break;
        case 7: lines.LSTRB = 1; selected_drive->write_reg(get_drive_reg());break;
        case 8: lines.ENABLE = 0; break;
        case 9: lines.ENABLE = 1; break;
        case 10: lines.SELECT = 0; break; // currently no second drive support so do nothing
        case 11: lines.SELECT = 1; break;
        case 12: lines.Q6 = 0; break;
        case 13: lines.Q6 = 1; break;
        case 14: lines.Q7 = 0; break;
        case 15: lines.Q7 = 1; break;
    }

}

void iwm::write_HEADSEL(u8 what) {
    lines.HEADSEL = what;
}

void iwm::do_write(u32 addr, u16 mask, u16 val) {
    access(addr);
    u8 q67e = (lines.Q6 << 2) | (lines.Q7 << 1) | lines.ENABLE;
    switch (q67e) {
        case 0b110: // write MODE. copy protection stuff, ignore for now
            //set_mode(val);
            //printf("\nMODE write %02x", regs.mode.u);
            // TODO: switch mhz?
            regs.mode.u = val;
            return;
        case 0b111:
            if (write.buffer == -1) {
                printf("\nDisk write with data in buffer..");
            }
            write.buffer = val;
            return;
    }
}


}