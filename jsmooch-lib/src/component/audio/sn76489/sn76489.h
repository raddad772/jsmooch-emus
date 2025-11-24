//
// Created by Dave on 2/8/2024.
//

#pragma once

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/int.h"
#include "helpers/serialize/serialize.h"

/*
Big thanks to TotalJustice of TotalSMS, who allowed me to
 use their implementation of the SMS PSG with minor
 modifications
 */

struct SN76489 {
    void reset();
    void cycle();
    i16 sample_channel(int i);
    i16 mix_sample(bool for_debug);
    void write_data(u8 val);
    void serialize(serialized_state &state);
    void deserialize(serialized_state &state);

private:
    void cycle_squares();
    void cycle_noise();

public:
    u32 vol[4]{};
    i16 polarity[4]{};
    struct SN76489_noise {
        u32 lfsr{};
        u32 shift_rate{};
        u32 mode{};
        i32 counter{};
        u32 countdown{};
        bool ext_enable{true};
    } noise{};

    struct SN76489_SW {
        i32 counter{};
        i32 freq{};
        bool ext_enable{true};
    } sw[3]{};

    bool ext_enable{true};
    u32 io_reg{}; // current register selected
    u32 io_kind{}; // 0 = tone, 1 = volume

    DBG_EVENT_VIEW_ONLY;
};

