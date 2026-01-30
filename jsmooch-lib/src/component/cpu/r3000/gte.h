//
// Created by . on 2/11/25.
//

#pragma once

#include "helpers/int.h"

namespace R3000::GTE {

enum MATRIX {
    MTX_Rotation = 0,
    MTX_Light = 1,
    MTX_Color = 2,
    MTX_Invalid = 3,
};

enum control_vector {
    CV_Translation = 0,
    CV_BackgroundColor = 1,
    CV_FarColor = 2,
    CV_Zero = 3
};

struct CMD {
    void from_command(u32 cmd);
    u8 shift{};
    u32 clamp_negative{};
    MATRIX matrix{};
    u8 vector_mul{};
    control_vector vector_add{};
};


struct core {
    core();
    void multiply_matrix_by_vector(CMD *config, MATRIX mat, u8 vei, control_vector crv);
    void set_flag(int num) { flags |= 1 << num; }
    i16 i32_to_i16_saturate(const CMD *config, const u8 flag, const i32 val) {
        const i32 min = config->clamp_negative ? 0 : -32768;
        const i32 max = 32767;

        if (val > max) {
            set_flag(24 - flag);
            return 32767;
        }
        else if (val < min) {
            set_flag(24 - flag);
            return static_cast<i16>(min);
        }
        return static_cast<i16>(val);

    }

    i64 i64_to_i44(const u8 flag, const i64 val)
    {
        if (val >= (1LL << 43)) {
            set_flag(30 - flag);
        }
        else if (val < -(1LL << 43)) {
            set_flag(27 - flag);
        }

        // sign-extend result
        return (val << 20) >> 20;
    }

    i16 i32_to_i11_saturate(const u8 flag, const i32 val)
    {
        if (val < -0x400) {
            set_flag(14 - flag);
            return -0x400;
        }
        else if (val > 0x3FF) {
            set_flag(14 - flag);
            return 0x3FF;
        }
        return static_cast<i16>(val);
    }

    void check_mac_overflow(i64 val)
    {
        if (val < -0x80000000L) {
            set_flag(15);
        } else if (val > 0x7fffffffL) {
            set_flag(16);
        }
    }

    void mac_to_ir(CMD *config)
    {
        IR[1] = i32_to_i16_saturate(config, 0, MAC[1]);
        IR[2] = i32_to_i16_saturate(config, 1, MAC[2]);
        IR[3] = i32_to_i16_saturate(config, 2, MAC[3]);
    }

    u8 mac_to_color(i32 mac, u8 which)
    {
        i32 c = mac >> 4;
        if (c < 0) {
            set_flag(21 - which);
            return 0;
        }
        else if (c > 0xFF) {
            set_flag(21 - which);
            return 0xFF;
        }
        return static_cast<u8>(c);
    }

    void mac_to_rgb_fifo()
    {
        const i32 mac1 = MAC[1];
        const i32 mac2 = MAC[2];
        const i32 mac3 = MAC[3];

        const u8 r = mac_to_color(mac1, 0);
        const u8 g = mac_to_color(mac2, 1);
        const u8 b = mac_to_color(mac3, 2);
        rgb_fifo[0][0] = rgb_fifo[1][0];
        rgb_fifo[0][1] = rgb_fifo[1][1];
        rgb_fifo[0][2] = rgb_fifo[1][2];
        rgb_fifo[0][3] = rgb_fifo[1][3];

        rgb_fifo[1][0] = rgb_fifo[2][0];
        rgb_fifo[1][1] = rgb_fifo[2][1];
        rgb_fifo[1][2] = rgb_fifo[2][2];
        rgb_fifo[1][3] = rgb_fifo[2][3];

        rgb_fifo[2][0] = r;
        rgb_fifo[2][1] = g;
        rgb_fifo[2][2] = b;
        rgb_fifo[2][3] = RGB[3];
    }

    u16 i64_to_otz(i64 average) {
        const i64 value = average >> 12;

        if (value < 0) {
            set_flag(18);
            return 0;
        } else if (value > 0xFFFF) {
            set_flag(18);
            return 0xFFFF;
        }
        return static_cast<u16>(value);
    }

    void command(u32 opcode, u64 current_clock);
    void write_reg(u32 reg, u32 val);
    u32 read_reg(u32 reg);
    u32 do_RTP(CMD *config, u32 vector_index);
    void do_ncc(CMD *config, u8 vector_index);
    void do_ncd(CMD *config, u8 vector_index);
    void do_dpc(CMD *config);
    void do_nc(CMD *config, u8 vector_index);
    void depth_queueing(u32 pf);

    void cmd_RTPS(CMD *config);
    void cmd_NCLIP();
    void cmd_OP(CMD *config);
    void cmd_DPCS(CMD *config);
    void cmd_INTPL(CMD *config);
    void cmd_DCPL(CMD *config);
    void cmd_MVMVA(CMD *config);
    void cmd_NCDS(CMD *config);
    void cmd_CDP(CMD *config);
    void cmd_NCDT(CMD *config);
    void cmd_NCCS(CMD *config);
    void cmd_CC(CMD *config);
    void cmd_NCS(CMD *config);
    void cmd_NCT(CMD *config);
    void cmd_SQR(CMD *config);
    void cmd_DPCT(CMD *config);
    void cmd_AVSZ3();
    void cmd_AVSZ4();
    void cmd_RTPT(CMD *config);
    void cmd_GPF(CMD *config);
    void cmd_GPL(CMD *config);
    void cmd_NCCT(CMD *config);

    CMD cfg{}, config0{}, config1{};
    u32 op_going{};
    u32 op_kind{}; // opcode
    u64 clock_start{};
    u64 clock_end{};
    i32 OFX{}, OFY{};
    u16 h{};
    i16 DQA{};
    i32 DQB{};

    i16 zsf3{}, zsf4{};
    u32 cnt{};
    u32 flags{};
    i32 MAC[4]{};
    u16 otz{};
    u8 RGB[4]{};
    i16 IR[4]{};
    u16 z_fifo[4]{};
    u32 lzcs{};
    u8 lzcr{};
    u32 reg_23{};

    i16 matrices[4][3][3]{};
    i32 control_vectors[4][3]{};
    i16 V[4][3]{};

    i16 xy_fifo[4][2]{};
    u8 rgb_fifo[3][4]{};

    i64 cycle_count{};
};


}