//
// Created by . on 2/20/25.
//

#pragma once
#include "helpers/int.h"

namespace PS1 {

struct SPU_FIFO {
    u16 items[32]{};
    u32 len{}, head{}, tail{};

    void push(u16 item);
    u16 pop();
    void clear();

    u32 still_sch{};
    u64 sch_id{};
};
struct core;
struct SPU_VOICE {
    void reset(core *ps1, u32 num);
    u32 num{};
    core *bus{};

    u16 read_reg(u32 regnum);
    void write_reg(u32 regnum, u16 val);

    struct {
        bool PMON;
        u16 sample_rate{};
        u32 adpcm_start_addr{};
        u32 adpcm_repeat_addr{};
    } io{};
    u32 pitch_counter{};
    void cycle();
};
struct SPU {
    explicit SPU(core *parent) : bus(parent) {}
    void mainbus_write(u32 addr, u8 sz, u32 val);
    u32 mainbus_read(u32 addr, u8 sz, bool has_effect);
    u16 RAM[0x40000]{};
    SPU_FIFO FIFO{};
    void reset();
    core *bus;

    void update_IRQs();
    void schedule_FIFO_transfer(u64 clock);

    u16 read_RAM(u32 addr, bool has_effect, bool triggers_irq);
    void write_RAM(u32 addr, u16 val, bool triggers_irq);
    void FIFO_transfer(u64 clock);
    void check_irq_addr(u32 addr);
    u32 DMA_read();
    void DMA_write(u32 val);

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
        u32 RAM_transfer_addr;
        u32 IRQ_level;
        u32 IRQ_addr;
    } io{};

    struct {
        u32 RAM_transfer_addr;
    } latch{};

    SPU_VOICE voices[24]{};

private:
    void write_control_regs(u32 addr, u8 sz, u32 val);
    u32 read_control_regs(u32 addr, u8 sz);
    void write_SPUCNT(u16 val);
};
}