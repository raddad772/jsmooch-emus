//
// Created by . on 9/8/24.
//

#ifndef JSMOOCH_EMUS_NES_APU_H
#define JSMOOCH_EMUS_NES_APU_H

#include "helpers/int.h"

struct NES_APU {
    u32 ext_enable;
    
    struct NESSNDCHAN {
        u32 ext_enable;
        u32 number;
        i32 output;

        u32 vol;

        struct {
            i32 reload;
            i32 enabled;
            i32 count;
            u32 reload_flag;
        } linear_counter;

        struct {
            u32 counter;
            u32 kind;
            u32 current;
        } duty;

        u32 env_loop_or_length_counter_halt, constant_volume, volume_or_envelope;
        u32 mode;
        struct {
            i32 reload, count;
        } period;

        struct {
            i32 period_count;
            u32 decay_level;
            u32 start_flag;
        } envelope;

        struct {
            u32 enabled, shift, overflow, reload;
            i32 negate;
            struct {
                i32 counter, reload;
            } period;
        } sweep;

        struct {
            i32 count;
            u32 overflow;
            u32 enabled;
            u32 count_write_value;
        } length_counter;

        i32 first_clock;
    } channels[4];


    u64 *master_cycles;

    struct {
        i32 bytes_remaining;
        u32 ext_enable;
        u32 enabled;

        u32 IF, new_IF;
    } dmc;

    struct {
        u32 step5_mode;
    } io;

    struct {
        u32 step;
        u32 step_mod;
        u32 mode;
        u32 IF; // interrupt flag
        u32 new_IF;
        u32 interrupt_inhibit;
    } frame_counter;

    struct {
        u32 counter_1;
        i32 counter_240hz;
        i32 every_other;
    } clocks;

    struct {
        void (*func)(void *, u32);
        void *ptr;
        u32 just_set;
    } IRQ_pin;
};

void NES_APU_init(struct NES_APU* this);
void NES_APU_write_IO(struct NES_APU* this, u32 addr, u8 val);
u8 NES_APU_read_IO(struct NES_APU* this, u32 addr, u8 old_val, u32 has_effect);
void NES_APU_cycle(struct NES_APU* this);
float NES_APU_mix_sample(struct NES_APU* this, u32 is_debug);
float NES_APU_sample_channel(struct NES_APU* this, int cnum);
void NES_APU_reset(struct NES_APU* this);

#endif //JSMOOCH_EMUS_NES_APU_H
