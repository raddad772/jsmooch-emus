#ifndef _JSMOOCH_GB_CART_H
#define _JSMOOCH_GB_CART_H

#include "gb_enums.h"
#include "helpers/physical_io.h"
#include "helpers/sram.h"

struct GB_cart {
	enum GB_variants variant;
	struct GB_clock* clock;
	struct GB_bus* bus;

	u8* ROM;
	u64 ROM_size;
    struct persistent_store *SRAM;

	struct GB_mapper* mapper;
	struct {
		u32 ROM_banks;
		u32 ROM_size;
		u32 RAM_size;
		u32 RAM_banks;
		u32 RAM_mask;
		enum GB_mappers mapper; // : GB_MAPPERS = GB_MAPPERS.none;
		u32 ram_present;
		u32 battery_present;
		u32 timer_present;
		u32 rumble_present;
		u32 sensor_present;
		u32 sgb_functions;
		u32 gb_compatible;
		u32 cgb_compatible;
	} header;
};



void GB_cart_init(GB_cart* cart, enum GB_variants variant, GB_clock* clock, GB_bus* bus);
void GB_cart_delete(GB_cart*);
void GB_cart_load_ROM_from_RAM(GB_cart*, void* ibuf, u64 size, physical_io_device *pio);


#endif