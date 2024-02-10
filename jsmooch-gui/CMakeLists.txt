set(CMAKE_C_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
set(SDL2_PATH "C:\\dev\\SDL2\\SDL2-2.28.5\\x86_64-w64-mingw32")

add_executable(jsmooch-gui
        src/jsmooch-gui.c
        src/jsmooch-gui.h
        ../jsmooch-lib/src/component/cpu/z80/z80_opcodes.c
)

include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src) # include files from ProjectB

find_package(SDL2 REQUIRED)
include_directories( ${SDL2_INCLUDE_DIR} )
include_directories(C:/dev/SDL2/SDL2-2.28.5/x86_64-w64-mingw32/include)

target_link_libraries(jsmooch-gui jsmooch-lib ${SDL2_LIBRARY})

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)