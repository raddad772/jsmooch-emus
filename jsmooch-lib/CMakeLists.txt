set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_C_FLAGS "$CMAKE_C_FLAGS -Wincompatible-pointer-types")

set(SOURCE_LIST
        "src/component/audio/nes_apu/nes_apu.cpp"
        "src/component/audio/nes_apu/nes_apu.h"
        "src/component/controller/nes/nes_joypad.cpp"
        "src/component/controller/nes/nes_joypad.h"
        "src/component/cpu/m6502/m6502.cpp"
        "src/component/cpu/m6502/m6502_disassembler.cpp"
        "src/component/cpu/m6502/nesm6502_opcodes.h"
        "src/component/cpu/m6502/nesm6502_opcodes.cpp"
        "src/component/cpu/m6502/m6502_opcodes.cpp"
        "src/component/cpu/m6502/m6502_opcodes.h"
        "src/system/nes/nes.h"
        "src/system/nes/nes.cpp"
        "src/system/nes/nes_clock.cpp"
        "src/system/nes/nes_clock.h"
        "src/system/nes/nes_ppu.cpp"
        "src/system/nes/nes_ppu.h"
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
        "src/helpers/int.h"
        "src/helpers/inifile.cpp"
        "src/helpers/inifile.h"
        "src/helpers/irq_multiplexer.cpp"
        "src/helpers/irq_multiplexer.h"
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
