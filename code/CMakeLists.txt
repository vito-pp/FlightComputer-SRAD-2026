cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD myboard)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/src/board)

include(pico_sdk_import.cmake)

project(flight_computer C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build_sd)

add_executable(flight_computer
    src/main.c
    src/hw_config.c
    src/sd_logger.c
    src/sensors/adxl375.c
)

target_include_directories(flight_computer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src/board
)

target_link_libraries(flight_computer
    pico_stdlib
    hardware_i2c
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

pico_enable_stdio_usb(flight_computer 1)
pico_enable_stdio_uart(flight_computer 0)

pico_add_extra_outputs(flight_computer)
