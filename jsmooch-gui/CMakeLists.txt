set(CMAKE_C_STANDARD 11)

add_executable(jsmooch-gui
        src/jsmooch-gui.c
        src/jsmooch-gui.h
        ../jsmooch-lib/src/component/cpu/z80/z80_opcodes.c
)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

FIND_PACKAGE(SDL2 REQUIRED)

include_directories( ${SDL2_INCLUDE_DIR} )
include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src) # include files from ProjectB

target_link_libraries(jsmooch-gui jsmooch-lib ${SDL2_LIBRARIES})

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)