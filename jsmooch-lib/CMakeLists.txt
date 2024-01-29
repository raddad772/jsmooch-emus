set(CMAKE_C_STANDARD 11)

add_library(jsmooch-lib
        src/component/cpu/sm83/sm83.c
        src/component/cpu/sm83/sm83.h
        src/component/cpu/sm83/sm83_misc.h
        src/component/cpu/sm83/sm83_opcodes.c
        src/component/cpu/sm83/sm83_opcodes.h
        src/helpers/enums.h
        src/helpers/int.h
        src/helpers/sys_interface.c
        src/helpers/sys_interface.h
        src/system/gb/mappers/mapper.c
        src/system/gb/mappers/mapper.h
        src/system/gb/mappers/no_mapper.c
        src/system/gb/mappers/no_mapper.h
        src/system/gb/cart.c
        src/system/gb/cart.h
        src/system/gb/gb.c
        src/system/gb/gb.h
        src/system/gb/gb_bus.c
        src/system/gb/gb_bus.h
        src/system/gb/gb_clock.c
        src/system/gb/gb_clock.h
        src/system/gb/gb_cpu.c
        src/system/gb/gb_cpu.h
        src/system/gb/gb_enums.h
        src/system/gb/gb_ppu.c
        src/system/gb/gb_ppu.h
        src/helpers/sys_present.c
        src/helpers/sys_present.h
        src/helpers/debug.h
        src/helpers/debug.c
        src/system/gb/mappers/mbc1.h
        src/system/gb/mappers/mbc1.c
)

include_directories(src/)

install(TARGETS jsmooch-lib
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)