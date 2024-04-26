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
        src/cpu-test-generators/sh4.c
        src/helper-tests/scheduler-tests.c
        src/helper-tests/scheduler-tests.h
)

include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src ${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)
link_directories(${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)

target_link_libraries(jsmooch-tests jsmooch-lib)# jansson)

install(TARGETS jsmooch-tests RUNTIME DESTINATION bin)
