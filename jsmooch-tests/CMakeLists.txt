set(CMAKE_C_STANDARD 11)

add_executable(jsmooch-tests
        src/jsmooch-tests.c
        src/jsmooch-tests.h
        src/cpu-tests/sm83-tests.c
)

include_directories(${CMAKE_SOURCE_DIR}/jsmooch-lib/src ${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)
link_directories(${CMAKE_SOURCE_DIR}/jsmooch-tests/lib)

target_link_libraries(jsmooch-tests jsmooch-lib jansson)

install(TARGETS jsmooch-tests RUNTIME DESTINATION bin)
