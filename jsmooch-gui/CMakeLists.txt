set(CMAKE_C_STANDARD 11)

add_executable(jsmooch-gui
        src/jsmooch-gui.c
        src/jsmooch-gui.h
        ../jsmooch-lib/src/component/cpu/z80/z80_opcodes.c
)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

set(SDL2_PATH, "/opt/homebrew/Cellar/sdl2/2.30.0")

set(SDL2_LIBRARY_TEMP "/opt/homebrew/Cellar/sdl2/2.30.0/lib/libSDL2.dylib")

FIND_PACKAGE(SDL2 REQUIRED)
Message( STATUS "FINDING SDL2" )
IF (${SDL2_FOUND})
    Message( STATUS "SDL2_FOUND: " ${SDL2_FOUND})
    Message( STATUS "SDL2_INCLUDE_DIR:" ${SDL2_INCLUDE_DIR})
    Message( STATUS "SDL2_LIBRARY_TEMP: " ${SDL2_LIBRARY_TEMP})
    Message( STATUS "SDL2_LIBRARY: " ${SDL2_LIBRARY})
    Message( STATUS "YO: " ${YORHO})
ELSE()
    Message( STATUS "SDL2_FOUND: " ${SDL2_FOUND})
    Message( FATAL_ERROR "SDL2 NOT FOUND" )
ENDIF()
set(SDL2_LIBRARY "libSDL2.dylib")

include_directories( ${SDL2_INCLUDE_DIR} )
include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src) # include files from ProjectB

target_link_libraries(jsmooch-gui jsmooch-lib ${SDL2_LIBRARIES})

install(TARGETS jsmooch-gui RUNTIME DESTINATION bin)