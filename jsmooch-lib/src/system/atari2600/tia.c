//
// Created by . on 4/14/24.
//

#include <string.h>
#include "stdio.h"

#include "tia.h"
#include "atari2600.h"

void TIA_init(struct atari_TIA* this)
{
    TIA_reset(this);
}

void TIA_reset(struct atari_TIA* this)
{
    this->hcounter = 0;
    this->vcounter = 0;
    this->cpu_RDY = 0;
    this->frames_since_restart = 0;
    this->io.vblank = this->io.vsync = 0;
    for (u32 i = 0; i < TIA_WQ_MAX; i++) {
        this->write_queue[i].active = 0;
    }
}

#define IN_VBLANK_IN (this->vcounter < this->timing.display_line_start)
#define IN_DISPLAY ((this->vcounter >= this->timing.display_line_start) && (this->vcounter < this->timing.vblank_out_start))
#define IN_VBLANK_OUT (this->sy >= this->timing.vblank_out_start)
#define OUT_OF_FRAME (this->vcounter > (this->timing.vblank_out_lines + this->timing.vblank_out_start))

static void TIA_WQ_add(struct atari_TIA* this, u32 num, u32 val, u32 delay)
{
    for (u32 i = 0; i < TIA_WQ_MAX; i++) {
        if (this->write_queue[i].active == 0) {
            this->write_queue[i].active = 1;
            this->write_queue[i].delay = delay;
            this->write_queue[i].data = val;
            this->write_queue[i].address = num;
            return;
        }
    }
    printf("\nWRITE QUEUE FULL! ERROR!");
}

static void TIA_WQ_finish(struct atari_TIA* this, struct atari_TIA_WQ_item* item)
{
    item->active = 0;
    u32 val = item->data;
    switch(item->address) {
        case 0x01: // VBLANK
            this->io.vblank = (val >> 1) & 1;
            this->io.inpt_4_5_ctrl = (val >> 6) & 1;
            this->io.inpt_0_3_ctrl = (val >> 7) & 1;
            return;
        case 0x0B: // REFP0 reflect player 0
            this->io.REFP[0] = (val >> 3) & 1;
            return;
        case 0x0C: // REFP1 reflect player 1
            this->io.REFP[1] = (val >> 3) & 1;
            return;
        case 0x0D: // PF0 playfield register byte 0
            this->io.pf &= 0xFFFF;
            this->io.pf |= ((val >> 4) & 15) << 16;
            return;
        case 0x0E: // PF1 playfield register byte 1
            this->io.pf &= 0xF00FF;
            this->io.pf |= (val & 0xFF) << 8;
            return;
        case 0x0F: // PF2 playfield register byte 2
            this->io.pf &= 0xFFF00;
            this->io.pf |= (val & 0xFF);
            return;
        case 0x1B: // GRP0 graphics player 0
            this->p[0].GRP_cache = val & 0xFF;
            if (!this->io.VDELP[0])
                this->p[0].GRP = val & 0xFF;
            if (this->io.VDELP[1]) // Commit GRP1 now that GRP0 is comitted
                this->p[1].GRP = this->p[1].GRP_cache;
            return;
        case 0x1C: // GRP1 graphics player 1
            this->p[1].GRP_cache = val & 0xFF;
            if (!this->io.VDELP[1])
                this->p[1].GRP = val & 0xFF;
            if (this->io.VDELP[0]) // Commit GRP0 now that GRP1 is comitted
                this->p[0].GRP = this->p[0].GRP_cache;
            if (this->io.VDELBL)
                this->ball.enable = this->ball.enable_cache;
            return;
        case 0x1D: // ENAM0 enable missile 0
            this->m[0].enable = (val >> 1) & 1;
            return;
        case 0x1E: // ENAM1 enable missile 1
            this->m[1].enable = (val >> 1) & 1;
            return;
        case 0x1F: // ENABL enable ball
            this->ball.enable_cache = (val >> 1) & 1;
            if (!this->io.VDELBL)
                this->ball.enable = (val >> 1) & 1;
            return;
        case 0x20: // HMP0 horizontal motion player 0
            this->p[0].hm = (i32)SIGNe4to32(val);
            return;
        case 0x21: // HMP1 horizontal motion player 1
            this->p[1].hm = (i32)SIGNe4to32(val);
            return;
        case 0x22: // HMM0 horizontal motion missile 0
            this->m[0].hm = (i32)SIGNe4to32(val);
            return;
        case 0x23: // HMM1 horizontal motion missile 1
            this->m[1].hm = (i32)SIGNe4to32(val);
            return;
        case 0x24: // HMBL horizontal motion ball
            this->ball.hm = (i32)SIGNe4to32(val);
            return;
        case 0x2A: // HMOVE apply horizontal motion <strobe>
        {
            this->io.hmoved = 1;
            i32 npx[2] = {(i32)this->p[0].x + this->p[0].hm, (i32)this->p[1].x + this->p[1].hm};
            i32 nmx[2] = {(i32)this->m[0].x + this->m[0].hm, (i32)this->m[1].x + this->m[1].hm};
            i32 nbx = (i32)this->ball.x + this->ball.hm;

#define HMOV_CORRECT(rrr) ((rrr) < 0 ? (rrr) + 160 : (rrr) > 159 ? (rrr) - 160 : (rrr));
            this->p[0].x = HMOV_CORRECT(npx[0]);
            this->p[1].x = HMOV_CORRECT(npx[1]);
            this->m[0].x = HMOV_CORRECT(nmx[0]);
            this->m[1].x = HMOV_CORRECT(nmx[1]);
            this->ball.x = HMOV_CORRECT(nbx);
#undef HMOV_CORRECT
            // TODO: HMOVE extra offsets depending on where we are outside of line
            return;
        }
        case 0x2B: // HMCLR clear horizontal motion registers <strobe>
            this->p[0].x = this->p[1].x = this->m[0].x = this->m[1].x = this->ball.x = 0;
            return;

    }
}


