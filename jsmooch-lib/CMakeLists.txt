set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_C_FLAGS "$CMAKE_C_FLAGS -Wincompatible-pointer-types")

set(SOURCE_LIST
        "src/component/controller/sms/sms_gamepad.cpp"
        "src/component/controller/sms/sms_gamepad.cpp"
        "src/system/sms_gg/sms_gg.cpp"
        "src/system/sms_gg/sms_gg.h"
        "src/system/sms_gg/sms_gg_clock.cpp"
        "src/system/sms_gg/sms_gg_clock.h"
        "src/system/sms_gg/sms_gg_io.cpp"
        "src/system/sms_gg/sms_gg_io.h"
        "src/system/sms_gg/sms_gg_mapper_sega.cpp"
        "src/system/sms_gg/sms_gg_mapper_sega.h"
        "src/system/sms_gg/sms_gg_serialize.cpp"
        "src/system/sms_gg/sms_gg_serialize.h"
        "src/system/sms_gg/sms_gg_vdp.h"
        "src/system/sms_gg/sms_gg_vdp.cpp"
        "src/system/sms_gg/smsgg_debugger.h"
        "src/system/sms_gg/smsgg_debugger.cpp"
        "src/component/controller/nes/nes_joypad.cpp"
        "src/component/controller/nes/nes_joypad.h"
        "src/component/controller/snes/snes_joypad.cpp"
        "src/component/controller/snes/snes_joypad.h"
        "src/component/cpu/m6502/m6502.h"
        "src/component/cpu/m6502/m6502.cpp"
        "src/component/cpu/m6502/m6502_disassembler.h"
        "src/component/cpu/m6502/m6502_disassembler.cpp"
        "src/component/cpu/m6502/nesm6502_opcodes.h"
        "src/component/cpu/m6502/nesm6502_opcodes.cpp"
        "src/component/cpu/m6502/m6502_opcodes.cpp"
        "src/component/cpu/m6502/m6502_opcodes.h"
        "src/component/cpu/spc700/spc700.cpp"
        "src/component/cpu/spc700/spc700.h"
        "src/component/cpu/spc700/spc700_boot_rom.h"
        "src/component/cpu/spc700/spc700_disassembler.cpp"
        "src/component/cpu/spc700/spc700_disassembler.h"
        "src/component/cpu/spc700/spc700_generated_opcodes.cpp"
        "src/component/cpu/m68000/m68000.cpp"
        "src/component/cpu/m68000/m68000.h"
        "src/component/cpu/m68000/m68000_disassembler.cpp"
        "src/component/cpu/m68000/m68000_disassembler.h"
        "src/component/cpu/m68000/m68000_instructions.cpp"
        "src/component/cpu/m68000/m68000_instructions.h"
        "src/component/cpu/m68000/m68000_internal.cpp"
        "src/component/cpu/m68000/m68000_internal.h"
        "src/component/cpu/z80/z80.cpp"
        "src/component/cpu/z80/z80.h"
        "src/component/cpu/z80/z80_disassembler.cpp"
        "src/component/cpu/z80/z80_disassembler.h"
        "src/component/cpu/z80/z80_opcodes.cpp"
        "src/component/cpu/z80/z80_opcodes.h"
        "src/component/cpu/wdc65816/wdc65816.cpp"
        "src/component/cpu/wdc65816/wdc65816.h"
        "src/component/cpu/wdc65816/wdc65816_disassembler.cpp"
        "src/component/cpu/wdc65816/wdc65816_disassembler.h"
        "src/component/cpu/wdc65816/wdc65816_generated_opcodes.cpp"
        "src/system/nes/nes.h"
        "src/system/nes/nes.cpp"
        "src/system/nes/nes_clock.cpp"
        "src/system/nes/nes_clock.h"
        "src/system/nes/nes_ppu.cpp"
        "src/system/nes/nes_ppu.h"
        "src/component/audio/nes_apu/nes_apu.cpp"
        "src/component/audio/nes_apu/nes_apu.h"
        "src/component/audio/sn76489/sn76489.cpp"
        "src/component/audio/sn76489/sn76489.h"
        "src/system/nes/nes_cart.cpp"
        "src/system/nes/nes_cart.h"
        "src/system/nes/nes_cpu.cpp"
        "src/system/nes/nes_cpu.h"
        "src/system/nes/nes_debugger.cpp"
        "src/system/nes/nes_debugger.h"
        "src/system/nes/nes_serialize.cpp"
        "src/system/nes/nes_serialize.h"
        "src/system/nes/mappers/mapper.h"
        "src/system/nes/mappers/mapper.h"
        "src/system/nes/mappers/mapper.h"
        "src/system/nes/mappers/nrom.cpp"
        "src/system/nes/mappers/nrom.h"
        "src/system/nes/mappers/mmc3b_dxrom/mmc3b_dxrom.cpp"
        "src/system/nes/mappers/mmc3b_dxrom/mmc3b_dxrom.h"
        "src/system/nes/mappers/mmc3b_dxrom/a12_watcher.cpp"
        "src/system/nes/mappers/mmc3b_dxrom/a12_watcher.h"
        "src/system/nes/mappers/axrom.cpp"
        "src/system/nes/mappers/axrom.h"
        "src/system/nes/mappers/uxrom.cpp"
        "src/system/nes/mappers/uxrom.h"
        "src/system/nes/mappers/cnrom_gnrom_jf11_jf14_color_dreams.cpp"
        "src/system/nes/mappers/cnrom_gnrom_jf11_jf14_color_dreams.h"
        "src/system/nes/mappers/mmc1_sxrom.cpp"
        "src/system/nes/mappers/mmc1_sxrom.h"
        "src/system/nes/mappers/vrc_2b_2e_4f.cpp"
        "src/system/nes/mappers/vrc_2b_2e_4f.h"
        "src/system/nes/mappers/sunsoft_5_7.cpp"
        "src/system/nes/mappers/sunsoft_5_7.h"
        "src/system/nes/mappers/mmc5.cpp"
        "src/system/nes/mappers/mmc5.h"
        "src/system/nes/mappers/nes_memmap.cpp"
        "src/system/nes/mappers/nes_memmap.h"
        "src/system/snes/gauss_table.h"
        "src/system/snes/r5a22.cpp"
        "src/system/snes/r5a22.h"
        "src/system/snes/snes.cpp"
        "src/system/snes/snes.h"
        "src/system/snes/snes_apu.cpp"
        "src/system/snes/snes_apu.h"
        "src/system/snes/snes_bus.cpp"
        "src/system/snes/snes_bus.h"
        "src/system/snes/snes_cart.cpp"
        "src/system/snes/snes_cart.h"
        "src/system/snes/snes_clock.cpp"
        "src/system/snes/snes_clock.h"
        "src/system/snes/snes_controller_port.cpp"
        "src/system/snes/snes_controller_port.h"
        "src/system/snes/snes_debugger.cpp"
        "src/system/snes/snes_debugger.h"
        "src/system/snes/snes_mem.cpp"
        "src/system/snes/snes_mem.h"
        "src/system/snes/snes_ppu.cpp"
        "src/system/snes/snes_ppu.h"
        "src/system/snes/snesdefs.h"
        "src/system/tg16/tg16.cpp"
        "src/system/tg16/tg16.h"
        "src/system/tg16/tg16_bus.cpp"
        "src/system/tg16/tg16_bus.h"
        "src/system/tg16/tg16_cart.cpp"
        "src/system/tg16/tg16_cart.h"
        "src/system/tg16/tg16_clock.cpp"
        "src/system/tg16/tg16_clock.h"
        "src/system/tg16/tg16_controllerport.cpp"
        "src/system/tg16/tg16_controllerport.h"
        "src/system/tg16/tg16_debugger.cpp"
        "src/system/tg16/tg16_debugger.h"
        "src/component/controller/tg16b2/tg16b2.cpp"
        "src/component/controller/tg16b2/tg16b2.h"
        "src/system/genesis/genesis.cpp"
        "src/system/genesis/genesis.h"
        "src/system/genesis/genesis_bus.cpp"
        "src/system/genesis/genesis_bus.h"
        "src/system/genesis/genesis_cart.cpp"
        "src/system/genesis/genesis_cart.h"
        "src/system/genesis/genesis_clock.cpp"
        "src/system/genesis/genesis_clock.h"
        "src/system/genesis/genesis_controllerport.cpp"
        "src/system/genesis/genesis_controllerport.h"
        "src/system/genesis/genesis_debugger.cpp"
        "src/system/genesis/genesis_debugger.h"
        "src/system/genesis/genesis_serialize.cpp"
        "src/system/genesis/genesis_serialize.h"
        "src/system/genesis/genesis_vdp.cpp"
        "src/system/genesis/genesis_vdp.h"
        "src/component/controller/genesis3/genesis3.cpp"
        "src/component/controller/genesis3/genesis3.h"
        "src/component/controller/genesis6/genesis6.cpp"
        "src/component/controller/genesis6/genesis6.h"
        "src/component/audio/ym2612/ym2612.cpp"
        "src/component/audio/ym2612/ym2612.h"
        "src/helpers/int.h"
        "src/helpers/bitbuffer.cpp"
        "src/helpers/bitbuffer.h"
        "src/helpers/inifile.cpp"
        "src/helpers/inifile.h"
        "src/helpers/irq_multiplexer.cpp"
        "src/helpers/irq_multiplexer.h"
        "src/helpers/debugger/debugger.cpp"
        "src/helpers/debugger/debugger.h"
        "src/helpers/audiobuf.cpp"
        "src/helpers/audiobuf.h"
        "src/helpers/inifile.cpp"
        "src/helpers/inifile.h"
        "src/helpers/jsm_string.cpp"
        "src/helpers/jsm_string.h"
        "src/helpers/buf.cpp"
        "src/helpers/buf.h"
        "src/helpers/physical_io.cpp"
        "src/helpers/physical_io.h"
        "src/helpers/user.cpp"
        "src/helpers/user.h"
        "src/helpers/sram.cpp"
        "src/helpers/sram.h"
        "src/helpers/img.cpp"
        "src/helpers/img.h"
        "src/helpers/scheduler.cpp"
        "src/helpers/scheduler.h"
        "src/helpers/file_exists.cpp"
        "src/helpers/file_exists.h"
        "src/helpers/simplebuf.cpp"
        "src/helpers/simplebuf.h"
        "src/helpers/sys_present.cpp"
        "src/helpers/sys_present.h"
        "src/helpers/debug.cpp"
        "src/helpers/debug.h"
        "src/helpers/sys_interface.cpp"
        "src/helpers/sys_interface.h"
        "src/helpers/serialize/serialize.cpp"
        "src/helpers/serialize/serialize.h"
        "src/helpers/debugger/console.cpp"
        "src/helpers/debugger/console.h"
        "src/helpers/debugger/dbglog.cpp"
        "src/helpers/debugger/dbglog.h"
        "src/helpers/debugger/debugger.cpp"
        "src/helpers/debugger/debugger.h"
        "src/helpers/debugger/debuggerdefs.h"
        "src/helpers/debugger/disassembly.cpp"
        "src/helpers/debugger/disassembly.h"
        "src/helpers/debugger/events.cpp"
        "src/helpers/debugger/events.h"
        "src/helpers/debugger/image.cpp"
        "src/helpers/debugger/image.h"
        "src/helpers/debugger/memory.cpp"
        "src/helpers/debugger/memory.h"
        "src/helpers/debugger/trace.cpp"
        "src/helpers/debugger/trace.h"
        "src/helpers/debugger/waveform.cpp"
        "src/helpers/debugger/waveform.h"
        "src/helpers/cvec.cpp"
        "src/helpers/cvec.h"
        src/system/nes/nes_bus.cpp
        src/system/nes/nes_bus.h
        src/system/snes/snes_serialize.cpp
        src/system/snes/snes_serialize.h
        src/component/cpu/arm7tdmi/arm7tdmi.cpp
        src/component/cpu/arm7tdmi/arm7tdmi.h
        src/component/cpu/arm7tdmi/arm7tdmi_decode.cpp
        src/component/cpu/arm7tdmi/arm7tdmi_decode.h
        src/component/cpu/arm7tdmi/arm7tdmi_instructions.cpp
        src/component/cpu/arm7tdmi/arm7tdmi_instructions.h
        src/component/cpu/arm7tdmi/armv4_disassembler.cpp
        src/component/cpu/arm7tdmi/armv4_disassembler.h
        src/component/cpu/arm7tdmi/thumb_instructions.cpp
        src/component/cpu/arm7tdmi/thumb_instructions.h
        src/component/cpu/arm7tdmi/thumb_disassembler.cpp
        src/component/cpu/arm7tdmi/thumb_disassembler.h
        #src/component/cpu/arm946es/arm946es.cpp
        #src/component/cpu/arm946es/arm946es.h
        #src/component/cpu/arm946es/arm946es_decode.cpp
        #src/component/cpu/arm946es/arm946es_decode.h
        #src/component/cpu/arm946es/arm946es_instructions.cpp
        #src/component/cpu/arm946es/arm946es_instructions.h
        #src/component/cpu/arm946es/armv5_disassembler.cpp
        #src/component/cpu/arm946es/armv5_disassembler.h
        #src/component/cpu/arm946es/nds_cp15.cpp
        #src/component/cpu/arm946es/nds_cp15.h
        #src/component/cpu/arm946es/thumb2_disassembler.cpp
        #src/component/cpu/arm946es/thumb2_disassembler.h
        #src/component/cpu/arm946es/thumb2_instructions.cpp
        #src/component/cpu/arm946es/thumb2_instructions.h
        src/component/cpu/huc6280/huc6280.cpp
        src/component/cpu/huc6280/huc6280.h
        src/component/cpu/huc6280/huc6280_disassembler.cpp
        src/component/cpu/huc6280/huc6280_disassembler.h
        src/component/cpu/huc6280/huc6280_misc.h
        src/component/cpu/huc6280/huc6280_opcodes.cpp
        src/component/cpu/huc6280/huc6280_opcodes.h
        src/component/audio/huc6280_psg/huc6280_psg.cpp
        src/component/audio/huc6280_psg/huc6280_psg.h
        src/component/gpu/huc6260/huc6260.cpp
        src/component/gpu/huc6260/huc6260.h
        src/component/gpu/huc6270/huc6270.cpp
        src/component/gpu/huc6270/huc6270.h
        src/component/cpu/cdp1802/cdp1802.cpp
        src/component/cpu/cdp1802/cdp1802.h
        src/component/cpu/cdp1802/cdp1802_instructions.cpp
        src/component/cpu/cdp1802/cdp1802_instructions.h
        src/component/cpu/cdp1802/cdp1802_disassembler.cpp
        src/component/cpu/cdp1802/cdp1802_disassembler.h
        src/system/mac/mac.cpp
        src/system/mac/mac.h
        src/system/mac/mac_debugger.cpp
        src/system/mac/mac_debugger.h
        src/system/mac/mac_display.cpp
        src/system/mac/mac_display.h
        src/system/mac/mac_bus.cpp
        src/system/mac/mac_bus.h
        src/system/mac/mac_iwm.cpp
        src/system/mac/mac_iwm.h
        src/system/gba/gba.cpp
        src/system/gba/gba.h
        src/system/gba/gba_apu.cpp
        src/system/gba/gba_apu.h
        src/system/gba/gba_bus.cpp
        src/system/gba/gba_bus.h
        src/system/gba/gba_clock.cpp
        src/system/gba/gba_clock.h
        src/system/gba/gba_controller.cpp
        src/system/gba/gba_controller.h
        src/system/gba/gba_debugger.cpp
        src/system/gba/gba_debugger.h
        src/system/gba/gba_dma.cpp
        src/system/gba/gba_dma.h
        src/system/gba/gba_ppu.cpp
        src/system/gba/gba_ppu.h
        src/system/gba/gba_timers.cpp
        src/system/gba/gba_timers.h
        src/system/gba/cart/eeprom.cpp
        src/system/gba/cart/eeprom.h
        src/system/gba/cart/flash.cpp
        src/system/gba/cart/flash.h
        src/system/gba/cart/gba_cart.cpp
        src/system/gba/cart/gba_cart.h
        src/system/gba/cart/sram.cpp
        src/system/gba/cart/sram.h
        src/system/cosmac_vip/cosmac_vip.cpp
        src/system/cosmac_vip/cosmac_vip.h
        src/system/cosmac_vip/vip_bus.cpp
        src/system/cosmac_vip/vip_bus.h
        src/component/gpu/cdp1861/cdp1861.cpp
        src/component/gpu/cdp1861/cdp1861.h
        src/system/cosmac_vip/vip_clock.cpp
        src/system/cosmac_vip/vip_clock.h
        src/system/cosmac_vip/vip_serialize.cpp
        src/system/cosmac_vip/vip_serialize.h
        src/system/cosmac_vip/vip_debugger.cpp
        src/system/cosmac_vip/vip_debugger.h
        src/system/sms_gg/sms_gg_bus.cpp
        src/system/sms_gg/sms_gg_bus.h
        src/system/commodore64/commodore64.cpp
        src/system/commodore64/commodore64.h
        src/system/commodore64/c64_bus.cpp
        src/system/commodore64/c64_bus.h
        src/system/commodore64/vic2.cpp
        src/system/commodore64/vic2.h
        src/system/commodore64/c64_clock.cpp
        src/system/commodore64/c64_clock.h
        src/system/commodore64/c64_debugger.cpp
        src/system/commodore64/c64_debugger.h
        src/system/commodore64/c64_mem.cpp
        src/system/commodore64/c64_mem.h
        src/system/commodore64/c64_vic2_color.cpp
        src/system/commodore64/c64_vic2_color.h
        src/component/audio/m6581/m6581.cpp
        src/component/audio/m6581/m6581.h
        src/system/commodore64/c64_serialize.cpp
        src/system/commodore64/c64_serialize.h
        src/component/misc/m6526/m6526.cpp
        src/component/misc/m6526/m6526.h
        #src/component/misc/via6522/via6522.cpp
        #src/component/misc/via6522/via6522.h
        src/system/commodore64/c64_keyboard.cpp
        src/system/commodore64/c64_keyboard.h
        src/system/gba/gba_serialize.cpp
        src/system/gba/gba_serialize.h
        src/system/mac/mac_clock.cpp
        src/system/mac/mac_clock.h
        src/system/mac/mac_via.cpp
        src/system/mac/mac_via.h
        src/component/floppy/generic_floppy.cpp
        src/component/floppy/generic_floppy.h
        src/component/floppy/mac_floppy.cpp
        src/component/floppy/mac_floppy.h
        src/system/mac/mac_fdd.cpp
        src/system/mac/mac_fdd.h
        src/system/mac/mac_rtc.cpp
        src/system/mac/mac_rtc.h
        src/system/mac/mac_regs.h
        src/system/mac/mac_serialize.cpp
        src/system/mac/mac_serialize.h
)

