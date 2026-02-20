//
// Created by . on 2/20/25.
//

#pragma once
#include "helpers/int.h"

namespace PS1 {
    struct core;
}
namespace PS1::SPU {
struct FIFO {
    u16 items[32]{};
    u32 len{}, head{}, tail{};

    void push(u16 item);
    u16 pop();
    void clear();

    u32 still_sch{};
    u64 sch_id{};
};
struct core;

struct FIR_filter {
    void add_sample(i32 smp);
    i32 run();
    u32 pos{};
    i32 samples[39]{};
};

enum E_MODE {
    EM_LINEAR=0,
    EM_EXPONENTIAL=1
};

enum E_DIR {
    ED_INCREASE=0,
    ED_DECREASE=1
};

enum V_PHASE {
    VP_POSITIVE=0,
    VP_NEGATIVE=1
};

enum E_PHASE {
    EP_ATTACK = 0,
    EP_DECAY = 1,
    EP_SUSTAIN = 2,
    EP_RELEASE = 3
};

enum VV_MODE {
    VVM_FIXED=0,
    VVM_SWEEP=1
};

struct ADSR_GENERATOR {
    i32 exponential{};
    i32 decreasing{};
    i32 shift{};
    i32 step{};
    i32 negative{}; // 1=negative
    i32 counter{};
    i32 output{};
    void calc();
    void cycle();

    i32 adsr_step{};
    i32 counter_reload{};
};

struct VOICE_VOL {
    u16 io_val{};
    VV_MODE mode{};
    void cycle();
    void write(u16 v);
    u16 read();
    i32 phase{};

    ADSR_GENERATOR sweep{};

    //i32 adsr_step{};
};

struct ADSR_ENVELOPE {
    void cycle();

    E_PHASE phase{EP_RELEASE};

    ADSR_GENERATOR adsr{};
    i32 sustain_level{};
    u32 num{};

    i32 attack_exponential{}, attack_shift{}, attack_step{};
    i32 decay_shift{};
    i32 sustain_exponential{}, sustain_decreasing{}, sustain_shift{}, sustain_step{};
    i32 release_exponential{}, release_shift{};

    void load_attack();
    void load_decay();
    void load_sustain();
    void load_release();
};

struct ADPCM {
    u32 start_addr{};
    u32 repeat_addr{};
    u32 cur_addr{};

    i16 samples[28];
};


struct VOICE {
    void reset(PS1::core *ps1, u32 num);
    void keyon();
    void keyoff();
    bool ext_enable{true};
    u32 num{};
    bool key_is_on{};
    PS1::core *bus{};

    u16 read_reg(u32 regnum);
    void write_reg(u32 regnum, u16 val);

    struct {
        bool PMON{};
        i16 sample_rate{};
        VOICE_VOL vol_l{}, vol_r{};
        u32 reached_loop_end{};
        u32 reverb_on{};
        u16 env_lo{}, env_hi{};
        bool noise_enable{};
    } io{};
    i16 sample{}, sample_l{}, sample_r{};

    ADSR_ENVELOPE env{};
    u32 pitch_counter{}; // 17 bits?
    void cycle(i16 noise_level);

    ADPCM adpcm{};
    void adpcm_start();
    void adpcm_decode();
    void adpcm_get_sample();
    void gaussian_me_up();
    struct {
        i32 old_sample_index{-1};
        i16 samples[4]{};
        u32 idx{3};
    } gauss{};
private:
    void write_env_lo(u16 val);;
    void write_env_hi(u16 val);
};

struct core {
    explicit core(PS1::core *parent) : bus(parent) {}
    void mainbus_write(u32 addr, u8 sz, u32 val);
    u32 mainbus_read(u32 addr, u8 sz, bool has_effect);
    u32 snooped_mainbus_read(u32 addr, u8 sz, bool has_effect);
    u16 RAM[0x40000]{};
    FIFO FIFO{};
    void reset();
    PS1::core *bus;

    u64 local_clock{};

    void update_IRQs();
    void schedule_FIFO_transfer(u64 clock);