static void TIA_WQ_cycle(struct atari_TIA* this) {
    for (u32 i = 0; i < TIA_WQ_MAX; i++) {
        if (this->write_queue[i].active) {
            struct atari_TIA_WQ_item* item = &this->write_queue[i];
            item->delay--;
            if (item->delay == 0)
                TIA_WQ_finish(this, item);
        }
    }
}

static u32 TIA_read(struct atari_TIA* this, u32 addr, u32 *data)
{
    switch((addr & 0x0F) | 0x30) {
        case 0x30: // CXM0P read collision data for M0-P1, M0-P0
            return (this->col.m0_p1 << 7) | (this->col.m0_p0 << 6);
        case 0x31: // CXM1P read collision data for M1-P0, M1-P1
            return (this->col.m1_p0 << 7) | (this->col.m1_p1 << 6);
        case 0x32: // CXP0FB read collision data for P0-PF, P0-BALL
            return (this->col.p0_pf << 7) | (this->col.p0_ball << 6);
        case 0x33: // CXP1FB read collision data for P1-PF, P1-BALL
            return (this->col.p1_pf << 7) | (this->col.p1_ball << 6);
        case 0x34: // CXM0FB read collision data for M0-PF, M0-BALL
            return (this->col.m0_pf << 7) | (this->col.m0_ball << 6);
        case 0x35: // CXM1FB read collision data for M1-PF, M1-BALL
            return (this->col.m1_pf << 7) | (this->col.m1_ball << 6);
        case 0x36: // CXBLPF read collision data for BALL-PF
            return (this->col.ball_pf << 7);
        case 0x37: // CXPPMM read collision data for P0-P1, M0-M1
            return (this->col.p0_p1 << 7) | (this->col.m0_m1 << 6);
        case 0x38: // INPT0 read pot 0
            return this->io.INPT[0];
        case 0x39: // INPT1 read pot 1
            return this->io.INPT[1];
        case 0x3A: // INPT2 read pot 2
            return this->io.INPT[2];
        case 0x3B: // INPT3 read pot 3
            return this->io.INPT[3];
        case 0x3C: // INPT4 read pot 4
            return this->io.INPT[4];
        case 0x3D: // INPT5 read pot 5
            return this->io.INPT[5];
        default:
            printf("\nUnknown TIA read %02x", addr);
            return 0;
    }
}


static const u32 centering_offsets[8] = { 3, 3, 3, 3, 3, 6, 3, 10};
static void update_RESMPn(struct atari_TIA* this, u32 num)
{
    // If RESMP behavior is enabled...
    // "As long as Bit 1 is set, the missile is hidden and its horizontal position is
    // centered on the players position. The centering offset is +3 for normal,
    // +6 for double, and +10 quad sized player (that is giving good centering
    // results with missile widths of 2, 4, and 8 respectively).
    if (this->io.RESMP[num])
        this->m[num].x = this->p[num].x + centering_offsets[this->p[num].size];
    // TODO: do real centering logic here, or is this good!??!
}

static void update_RESMP(struct atari_TIA* this)
{
    update_RESMPn(this, 0);
    update_RESMPn(this, 1);
}

static void TIA_new_frame(struct atari_TIA* this);