# TODO: Fix elf-parser repo for windows
if(NOT WIN32)
    list(APPEND SOURCE_LIST
        "src/vendor/elf-parser/elf.h"
        "src/vendor/elf-parser/elf-parser.h"
        "src/vendor/elf-parser/elf-parser.c"
    )
endif()

add_library(jsmooch-lib ${SOURCE_LIST})

# CMake 3.24 adds CMAKE_COMPILE_WARNING_AS_ERROR; it is ignored on lower versions.
set_property(TARGET jsmooch-lib PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Compiler-specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # Linux Clang
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-format")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # GCC
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-format -Wno-unused-result")
    target_link_libraries(jsmooch-lib PRIVATE m) # link against maths library (-lm) for sh4_interpreter_opcodes.c sqrt/sqrtf
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(
        jsmooch-lib
        PRIVATE

        # TODO: Remove these and fix all warnings
        /wd4244 # warning C4244: '=': conversion from 'u64' to 'u8', possible loss of data
        /wd4098 # warning C4098: 'M68k_ins_ADDX': 'void' function returning a value
        /wd4101 # warning C4101: 'unused_variable': unreferenced local variable
        /wd4018 # warning C4018: '<': signed/unsigned mismatch
        /wd4267 # warning C4267: 'initializing': conversion from 'size_t' to 'u32', possible loss of data
    )

    # Allow use of portable fopen (fopen_s is Windows only)
	target_compile_definitions(jsmooch-lib PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
	message(FATAL_ERROR, "Unknown compiler")
endif()

include_directories(src/)

install(TARGETS jsmooch-lib
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Define grouping for source files in IDE project generation. This is just for Visual Studio IDE convenience.
# Without this, all .cpp files are in the "Source Files" filter, and all .h" files are in the "Header files" filter.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST}) # Requires CMake 3.8
