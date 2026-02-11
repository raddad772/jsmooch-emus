//
// Created by . on 2/15/25.
//

#pragma once
#include "helpers/int.h"

namespace PS1 {

enum DMA_direction {
    D_to_ram,
    D_from_ram,
};

enum DMA_step {
    D_increment = 0,
    D_decrement = 1
};

enum DMA_sync {
    D_manual,
    D_request,
    D_linked_list
};

struct core;
struct DMA_channel {
    void do_linked_list();
    void do_block();
    void do_dma();
    bool active();
    u32 get_control();
    void set_control(u32 val);
    [[nodiscard]] u32 transfer_size() const;
    core *bus{};
    u32 num{}, enable{}, trigger{}, chop{};
    u32 chop_dma_size{}, chop_cpu_size{};
    u32 unknown{};
    u32 base_addr{};
    u32 block_size{}, block_count{};
    DMA_step step{D_increment};
    DMA_sync sync{D_manual};
    DMA_direction direction{D_to_ram};
};

struct DMA {
    u32 irq_status();
    u32 read(u32 addr, u32 sz);
    void write(u32 addr, u32 sz, u32 val);
    DMA_channel channels[7]{};
    explicit DMA(core *parent) : bus(parent) {
        for (u32 i = 0; i < 7; i++) {
            auto &c = channels[i];
            c.bus = parent;
            c.num = i;
        }
    }
    core *bus;
    u32 control{};
    u32 irq_enable{}, irq_enable_ch{};
    u32 irq_flags_ch{};
    u32 irq_force{};
    u32 unknown1{};
};


}