static void TIA_vsync(struct atari_TIA* this, u32 val)
{
    // 0...1
    if (val && (this->io.vsync != val)) {
        TIA_new_frame(this);
    }
    this->io.vsync = val;
}

static void TIA_write(struct atari_TIA* this, u32 addr, u32 *data)
{
    // b-f, 1b-24
    u32 val = *data;
    i32 msx = (i32)this->hcounter - 68;
#define DELAY(num, howlong) case num: TIA_WQ_add(this, num, val, howlong); return
    switch(addr & 0x3F) {
        DELAY(0x01, 1);
        DELAY(0x0B, 1);
        DELAY(0x0C, 1);
        DELAY(0x0D, 2);
        DELAY(0x0E, 2);
        DELAY(0x0F, 2);
        DELAY(0x1B, 1);
        DELAY(0x1C, 1);
        DELAY(0x1D, 1);
        DELAY(0x1E, 1);
        DELAY(0x1F, 1);
        DELAY(0x20, 2);
        DELAY(0x21, 2);
        DELAY(0x22, 2);
        DELAY(0x23, 2);
        DELAY(0x24, 2);
        DELAY(0x2A, 6);
        DELAY(0x2B, 2);
        case 0x00: // VSYNC
            TIA_vsync(this, (val >> 1) & 1);
            return;
        case 0x02: // halt processor until end of current scanline
            this->cpu_RDY = 1;
            return;
        case 0x03: // RSYNC
            this->hcounter = 0;
            return;
        case 0x04: // NUSIZ0...number and size of missile 0
            this->p[0].size = val & 7;
            this->m[0].size = (val >> 4) & 3;
            return;
        case 0x05: // NUSIZ1...number and size of missile 1
            this->p[1].size = val & 7;
            this->m[1].size = (val >> 4) & 3;
            return;
            return;
        case 0x06: // COLUP0 colum-lum player 0 missile 0
            this->io.COLUP0 = (val >> 1) & 0x7F;
            return;
        case 0x07: // COLUP1 colum-lum player 1 missile 1
            this->io.COLUP1 = (val >> 1) & 0x7F;
            return;
        case 0x08: // COLUPF colum-lum playfield and ball
            this->io.COLUPF = (val >> 1) & 0x7F;
            return;
        case 0x09: // COLUBK colum-lum background
            this->io.COLUBK = (val >> 1) & 0x7F;
            return;
        case 0x0A: // CTRLPF control playfield ball size & collissions
            this->io.CTRLPF = val & 0b110111;
            return;
        case 0x10: // RESP0 reset player 0 <strobe>

            if (msx < 0)
                this->p[0].x = 3;
            else
                this->p[0].x = msx;
            update_RESMP(this);
            return;
        case 0x11: // RESP1 reset player 1 <strobe>
            if (msx < 0)
                this->p[1].x = 3;
            else
                this->p[1].x = msx;
            update_RESMP(this);
            return;
        case 0x12: // RESM0 reset missile 0 <strobe>
            if (msx < 0)
                this->m[0].x = 2;
            else
                this->m[0].x = msx;
            return;
        case 0x13: // RESM1 reset missile 1 <strobe>
            if (msx < 0)
                this->m[1].x = 2;
            else
                this->m[1].x = msx;
            update_RESMP(this);
            return;
        case 0x14: // RESBL reset ball <strobe>
            if (msx < 0)
                this->ball.x = 2;
            else
                this->ball.x = msx;
            update_RESMP(this);
            return;
        case 0x15: // AUDC0 audio control 0
            return;
        case 0x16: // AUDC1 audio control 1
            return;
        case 0x17: // AUDF0 audio frequency 0
            return;
        case 0x18: // AUDF1 audio frequency 1
            return;
        case 0x19: // AUDV0 audio volume 0
            return;
        case 0x1A: // AUDV1 audio volume 1
            return;
        case 0x25: // VDELP0 vertical delay player 0
            this->io.VDELP[0] = val & 1;
            return;
        case 0x26: // VDELP1 vertical delay player 1
            this->io.VDELP[1] = val & 1;
            return;
        case 0x27: // VDELBL vertical delay ball
            this->io.VDELBL = val & 1;
            return;
        case 0x28: // RESMP0 reset missile 0 to player 0
            this->io.RESMP[0] = (val >> 1) & 1;
            return;
        case 0x29: // RESMP1 reset missile 1 to player 1
            this->io.RESMP[1] = (val >> 1) & 1;
            return;
        case 0x2C: // CXCLR clear collision latches <strobe>
            memset(&this->col, 0, sizeof(struct atari_TIA_col));
            return;
        default:
            printf("\nUnknow TIA write to %02x", addr & 0x3F);
            return;
    }
#undef DELAY
}

