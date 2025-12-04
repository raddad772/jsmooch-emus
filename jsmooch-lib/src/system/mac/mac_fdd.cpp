//
// Created by . on 12/4/25.
//

#include "mac_bus.h"
#include "mac_fdd.h"

namespace mac {
void FDD::setup_track()
{
    //struct JSMAC_DRIVE *drv = &drive[selected_drive];
    auto &track = disc->disc.tracks[head.track_num];
    RPM = track.rpm;
    last_RPM_change_time = bus->clock.master_cycles;
}

void FDD::write_motor_on(bool onoff)
{
    if (disc == nullptr) { // No disk inserted
        return;
    }

    if (motor_on == onoff) { // No change
        return;
    }

    if (onoff != motor_on) {
        if (onoff) { // Turn motor on
            setup_track();
        } else { // Turn motor off

        }
    }
    motor_on = onoff;
}

void FDD::do_step()
{
    if (head_step_direction == 0) {
        if (head.track_num > 0) {
            head.track_num -= 1;
            printf("\nTRACK CHANGE DOWN TO %d", head.track_num);
        }
    }
    else {
        if (disc) {
            if ((head.track_num + 1) < disc->disc.tracks.size()) {
                head.track_num += 1;
                printf("\nTRACK CHANGE UP TO %d", head.track_num);
            }
        }
    }

    head.stepping.start = bus->clock.master_cycles;
    head.stepping.status = 1;
    head.stepping.end = bus->clock.master_cycles + ((704*370 * 60) / 100); //10ms
}

u32 FDD::read_reg(u8 which) {
    u32 v = 0;

    switch (which) {
        case 0:
            v = head_step_direction;
            printf("\nSTATUSREG.0: head step direction: %d", v);
            return v;
        case 1: // head is stepping = 0. head is NOT stepping = 1
            v = head.stepping.status == 0;
            printf("\nSTATUSREG.1: head step status: %d", v);
            return v;
        case 2: // 0 = motor on, 1 = motor stopped
            v = motor_on == 0;
            printf("\nSTATUSREG.2: motor power on: %d", v);
            return v;
        case 3:
            v = disk_switched;
            printf("\nSTATUSREG.3: motor power on: %d", v);
            return v;
        case 4:
            v = 1;
            printf("\nSTATUSREG.4: head #: %d", v);
            return v;
        case 5:
            v = 0;
            printf("\nSTATUSREG.5: is_superdrive: %d", v);
            return v;
        case 6:
            v = 0;
            printf("\nSTATUSREG.6: # sides: %d", v);
            return v;
        case 7: //
            v = 1;
            printf("\nSTATUSREG.7: drive installed: %d", v);
            return v;
        case 8: // 0 = disk in drive, 1 = no disk
            v = disc ? 0 : 1;
            printf("\nSTATUSREG.8: disk inserted: %d", v);
            return v;
        case 9:
            if (disc)
                v = disc->write_protect;
            else v = 0;
            printf("\nSTATUSREG.9: drive locked: %d", v);
            return v;

        case 10:
            v = head.track_num != 0;
            printf("\nSTATUSREG.A: !head track0: %d", v);
            return v;
        case 11: {
            u32 pwm = 65536 - pwm_val;

            v = ((((120 * pwm) / 32768) * pwm_pos) / pwm_len) & 1;
            v = !v;
            printf("\nSTATUSREG.B: tachometer: %d", v);
            return (v == 0); }
        case 12:
            v = 1;
            printf("\nSTATUSREG.C: head: %d", v);
            return v;
        case 13:
            printf("\nSTATUSREG.D: none: 1");
            return true;
        case 14:
            v = 1;
            printf("\nSTATUSREG.E: ready: %d", v);
            return v;
        case 15:
            v = 1;
            printf("\nSTATUSREG.F: new interface: %d", v);
            return v;
    }
    NOGOHERE;
    return 0;
}

void FDD::write_reg(u8 which, u8 val) {
    switch (which) {
        case 0: {
            printf("\nWRITE CMD.STEP DIR:%d  cyc:%lld", val, bus->clock.master_cycles);
            head_step_direction = val;
            break;
        }
        case 1:
            if (val == 0) {
                printf("\nWRITE CMD.DO STEP   cyc:%lld", bus->clock.master_cycles);
                do_step();
            }
            break;

        case 2:
            printf("\n WRITE CMD.MOTOR ON:%d    cyc:%lld", val == 0,
                   bus->clock.master_cycles);
            write_motor_on(val == 0);
            break;
        case 3:
            if (val) {
                printf("\n EJECT    cyc:%lld", bus->clock.master_cycles);
            }
            break;
        case 4:
            if (val) {
                printf("\n CLEAR DISK SWITCHED   cyc:%lld", bus->clock.master_cycles);
                disk_switched = 0;
            }
            break;
        default:
            printf("\n unknown reg:%d val:%d   cyc:%lld", which, val, bus->clock.master_cycles);
            break;
    }
}
    
}