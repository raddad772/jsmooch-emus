//
// Created by . on 7/6/25.
//

#pragma once
#include "helpers/int.h"
#include "helpers/buf.h"
#include "helpers/physical_io.h"
namespace TG16 {

struct cart {
    static void reset();
    void write(u32 addr, u8 val);
    [[nodiscard]] u8 read(u32 addr, u8 old) const;
    [[nodiscard]] u8 read_SRAM(u32 addr) const;
    void write_SRAM(u32 addr, u8 val);
    void load_ROM_from_RAM(void *ptr, u64 sz, physical_io_device &pio);

    buf ROM{};
    persistent_store *SRAM{};
    u8 *ptrs[8]{};
};

}