void TIA_bus_cycle(struct atari_TIA* this, u32 addr, u32 *data, u32 rw)
{
    if (rw == 0) *data = TIA_read(this, addr, data);
    else TIA_write(this, addr, data);
}

static void TIA_new_frame(struct atari_TIA* this)
{
    // Update internal state
    this->vcounter = 0;
    this->io.hmoved = 0;

    // Update meta state
    this->master_frame++;
    this->frames_since_restart++;

    // Flip framebuffer
    this->cur_output = this->display->device.display.output[this->display->device.display.last_written];
    this->display->device.display.last_written ^= 1;
}

static void TIA_new_line(struct atari_TIA* this)
{
    this->hcounter = 0;
    this->cpu_RDY = 0;
    this->vcounter++;
    if (OUT_OF_FRAME) TIA_new_frame(this);
}

/*
 * Normal (REF=0) : PF0.4-7 PF1.7-0 PF2.0-7  PF0.4-7 PF1.7-0 PF2.0-7
 * Mirror (REF=1) : PF0.4-7 PF1.7-0 PF2.0-7  PF2.7-0 PF1.0-7 PF0.7-4
 */

// Missile size depending on NUSIZx.missile_size
static const u32 M_SIZE[4] = { 1, 2, 4, 8};

// A table to help us exract correct bit based on x-position inside player sprite and if it's flipped
static const u32 PLAYER_SHIFTS[2][8] = {
        // Normal. MSB first left to right
        {7, 6, 5, 4, 3, 2, 1, 0},
        // Mirrored. LSB first left to right
        {0, 1, 2, 3, 4, 5, 6, 7}
};

// A table to help us extract playfield bits for any of the 40 valid horizontal positions, including with mirroring, and considering the odd way playfields are written
static const u32 PF_SHIFTS[2][40] = {
        // normal
        {   // left half
            16, 17, 18, 19,  // PF0.4-7
            15, 14, 13, 12, 11, 10, 9, 8, // PF1.7-0
            0, 1, 2, 3, 4, 5, 6, 7, // PF2.0-7
            // right half
            16, 17, 18, 19,
            15, 14, 13, 12, 11, 10, 9, 8,
            0, 1, 2, 3, 4, 5, 6, 7
         },
        // mirrored
        {
            // left half
            16, 17, 18, 19,  // PF0.4-7
            15, 14, 13, 12, 11, 10, 9, 8, // PF1.7-0
            0, 1, 2, 3, 4, 5, 6, 7, // PF2.0-7
            // right half PF2.7-0 PF1.0-7 PF0.7-4
            7, 6, 5, 4, 3, 2, 1, 0, // PF2.7-0
            8, 9, 10, 11, 12, 13, 14, 15, // PF1.0-7
            19, 18, 17, 16 // PF0.7-4
        }
};

static const u32 NUSIZ_copies[8][10] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0  One copy              (X.........)
        {1, 0, 1, 0, 0, 0, 0, 0, 0, 0}, // 1  Two copies - close    (X.X.......)
        {1, 0, 0, 0, 1, 0, 0, 0, 0, 0}, // 2  Two copies - medium   (X...X.....)
        {1, 0, 1, 0, 1, 0, 0, 0, 0, 0}, // 3  Three copies - close  (X.X.X.....)
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0}, // 4  Two copies - wide     (X.......X.)
        {1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // 5  Double sized player   (XX........)
        {1, 0, 0, 0, 1, 0, 0, 0, 1, 0}, // 6  Three copies - medium (X...X...X.)
        {1, 1, 1, 1, 0, 0, 0, 0, 0, 0}  // 7  Quad sized player     (XXXX......)
};

static const u32 DIFF_SHIFTS[8] = {
        0, 0, 0, 0, 0, 1, 0, 2
};

static u32 get_player_pixel(struct atari_TIA* this, u32 msx, u32 player_num)
{
    // No player pixels off left side of screen
    if (this->hcounter < 68) return 0;

    // Invisible players give no pixels, skip all the checks!
    if (this->p[player_num].GRP == 0) return 0;

    u32 player_x = this->p[player_num].x; // Player X
    // If we're not within 80 pixels of the position, return 0
    if ((msx - player_x) >= 80) return 0;

    // We haven't yet gotten to the left edge of the player X...
    if (msx < player_x) return 0;

    u32 pm_num_size = this->p[player_num].size;

    // Check if we're inside it depending on copy mode
    if (!NUSIZ_copies[pm_num_size][player_x >> 3]) return 0;

    u32 player_px = ((msx - player_x) >> DIFF_SHIFTS[pm_num_size]) & 7;
    // Extract bit from GRP
    return (this->p[player_num].GRP >> PLAYER_SHIFTS[this->io.REFP[player_num]][player_px]) & 1;
}

