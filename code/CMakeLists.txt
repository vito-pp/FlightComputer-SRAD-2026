cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD myboard)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/src/board)

include(pico_sdk_import.cmake)

project(my_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(my_project
    src/main.c
)

target_include_directories(my_project PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(my_project
    pico_stdlib
)

pico_add_extra_outputs(my_project)
