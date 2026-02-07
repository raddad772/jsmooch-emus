//
// Created by . on 2/11/25.
//

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdio>

#include "helpers/intrinsics.h"

#include "gte.h"

namespace R3000::GTE {

static constexpr u8 UNR_TABLE[257] = {
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


static inline u32 reciprocal(const u16 d) {
    const u32 index = ((d & 0x7FFF) + 0x40) >>7;
    const i32 factor = static_cast<i32>(UNR_TABLE[index]) + 0x101; // TODO: is this good?
    const i32 di = (d | 0x8000);
    const u32 tmp = ((di * -factor) + 0x80) >> 8;

    return (((factor * (0x20000 + tmp)) + 0x80) >> 8);
}

static inline u32 GTEdivide(const u16 numerator, const u16 divisor) {
    const u32 shift = __builtin_clz(divisor) - 16;
    const u64 n = static_cast<u64>(numerator) << shift;
    const u64 d = static_cast<u64>(divisor) << shift;
    const u64 rec = reciprocal(static_cast<u16>(d));
    const u64 res = (n * rec + 0x8000) >> 16;
    if (res <= 0x1FFFF)
        return static_cast<u32>(res);
    else
        return 0x1FFFF;
}

static inline u32 saturate5s(i16 v) {
    if (v < 0) return 0;
    else if (v > 0x1f) return 0x1f;
    return static_cast<u32>(v) & 0x1F;
}

u32 core::do_RTP(CMD *config, u32 vector_index) {
    i32 z_shifted = 0;

    constexpr MATRIX rm = MTX_Rotation;
    constexpr control_vector tr = CV_Translation;

    for (u32 r = 0; r < 3; r++) {
        i64 res = static_cast<i64>(control_vectors[tr][r]) << 12;
        for (u32 c = 0; c < 3; c++) {
            const i32 v = V[vector_index][c];
            const i32 m = matrices[rm][r][c];
            const i32 rot = v * m;

            res = i64_to_i44(static_cast<u8>(r), res + static_cast<i64>(rot));
        }
        MAC[r+1] = static_cast<i32>(res >> config->shift);
        z_shifted = static_cast<i32>(res >> 12);
    }

    IR[1] = i32_to_i16_saturate(config, 0, MAC[1]);
    IR[2] = i32_to_i16_saturate(config, 1, MAC[2]);

    // Weird behavior on Z clip
    i32 min = -32768;
    i32 max = 32767;
    if ((z_shifted > max) || (z_shifted < min))
        set_flag(22);

    min = config->clamp_negative ? 0 : -32768;
    i32 val = MAC[3];
    if (val < min) IR[3] = static_cast<i16>(min);
    else if (val > max) IR[3] = 32767;
    else IR[3] = static_cast<i16>(val);

    u16 z_saturated;
    if (z_shifted < 0) {
        set_flag(18);
        z_saturated = 0;
    }
    else if (z_shifted > 65535) {
        set_flag(18);
        z_saturated = 65535;
    } else {
        z_saturated = static_cast<u16>(z_shifted);
    }

    z_fifo[0] = z_fifo[1];
    z_fifo[1] = z_fifo[2];
    z_fifo[2] = z_fifo[3];
    z_fifo[3] = z_saturated;

    u32 projection_factor;
    if (z_saturated > (h / 2)) {
        projection_factor = GTEdivide(h, z_saturated);
    }
    else {
        set_flag(17);
        projection_factor = 0x1FFFF;
    }

    const i64 factor = projection_factor;
    const i64 x = IR[1];
    const i64 y = IR[2];
    const i64 ofx = OFX;
    const i64 ofy = OFY;

    i64 screen_xa = x * factor + ofx;
    i64 screen_ya = y * factor + ofy;

    check_mac_overflow(screen_xa);
    check_mac_overflow(screen_ya);

    i32 screen_x = static_cast<i32>(screen_xa >> 16);
    i32 screen_y = static_cast<i32>(screen_ya >> 16);

    //xy_fifo[3][0] = (i16)((u16)i32_to_i11_saturate(0, screen_x) & 0x7FF);
    //xy_fifo[3][1] = (i16)((u16)i32_to_i11_saturate(1, screen_y) & 0x7FF);
    xy_fifo[3][0] = i32_to_i11_saturate(0, screen_x);
    xy_fifo[3][1] = i32_to_i11_saturate(1, screen_y);
    xy_fifo[0][0] = xy_fifo[1][0];
    xy_fifo[0][1] = xy_fifo[1][1];
    xy_fifo[1][0] = xy_fifo[2][0];
    xy_fifo[1][1] = xy_fifo[2][1];
    xy_fifo[2][0] = xy_fifo[3][0];
    xy_fifo[2][1] = xy_fifo[3][1];

    return projection_factor;
}

void core::depth_queueing(u32 pf)
{
    const i64 factor = pf;
    const i64 dqa = DQA;
    const i64 dqb = DQB;

    i64 depth = dqb + dqa * factor;

    check_mac_overflow(depth);

    MAC[0] = static_cast<i32>(depth);

    depth >>= 12;

    if (depth < 0) {
        set_flag(12);
        IR[0] = 0;
    }
    else if (depth > 4096) {
        set_flag(12);
        IR[0] = 4096;
    }
    else
        IR[0] = static_cast<i16>(depth);
}

void core::cmd_RTPS(CMD *config)
{
    const u32 pf = do_RTP(config, 0);
    depth_queueing(pf);
}

void core::cmd_NCLIP()
{
    const i32 x0 = xy_fifo[0][0];
    const i32 y0 = xy_fifo[0][1];

    const i32 x1 = xy_fifo[1][0];
    const i32 y1 = xy_fifo[1][1];

    const i32 x2 = xy_fifo[2][0];
    const i32 y2 = xy_fifo[2][1];

    const i32 a = x0 * (y1 - y2);
    const i32 b = x1 * (y2 - y0);
    const i32 c = x2 * (y0 - y1);

    const i64 sum = static_cast<i64>(a) + static_cast<i64>(b) + static_cast<i64>(c);

    check_mac_overflow(sum);

    MAC[0] = static_cast<i32>(sum);
}

void core::cmd_OP(CMD *config)
{
    constexpr MATRIX rm = MTX_Rotation;

    const i32 ir1 = IR[1];
    const i32 ir2 = IR[2];
    const i32 ir3 = IR[3];

    const i32 r0 = matrices[rm][0][0];
    const i32 r1 = matrices[rm][1][1];
    const i32 r2 = matrices[rm][2][2];

    const i32 shift = config->shift;

    MAC[1] = (r1 * ir3 - r2 * ir2) >> shift;
    MAC[2] = (r2 * ir1 - r0 * ir3) >> shift;
    MAC[3] = (r0 * ir2 - r1 * ir1) >> shift;

    mac_to_ir(config);
}

void CMD::from_command(u32 cmd) {
    shift = ((cmd & (1 << 19)) != 0) ? 12 : 0;
    clamp_negative = (cmd >> 10) & 1;
    matrix = static_cast<MATRIX>((cmd >> 17) & 3);
    vector_mul = static_cast<u8>((cmd >> 15) & 3);
    vector_add = static_cast<control_vector>((cmd >> 13) & 3);
}

core::core()
{
    config0.from_command(0);
    config1.clamp_negative = 1;
}

void core::cmd_DPCS(CMD *config)
{
    constexpr control_vector fca = CV_FarColor;
    u8 rgb[3];
    rgb[0] = RGB[0];
    rgb[1] = RGB[1];
    rgb[2] = RGB[2];

    for (u32 i = 0; i < 3; i++) {
        const i64 fc = static_cast<i64>(control_vectors[fca][i]) << 12;
        const i64 col = static_cast<i64>(rgb[i]) << 16;

        const i64 sub = fc - col;

        const i64 tmp = static_cast<i32>(i64_to_i44(static_cast<u8>(i), sub) >> config->shift);

        const i64 ir0 = IR[0];

        const i64 sat = i32_to_i16_saturate(&config0, static_cast<u8>(i), tmp);

        const i64 res = i64_to_i44(static_cast<u8>(i), col + ir0 * sat);

        MAC[i + 1] = static_cast<i32>(res >> config->shift);
    }

    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::cmd_INTPL(CMD *config)
{
    constexpr control_vector fca = CV_FarColor;

    for (u32 i = 0; i < 3; i++) {
        const i64 fc = static_cast<i64>(control_vectors[fca][i]) << 12;
        const i64 ir = static_cast<i64>(IR[i + 1]) << 12;

        const i64 sub = fc - ir;
        const i32 tmp = static_cast<i32>(i64_to_i44(static_cast<u8>(i), sub) >> config->shift);
        const i64 ir0 = IR[0];
        const i64 sat = i32_to_i16_saturate(&config0, static_cast<u8>(i), tmp);
        const i64 res = i64_to_i44(static_cast<u8>(i), ir + ir0 * sat);
        MAC[i + 1] = static_cast<i32>(res >> config->shift);
    }
    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::cmd_DCPL(CMD *config)
{
    constexpr control_vector fca = CV_FarColor;

    u8 crol[3];
    crol[0] = RGB[0];
    crol[1] = RGB[1];
    crol[2] = RGB[2];

    for (u32 i = 0; i < 3; i++) {
        const i64 fc = static_cast<i64>(control_vectors[fca][i]) << 12;
        const i32 ir = IR[i + 1];
        const i64 col = static_cast<i32>(crol[i]) << 4;

        const i64 shading = (col * ir);

        const i64 tmpr = fc - shading;

        const i32 tmp = static_cast<i32>(i64_to_i44(static_cast<u8>(i), tmpr) >> config->shift);
        const i64 ir0 = IR[0];
        i64 res = i32_to_i16_saturate(&config0, static_cast<u8>(i), tmp);

        res = i64_to_i44(static_cast<u8>(i), shading + ir0 * res);

        MAC[i+1] = static_cast<i32>(res >> config->shift);
    }
    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::do_ncd(CMD *config, u8 vector_index)
{
    multiply_matrix_by_vector(config, MTX_Light, vector_index, CV_Zero);

    V[3][0] = IR[1];
    V[3][1] = IR[2];
    V[3][2] = IR[3];

    multiply_matrix_by_vector(config, MTX_Color, 3, CV_BackgroundColor);

    cmd_DCPL(config);
}

void core::multiply_matrix_by_vector(CMD *config, MATRIX mat, u8 vei, control_vector crv)
{
    i32 vector_index = vei;
    if (mat == MTX_Invalid) {
        matrices[mat][0][0] = (-static_cast<i16>(RGB[0])) << 4;
        matrices[mat][0][1] = static_cast<i16>(RGB[0]) << 4;
        matrices[mat][0][2] = IR[0];
        matrices[mat][1][0] = matrices[mat][1][1] = matrices[mat][1][2] = matrices[0][0][2];
        matrices[mat][2][0] = matrices[mat][2][1] = matrices[mat][2][2] = matrices[0][1][1];
    }

    const u32 far_color = crv == CV_FarColor;

    for (u32 r = 0; r < 3; r++) {
        i64 res = static_cast<i64>(control_vectors[crv][r]) << 12;
        for (u32 c = 0; c < 3; c++) {
            const i32 v = V[vector_index][c];
            const i32 m = matrices[mat][r][c];

            const i32 product = v * m;

            res = i64_to_i44(static_cast<u8>(r), res + static_cast<i64>(product));
            if (far_color && (c == 0)) {
                i32_to_i16_saturate(&cfg, r, static_cast<i32>(res >> config->shift));
                res = 0;
            }
        }

        MAC[r + 1] = static_cast<i32>(res >> config->shift);
    }

    mac_to_ir(config);
}

void core::multiply_matrix_by_vector_MVMVA(CMD *config, MATRIX mat, u8 vei, control_vector crv)
{
    i32 vector_index = vei;
    if (mat == MTX_Invalid) {
        matrices[mat][0][0] = (-static_cast<i16>(RGB[0])) << 4;
        matrices[mat][0][1] = static_cast<i16>(RGB[0]) << 4;
        matrices[mat][0][2] = IR[0];
        matrices[mat][1][0] = matrices[mat][1][1] = matrices[mat][1][2] = matrices[0][0][2];
        matrices[mat][2][0] = matrices[mat][2][1] = matrices[mat][2][2] = matrices[0][1][1];
    }

    const u32 far_color = crv == CV_FarColor;

    for (u32 r = 0; r < 3; r++) {
        i64 res = static_cast<i64>(control_vectors[crv][r]) << 12;
        for (u32 c = 0; c < 3; c++) {
            const i32 v = V[vector_index][c];
            const i32 m = matrices[mat][r][c];

            const i32 product = v * m;

            res = i64_to_i44(static_cast<u8>(r), res + static_cast<i64>(product));
            if (far_color && (c == 0)) {
                i32_to_i16_saturate(&cfg, r, static_cast<i32>(res >> config->shift));
                res = 0;
            }
        }

        MAC[r + 1] = static_cast<i32>(res >> config->shift);
    }

    mac_to_ir(config);
}


void core::do_ncc(CMD *config, u8 vector_index)
{
    multiply_matrix_by_vector(config, MTX_Light, vector_index, CV_Zero);

    V[3][0] = IR[1];
    V[3][1] = IR[2];
    V[3][2] = IR[3];

    multiply_matrix_by_vector(config, MTX_Color, 3, CV_BackgroundColor);

    u8 crol[3];
    crol[0] = RGB[0];
    crol[1] = RGB[1];
    crol[2] = RGB[2];

    for (u32 i = 0; i < 3; i++) {
        i32 col = static_cast<i32>(crol[i]) << 4;
        i32 ir = IR[i + 1];

        MAC[i + 1] = (col * ir) >> config->shift;
    }

    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::cmd_MVMVA(CMD *config) {
    V[3][0] = IR[1];
    V[3][1] = IR[2];
    V[3][2] = IR[3];

    multiply_matrix_by_vector_MVMVA(config, config->matrix, config->vector_mul, config->vector_add);
}

void core::cmd_NCDS(CMD *config)
{
    do_ncd(config, 0);
}

void core::cmd_CDP(CMD *config)
{
    V[3][0] = IR[1];
    V[3][1] = IR[2];
    V[3][2] = IR[3];

    multiply_matrix_by_vector(config, MTX_Color, 3, CV_BackgroundColor);

    cmd_DCPL(config);
}

void core::cmd_NCDT(CMD *config)
{
    do_ncd(config, 0);
    do_ncd(config, 1);
    do_ncd(config, 2);
}

void core::cmd_NCCS(CMD *config)
{
    do_ncc(config, 0);
}

void core::cmd_CC(CMD *config)
{
    V[3][0] = IR[1];
    V[3][1] = IR[2];
    V[3][2] = IR[3];

    multiply_matrix_by_vector(config, MTX_Color, 3, CV_BackgroundColor);

    u8 crol[3];
    crol[0] = RGB[0];
    crol[1] = RGB[1];
    crol[2] = RGB[2];

    for (u32 i = 0; i < 3; i++) {
        const i32 col = static_cast<i32>(crol[i]) << 4;
        const i32 ir = IR[i + 1];

        MAC[i + 1] = (col * ir) >> config->shift;
    }

    mac_to_ir(config);
    mac_to_rgb_fifo();
}

// TODO: what
void core::do_dpc(CMD *config)
{
    constexpr control_vector fca = CV_FarColor;

    u8 crol[3];
    crol[0] = rgb_fifo[0][0];
    crol[1] = rgb_fifo[0][1];
    crol[2] = rgb_fifo[0][2];

    for (u32 i = 0; i < 3; i++) {
        const i64 fc = static_cast<i64>(control_vectors[fca][i]) << 12;
        const i64 col = static_cast<i64>(crol[i]) << 16;

        const i64 sub = fc - col;
        const i32 tmp = static_cast<i32>(i64_to_i44(static_cast<u8>(i), sub) >> config->shift);
        const i64 ir0 = IR[0];
        const i64 sat = i32_to_i16_saturate(&config0, static_cast<u8>(i), tmp);

        const i64 res = i64_to_i44(static_cast<u8>(i), col + ir0 * sat);

        MAC[i + 1] = static_cast<i32>(res >> config->shift);
    }

    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::do_nc(CMD *config, u8 vector_index)
{
    multiply_matrix_by_vector(config, MTX_Light, vector_index, CV_Zero);

    V[3][0] = IR[1];
    V[3][1] = IR[2];
    V[3][2] = IR[3];

    multiply_matrix_by_vector(config, MTX_Color, 3, CV_BackgroundColor);

    u8 crol[3];
    crol[0] = RGB[0];
    crol[1] = RGB[1];
    crol[2] = RGB[2];

    mac_to_rgb_fifo();
}

void core::cmd_NCS(CMD *config)
{
    do_nc(config, 0);
}

void core::cmd_NCT(CMD *config)
{
    do_nc(config, 0);
    do_nc(config, 1);
    do_nc(config, 2);
}

void core::cmd_SQR(CMD *config)
{
    for (u32 i = 1; i < 4; i++) {
        i32 ir = (i32)IR[i];
        MAC[i] = (ir * ir) >> config->shift;
    }
    mac_to_ir(config);
}

void core::cmd_DPCT(CMD *config)
{
    do_dpc(config);
    do_dpc(config);
    do_dpc(config);
}

void core::cmd_AVSZ3()
{
    const u32 z1 = z_fifo[1];
    const u32 z2 = z_fifo[2];
    const u32 z3 = z_fifo[3];

    const u32 sum = z1 + z2 + z3;

    const i64 mzsf3 = zsf3;
    const i64 average = mzsf3 * static_cast<i64>(sum);

    check_mac_overflow(average);

    MAC[0] = static_cast<i32>(average);
    otz = i64_to_otz(average);
}

void core::cmd_AVSZ4()
{
    const u32 z0 = z_fifo[0];
    const u32 z1 = z_fifo[1];
    const u32 z2 = z_fifo[2];
    const u32 z3 = z_fifo[3];

    const u32 sum = z0 + z1 + z2 + z3;

    const i64 mzsf4 = zsf4;

    i64 average = mzsf4 * static_cast<i64>(sum);

    check_mac_overflow(average);

    MAC[0] = static_cast<i32>(average);
    otz = i64_to_otz(average);
}

void core::cmd_RTPT(CMD *config)
{
    do_RTP(config, 0);
    do_RTP(config, 1);

    const u32 pf = do_RTP(config, 2);

    depth_queueing(pf);
}

void core::cmd_GPF(CMD *config)
{
    const i32 ir0 = IR[0];

    for (u32 i = 1; i < 4; i++) { // TODO 1?
        const i32 ir = IR[i];

        MAC[i] = (ir * ir0) >> config->shift;
    }

    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::cmd_GPL(CMD *config)
{
    i32 ir0 = IR[0];
    u8 shift = config->shift;
    for (u32 i = 1; i < 4; i++) { // TODO: 1?
        const i32 ir = IR[i];

        //const i64 ir_prod = (i64)(ir * ir0); // PRECISION LOSS?
        const i64 ir_prod = static_cast<i64>(ir) * static_cast<i64>(ir0); // PRECISION LOSS?

        const i64 mac = static_cast<i64>(MAC[i]) << shift;

        const i64 sum = i64_to_i44(static_cast<u8>(i - 1), mac + ir_prod);

        MAC[i] = static_cast<i32>(sum >> shift);
    }
    mac_to_ir(config);
    mac_to_rgb_fifo();
}

void core::cmd_NCCT(CMD *config)
{
    do_ncc(config, 0);
    do_ncc(config, 1);
    do_ncc(config, 2);
}

void core::command(u32 opcode, u64 current_clock)
{
    u32 opc = opcode & 0x3F;
    CMD *config = &cfg;
    config->from_command(opcode);
    flags = 0;
    switch(opc) {
        /*case 0x01: cmd_RTPS(config); cycle_count = 14; break;
        case 0x06: cmd_NCLIP(); cycle_count = 7; break;
        case 0x0C: cmd_OP(config); cycle_count = 5; break;
        case 0x10: cmd_DPCS(config); cycle_count = 7; break;
        case 0x11: cmd_INTPL(config); cycle_count = 7; break;*/
        case 0x12: cmd_MVMVA(config); cycle_count = 7; break;
        /*case 0x13: cmd_NCDS(config); cycle_count = 18; break;
        case 0x14: cmd_CDP(config); cycle_count = 13; break;
        case 0x16: cmd_NCDT(config); cycle_count = 43; break;
        case 0x1B: cmd_NCCS(config); cycle_count = 16; break;
        case 0x1C: cmd_CC(config); cycle_count = 10; break;
        case 0x1E: cmd_NCS(config); cycle_count = 13; break;
        case 0x20: cmd_NCT(config); cycle_count = 29; break;
        case 0x28: cmd_SQR(config); cycle_count = 4; break;
        case 0x29: cmd_DCPL(config); cycle_count = 7; break;
        case 0x2A: cmd_DPCT(config); cycle_count = 16; break;
        case 0x2D: cmd_AVSZ3(); cycle_count = 4; break;
        case 0x2E: cmd_AVSZ4(); cycle_count = 4; break;
        case 0x30: cmd_RTPT(config); cycle_count = 22; break;
        case 0x3D: cmd_GPF(config); cycle_count = 4; break;
        case 0x3E: cmd_GPL(config); cycle_count = 4; break;
        case 0x3F: cmd_NCCT(config); cycle_count = 38; break;*/
        default:
            printf("\nUnsupported GTE opcode %02x", opc);
            break;
    }

    u32 msb = ((flags & 0x7f87e000) != 0) ? 0x80000000 : 0;
    flags |= msb;
    clock_start = current_clock;
    clock_end = current_clock + cycle_count;
}

void core::write_reg(u32 reg, u32 val)
{
    switch(reg) {
        case 0:
            V[0][0] = static_cast<i16>(val);
            V[0][1] = static_cast<i16>(val >> 16);
            break;
        case 1:
            V[0][2] = static_cast<i16>(val);
            break;
        case 2:
            V[1][0] = static_cast<i16>(val);
            V[1][1] = static_cast<i16>(val >> 16);
            break;
        case 3:
            V[1][2] = static_cast<i16>(val);
            break;
        case 4:
            V[2][0] = static_cast<i16>(val);
            V[2][1] = static_cast<i16>(val >> 16);
            break;
        case 5:
            V[2][2] = static_cast<i16>(val);
            break;
        case 6:
            RGB[0] = static_cast<u8>(val);
            RGB[1] = static_cast<u8>(val >> 8);
            RGB[2] = static_cast<u8>(val >> 16);
            RGB[3] = static_cast<u8>(val >> 24);
            break;
        case 7: otz = static_cast<u16>(val); break;
        case 8: IR[0] = static_cast<i16>(val); break;
        case 9: IR[1] = static_cast<i16>(val); break;
        case 10: IR[2] = static_cast<i16>(val); break;
        case 11: IR[3] = static_cast<i16>(val); break;
        case 12:
            xy_fifo[0][0] = static_cast<i16>(val);
            xy_fifo[0][1] = static_cast<i16>(val >> 16);
            break;
        case 13:
            xy_fifo[1][0] = static_cast<i16>(val);
            xy_fifo[1][1] = static_cast<i16>(val >> 16);
            break;
        case 14:
            xy_fifo[2][0] = xy_fifo[3][0] = static_cast<i16>(val);
            xy_fifo[2][1] = xy_fifo[3][1] = static_cast<i16>(val >> 16);
            break;
        case 15:
            xy_fifo[0][0] = xy_fifo[1][0];
            xy_fifo[0][1] = xy_fifo[1][1];
            xy_fifo[1][0] = xy_fifo[2][0];
            xy_fifo[1][1] = xy_fifo[2][1];
            xy_fifo[2][0] = xy_fifo[3][0] = static_cast<i16>(val);
            xy_fifo[2][1] = xy_fifo[3][1] = static_cast<i16>(val >> 16);
            break;
        case 16: z_fifo[0] = static_cast<u16>(val); break;
        case 17: z_fifo[1] = static_cast<u16>(val); break;
        case 18: z_fifo[2] = static_cast<u16>(val); break;
        case 19: z_fifo[3] = static_cast<u16>(val); break;
        case 20:
            rgb_fifo[0][0] = static_cast<u8>(val);
            rgb_fifo[0][1] = static_cast<u8>(val >> 8);
            rgb_fifo[0][2] = static_cast<u8>(val >> 16);
            rgb_fifo[0][3] = static_cast<u8>(val >> 24);
            break;
        case 21:
            rgb_fifo[1][0] = static_cast<u8>(val);
            rgb_fifo[1][1] = static_cast<u8>(val >> 8);
            rgb_fifo[1][2] = static_cast<u8>(val >> 16);
            rgb_fifo[1][3] = static_cast<u8>(val >> 24);
            break;
        case 22:
            rgb_fifo[2][0] = static_cast<u8>(val);
            rgb_fifo[2][1] = static_cast<u8>(val >> 8);
            rgb_fifo[2][2] = static_cast<u8>(val >> 16);
            rgb_fifo[2][3] = static_cast<u8>(val >> 24);
            break;
        case 23: reg_23 = val; break;
        case 24: MAC[0] = static_cast<i32>(val); break;
        case 25: MAC[1] = static_cast<i32>(val); break;
        case 26: MAC[2] = static_cast<i32>(val); break;
        case 27: MAC[3] = static_cast<i32>(val); break;
        case 28:
            IR[1] = static_cast<i16>(((val) & 0x1F) << 7);
            IR[2] = static_cast<i16>(((val >> 5) & 0x1F) << 7);
            IR[3] = static_cast<i16>(((val >> 10) & 0x1F) << 7);
            break;
        case 29:
            break;
        case 30: {
            lzcs = val;
            u32 tmp = ((val >> 31) & 1) ? (val ^ 0xFFFFFFFF) : val;
            lzcr = static_cast<u8>(__builtin_clz(tmp));
            break; }
        case 31:
            //printf("\nWrite to read-only GTE reg 31");
            break;
        case 32: // 0
            matrices[0][0][0] = static_cast<i16>(val);
            matrices[0][0][1] = static_cast<i16>(val >> 16);
            break;
        case 33: // 1
            matrices[0][0][2] = static_cast<i16>(val);
            matrices[0][1][0] = static_cast<i16>(val >> 16);
            break;
        case 34: // 2
            matrices[0][1][1] = static_cast<i16>(val);
            matrices[0][1][2] = static_cast<i16>(val >> 16);
            break;
        case 35: // 3
            matrices[0][2][0] = static_cast<i16>(val);
            matrices[0][2][1] = static_cast<i16>(val >> 16);
            break;
        case 36: // 4
            matrices[0][2][2] = static_cast<i16>(val);
            break;
        case 37: // 5-7
        case 38:
        case 39:
            control_vectors[0][reg - 37] = static_cast<i32>(val);
            break;
        case 40: // 8
            matrices[1][0][0] = static_cast<i16>(val);
            matrices[1][0][1] = static_cast<i16>(val >> 16);
            break;
        case 41: // 9
            matrices[1][0][2] = static_cast<i16>(val);
            matrices[1][1][0] = static_cast<i16>(val >> 16);
            break;
        case 42: // 10
            matrices[1][1][1] = static_cast<i16>(val);
            matrices[1][1][2] = static_cast<i16>(val >> 16);
            break;
        case 43: // 11
            matrices[1][2][0] = static_cast<i16>(val);
            matrices[1][2][1] = static_cast<i16>(val >> 16);
            break;
        case 44: // 12
            matrices[1][2][2] = static_cast<i16>(val);
            break;
        case 45: // 13-15
        case 46:
        case 47:
            control_vectors[1][reg - 45] = static_cast<i32>(val);
            break;
        case 48: // 16
            matrices[2][0][0] = static_cast<i16>(val);
            matrices[2][0][1] = static_cast<i16>(val >> 16);
            break;
        case 49: // 17
            matrices[2][0][2] = static_cast<i16>(val);
            matrices[2][1][0] = static_cast<i16>(val >> 16);
            break;
        case 50: // 18
            matrices[2][1][1] = static_cast<i16>(val);
            matrices[2][1][2] = static_cast<i16>(val >> 16);
            break;
        case 51: // 19
            matrices[2][2][0] = static_cast<i16>(val);
            matrices[2][2][1] = static_cast<i16>(val >> 16);
            break;
        case 52: // 20
            matrices[2][2][2] = static_cast<i16>(val);
            break;
        case 53: // 21-23
        case 54:
        case 55:
            control_vectors[2][reg - 53] = static_cast<i32>(val);
            break;
        case 56: // 24
            OFX = static_cast<i32>(val);
            break;
        case 57: // 25
            OFY = static_cast<i32>(val);
            break;
        case 58: // 26
            h = static_cast<u16>(val);
            break;
        case 59: // 27
            DQA = static_cast<i16>(val);
            break;
        case 60: // 28
            DQB = static_cast<i32>(val);
            break;
        case 61: // 29
            zsf3 = static_cast<i16>(val);
            break;
        case 62: // 30
            zsf4 = static_cast<i16>(val);
            break;
        case 63: // 31
            flags = (val & 0x7ffff000) | ((val & 0x7f87e000) ? (1 << 31) : 0);
            break;
        default:
    }
}

u32 core::read_reg(const u32 reg)
{
    switch(reg) {
        case 0: return static_cast<u32>(static_cast<u16>(V[0][0])) | (static_cast<u32>(static_cast<u16>(V[0][1])) << 16);
        case 1: return static_cast<u32>(static_cast<i32>(V[0][2]));
        case 2: return static_cast<u32>(static_cast<u16>(V[1][0])) | (static_cast<u32>(static_cast<u16>(V[1][1])) << 16);
        case 3: return static_cast<u32>(static_cast<i32>(V[1][2]));
        case 4: return static_cast<u32>(static_cast<u16>(V[2][0])) | (static_cast<u32>(static_cast<u16>(V[2][1])) << 16);
        case 5: return static_cast<u32>(static_cast<i32>(V[2][2]));
        case 6: return static_cast<u32>(RGB[0]) | (static_cast<u32>(RGB[1]) << 8) | (static_cast<u32>(RGB[2]) << 16) | (static_cast<u32>(RGB[3]) << 24);
        case 7: return otz;
        case 8:return static_cast<u32>(IR[0]);
        case 9: return static_cast<u32>(IR[1]);
        case 10: return static_cast<u32>(IR[2]);
        case 11: return static_cast<u32>(IR[3]);
        case 12: return static_cast<u32>(static_cast<u16>(xy_fifo[0][0])) | (static_cast<u32>(static_cast<u16>(xy_fifo[0][1])) << 16);
        case 13: return static_cast<u32>(static_cast<u16>(xy_fifo[1][0])) | (static_cast<u32>(static_cast<u16>(xy_fifo[1][1])) << 16);
        case 14: return static_cast<u32>(static_cast<u16>(xy_fifo[2][0])) | (static_cast<u32>(static_cast<u16>(xy_fifo[2][1])) << 16);
        case 15:
            return static_cast<u32>(static_cast<u16>(xy_fifo[3][0])) | (static_cast<u32>(static_cast<u16>(xy_fifo[3][1])) << 16);
        case 16: return z_fifo[0];
        case 17: return z_fifo[1];
        case 18: return z_fifo[2];
        case 19: return z_fifo[3];
        case 20: return static_cast<u32>(rgb_fifo[0][0]) | (static_cast<u32>(rgb_fifo[0][1]) << 8) | (static_cast<u32>(rgb_fifo[0][2]) << 16) | (static_cast<u32>(rgb_fifo[0][3]) << 24);
        case 21: return static_cast<u32>(rgb_fifo[1][0]) | (static_cast<u32>(rgb_fifo[1][1]) << 8) | (static_cast<u32>(rgb_fifo[1][2]) << 16) | (static_cast<u32>(rgb_fifo[1][3]) << 24);
        case 22: return static_cast<u32>(rgb_fifo[2][0]) | (static_cast<u32>(rgb_fifo[2][1]) << 8) | (static_cast<u32>(rgb_fifo[2][2]) << 16) | (static_cast<u32>(rgb_fifo[2][3]) << 24);
        case 23: return reg_23;
        case 24: return static_cast<u32>(MAC[0]);
        case 25: return static_cast<u32>(MAC[1]);
        case 26: return static_cast<u32>(MAC[2]);
        case 27: return static_cast<u32>(MAC[3]);
        case 28:
        case 29:
            return saturate5s(IR[1] >> 7) | (saturate5s(IR[2] >> 7) << 5) | (saturate5s(IR[3] >> 7) << 10);
        case 30: return lzcs;
        case 31: return (u32)lzcr;
        case 32: return static_cast<u32>(static_cast<u16>(matrices[0][0][0])) | (static_cast<u32>(static_cast<u16>(matrices[0][0][1])) << 16);
        case 33: return static_cast<u32>(static_cast<u16>(matrices[0][0][2])) | (static_cast<u32>(static_cast<u16>(matrices[0][1][0])) << 16);
        case 34: return static_cast<u32>(static_cast<u16>(matrices[0][1][1])) | (static_cast<u32>(static_cast<u16>(matrices[0][1][2])) << 16);
        case 35: return static_cast<u32>(static_cast<u16>(matrices[0][2][0])) | (static_cast<u32>(static_cast<u16>(matrices[0][2][1])) << 16);
        case 36: return static_cast<u32>(static_cast<i32>(matrices[0][2][2]));
        case 37:
        case 38:
        case 39:
            return static_cast<u32>(control_vectors[0][reg - 37]);
        case 40: return static_cast<u32>(static_cast<u16>(matrices[1][0][0])) | (static_cast<u32>(static_cast<u16>(matrices[1][0][1])) << 16);
        case 41: return static_cast<u32>(static_cast<u16>(matrices[1][0][2])) | (static_cast<u32>(static_cast<u16>(matrices[1][1][0])) << 16);
        case 42: return static_cast<u32>(static_cast<u16>(matrices[1][1][1])) | (static_cast<u32>(static_cast<u16>(matrices[1][1][2])) << 16);
        case 43: return static_cast<u32>(static_cast<u16>(matrices[1][2][0])) | (static_cast<u32>(static_cast<u16>(matrices[1][2][1])) << 16);
        case 44: return static_cast<u32>(static_cast<i32>(matrices[1][2][2]));
        case 45:
        case 46:
        case 47:
            return static_cast<u32>(control_vectors[1][reg - 45]);
        case 48: return static_cast<u32>(static_cast<u16>(matrices[2][0][0])) | (static_cast<u32>(static_cast<u16>(matrices[2][0][1])) << 16);
        case 49: return static_cast<u32>(static_cast<u16>(matrices[2][0][2])) | (static_cast<u32>(static_cast<u16>(matrices[2][1][0])) << 16);
        case 50: return static_cast<u32>(static_cast<u16>(matrices[2][1][1])) | (static_cast<u32>(static_cast<u16>(matrices[2][1][2])) << 16);
        case 51: return static_cast<u32>(static_cast<u16>(matrices[2][2][0])) | (static_cast<u32>(static_cast<u16>(matrices[2][2][1])) << 16);
        case 52: return static_cast<u32>(static_cast<i32>(matrices[2][2][2]));
        case 53:
        case 54:
        case 55:
            return static_cast<u32>(control_vectors[2][reg - 53]);
        case 56: return static_cast<u32>(OFX);
        case 57: return static_cast<u32>(OFY);
        case 58: return static_cast<u32>(static_cast<i32>(static_cast<i16>(h))); // H reads back as signed even though unsigned
        case 59: return static_cast<u32>(DQA);
        case 60: return static_cast<u32>(DQB);
        case 61: return static_cast<u32>(zsf3);
        case 62: return static_cast<u32>(zsf4);
        case 63: {
            flags &= 0x7FFFF000;
            //30...23, 18..13
            //   7F80FC00
            // 0x7f87e00
            if (flags & 0x7F87E000) flags |= 0x80000000;
            return flags;
        }
        default:
    }
    NOGOHERE;
    return 0;
}
}