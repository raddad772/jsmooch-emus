set(CMAKE_C_STANDARD 11)

set(SOURCE_LIST
        "src/cpu-test-generators/sh4_test_generator.c"
        "src/cpu-test-generators/sh4_test_generator.h"
        "src/cpu-test-generators/myrandom.h"
        "src/cpu-test-generators/myrandom.c"
        "src/cpu-tests/arm7tdmi_tests.c"
        "src/cpu-tests/arm7tdmi_tests.h"
        "src/cpu-tests/cpu-test-helpers.c"
        "src/cpu-tests/cpu-test-helpers.h"
        "src/cpu-tests/json_tests.c"
        "src/cpu-tests/json_tests.h"
        "src/cpu-tests/huc6280_tests.c"
        "src/cpu-tests/m6502_tests.c"
        "src/cpu-tests/m6502_tests.h"
        "src/cpu-tests/m68000-tests.c"
        "src/cpu-tests/m68000-tests.h"
        "src/cpu-tests/rfb.c"
        "src/cpu-tests/rfb.h"
        "src/cpu-tests/sh4-tests.c"
        "src/cpu-tests/sh4-tests.h"
        "src/cpu-tests/sm83-tests.c"
        "src/cpu-tests/spc700_tests.c"
        "src/cpu-tests/spc700_tests.h"
        "src/cpu-tests/wdc65816_tests.c"
        "src/cpu-tests/wdc65816_tests.h"
        "src/cpu-tests/z80-tests.c"
        "src/cpu-tests/z80-tests.h"
        "src/helper-tests/scheduler-tests.c"
        "src/helper-tests/scheduler-tests.h"
        "src/helper-tests/dasm-range-tests.c"
        "src/helper-tests/dasm-range-tests.h"
        "src/jsmooch-tests.c"
        "src/jsmooch-tests.h"
        "src/z80_drag_race.c"
        "src/z80_drag_race.h"
        ".editorconfig"
)

add_executable(jsmooch-tests ${SOURCE_LIST})

# CMake 3.24 adds CMAKE_COMPILE_WARNING_AS_ERROR; it is ignored on lower versions.
set_property(TARGET jsmooch-tests PROPERTY COMPILE_WARNING_AS_ERROR ON)

include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src ${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)

# Compiler-specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(
        jsmooch-tests
        PRIVATE

        # TODO: Remove these and fix all warnings
        /wd4244 # warning C4244: '=': conversion from 'u64' to 'u8', possible loss of data
        /wd4018 # warning C4018: '<': signed/unsigned mismatch
        /wd4101 # warning C4101: 'pc2': unreferenced local variable
    )

    # Allow use of portable fopen (fopen_s is Windows only)
	target_compile_definitions(jsmooch-tests PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

link_directories(${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)

target_link_libraries(jsmooch-tests jsmooch-lib)# jansson)

install(TARGETS jsmooch-tests RUNTIME DESTINATION bin)

# Define grouping for source files in IDE project generation. This is just for Visual Studio IDE convenience.
# Without this, all .cpp files are in the "Source Files" filter, and all .h files are in the "Header files" filter.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST}) # Requires CMake 3.8