static const u32 BALL_SIZES[4] = {1, 2, 4, 8};

static u32 get_ball_pixel(struct atari_TIA* this, u32 msx)
{
    if (this->ball.enable == 0) return 0; // Ball is not enabled
    if (msx < this->ball.x) return 0; // We aren't at left edge of ball yet

    u32 ball_size = BALL_SIZES[(this->io.CTRLPF >> 4) & 3];
    return msx < (this->ball.x + ball_size);
}

// Get m0 pixel
static u32 get_missile_pixel(struct atari_TIA* this, u32 msx, u32 missile_num)
{
    if (this->m[missile_num].enable == 0) return 0; // Missile not enabled
    if (this->io.RESMP[missile_num]) return 0; // Missile is hidden and centering
    if (msx < this->m[missile_num].x) return 0; // We aren't at left edge of missile yet

    u32 missile_size = BALL_SIZES[(this->m[missile_num].size)];
    return msx < (this->m[missile_num].x + missile_size);
}

void TIA_draw_pixel(struct atari_TIA* this)
{
    u32 msx = this->hcounter - 68;
    u32 pf_x = msx >> 2;
    u32 msy = this->vcounter - this->timing.display_line_start;
    u8 color = 0;
    if (!this->io.vblank) {
        // Get pixels for pf, ball, m0, m1, p0, p1
        u32 pf, ball, m0, m1, p0, p1;

        // Get playfield pixel
        pf = (this->io.pf >> PF_SHIFTS[this->io.CTRLPF & 1][pf_x]) & 1;

        // Get p0 pixel
        p0 = get_player_pixel(this, msx, 0);
        p1 = get_player_pixel(this, msx, 1);
        // Get ball pixel
        ball = get_ball_pixel(this, msx);

        // Get m0 pixel
        m0 = get_missile_pixel(this, msx, 0);
        m1 = get_missile_pixel(this, msx, 1);

        // Do collisions
        this->col.m0_p0 |= m0 & p0;
        this->col.m0_p1 |= m0 & p1;
        this->col.m1_p0 |= m1 & p0;
        this->col.m1_p1 |= m1 & p1;
        this->col.p0_pf |= p0 & pf;
        this->col.p0_ball |= p0 & ball;
        this->col.p1_pf |= p1 & pf;
        this->col.p1_ball |= p1 & ball;
        this->col.m0_pf |= m0 & pf;
        this->col.m0_ball |= m0 & ball;
        this->col.m1_pf |= m1 & pf;
        this->col.m1_ball |= m1 & ball;
        this->col.ball_pf |= ball & pf;
        this->col.p0_p1 |= p0 & p1;
        this->col.m0_m1 |= m0 & m1;

        // Convert pixels to the 4 colors
        u8 COLUP0 = 0, COLUP1 = 0, COLUPF = 0;
        // SCORE-mode assignment
        if ((this->io.CTRLPF & 2) && (!(this->io.CTRLPF & 4))) {
            COLUP0 = p0 || m0 || ((msx < 80) && pf);
            COLUP1 = p1 || m1 || ((msx >= 80) && pf);
            COLUPF = ball;
        }
        else { // Regular-mode assignment
            COLUP0 = p0 || m0;
            COLUP1 = p1 || m1;
            COLUPF = ball || pf;
        }

        color = this->io.COLUBK;

        if((!(this->io.CTRLPF & 4)) && (pf || ball)) color = this->io.COLUPF;
        if(p1 || m1) color = this->io.COLUP1;
        if(p0 || m0) color = this->io.COLUP0;
        if((!(this->io.CTRLPF & 4)) && (pf || ball))  color = this->io.COLUPF;
    }
    u32 bo = (msy * 160) + msx;
    this->cur_output[bo] = color;

}

void TIA_run_cycle(struct atari_TIA* this)
{
    // A frame is...
    // 40 lines vsync
    // 192 lines NTSC
    //
    update_RESMP(this);
    TIA_WQ_cycle(this);
    if (IN_VBLANK_IN) { // 0...39
        // Don't really do anything.
    }
    else if (IN_DISPLAY) { // display lines
        if (this->hcounter >= 68) {
            TIA_draw_pixel(this);
        }
    }
    else { // vblank out
        // Don't really do anything.
    }

    this->hcounter++;
    if (this->hcounter >= 228) TIA_new_line(this);
}

