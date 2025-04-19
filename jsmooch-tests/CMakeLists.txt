set(CMAKE_C_STANDARD 11)

add_executable(jsmooch-tests
        src/jsmooch-tests.c
        src/jsmooch-tests.h
        src/cpu-tests/sm83-tests.c
        src/cpu-tests/json_tests.c
        src/cpu-tests/json_tests.h
        src/cpu-tests/z80-tests.c
        src/cpu-tests/z80-tests.h
        src/cpu-tests/rfb.c
        src/cpu-tests/rfb.h
        src/cpu-test-generators/sh4_test_generator.c
        src/cpu-test-generators/sh4_test_generator.h
        src/helper-tests/scheduler-tests.c
        src/helper-tests/scheduler-tests.h
        src/cpu-test-generators/myrandom.h
        src/cpu-test-generators/myrandom.c
        src/cpu-tests/sh4-tests.c
        src/cpu-tests/sh4-tests.h
        src/cpu-tests/m68000-tests.c
        src/cpu-tests/m68000-tests.h
        src/helper-tests/dasm-range-tests.c
        src/helper-tests/dasm-range-tests.h
        src/cpu-tests/m6502_tests.c
        src/cpu-tests/m6502_tests.h
        src/cpu-tests/arm7tdmi_tests.c
        src/cpu-tests/arm7tdmi_tests.h
        src/z80_drag_race.c
        src/z80_drag_race.h
        src/cpu-tests/wdc65816_tests.c
        src/cpu-tests/wdc65816_tests.h
        src/cpu-tests/spc700_tests.c
        src/cpu-tests/spc700_tests.h
        src/cpu-tests/spc700_tests.c
)

include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src ${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)
link_directories(${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)

target_link_libraries(jsmooch-tests jsmooch-lib)# jansson)

install(TARGETS jsmooch-tests RUNTIME DESTINATION bin)
