#pragma once
#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/cvec.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/sram.h"
#include "gb_ppu.h"
#include "gb_clock.h"
#include "gb_cpu.h"
#include "component/audio/gb_apu/gb_apu.h"
#include "mappers/mapper.h"

namespace GB {
struct inputs {
	u32 a{};
	u32 b{};
	u32 start{};
	u32 select{};
	u32 up{};
	u32 down{};
	u32 left{};
	u32 right{};
};

struct DBGGBROW {
	struct {
		u32 SCX{}, SCY{}, wx{}, wy{}, bg_tile_map_base{}, window_tile_map_base{}, window_enable{}, bg_window_tile_data_base{};
	} io{};
	u16 sprite_pixels[160]{};
};
struct MAPPER;
struct cart;

struct core : jsm_system {
	explicit core(variants variant_in);
	u32 CPU_read(u32 addr, u32 val, bool has_effect);
	void CPU_write(u32 addr, u32 val);
	u32 PPU_read(u32 addr);
	void generic_mapper_reset();

	void write_IO(u32 addr, u32 val);
	u32 read_IO(u32 addr, u32 val);
	void set_BIOS(u8 *BIOSbuf, u32 sz);

	clock clock{};
	GB::CPU cpu;
	PPU::core ppu;
	GB_APU::core apu{};
	variants variant{};
	MAPPER *mapper{};

	bool described_inputs{};

	cart cart;
	inputs controller_in{};
	i32 cycles_left{};

	buf BIOS{};

	struct {
		double master_cycles_per_audio_sample{};
		double next_sample_cycle{};
		audiobuf *buf{};
	} audio{};

	u32 VRAM_bank{};
	u32 WRAM_bank{};

	// Provided by gb_cpu
	/*u32 (*read_IO)(core*, u32, u32){};
	void (*write_IO)(core*, u32, u32){};

	// Provided by gb.c?
	u32 (*DMA_read)(core*, u32){};
	void (*IRQ_vblank_up)(core*){};
	void (*IRQ_vblank_down)(core*){};*/

	struct {
		DBGGBROW rows[144]{};
		DBGGBROW *row{};
	} dbg_data{};

	struct {
		u8 WRAM[8192 * 8]{};
		u8 HRAM[128]{};
		u8 VRAM[16384]{};

		u32 VRAM_bank_offset{};
		u32 WRAM_bank_offset{0x1000};
		u32 BIOS_big{};
	} generic_mapper{};

	DBG_START
	DBG_CPU_REG_START1 *A{}, *X{}, *Y{}, *P{}, *S{}, *PC{} DBG_CPU_REG_END1
	DBG_EVENT_VIEW

	DBG_IMAGE_VIEWS_START
	MDBG_IMAGE_VIEW(nametables)
	MDBG_IMAGE_VIEW(sprites)
	MDBG_IMAGE_VIEW(tiles)
	DBG_IMAGE_VIEWS_END

	DBG_WAVEFORM_START1
		DBG_WAVEFORM_MAIN
		DBG_WAVEFORM_CHANS(4)
	DBG_WAVEFORM_END1

	DBG_END

private:
	void setup_debug_waveform(debug_waveform *dw);
	void serialize_console(serialized_state &state);
	void serialize_cartridge(serialized_state &state);
	void serialize_clock(serialized_state &state);
	void serialize_cpu(serialized_state &state);
	void serialize_apu(serialized_state &state);
	void serialize_ppu(serialized_state &state);
	void deserialize_console(serialized_state &state);
	void deserialize_cartridge(serialized_state &state);
	void deserialize_clock(serialized_state &state);
	void deserialize_cpu(serialized_state &state);
	void deserialize_apu(serialized_state &state);
	void deserialize_ppu(serialized_state &state);
	void setup_audio();
	void sample_audio();


public:
	void serialize_sp_obj_pointer(GB::PPU::sprite *fo, serialized_state &state);
	void deserialize_sp_obj_pointer(GB::PPU::sprite **fo, serialized_state &state);
	u32 DMA_read(u32 addr);
	void IRQ_vblank_up();
	void IRQ_vblank_down();

public:
	void play() final;
	void pause() final;
	void stop() final;
	void get_framevars(framevars& out) final;
	void reset() final;
	void killall();
	u32 finish_frame() final;
	u32 finish_scanline() final;
	u32 step_master(u32 howmany) final;
	void load_BIOS(multi_file_set& mfs) final;
	void enable_tracing();
	void disable_tracing();
	void describe_io() final;
	void save_state(serialized_state &state) final;
	void load_state(serialized_state &state, deserialize_ret &ret) final;
	void set_audiobuf(audiobuf *ab) final;
	void setup_debugger_interface(debugger_interface &intf) final;
	//void sideload(multi_file_set& mfs) final;

};

}
