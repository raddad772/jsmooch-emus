//
// Created by . on 8/5/24.
//

#include <assert.h>
#include <stdio.h>

#include "mac_internal.h"
void mac_iwm_init(struct mac* mac) {
    cvec_init(&mac->iwm.my_disks, sizeof(struct mac_floppy), 2);
    mac->iwm.active = IWMM_IDLE;
    mac->iwm.rw = IWMM_IDLE;
    mac->iwm.regs.data = 0;
    mac->iwm.regs.status.u = 0;
    mac->iwm.regs.write_shift = 0;
    mac->iwm.regs.read_shift = 0;
    mac->iwm.regs.write_handshake = 0xBD;
    mac->iwm.selected_drive = -1;
    mac->iwm.cur_drive = NULL;
}

void mac_iwm_delete(struct mac* mac) {
    for (u32 i = 0; i < cvec_len(&mac->iwm.my_disks); i++) {
        mac_floppy_delete(cvec_get(&mac->iwm.my_disks, i));
    }
    cvec_delete(&mac->iwm.my_disks);
}

void mac_iwm_reset(struct mac* mac) {
    for (u32 i = 0; i < 2; i++) {
        mac->iwm.drive[i].head.track_num = 0;
        mac->iwm.drive[i].RPM = 0;
    }
    if (mac->iwm.drive[0].connected)
        mac->iwm.drive[0].device = cvec_get(mac->iwm.IOs, mac->iwm.drive[0].io_index);
    if (mac->iwm.drive[1].connected)
        mac->iwm.drive[1].device = cvec_get(mac->iwm.IOs, mac->iwm.drive[1].io_index);
    mac->iwm.active = IWMM_IDLE;
}


static void drive_write_motor_on(struct mac* this, u8 onoff)
{
    struct JSMAC_DRIVE *drv = &this->iwm.drive[this->iwm.selected_drive];

    if (drv->disk == NULL) { // No disk inserted
        return;
    }

    if (drv->motor_on == onoff) { // No change

    }
}

static void drive_select(struct mac* this, u8 towhich)
{
    if (towhich == 0) {
        printf("\nSELECT NO DRIVE");
        this->iwm.selected_drive = -1;
        this->iwm.cur_drive = NULL;
    }
    else if (this->iwm.regs.drive_select != this->iwm.selected_drive) {
        printf("\nSELECT DRIVE %d", this->iwm.regs.drive_select);
        this->iwm.selected_drive = this->iwm.regs.drive_select;
        this->iwm.cur_drive = &this->iwm.drive[this->iwm.selected_drive];
    }
}

static u32 floppy_inserted(struct mac* this)
{
    if (this->iwm.cur_drive == NULL) return 0;
    if (this->iwm.cur_drive->disk == NULL) return 0;
    return 1;
}

static u32 floppy_write_protected(struct mac* this) {
    if (this->iwm.cur_drive == NULL) return 0;
    if (this->iwm.cur_drive->disk == NULL) return 0;
    return this->iwm.cur_drive->disk->write_protect;

}

static void iwm_write_data(struct mac* this, u8 val)
{
    this->iwm.regs.data = val;
    if (!this->iwm.regs.mode.async && (this->iwm.rw == IWMM_WRITE)) {
        this->iwm.regs.write_shift = val;
    }
    if (this->iwm.regs.mode.latched)
        this->iwm.regs.write_shift &= 0x7F;
}

static void iwm_write_mode(struct mac* this, u8 val)
{
    if (this->iwm.regs.mode.u != val) {
        printf("\nUPDATE MODE: %02x %s%s%s%s%s%s%s%s      cyc:%lld", val,
               val & 0x80 ? " b7" : "",
               val & 0x40 ? " mz-reset" : "",
               val & 0x20 ? " test" : " normal",
               val & 0x10 ? " 8MHz" : " 7MHz",
               val & 0x08 ? " fast" : " slow",
               val & 0x04 ? "" : " timer",
               val & 0x02 ? " async" : " sync",
               val & 0x01 ? " latched" : "",
               this->clock.master_cycles);
    }
    this->iwm.regs.mode.u = val;
    this->iwm.regs.status.lower5 = (val & 0x1F);
}

static void update_phases(struct mac* this)
{
    this->iwm.lines.phases = (this->iwm.lines.phases & 0xF0) | (this->iwm.lines.CA0) | (this->iwm.lines.CA1 << 1) | (this->iwm.lines.CA2 << 2) | (this->iwm.lines.LSTRB << 3);
    /*
    u8 mask = this->iwm.lines.phases >> 4;
    u8 m_phases_cb = ((this->iwm.lines.phases & mask) | (m_phases_input & (mask ^ 0xf)));
    if (this->iwm.cur_drive)
        updates_drive_phases(this, m_phases_cb)*/
}

static void set_drive_reg(struct mac* this)
{
    struct JSMAC_DRIVE *drv = &this->iwm.drive[this->iwm.selected_drive];
    u8 reg = (this->iwm.lines.CA0 | (this->iwm.lines.CA1 << 1) | (this->iwm.lines.SELECT << 2));

    u8 val = this->iwm.lines.CA2;

    switch(reg) {
        case 0:
    }
}

    switch (reg) {
        case 0:
            iwm_drv_set_step_direction (drv, val == 0);
            break;

        case 1:
            if (val == 0) {
                iwm_drv_set_step (drv);
            }
            break;

        case 2:
            iwm_drv_set_motor_on (drv, val == 0);
            mac_iwm_set_motor (iwm);
            break;

        case 3:
            if (val) {
                iwm_drv_set_eject (drv);
            }
            break;

        case 4:
            if (val) {
                iwm_drv_set_disk_switched (drv);
            }
            break;

}