    u16 read_RAM(u32 addr, bool has_effect, bool triggers_irq);
    u8 read_RAM8(u32 addr, bool has_effect, bool triggers_irq);

    void write_RAM(u32 addr, u16 val, bool triggers_irq);
    void FIFO_transfer(u64 clock);
    void check_irq_addr(u32 addr);
    u32 DMA_read();
    void DMA_write(u32 val);

    bool ext_enable{true};

    struct {
        union {
            struct {
                u16 _unk1 : 1;
                u16 mode : 3;
                u16 _unk2 : 12;
            };
            u16 u{};
        } RAMCNT{};

        union {
            struct {
                u16 cd_audio_enable : 1;
                u16 external_audio_enable : 1;
                u16 cd_audio_reverb : 1;
                u16 external_audio_reverb : 1;
                u16 sound_ram_transfer_mode : 2;
                u16 irq9_enable : 1;
                u16 reverb_master_enable : 1;
                u16 noise_frequency_step : 2;
                u16 noise_frequency_shift : 4;
                u16 master_mute : 1;
                u16 master_enable : 1;
            };
            u16 u{};
        } SPUCNT;

        union {
            struct {
                u16 spu_mode : 6;
                u16 irq9 : 1; //
                u16 data_transfer_dma_rw_req : 1;
                u16 data_transfer_dma_write_req : 1;
                u16 data_transfer_dma_read_req : 1;
                u16 data_transfer_busy : 1; // 0=ready, 1=busy
                u16 capture_buffer_half : 1; // 0=first, 1=second
                u16 _res : 4;
            };
            u16 u{};
        } SPUSTAT{};
        u32 RAM_transfer_addr{};
        u32 IRQ_level{};
        u32 IRQ_addr{};
        u16 vol_L{}, vol_R{}, vol_CD_L{}, vol_CD_R{}, vol_ext_L{}, vol_ext_R{};
        i16 cur_vol_L{}, cur_vol_R{};
        u16 keyon_lo{}, keyon_hi{};
        u16 keyoff_lo{}, keyoff_hi{};
        u16 non_lo{}, non_hi{};
        u16 pmon_lo{}, pmon_hi{};
        struct {
            i32 vol_out_l{}, vol_out_r{};
            u32 mBASE{};
            u16 regs[0x20]{};
            u16 on_lo{}, on_hi{};
        } reverb{};

    } io{};
    struct {
        i32 timer{};
        i32 step{};
        i32 shift{};
        i16 level{};
    } noise{};
    void cycle();
    void do_capture();
    void do_noise();
    void process_reverb();
    struct {
        u16 index{};
        struct {
            i16 cd_l{}, cd_r{};
        } sample;
        bool cd_ext_enable{};
    } capture{};

    struct {
        bool ext_enable_in{}, ext_enable_out{};
        i32 in_l{}, in_r{};

        i32 sample_l{}, sample_r{};
        u32 counter{0};

        struct REVERBPROC {
            FIR_filter in{}, out{};
        } filter_l{}, filter_r{};
        u32 buf_addr{};

        i32 proc_l{}, proc_r{};
    } reverb{};

    struct {
        u32 RAM_transfer_addr;
    } latch{};

    VOICE voices[24]{};
    i16 sample_l{}, sample_r{};
    void commit_FIFO();
    void FIFO_instant_transfer(u16 val);
private:
    void write_control_regs(u32 addr, u8 sz, u32 val);
    u32 read_control_regs(u32 addr, u8 sz);
    void write_SPUCNT(u16 val);
    void write_reverb_reg(u32 addr, u8 sz, u32 val);
    u32 read_reverb_reg(u32 addr, u8 sz);
    void apply_reflection(i32 l, i32 r);
    u32 reverb_addr(u32 inaddr) {
        i32 a = inaddr + io.reverb.mBASE + reverb.buf_addr;
        return a < 0 ? 0x7FFFE : a > 0x7FFFE ? io.reverb.mBASE >> 1 : a >> 1;
    }
    void apply_comb_filter();
    void apply_all_pass_filter_1();
    void apply_all_pass_filter_2();

    void do_reflect(i32 smp, u32 d, u32 m);
};
}