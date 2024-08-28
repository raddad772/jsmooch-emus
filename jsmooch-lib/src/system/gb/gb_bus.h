#ifndef _GB_BUS_H
#define _GB_BUS_H

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/debugger/debuggerdefs.h"

#ifndef NULL
#define NULL 0
#endif

struct GB;
struct GB_bus {
	struct GB_cart* cart;
	struct GB_mapper* mapper;
	struct GB_PPU* ppu;
	struct GB_CPU* cpu;
    struct GB_clock* clock;
    struct GB* gb;

    struct {
        u8 WRAM[8192 * 8];
        u8 HRAM[128];
        u8 VRAM[16384];

        u32 VRAM_bank_offset;
        u32 WRAM_bank_offset;
        u32 BIOS_big;
    } generic_mapper;

    u32 VRAM_bank;
    u32 WRAM_bank;

	// Provided by mapper
	//u32 (*CPU_read)(struct GB_bus*, u32, u32);
	//void (*CPU_write)(struct GB_bus*, u32, u32);
	//u32 (*PPU_read)(struct GB_bus*, u32);

	// Provided by gb_cpu
	u32 (*read_IO)(struct GB_bus*, u32, u32);
	void (*write_IO)(struct GB_bus*, u32, u32);

	// Provided by gb_ppu
	u32 (*read_OAM)(struct GB_bus*, u32);
	void (*write_OAM)(struct GB_bus*, u32, u32);

	// Provided by gb.c?
	u32 (*DMA_read)(struct GB_bus*, u32);
	void (*IRQ_vblank_up)(struct GB_bus*);
	void (*IRQ_vblank_down)(struct GB_bus*);

    DBG_EVENT_VIEW_ONLY;

    // Pointer to BIOS, owned by GB
	u8* BIOS;
};

void GB_bus_init(struct GB_bus*, struct GB_clock *clock);
void GB_bus_delete(struct GB_bus *);
void GB_bus_reset(struct GB_bus *);
u32 GB_bus_PPU_read(struct GB_bus*, u32 addr);
u32 GB_bus_CPU_read(struct GB_bus*, u32 addr, u32 val, u32 has_effect);
void GB_bus_CPU_write(struct GB_bus*, u32 addr, u32 val);
void GB_bus_set_cart(struct GB_bus*, struct GB_cart* cart);

#endif