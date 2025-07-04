//
// Created by . on 2/11/25.
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(_MSC_VER)
#include <stdio.h>
#else
#include <printf.h>
#endif

#include "helpers/intrinsics.h"

#include "gte.h"

static inline void multiply_matrix_by_vector(struct R3000_GTE *this, struct gte_cmd *config, enum gteMatrix mat, u8 vei, enum gteControlVector crv);

static const u8 UNR_TABLE[257] = {
    0xff, 0xfd, 0xfb, 0xf9, 0xf7, 0xf5, 0xf3, 0xf1,
    0xef, 0xee, 0xec, 0xea, 0xe8, 0xe6, 0xe4, 0xe3,
    0xe1, 0xdf, 0xdd, 0xdc, 0xda, 0xd8, 0xd6, 0xd5,
    0xd3, 0xd1, 0xd0, 0xce, 0xcd, 0xcb, 0xc9, 0xc8,
    0xc6, 0xc5, 0xc3, 0xc1, 0xc0, 0xbe, 0xbd, 0xbb,
    0xba, 0xb8, 0xb7, 0xb5, 0xb4, 0xb2, 0xb1, 0xb0,
    0xae, 0xad, 0xab, 0xaa, 0xa9, 0xa7, 0xa6, 0xa4,
    0xa3, 0xa2, 0xa0, 0x9f, 0x9e, 0x9c, 0x9b, 0x9a,
    0x99, 0x97, 0x96, 0x95, 0x94, 0x92, 0x91, 0x90,
    0x8f, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x87, 0x86,
    0x85, 0x84, 0x83, 0x82, 0x81, 0x7f, 0x7e, 0x7d,
    0x7c, 0x7b, 0x7a, 0x79, 0x78, 0x77, 0x75, 0x74,
    0x73, 0x72, 0x71, 0x70, 0x6f, 0x6e, 0x6d, 0x6c,
    0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64,
    0x63, 0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d, 0x5d,
    0x5c, 0x5b, 0x5a, 0x59, 0x58, 0x57, 0x56, 0x55,
    0x54, 0x53, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x4e,
    0x4d, 0x4d, 0x4c, 0x4b, 0x4a, 0x49, 0x48, 0x48,
    0x47, 0x46, 0x45, 0x44, 0x43, 0x43, 0x42, 0x41,
    0x40, 0x3f, 0x3f, 0x3e, 0x3d, 0x3c, 0x3c, 0x3b,
    0x3a, 0x39, 0x39, 0x38, 0x37, 0x36, 0x36, 0x35,
    0x34, 0x33, 0x33, 0x32, 0x31, 0x31, 0x30, 0x2f,
    0x2e, 0x2e, 0x2d, 0x2c, 0x2c, 0x2b, 0x2a, 0x2a,
    0x29, 0x28, 0x28, 0x27, 0x26, 0x26, 0x25, 0x24,
    0x24, 0x23, 0x22, 0x22, 0x21, 0x20, 0x20, 0x1f,
    0x1e, 0x1e, 0x1d, 0x1d, 0x1c, 0x1b, 0x1b, 0x1a,
    0x19, 0x19, 0x18, 0x18, 0x17, 0x16, 0x16, 0x15,
    0x15, 0x14, 0x14, 0x13, 0x12, 0x12, 0x11, 0x11,
    0x10, 0x0f, 0x0f, 0x0e, 0x0e, 0x0d, 0x0d, 0x0c,
    0x0c, 0x0b, 0x0a, 0x0a, 0x09, 0x09, 0x08, 0x08,
    0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04,
    0x03, 0x03, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00,
    0x00
};


static inline u32 reciprocal(u16 d) {
    u32 index = ((d & 0x7FFF) + 0x40) >>7;
    i32 factor = (i32)UNR_TABLE[index] + 0x101; // TODO: is this good?
    i32 di = (i32)(d | 0x8000);
    u32 tmp = ((di * -factor) + 0x80) >> 8;

    return (u32)(((factor * (0x20000 + tmp)) + 0x80) >> 8);
}

static inline u32 GTEdivide(u16 numerator, u16 divisor) {
    u32 shift = __builtin_clz((u32)divisor) - 16;
    u64 n = (u64) numerator << shift;
    u64 d = (u64) divisor << shift;
    u64 rec = (u64)reciprocal((u16) d);
    u64 res = (n * rec + 0x8000) >> 16;
    if (res <= 0x1FFFF)
        return (u32) res;
    else
        return 0x1FFFF;
}

//@ts-ignore
static inline u32 saturate5s(i16 v) {
    if (v < 0) return 0;
    else if (v > 0x1f) return 0x1f;
    return (u32)v & 0x1F;
}

inline static void set_flag(struct R3000_GTE *this, int num)
{
    /*static int c = 0;
    if (num == 15)  {
        c++;
        if (c == 5)
            printf("\nHEY! %d", c);
    }*/
    this->flags |= (1 << num);
}

inline static i16 i32_to_i16_saturate(struct R3000_GTE *this, struct gte_cmd *config, u8 flag, i32 val)
{
   i32 min = config->clamp_negative ? 0 : -32768;
    i32 max = 32767;

    if (val > max) {
        set_flag(this, 24 - flag);
        return 32767;
    }
    else if (val < min) {
        set_flag(this, 24 - flag);
        return (i16)min;
    }
    return (i16)val;

}

