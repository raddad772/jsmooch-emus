//
// Created by . on 7/19/25.
//

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/int.h"

namespace HUC6280::PSG {
struct core;
struct CH {
    explicit CH(core *parent, u32 num_in) : psg(parent), num(num_in) {}
    u16 debug_sample() const;
    core *psg;
    void clock_noise();
    bool tick();
    UN16 FREQ{};
    u32 num{};
    u8 ON{}, DDA{}, AL{}; // AL = amplitude level
    i16 output{};
    u16 output_l{}, output_r{};
    u8 LAL{}, RAL{}; // left/right amplitude
    u8 WAVEDATA[32]{};
    u32 wavectr{};
    struct {
        i32 E{}, FREQ{};
        i32 COUNTER{};
        u32 state{1};
        i32 output{};
    } NOISE{};
    i32 counter{};
    bool ext_enable{true};
};

struct core {
    core();
    void reset() {}
    void mix_sample();
    void cycle();
    void update_ch_output(CH &ch);
    void update_DDA(CH &ch);
    void write(u16 addr, u8 val);

    u8 LMAL{}, RMAL{}; // left/right main amplitude
    u8 SEL{};
    bool updates{};
    bool ext_enable{true};

    CH channels[6];

    struct {
        u8 FREQ{}, TRG{}, CTL{};
    } LFO{};

    struct {
        u16 l{}, r{};
    } out{};

};
}
