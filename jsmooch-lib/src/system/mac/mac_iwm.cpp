//
// Created by . on 8/5/24.
//

#include <cstdlib>
#include <cassert>

#include "mac_iwm.h"
#include "mac_bus.h"

namespace mac {
iwm::iwm(core* parent) : bus(parent), drive{FDD(parent), FDD(parent)}
{
    regs.write_handshake = 0xBD;

    floppy::mac::DISC &f = my_disks.emplace_back();
    f.write_protect = false;
    drive[0].disc = &f;
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
    active = M_IDLE;
}

void iwm::drive_select(u8 towhich)
{
    if (towhich == 0) {
        printf("\nSELECT NO DRIVE");
        selected_drive = -1;
        cur_drive = nullptr;
    }
    else if (regs.drive_select != selected_drive) {
        printf("\nSELECT DRIVE %d", regs.drive_select);
        selected_drive = regs.drive_select;
        cur_drive = &drive[selected_drive];
    }
}

bool iwm::floppy_inserted() const
{
    if (!cur_drive) return false;
    if (!cur_drive->disc) return false;
    return true;
}

bool iwm::floppy_write_protected() const {
    if (!cur_drive) return false;
    if (!cur_drive->disc) return false;
    return cur_drive->disc->write_protect;
}

void iwm::write_data(u8 val)
{
    regs.data = val;
    if (!regs.mode.async && (rw ==  M_WRITE)) {
        regs.write_shift = val;
    }
    if (regs.mode.latched)
        regs.write_shift &= 0x7F;
}

void iwm::write_mode(u8 val)
{
    if (regs.mode.u != val) {
        printf("\nUPDATE MODE: %02x %s%s%s%s%s%s%s%s      cyc:%lld", val,
               val & 0x80 ? " b7" : "",
               val & 0x40 ? " mz-reset" : "",
               val & 0x20 ? " test" : " normal",
               val & 0x10 ? " 8MHz" : " 7MHz",
               val & 0x08 ? " fast" : " slow",
               val & 0x04 ? "" : " timer",
               val & 0x02 ? " async" : " sync",
               val & 0x01 ? " latched" : "",
               bus->clock.master_cycles);
    }
    regs.mode.u = val;
    regs.status.lower5 = (val & 0x1F);
}

void iwm::update_phases()
{
    lines.phases = (lines.phases & 0xF0) | (lines.CA0) | (lines.CA1 << 1) | (lines.CA2 << 2) | (lines.LSTRB << 3);
    /*
    u8 mask = lines.phases >> 4;
    u8 m_phases_cb = ((lines.phases & mask) | (m_phases_input & (mask ^ 0xf)));
    if (cur_drive)
        updates_drive_phases(m_phases_cb)*/
}


u32 iwm::get_drive_reg() {
    auto &drv = drive[selected_drive];

    u8 reg = (lines.CA0 | (lines.CA1 << 1) | (lines.CA2 << 2) | (lines.SELECT << 3));
    return drv.read_reg(reg);
}


void iwm::clock()
{
    u32 clk, bit;
    auto &drv = drive[selected_drive];
    clk = drv.input_clock_cnt + 500000UL;
    bit = clk / (7833600 * 2);
    clk = clk % (7833600 * 2);
    drv.input_clock_cnt = clk;

    /*if (drv.cur_track_len > 0) {
        drv.cur_track_pos += bit;

        while (drv.cur_track_pos >= drv.cur_track_len) {
            drv.cur_track_pos -= drv.cur_track_len;
        }
    }*/

    if (drv.pwm_len > 0) {
        drv.pwm_pos += bit;

        if (drv.pwm_pos >= drv.pwm_len) {
            drv.pwm_pos -= drv.pwm_len;
        }
    }

    /*if (iwm->writing) {
        mac_iwm_write (iwm, drv);
    }
    else if ((iwm->lines & (MAC_IWM_Q6 | MAC_IWM_Q7)) == 0) {
        mac_iwm_read (iwm, drv);
    }

    drv.read_pos = drv.cur_track_pos;
    drv.write_pos = drv.cur_track_pos;*/
}

void iwm::set_drive_reg()
{
    auto &drv = drive[selected_drive];
    u8 reg = (lines.CA0 | (lines.CA1 << 1) | (lines.SELECT << 2));

    u8 val = lines.CA2;
    drv.write_reg(reg, val);

}

u16 iwm::control(u8 addr, u8 val, u32 is_write) {
    u32 onoff = (addr & 1); // 0 = turn off, 1 = turn on
    switch (addr & 0xFE) { // Phase lines
        case 0: // CA0
            lines.CA0 = onoff;
            update_phases();
            break;
        case 2: // CA1
            lines.CA1 = onoff;
            update_phases();
            break;
        case 4: // CA2
            lines.CA2 = onoff;
            update_phases();
            break;
        case 6: // LSTRB
            lines.LSTRB = onoff;
            update_phases();
            break;
        case 8: // Control lines
            // motor on/off
            lines.enable = onoff;
            break;
        case 10:
            // external disk select
            regs.drive_select = onoff;
            break;
        case 12:
            lines.Q6 = onoff;
            break;
        case 14:
            lines.Q7 = onoff;
            break;
    }

    if (lines.enable) {
        if (active !=  M_ACTIVE) { // If we're not active, turn active
            active =  M_ACTIVE;
            regs.status.active = 1;
            //drive_write_motor_on(0);
        }

        if (lines.Q7 == 0) {
            if (rw !=  M_READ) { // If we're not in read mode, change to read mode
                if (rw ==  M_WRITE) { // If we're writing...
                    // Finish up any write
                }
                printf("\nCHANGE TO READ MODE");
                rw =  M_READ;
                regs.data = 0;
            }
        } else { // if Q7 on
            if (rw !=  M_WRITE) {
                printf("\nCHANGE TO WRITE MODE");
                rw =  M_WRITE;
                //m_rw_state = S_IDLE;
                regs.write_handshake |= 0x40;
            }
        }
    } // /motoron
    else {
        if (active ==  M_ACTIVE) {
            // flush any writes
            printf("\nINACTIVATE DRIVE");
            if (regs.mode.no_timer) { // If there's no timer and we're heading to inactive
                active =  M_IDLE;
                rw =  M_IDLE;
                regs.status.active = 0;
                regs.write_handshake &= ~0x40;
                //drive_write_motor_on(1);
            } else { // If there *IS* a timer...set a delay
                active =  M_MOTOR_STOP_DELAY;
                motor_stop_timer = bus->clock.master_cycles + 16777200; // about 1 second
                drive_select(selected_drive);
                printf("\nMOTOR STOP TIMER SET!");
            }
        }
    }

    // Drive select previously was...
    // IF we're not idle and we're not off
    u8 devsel = active !=  M_IDLE ? (lines.enable ? 2 : 1) : 0;
    if(devsel != old_drive_select) {
        old_drive_select = devsel;
        drive_select(devsel);
    }

    if((lines.Q6 == 1) && (lines.Q7 == 0) && (active ==  M_ACTIVE) && (rw ==  M_READ)) {
        regs.read_shift = 0;
    }

    auto &drv = drive[selected_drive];

    // 2uS bit window in fast mode, 4uS in slow mode
    // data shifted in 1 bit at a time LSB to MSB
    // when 1 is shifted into MSB, data is complete
    // every byte on disc has 1 in MSB
    // write protect sense->write load = write mode
    // Q3 period = 4uS

    // sync and asynch mode
    // in sync mode, ??
    // in async mode, write shift reg is buffered, wjhen it's empty,
    //  IWM sets MSB of write-handshake register to a one
    // buffer->write shfit register
    // Q6/Q7 = 00, read data register
    // Q6/Q7 = 01, read handshake register
    // Q6/Q7 = 10, read status register
    // Q6/Q7 = 11, if drive on, read data register. else write mode register

    printf("\nLSTRB set to %d", lines.LSTRB);
    if (lines.LSTRB) {
        set_drive_reg();
    }

    u8 q67 = (lines.Q6 << 1) | (lines.Q7);
    u16 v = 0;
    switch(q67) {
        case 0b00: {// read data register
            v = active ==  M_IDLE ? 0xFF : regs.data; }
            if (!is_write) printf("\nREAD FLOPPY DATA REGISTER: %02x cyc:%lld", v, bus->clock.master_cycles);
            return v;
        case 0b01: {// Q6=0 Q7=1 handhshake regiter
            v = regs.write_handshake;
            if (!is_write) printf("\nREAD FLOPPY HANDSHAKE REGISTER: %02x cyc:%lld", v, bus->clock.master_cycles);
            return v; }
        case 0b10: {// read status register. Q6=1, Q7=0
            v = (regs.status.u & 0x7F);
            v |= get_drive_reg() ? 0x80 : 0;
            if (!is_write) printf("\nREAD FLOPPY STATUS REGISTER: %02x phases:%02x sel:%d cyc:%lld", v, lines.phases, lines.SELECT, bus->clock.master_cycles);
            return v; }
        case 0b11: {
            if (onoff) { // If it's a write...
                if (active !=  M_IDLE) { // write mode register
                    printf("\nWRITE DATA %02x  cyc:%lld", val, bus->clock.master_cycles);
                    write_data(val);
                }
                else { // read/write data register
                    write_mode(val);
                }
            }
            return 0xFF; }
    }
    assert(1==0);
    return 0xFF;
}

u16 iwm::read(u8 addr)
{
    return control(addr, 0, 0);
}

void iwm::write(u8 addr, u8 val)
{
    control(addr, val, 1);
}
}