static inline i64 i64_to_i44(struct R3000_GTE *this, u8 flag, i64 val)
{
    if (val >= (1LL << 43)) {
        set_flag(this, 30 - flag);
    }
    else if (val < -(1LL << 43)) {
        set_flag(this, 27 - flag);
    }

    // sign-extend result
    return (val << 20) >> 20;
}

static inline i16 i32_to_i11_saturate(struct R3000_GTE *this, u8 flag, i32 val)
{
    if (val < -0x400) {
        set_flag(this, 14 - flag);
        return -0x400;
    }
    else if (val > 0x3FF) {
        set_flag(this, 14 - flag);
        return 0x3FF;
    }
    return (i16)val;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

static inline void check_mac_overflow(struct R3000_GTE *this, i64 val)
{
    if (val < -0x80000000L) {
        set_flag(this, 15);
    } else if (val > 0x7fffffffL) {
        set_flag(this, 16);
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static u32 do_RTP(struct R3000_GTE *this, struct gte_cmd *config, u32 vector_index) {
    i32 z_shifted = 0;

    enum gteMatrix rm = GTE_Rotation;
    enum gteControlVector tr = GTE_Translation;

    for (u32 r = 0; r < 3; r++) {
        i64 res = ((i64)this->control_vectors[tr][r]) << 12;
        for (u32 c = 0; c < 3; c++) {
            i32 v = (i32)this->v[vector_index][c];
            i32 m = (i32)this->matrices[rm][r][c];
            i32 rot = v * m;

            res = i64_to_i44(this, (u8)r, res + (i64)rot);
        }
        this->mac[r+1] = (i32)(res >> config->shift);
        z_shifted = (i32)(res >> 12);
    }

    this->ir[1] = i32_to_i16_saturate(this, config, 0, this->mac[1]);
    this->ir[2] = i32_to_i16_saturate(this, config, 1, this->mac[2]);

    // Weird behavior on Z clip
    i32 min = -32768;
    i32 max = 32767;
    if ((z_shifted > max) || (z_shifted < min))
        set_flag(this, 22);

    min = config->clamp_negative ? 0 : -32768;
    i32 val = this->mac[3];
    if (val < min) this->ir[3] = (i16)min;
    else if (val > max) this->ir[3] = 32767;
    else this->ir[3] = (i16)val;

    u16 z_saturated;
    if (z_shifted < 0) {
        set_flag(this, 18);
        z_saturated = 0;
    }
    else if (z_shifted > 65535) {
        set_flag(this, 18);
        z_saturated = 65535;
    } else {
        z_saturated = (u16)z_shifted;
    }

    this->z_fifo[0] = this->z_fifo[1];
    this->z_fifo[1] = this->z_fifo[2];
    this->z_fifo[2] = this->z_fifo[3];
    this->z_fifo[3] = z_saturated;

    u32 projection_factor;
    if (z_saturated > (this->h / 2)) {
        projection_factor = GTEdivide(this->h, z_saturated);
    }
    else {
        set_flag(this, 17);
        projection_factor = 0x1FFFF;
    }

    i64 factor = (i64)projection_factor;
    i64 x = (i64)this->ir[1];
    i64 y = (i64)this->ir[2];
    i64 ofx = (i64)this->ofx;
    i64 ofy = (i64)this->ofy;

    i64 screen_xa = x * factor + ofx;
    i64 screen_ya = y * factor + ofy;

    check_mac_overflow(this, screen_xa);
    check_mac_overflow(this, screen_ya);

    i32 screen_x = (i32)(screen_xa >> 16);
    i32 screen_y = (i32)(screen_ya >> 16);

    //this->xy_fifo[3][0] = (i16)((u16)this->i32_to_i11_saturate(0, screen_x) & 0x7FF);
    //this->xy_fifo[3][1] = (i16)((u16)this->i32_to_i11_saturate(1, screen_y) & 0x7FF);
    this->xy_fifo[3][0] = (i16)i32_to_i11_saturate(this, 0, screen_x);
    this->xy_fifo[3][1] = (i16)i32_to_i11_saturate(this, 1, screen_y);
    this->xy_fifo[0][0] = this->xy_fifo[1][0];
    this->xy_fifo[0][1] = this->xy_fifo[1][1];
    this->xy_fifo[1][0] = this->xy_fifo[2][0];
    this->xy_fifo[1][1] = this->xy_fifo[2][1];
    this->xy_fifo[2][0] = this->xy_fifo[3][0];
    this->xy_fifo[2][1] = this->xy_fifo[3][1];

    return projection_factor;
}

static inline void depth_queueing(struct R3000_GTE *this, u32 pf)
{
    i64 factor = (i64)pf;
    i64 dqa = (i64)this->dqa;
    i64 dqb = (i64)this->dqb;

    i64 depth = dqb + dqa * factor;

    check_mac_overflow(this, depth);

    this->mac[0] = (i32)depth;

    depth >>= 12;

    if (depth < 0) {
        set_flag(this, 12);
        this->ir[0] = 0;
    }
    else if (depth > 4096) {
        set_flag(this, 12);
        this->ir[0] = 4096;
    }
    else
        this->ir[0] = (i16)depth;
}

static void cmd_RTPS(struct R3000_GTE *this, struct gte_cmd *config)
{
    u32 pf = do_RTP(this, config, 0);
    depth_queueing(this, pf);
}

static void cmd_NCLIP(struct R3000_GTE *this)
{
    i32 x0 = (i32)this->xy_fifo[0][0];
    i32 y0 = (i32)this->xy_fifo[0][1];

    i32 x1 = (i32)this->xy_fifo[1][0] & 0xFFFFFFFF;
    i32 y1 = (i32)this->xy_fifo[1][1] & 0xFFFFFFFF;

    i32 x2 = (i32)this->xy_fifo[2][0] & 0xFFFFFFFF;
    i32 y2 = (i32)this->xy_fifo[2][1] & 0xFFFFFFFF;

    i32 a = x0 * (y1 - y2);
    i32 b = x1 * (y2 - y0);
    i32 c = x2 * (y0 - y1);

    i64 sum = (i64)a + (i64)b + (i64)c;

    check_mac_overflow(this, sum);

    this->mac[0] = (i32)sum;
}

static inline void mac_to_ir(struct R3000_GTE *this, struct gte_cmd *config)
{
    this->ir[1] = i32_to_i16_saturate(this, config, 0, this->mac[1]);
    this->ir[2] = i32_to_i16_saturate(this, config, 1, this->mac[2]);
    this->ir[3] = i32_to_i16_saturate(this, config, 2, this->mac[3]);
}

static inline u8 mac_to_color(struct R3000_GTE *this, i32 mac, u8 which)
{
    i32 c = mac >> 4;
    if (c < 0) {
        set_flag(this, 21 - which);
        return 0;
    }
    else if (c > 0xFF) {
        set_flag(this, 21 - which);
        return 0xFF;
    }
    return (u8)c;
}

static inline void mac_to_rgb_fifo(struct R3000_GTE *this)
{
    i32 mac1 = this->mac[1];
    i32 mac2 = this->mac[2];
    i32 mac3 = this->mac[3];

    u8 r = mac_to_color(this, mac1, 0);
    u8 g = mac_to_color(this, mac2, 1);
    u8 b = mac_to_color(this, mac3, 2);
    this->rgb_fifo[0][0] = this->rgb_fifo[1][0];
    this->rgb_fifo[0][1] = this->rgb_fifo[1][1];
    this->rgb_fifo[0][2] = this->rgb_fifo[1][2];
    this->rgb_fifo[0][3] = this->rgb_fifo[1][3];

    this->rgb_fifo[1][0] = this->rgb_fifo[2][0];
    this->rgb_fifo[1][1] = this->rgb_fifo[2][1];
    this->rgb_fifo[1][2] = this->rgb_fifo[2][2];
    this->rgb_fifo[1][3] = this->rgb_fifo[2][3];

    this->rgb_fifo[2][0] = r;
    this->rgb_fifo[2][1] = g;
    this->rgb_fifo[2][2] = b;
    this->rgb_fifo[2][3] = this->rgb[3];
}

static void cmd_OP(struct R3000_GTE *this, struct gte_cmd *config)
{
    enum gteMatrix rm = GTE_Rotation;

    i32 ir1 = (i32)this->ir[1];
    i32 ir2 = (i32)this->ir[2];
    i32 ir3 = (i32)this->ir[3];

    i32 r0 = (i32)this->matrices[rm][0][0];
    i32 r1 = (i32)this->matrices[rm][1][1];
    i32 r2 = (i32)this->matrices[rm][2][2];

    i32 shift = config->shift;

    this->mac[1] = (r1 * ir3 - r2 * ir2) >> shift;
    this->mac[2] = (r2 * ir1 - r0 * ir3) >> shift;
    this->mac[3] = (r0 * ir2 - r1 * ir1) >> shift;

    mac_to_ir(this, config);
}

static void cmd_from_command(struct gte_cmd *this, u32 cmd) {
    this->shift = ((cmd & (1 << 19)) != 0) ? 12 : 0;
    this->clamp_negative = (cmd >> 10) & 1;
    this->matrix = (cmd >> 17) & 3;
    this->vector_mul = (u8)((cmd >> 15) & 3);
    this->vector_add = (cmd >> 13) & 3;
}

void GTE_init(struct R3000_GTE *this)
{
    memset(this, 0, sizeof(*this));
    cmd_from_command(&this->config0, 0);
    this->config1.clamp_negative = 1;
}

static void cmd_DPCS(struct R3000_GTE *this, struct gte_cmd *config)
{
    enum gteControlVector fca = GTE_FarColor;
    u8 rgb[3];
    rgb[0] = this->rgb[0];
    rgb[1] = this->rgb[1];
    rgb[2] = this->rgb[2];


    for (u32 i = 0; i < 3; i++) {
        i64 fc = ((i64)this->control_vectors[fca][i]) << 12;
        i64 col = ((i64)rgb[i]) << 16;

        i64  sub = fc - col;

        i64 tmp = (i32)(i64_to_i44(this, (u8)i, sub) >> config->shift);

        i64 ir0 = (i64)this->ir[0];

        i64 sat = (i64)i32_to_i16_saturate(this, &this->config0, (u8)i, tmp);

        i64 res = i64_to_i44(this, (u8)i, col + ir0 * sat);

        this->mac[i + 1] = (i32)(res >> config->shift);
    }

    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static void cmd_INTPL(struct R3000_GTE *this, struct gte_cmd *config)
{
    enum gteControlVector fca = GTE_FarColor;

    for (u32 i = 0; i < 3; i++) {
        i64 fc = ((i64)this->control_vectors[fca][i]) << 12;
        i64 ir = ((i64)this->ir[i + 1]) << 12;

        i64 sub = fc - ir;
        i32 tmp = (i32)(i64_to_i44(this, (u8)i, sub) >> config->shift);
        i64 ir0 = (i64)this->ir[0];
        i64 sat = i32_to_i16_saturate(this, &this->config0, (u8)i, tmp);
        i64 res = i64_to_i44(this, (u8)i, ir + ir0 * sat);
        this->mac[i + 1] = (i32)(res >> config->shift);
    }
    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static void cmd_DCPL(struct R3000_GTE *this, struct gte_cmd *config)
{
    enum gteControlVector fca = GTE_FarColor;

    u8 crol[3];
    crol[0] = this->rgb[0];
    crol[1] = this->rgb[1];
    crol[2] = this->rgb[2];

    for (u32 i = 0; i < 3; i++) {
        i64 fc = ((i64)this->control_vectors[fca][i]) << 12;
        i32 ir = (i32)this->ir[i + 1];
        i64 col = ((i32)crol[i]) << 4;

        i64 shading = (i64)(col * ir);

        i64 tmpr = fc - shading;

        i32 tmp = (i32)(i64_to_i44(this, (u8)i, tmpr) >> config->shift);
        i64 ir0 = (i64)this->ir[0];
        i64 res = (i64)i32_to_i16_saturate(this, &this->config0, (u8)i, tmp);

        res = i64_to_i44(this, (u8)i, shading + ir0 * res);

        this->mac[i+1] = (i32)(res >> config->shift);
    }
    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static inline void do_ncd(struct R3000_GTE *this, struct gte_cmd *config, u8 vector_index)
{
    multiply_matrix_by_vector(this, config, GTE_Light, vector_index, GTE_Zero);

    this->v[3][0] = this->ir[1];
    this->v[3][1] = this->ir[2];
    this->v[3][2] = this->ir[3];

    multiply_matrix_by_vector(this, config, GTE_Color, 3, GTE_BackgroundColor);

    cmd_DCPL(this, config);
}

static inline void multiply_matrix_by_vector(struct R3000_GTE *this, struct gte_cmd *config, enum gteMatrix mat, u8 vei, enum gteControlVector crv)
{
    i32 vector_index = (i32)vei;
    if (mat == GTE_Invalid) {
        this->matrices[mat][0][0] = (-((i16)this->rgb[0])) << 4;
        this->matrices[mat][0][1] = ((i16)this->rgb[0]) << 4;
        this->matrices[mat][0][2] = this->ir[0];
        this->matrices[mat][1][0] = this->matrices[mat][1][1] = this->matrices[mat][1][2] = this->matrices[0][0][2];
        this->matrices[mat][2][0] = this->matrices[mat][2][1] = this->matrices[mat][2][2] = this->matrices[0][1][1];
    }

    u32 far_color = crv == GTE_FarColor;

    for (u32 r = 0; r < 3; r++) {
        i64 res = ((i64)this->control_vectors[crv][r]) << 12;
        for (u32 c = 0; c < 3; c++) {
            i32 v = (i32)this->v[vector_index][c];
            i32 m = (i32)this->matrices[mat][r][c];

            i32 product = v * m;

            res = i64_to_i44(this, (u8)r, res + (i64)product);
            if (far_color && (c == 0)) {
                i32_to_i16_saturate(this, &this->config, r, (i32)(res >> config->shift));
                res = 0;
            }
        }

        this->mac[r + 1] = (i32)(res >> config->shift);
    }

    mac_to_ir(this, config);
}

static void do_ncc(struct R3000_GTE *this, struct gte_cmd *config, u8 vector_index)
{
    multiply_matrix_by_vector(this, config, GTE_Light, vector_index, GTE_Zero);

    this->v[3][0] = this->ir[1];
    this->v[3][1] = this->ir[2];
    this->v[3][2] = this->ir[3];

    multiply_matrix_by_vector(this, config, GTE_Color, 3, GTE_BackgroundColor);

    u8 crol[3];
    crol[0] = this->rgb[0];
    crol[1] = this->rgb[1];
    crol[2] = this->rgb[2];

    for (u32 i = 0; i < 3; i++) {
        i32 col = ((i32)crol[i]) << 4;
        i32 ir = (i32)this->ir[i + 1];

        this->mac[i + 1] = (col * ir) >> config->shift;
    }

    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static void cmd_MVMVA(struct R3000_GTE *this, struct gte_cmd *config) {
    this->v[3][0] = this->ir[1];
    this->v[3][1] = this->ir[2];
    this->v[3][2] = this->ir[3];

    multiply_matrix_by_vector(this, config, config->matrix, config->vector_mul, config->vector_add);
}

static void cmd_NCDS(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_ncd(this, config, 0);
}

static void cmd_CDP(struct R3000_GTE *this, struct gte_cmd *config)
{
    this->v[3][0] = this->ir[1];
    this->v[3][1] = this->ir[2];
    this->v[3][2] = this->ir[3];

    multiply_matrix_by_vector(this, config, GTE_Color, 3, GTE_BackgroundColor);

    cmd_DCPL(this, config);
}

static void cmd_NCDT(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_ncd(this, config, 0);
    do_ncd(this, config, 1);
    do_ncd(this, config, 2);
}

static void cmd_NCCS(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_ncc(this, config, 0);
}

static void cmd_CC(struct R3000_GTE *this, struct gte_cmd *config)
{
    this->v[3][0] = this->ir[1];
    this->v[3][1] = this->ir[2];
    this->v[3][2] = this->ir[3];

    multiply_matrix_by_vector(this, config, GTE_Color, 3, GTE_BackgroundColor);

    u8 crol[3];
    crol[0] = this->rgb[0];
    crol[1] = this->rgb[1];
    crol[2] = this->rgb[2];

    for (u32 i = 0; i < 3; i++) {
        i32 col = ((i32)crol[i]) << 4;
        i32 ir = (i32)this->ir[i + 1];

        this->mac[i + 1] = (col * ir) >> config->shift;
    }

    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

// TODO: what
static void do_dpc(struct R3000_GTE *this, struct gte_cmd *config)
{
    enum gteControlVector fca = GTE_FarColor;

    u8 crol[3];
    crol[0] = this->rgb_fifo[0][0];
    crol[1] = this->rgb_fifo[0][1];
    crol[2] = this->rgb_fifo[0][2];

    for (u32 i = 0; i < 3; i++) {
        i64 fc = ((i64)this->control_vectors[fca][i]) << 12;
        i64 col = ((i64)crol[i]) << 16;

        i64 sub = fc - col;
        i32 tmp = (i32)(i64_to_i44(this, (u8)i, sub) >> config->shift);
        i64 ir0 = (i64)this->ir[0];
        i64 sat = (i64)i32_to_i16_saturate(this, &this->config0, (u8)i, tmp);

        i64 res = i64_to_i44(this, (u8)i, col + ir0 * sat);

        this->mac[i + 1] = (i32)(res >> config->shift);
    }

    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static void do_nc(struct R3000_GTE *this, struct gte_cmd *config, u8 vector_index)
{
    multiply_matrix_by_vector(this, config, GTE_Light, vector_index, GTE_Zero);

    this->v[3][0] = this->ir[1];
    this->v[3][1] = this->ir[2];
    this->v[3][2] = this->ir[3];

    multiply_matrix_by_vector(this, config, GTE_Color, 3, GTE_BackgroundColor);

    u8 crol[3];
    crol[0] = this->rgb[0];
    crol[1] = this->rgb[1];
    crol[2] = this->rgb[2];

    mac_to_rgb_fifo(this);
}

static void cmd_NCS(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_nc(this, config, 0);
}

static void cmd_NCT(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_nc(this, config, 0);
    do_nc(this, config, 1);
    do_nc(this, config, 2);
}

static void cmd_SQR(struct R3000_GTE *this, struct gte_cmd *config)
{
    for (u32 i = 1; i < 4; i++) {
        i32 ir = (i32)this->ir[i];
        this->mac[i] = (ir * ir) >> config->shift;
    }
    mac_to_ir(this, config);
}

static void cmd_DPCT(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_dpc(this, config);
    do_dpc(this, config);
    do_dpc(this, config);
}

static inline u16 i64_to_otz(struct R3000_GTE *this, i64 average) {
    i64 value = average >> 12;

    if (value < 0) {
        set_flag(this, 18);
        return 0;
    } else if (value > 0xFFFF) {
        set_flag(this, 18);
        return 0xFFFF;
    }
    return (u16)value;
}

static void cmd_AVSZ3(struct R3000_GTE *this)
{
    u32 z1 = this->z_fifo[1];
    u32 z2 = this->z_fifo[2];
    u32 z3 = this->z_fifo[3];

    u32 sum = z1 + z2 + z3;

    i64 zsf3 = (i64) this->zsf3;
    i64 average = zsf3 * (i64)sum;

    check_mac_overflow(this, average);

    this->mac[0] = (i32)average;
    this->otz = i64_to_otz(this, average);
}

static void cmd_AVSZ4(struct R3000_GTE *this)
{
    u32 z0 = this->z_fifo[0];
    u32 z1 = this->z_fifo[1];
    u32 z2 = this->z_fifo[2];
    u32 z3 = this->z_fifo[3];

    u32 sum = z0 + z1 + z2 + z3;

    i64 zsf4 = (i64)this->zsf4;

    i64 average = zsf4 * (i64)sum;

    check_mac_overflow(this, average);

    this->mac[0] = (i32)average;
    this->otz = i64_to_otz(this, average);
}

static void cmd_RTPT(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_RTP(this, config, 0);
    do_RTP(this, config, 1);

    u32 pf = do_RTP(this, config, 2);

    depth_queueing(this, pf);
}

static void cmd_GPF(struct R3000_GTE *this, struct gte_cmd *config)
{
    i32 ir0 = (i32)this->ir[0];

    for (u32 i = 1; i < 4; i++) { // TODO 1?
        i32 ir = (i32)this->ir[i];

        this->mac[i] = (ir * ir0) >> config->shift;
    }

    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static void cmd_GPL(struct R3000_GTE *this, struct gte_cmd *config)
{
    i32 ir0 = (i32)this->ir[0];
    u8 shift = config->shift;
    for (u32 i = 1; i < 4; i++) { // TODO: 1?
        i32 ir = (i32)this->ir[i];

        i64 ir_prod = (i64)(ir * ir0);

        i64 mac = ((i64)this->mac[i]) << shift;

        i64 sum = i64_to_i44(this, (u8)(i - 1), mac + ir_prod);

        this->mac[i] = (i32)(sum >> shift);
    }
    mac_to_ir(this, config);
    mac_to_rgb_fifo(this);
}

static void cmd_NCCT(struct R3000_GTE *this, struct gte_cmd *config)
{
    do_ncc(this, config, 0);
    do_ncc(this, config, 1);
    do_ncc(this, config, 2);
}

void GTE_command(struct R3000_GTE *this, u32 opcode, u64 current_clock)
{
    u32 opc = opcode & 0x3F;
    struct gte_cmd *config = &this->config;
    cmd_from_command(config, opcode);
    this->flags = 0;
    switch(opc) {
        case 0x01: cmd_RTPS(this, config); this->cycle_count = 14; break;
        case 0x06: cmd_NCLIP(this); this->cycle_count = 7; break;
        case 0x0C: cmd_OP(this, config); this->cycle_count = 5; break;
        case 0x10: cmd_DPCS(this, config); this->cycle_count = 7; break;
        case 0x11: cmd_INTPL(this, config); this->cycle_count = 7; break;
        case 0x12: cmd_MVMVA(this, config); this->cycle_count = 7; break;
        case 0x13: cmd_NCDS(this, config); this->cycle_count = 18; break;
        case 0x14: cmd_CDP(this, config); this->cycle_count = 13; break;
        case 0x16: cmd_NCDT(this, config); this->cycle_count = 43; break;
        case 0x1B: cmd_NCCS(this, config); this->cycle_count = 16; break;
        case 0x1C: cmd_CC(this, config); this->cycle_count = 10; break;
        case 0x1E: cmd_NCS(this, config); this->cycle_count = 13; break;
        case 0x20: cmd_NCT(this, config); this->cycle_count = 29; break;
        case 0x28: cmd_SQR(this, config); this->cycle_count = 4; break;
        case 0x29: cmd_DCPL(this, config); this->cycle_count = 7; break;
        case 0x2A: cmd_DPCT(this, config); this->cycle_count = 16; break;
        case 0x2D: cmd_AVSZ3(this); this->cycle_count = 4; break;
        case 0x2E: cmd_AVSZ4(this); this->cycle_count = 4; break;
        case 0x30: cmd_RTPT(this, config); this->cycle_count = 22; break;
        case 0x3D: cmd_GPF(this, config); this->cycle_count = 4; break;
        case 0x3E: cmd_GPL(this, config); this->cycle_count = 4; break;
        case 0x3F: cmd_NCCT(this, config); this->cycle_count = 38; break;
        default:
            printf("\nUnsupported GTE opcode %02x", opc);
            break;
    }

    u32 msb = ((this->flags & 0x7f87e000) != 0) ? 0x80000000 : 0;
    this->flags |= msb;
    this->clock_start = current_clock;
    this->clock_end = current_clock + this->cycle_count;
}

void GTE_write_reg(struct R3000_GTE *this, u32 reg, u32 val)
{
    switch(reg) {
        case 0:
            this->v[0][0] = (i16)val;
            this->v[0][1] = (i16)(val >> 16);
            break;
        case 1:
            this->v[0][2] = (i16)val;
            break;
        case 2:
            this->v[1][0] = (i16)val;
            this->v[1][1] = (i16)(val >> 16);
            break;
        case 3:
            this->v[1][2] = (i16)val;
            break;
        case 4:
            this->v[2][0] = (i16)val;
            this->v[2][1] = (i16)(val >> 16);
            break;
        case 5:
            this->v[2][2] = (i16)val;
            break;
        case 6:
            this->rgb[0] = (u8)val;
            this->rgb[1] = (u8)(val >> 8);
            this->rgb[2] = (u8)(val >> 16);
            this->rgb[3] = (u8)(val >> 24);
            break;
        case 7: this->otz = (u16)val; break;
        case 8: this->ir[0] = (i16)val; break;
        case 9: this->ir[1] = (i16)val; break;
        case 10: this->ir[2] = (i16)val; break;
        case 11: this->ir[3] = (i16)val; break;
        case 12:
            this->xy_fifo[0][0] = (i16)val;
            this->xy_fifo[0][1] = (i16)(val >> 16);
            break;
        case 13:
            this->xy_fifo[1][0] = (i16)val;
            this->xy_fifo[1][1] = (i16)(val >> 16);
            break;
        case 14:
            this->xy_fifo[2][0] = this->xy_fifo[3][0] = (i16)val;
            this->xy_fifo[2][1] = this->xy_fifo[3][1] = (i16)(val >> 16);
            break;
        case 15:
            this->xy_fifo[0][0] = this->xy_fifo[1][0];
            this->xy_fifo[0][1] = this->xy_fifo[1][1];
            this->xy_fifo[1][0] = this->xy_fifo[2][0];
            this->xy_fifo[1][1] = this->xy_fifo[2][1];
            this->xy_fifo[2][0] = this->xy_fifo[3][0] = (i16)val;
            this->xy_fifo[2][1] = this->xy_fifo[3][1] = (i16)(val >> 16);
            break;
        case 16: this->z_fifo[0] = (u16)val; break;
        case 17: this->z_fifo[1] = (u16)val; break;
        case 18: this->z_fifo[2] = (u16)val; break;
        case 19: this->z_fifo[3] = (u16)val; break;
        case 20:
            this->rgb_fifo[0][0] = (u8)val;
            this->rgb_fifo[0][1] = (u8)(val >> 8);
            this->rgb_fifo[0][2] = (u8)(val >> 16);
            this->rgb_fifo[0][3] = (u8)(val >> 24);
            break;
        case 21:
            this->rgb_fifo[1][0] = (u8)val;
            this->rgb_fifo[1][1] = (u8)(val >> 8);
            this->rgb_fifo[1][2] = (u8)(val >> 16);
            this->rgb_fifo[1][3] = (u8)(val >> 24);
            break;
        case 22:
            this->rgb_fifo[2][0] = (u8)val;
            this->rgb_fifo[2][1] = (u8)(val >> 8);
            this->rgb_fifo[2][2] = (u8)(val >> 16);
            this->rgb_fifo[2][3] = (u8)(val >> 24);
            break;
        case 23: this->reg_23 = val; break;
        case 24: this->mac[0] = (i32)val; break;
        case 25: this->mac[1] = (i32)val; break;
        case 26: this->mac[2] = (i32)val; break;
        case 27: this->mac[3] = (i32)val; break;
        case 28:
            this->ir[1] = (i16)(((val) & 0x1F) << 7);
            this->ir[2] = (i16)(((val >> 5) & 0x1F) << 7);
            this->ir[3] = (i16)(((val >> 10) & 0x1F) << 7);
            break;
        case 29:
            break;
        case 30:
            this->lzcs = val;
            u32 tmp = ((val >> 31) & 1) ? (val ^ 0xFFFFFFFF) : val;
            this->lzcr = (u8)__builtin_clz(tmp);
            break;
        case 31:
            //printf("\nWrite to read-only GTE reg 31");
            break;
        case 32: // 0
            this->matrices[0][0][0] = (i16)val;
            this->matrices[0][0][1] = (i16)(val >> 16);
            break;
        case 33: // 1
            this->matrices[0][0][2] = (i16)val;
            this->matrices[0][1][0] = (i16)(val >> 16);
            break;
        case 34: // 2
            this->matrices[0][1][1] = (i16)val;
            this->matrices[0][1][2] = (i16)(val >> 16);
            break;
        case 35: // 3
            this->matrices[0][2][0] = (i16)val;
            this->matrices[0][2][1] = (i16)(val >> 16);
            break;
        case 36: // 4
            this->matrices[0][2][2] = (i16)val;
            break;
        case 37: // 5-7
        case 38:
        case 39:
            this->control_vectors[0][reg - 37] = (i32)val;
            break;
        case 40: // 8
            this->matrices[1][0][0] = (i16)val;
            this->matrices[1][0][1] = (i16)(val >> 16);
            break;
        case 41: // 9
            this->matrices[1][0][2] = (i16)val;
            this->matrices[1][1][0] = (i16)(val >> 16);
            break;
        case 42: // 10
            this->matrices[1][1][1] = (i16)val;
            this->matrices[1][1][2] = (i16)(val >> 16);
            break;
        case 43: // 11
            this->matrices[1][2][0] = (i16)val;
            this->matrices[1][2][1] = (i16)(val >> 16);
            break;
        case 44: // 12
            this->matrices[1][2][2] = (i16)val;
            break;
        case 45: // 13-15
        case 46:
        case 47:
            this->control_vectors[1][reg - 45] = (i32)val;
            break;
        case 48: // 16
            this->matrices[2][0][0] = (i16)val;
            this->matrices[2][0][1] = (i16)(val >> 16);
            break;
        case 49: // 17
            this->matrices[2][0][2] = (i16)val;
            this->matrices[2][1][0] = (i16)(val >> 16);
            break;
        case 50: // 18
            this->matrices[2][1][1] = (i16)val;
            this->matrices[2][1][2] = (i16)(val >> 16);
            break;
        case 51: // 19
            this->matrices[2][2][0] = (i16)val;
            this->matrices[2][2][1] = (i16)(val >> 16);
            break;
        case 52: // 20
            this->matrices[2][2][2] = (i16)val;
            break;
        case 53: // 21-23
        case 54:
        case 55:
            this->control_vectors[2][reg - 53] = (i32)val;
            break;
        case 56: // 24
            this->ofx = (i32)val;
            break;
        case 57: // 25
            this->ofy = (i32)val;
            break;
        case 58: // 26
            this->h = (u16)val;
            break;
        case 59: // 27
            this->dqa = (i16)val;
            break;
        case 60: // 28
            this->dqb = (i32)val;
            break;
        case 61: // 29
            this->zsf3 = (i16)val;
            break;
        case 62: // 30
            this->zsf4 = (i16)val;
            break;
        case 63: // 31
            this->flags = (val & 0x7ffff000) | ((val & 0x7f87e000) ? (1 << 31) : 0);
            break;
    }
}

u32 GTE_read_reg(struct R3000_GTE *this, u32 reg)
{
    switch(reg) {
        case 0: return ((u32)(u16)this->v[0][0]) | (((u32)(u16)this->v[0][1]) << 16);
        case 1: return (u32)(i32)this->v[0][2];
        case 2: return ((u32)(u16)this->v[1][0]) | (((u32)(u16)this->v[1][1]) << 16);
        case 3: return (u32)(i32)this->v[1][2];
        case 4: return ((u32)(u16)this->v[2][0]) | (((u32)(u16)this->v[2][1]) << 16);
        case 5: return (u32)(i32)this->v[2][2];
        case 6: return (u32)this->rgb[0] | ((u32)this->rgb[1] << 8) | ((u32)this->rgb[2] << 16) | ((u32)this->rgb[3] << 24);
        case 7: return (u32)this->otz;
        case 8:return (u32)this->ir[0];
        case 9: return (u32)this->ir[1];
        case 10: return (u32)this->ir[2];
        case 11: return (u32)this->ir[3];
        case 12: return (u32)(u16)this->xy_fifo[0][0] | ((u32)(u16)this->xy_fifo[0][1] << 16);
        case 13: return (u32)(u16)this->xy_fifo[1][0] | ((u32)(u16)this->xy_fifo[1][1] << 16);
        case 14: return (u32)(u16)this->xy_fifo[2][0] | ((u32)(u16)this->xy_fifo[2][1] << 16);
        case 15: {
            u32 v = (u32)(u16)this->xy_fifo[3][0] | ((u32)(u16)this->xy_fifo[3][1] << 16);
            return v;
        }
        case 16: return (u32)this->z_fifo[0];
        case 17: return (u32)this->z_fifo[1];
        case 18: return (u32)this->z_fifo[2];
        case 19: return (u32)this->z_fifo[3];
        case 20: return (u32)this->rgb_fifo[0][0] | ((u32)this->rgb_fifo[0][1] << 8) | ((u32)this->rgb_fifo[0][2] << 16) | ((u32)this->rgb_fifo[0][3] << 24);
        case 21: return (u32)this->rgb_fifo[1][0] | ((u32)this->rgb_fifo[1][1] << 8) | ((u32)this->rgb_fifo[1][2] << 16) | ((u32)this->rgb_fifo[1][3] << 24);
        case 22: return (u32)this->rgb_fifo[2][0] | ((u32)this->rgb_fifo[2][1] << 8) | ((u32)this->rgb_fifo[2][2] << 16) | ((u32)this->rgb_fifo[2][3] << 24);
        case 23: return this->reg_23;
        case 24: return (u32)this->mac[0];
        case 25: return (u32)this->mac[1];
        case 26: return (u32)this->mac[2];
        case 27: return (u32)this->mac[3];
        case 28:
        case 29:
            return saturate5s(this->ir[1] >> 7) | (saturate5s(this->ir[2] >> 7) << 5) | (saturate5s(this->ir[3] >> 7) << 10);
        case 30: return this->lzcs;
        case 31: return (u32)this->lzcr;
        case 32: return ((u32)(u16)this->matrices[0][0][0]) | (((u32)(u16)this->matrices[0][0][1]) << 16);
        case 33: return ((u32)(u16)this->matrices[0][0][2]) | (((u32)(u16)this->matrices[0][1][0]) << 16);
        case 34: return ((u32)(u16)this->matrices[0][1][1]) | (((u32)(u16)this->matrices[0][1][2]) << 16);
        case 35: return ((u32)(u16)this->matrices[0][2][0]) | (((u32)(u16)this->matrices[0][2][1]) << 16);
        case 36: return (u32)(i32)this->matrices[0][2][2];
        case 37:
        case 38:
        case 39:
            return (u32)this->control_vectors[0][reg - 37];
        case 40: return ((u32)(u16)this->matrices[1][0][0]) | (((u32)(u16)this->matrices[1][0][1]) << 16);
        case 41: return ((u32)(u16)this->matrices[1][0][2]) | (((u32)(u16)this->matrices[1][1][0]) << 16);
        case 42: return ((u32)(u16)this->matrices[1][1][1]) | (((u32)(u16)this->matrices[1][1][2]) << 16);
        case 43: return ((u32)(u16)this->matrices[1][2][0]) | (((u32)(u16)this->matrices[1][2][1]) << 16);
        case 44: return (u32)(i32)this->matrices[1][2][2];
        case 45:
        case 46:
        case 47:
            return (u32)this->control_vectors[1][reg - 45];
        case 48: return ((u32)(u16)this->matrices[2][0][0]) | (((u32)(u16)this->matrices[2][0][1]) << 16);
        case 49: return ((u32)(u16)this->matrices[2][0][2]) | (((u32)(u16)this->matrices[2][1][0]) << 16);
        case 50: return ((u32)(u16)this->matrices[2][1][1]) | (((u32)(u16)this->matrices[2][1][2]) << 16);
        case 51: return ((u32)(u16)this->matrices[2][2][0]) | (((u32)(u16)this->matrices[2][2][1]) << 16);
        case 52: return (u32)(i32)this->matrices[2][2][2];
        case 53:
        case 54:
        case 55:
            return (u32)this->control_vectors[2][reg - 53];
        case 56: return (u32)this->ofx;
        case 57: return (u32)this->ofy;
        case 58: return (u32)(i32)(i16)this->h; // H reads back as signed even though unsigned
        case 59: return (u32)this->dqa;
        case 60: return (u32)this->dqb;
        case 61: return (u32)this->zsf3;
        case 62: return (u32)this->zsf4;
        case 63: {
            this->flags &= 0x7FFFF000;
            //30...23, 18..13
            //   7F80FC00
            // 0x7f87e00
            if (this->flags & 0x7F87E000) this->flags |= 0x80000000;
            return this->flags;
        }
    }
    NOGOHERE;
    return 0;
}
