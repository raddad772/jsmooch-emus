set(CMAKE_C_STANDARD 11)
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
        "src/system/nes/mappers/mapper.cpp"
        "src/system/nes/mappers/mapper.h"
        "src/system/nes/mappers/mapper.h"
        "src/system/nes/mappers/mapper.cpp"
        "src/system/nes/mappers/mapper.h"
        "src/system/nes/mappers/nrom.cpp"
        "src/system/nes/mappers/nrom.h"
        "src/system/nes/mappers/mmc3b_dxrom/mmc3b_dxrom.c"
        "src/system/nes/mappers/mmc3b_dxrom/mmc3b_dxrom.h"
        "src/system/nes/mappers/mmc3b_dxrom/a12_watcher.c"
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
        "src/helpers/irq_multiplexer.cpp"
        "src/helpers/irq_multiplexer.h"
        src/helpers/cvec.cpp
        src/helpers/cvec.h
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
