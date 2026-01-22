#pragma once

#include "gb_enums.h"
#include "helpers/physical_io.h"
#include "helpers/sram.h"
#include "gb_clock.h"
#include "mappers/mapper.h"

namespace GB {

struct MAPPER;
struct core;

struct cart {
	explicit cart(variants variant_in, core* parent);
	~cart();
	void read_ROM(u8 *inp, u64 size);
	void load_ROM_from_RAM(void* ibuf, u64 size, physical_io_device &pio);
	void setup_mapper();

	variants variant{};
	core* bus{};

	u8* ROM{};
	u64 ROM_size{};
    persistent_store *SRAM{};

	MAPPER* mapper{};
	struct {
		u32 ROM_banks{};
		u32 ROM_size{};
		u32 RAM_size{};
		u32 RAM_banks{};
		u32 RAM_mask{};
		mappers mapper{}; // : GB_MAPPERS = GB_MAPPERS.none{};
		bool ram_present{};
		bool battery_present{};
		bool timer_present{};
		bool rumble_present{};
		bool sensor_present{};
		u32 sgb_functions{};
		bool gb_compatible{};
		bool cgb_compatible{};
	} header{};
};


}