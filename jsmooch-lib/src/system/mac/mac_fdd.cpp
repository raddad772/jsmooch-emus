//
// Created by . on 12/4/25.
//

#include "mac_bus.h"
#include "mac_fdd.h"

namespace mac {

FDD::FDD(core *bus_in, u32 num_in) : bus(bus_in), num(num_in) {
}

void FDD::setup_track()
{
    auto &track = disc->disc.tracks[head.track_num];
    //RPM = track.rpm;
    last_RPM_change_time = bus->clock.master_cycles;
    next_flux_tick = bus->clock.master_cycles - 1;
    track_pos = 0;

}

void FDD::calculate_ticks_per_flux() {
    if (disc == nullptr)
        return;
    if (RPM == 0) { ticks_per_flux = 10000000000; }
    auto &track = disc->disc.tracks[head.track_num];
    long double num_bits = track.encoded_data.num_bits;
    long double bits_per_second = (num_bits * RPM) / 60.0l;
    ticks_per_flux = bus->clock.timing.cycles_per_second / bits_per_second;
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
    if (step_direction == 0) {
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
    printf("\nHead stepped to track %d @%lld", head.track_num, bus->clock.master_cycles);

    head.stepping.start = bus->clock.master_cycles;
    head.stepping.is_stepping = true;
    head.stepping.end = bus->clock.master_cycles + bus->clock.timing.cycles_per_second / 33; //~30ms. assumes 60fps
    setup_track();
}

bool FDD::floppy_inserted() const {
    return !!disc;
}


void FDD::set_pwm_dutycycle(i64 to) {
    if (to != pwm.dutycycle) {
        constexpr i64 DUTY_T0 = 9;
        constexpr i64 SPEED_T0 = (380 + 304) / 2;
        constexpr i64 DUTY_T79 = 91;
        constexpr i64 SPEED_T79 = (625 + 480) / 2;
        if (to < DUTY_T0) {
            RPM = 0;
        }
        else {
            RPM = ((to - DUTY_T0) * (SPEED_T79 * 100 + SPEED_T0 * 100)
                / (DUTY_T79 - DUTY_T0))
                / 100
                + SPEED_T0;
        }
        printf("\nNEW RPM: %d", RPM);
        calculate_ticks_per_flux();
        // TODO: adjust time til next bit!?!?!
    }
    pwm.dutycycle = to;
}

u8 FDD::read_reg(u8 which) {
    u32 v = 0;

    switch (which) {
        case 0b0000: // DIRTN head step direction
            v = step_direction;
            return v;
        case 0b0001: // disk in drive? 0=yes
            v = floppy_inserted() ^ 1;
            return v;
        case 0b0010: // STEP head is stepping = 0. head is NOT stepping = 1
            v = head.stepping.is_stepping == false;
            return v;
        case 0b0011: // WRTPRT: 0=disk write protect, 1 = no write protect
            if (!disc) return 1;
            v = !disc->write_protect;
            return v;
        case 0b0100: // MOTORON 0 = running, 1 = off
            v = motor_on == 0;
            printf("\nSTATUSREG.2: motor power on: %d", v);
            return v;
        case 0b0101: // TKO, head at track 0, 0 = track 0, 1 = other track
            v = head.track_num != 0;
            return v;
        case 0b0110: // disk switched!??!?
            v = disk_switched;
            return v;
        case 0b0111: { // TACH tachometer
            if (!motor_on) return 0;
            if (!disc) return 0;
            if (RPM == 0) return 0;
            //printf("\nGET TACH RPM %d", RPM);
            u64 edges_per_minute = RPM * 120;
            u64 ticks_per_edge = (8000000 * 60) / edges_per_minute;
            u8 r = (bus->clock.master_cycles/ticks_per_edge) & 1;
            static u8 last_returned = 0;
            return r;
            }
        case 0b1000: // read data low TODO
            return head.flux;
        case 0b1001: // read data hi TODO
            if (motor_on) return 1; // no upper head yet
            return 1; // if motor is off we return 1
        case 0b1010: // SUPERDRIVE
            return 0;
        case 0b1011: // MFM mode
            return mfm;
            return 0;
        case 0b1100: // SIDES. 0 = single
            return 0;
        case 0b1101: //  READY. 0 = ready, 1 = not ready
            return 0;
        case 0b1110: // INSTALLED 0=installed
            return 0;
        case 0b1111: // HD present
            return 0;
        }
    NOGOHERE;
    return 0;
}

void FDD::write_reg(u8 which) {
    enum regwrites {
        TRACKUP = 0b0000,
        TRACKDN = 0b1000,
        TRACKSTEP = 0b0010,
        MFMMODE = 0b0011,
        MOTORON = 0b0100,
        CLRSWITCHED = 0b1001,
        GCRMODE = 0b1011,
        MOTOROFF = 0b1100,
        EJECT = 0b1110,
    } whichrw = static_cast<regwrites>(which);
    printf("\nWRITE DRIVE REG %d", which);

    switch (whichrw) {
        case TRACKUP:
            step_direction = 1;
            return;
        case TRACKDN:
            step_direction = -1;
            return;
        case MOTORON:
            write_motor_on(true);
            return;
        case MOTOROFF:
            write_motor_on(false);
            return;
        case TRACKSTEP:
            do_step();
            return;
        case MFMMODE: // only for SuperDrive
            return;
        case GCRMODE: // also only for superdrive
            return;
        case CLRSWITCHED:
            // clear "disk switched" flag
            return;
        case EJECT:
            printf("\nEJECT? NAH!");
            return;
        default:
            printf("\n unknown reg:%d   cyc:%lld", which, bus->clock.master_cycles);
            break;
    }
}

bool FDD::clock() {
    if (!motor_on) return false;
    if (!disc) return false;
    u64 tc = bus->clock.master_cycles;
    if (head.stepping.is_stepping) {
        if (tc >= head.stepping.end) {
            head.stepping.is_stepping = false;
            setup_track();
            printf("\nHead step finished @%lld", bus->clock.master_cycles);
        }
        else return false;
    }
    // At this point, a disc is inserted, motor is on, and head is done stepping. Track is setup
    if (RPM != 0) {
        while (next_flux_tick <= tc) {
            next_flux_tick += ticks_per_flux;
            auto &track = disc->disc.tracks[head.track_num];
            head.flux = track.encoded_data.data[track_pos];
            track_pos = (track_pos + 1) % track.encoded_data.num_bits;
            return true;
        }
    }
    return false;
}
    
}