static u16 iwm_control(struct mac* this, u8 addr, u8 val, u32 is_write) {
    u32 onoff = (addr & 1); // 0 = turn off, 1 = turn on
    switch (addr & 0xFE) { // Phase lines
        case 0: // CA0
            this->iwm.lines.CA0 = onoff;
            update_phases(this);
            break;
        case 2: // CA1
            this->iwm.lines.CA1 = onoff;
            update_phases(this);
            break;
        case 4: // CA2
            this->iwm.lines.CA2 = onoff;
            update_phases(this);
            break;
        case 6: // LSTRB
            this->iwm.lines.LSTRB = onoff;
            update_phases(this);
            break;
        case 8: // Control lines
            // motor on/off
            this->iwm.lines.motor_on = onoff;
            break;
        case 10:
            // external disk select
            this->iwm.regs.drive_select = onoff;
            break;
        case 12:
            this->iwm.lines.Q6 = onoff;
            break;
        case 14:
            this->iwm.lines.Q7 = onoff;
            break;
    }

    if (this->iwm.lines.motor_on) {
        if (this->iwm.active != IWMM_ACTIVE) { // If we're not active, turn active
            this->iwm.active = IWMM_ACTIVE;
            this->iwm.regs.status.active = 1;
            drive_write_motor_on(this, 0);
        }

        if (this->iwm.lines.Q7 == 0) {
            if (this->iwm.rw != IWMM_READ) { // If we're not in read mode, change to read mode
                if (this->iwm.rw == IWMM_WRITE) { // If we're writing...
                    // Finish up any write
                }
                printf("\nCHANGE TO READ MODE");
                this->iwm.rw = IWMM_READ;
                this->iwm.regs.data = 0;
            }
        } else { // if Q7 on
            if (this->iwm.rw != IWMM_WRITE) {
                printf("\nCHANGE TO WRITE MODE");
                this->iwm.rw = IWMM_WRITE;
                //m_rw_state = S_IDLE;
                this->iwm.regs.write_handshake |= 0x40;
            }
        }
    } // /motoron
    else {
        if (this->iwm.active == IWMM_ACTIVE) {
            // flush any writes
            printf("\nINACTIVATE DRIVE");
            if (this->iwm.regs.mode.no_timer) { // If there's no timer and we're heading to inactive
                this->iwm.active = IWMM_IDLE;
                this->iwm.rw = IWMM_IDLE;
                this->iwm.regs.status.active = 0;
                this->iwm.regs.write_handshake &= ~0x40;
                drive_write_motor_on(this, 1);
            } else { // If there *IS* a timer...set a delay
                this->iwm.active = IWMM_MOTOR_STOP_DELAY;
                this->iwm.motor_stop_timer = this->clock.master_cycles + 16777200; // about 1 second
                drive_select(this, this->iwm.selected_drive);
                printf("\nMOTOR STOP TIMER SET!");
            }
        }
    }

    // Drive select previously was...
    // IF we're not idle and we're not off
    u8 devsel = this->iwm.active != IWMM_IDLE ? (this->iwm.lines.motor_on ? 2 : 1) : 0;
    if(devsel != this->iwm.old_drive_select) {
        this->iwm.old_drive_select = devsel;
        drive_select(this, devsel);
    }

    if((this->iwm.lines.Q6 == 1) && (this->iwm.lines.Q7 == 0) && (this->iwm.active == IWMM_ACTIVE) && (this->iwm.rw == IWMM_READ)) {
        this->iwm.regs.read_shift = 0;
    }

    struct JSMAC_DRIVE *drv = &this->iwm.drive[this->iwm.selected_drive];

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

    printf("\nLSTRB set to %d", this->iwm.lines.LSTRB);
    if (this->iwm.lines.LSTRB) {
        set_drive_reg(this);
    }

    u8 q67 = (this->iwm.lines.Q6 << 1) | (this->iwm.lines.Q7);
    u16 v = 0;
    switch(q67) {
        case 0b00: {// read data register
            v = this->iwm.active == IWMM_IDLE ? 0xFF : this->iwm.regs.data; }
            if (!is_write) printf("\nREAD FLOPPY DATA REGISTER: %02x cyc:%lld", v, this->clock.master_cycles);
            return v;
        case 0b01: {// Q6=0 Q7=1 handhshake regiter
            v = this->iwm.regs.write_handshake;
            if (!is_write) printf("\nREAD FLOPPY HANDSHAKE REGISTER: %02x cyc:%lld", v, this->clock.master_cycles);
            return v; }
        case 0b10: {// read status register. Q6=1, Q7=0
            v = (this->iwm.regs.status.u & 0x7F) | (((!floppy_inserted(this)) || floppy_write_protected(this)) ? 0x80 : 0);
            if (!is_write) printf("\nREAD FLOPPY STATUS REGISTER: %02x phases:%02x sel:%d cyc:%lld", v, this->iwm.lines.phases, this->iwm.lines.SELECT, this->clock.master_cycles);
            return v; }
        case 0b11: {
            if (onoff) { // If it's a write...
                if (this->iwm.active != IWMM_IDLE) { // write mode register
                    printf("\nWRITE DATA %02x  cyc:%lld", val, this->clock.master_cycles);
                    iwm_write_data(this, val);
                }
                else { // read/write data register
                    iwm_write_mode(this, val);
                }
            }
            return 0xFF; }
    }
    assert(1==0);
    return 0xFF;
}

u16 mac_iwm_read(struct mac *this, u8 addr)
{
    return iwm_control(this, addr, 0, 0);
}

void mac_iwm_write(struct mac *this, u8 addr, u8 val)
{
    iwm_control(this, addr, val, 1);
}
