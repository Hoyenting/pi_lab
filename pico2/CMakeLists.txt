cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(bmc_sim C CXX ASM)
set(CMAKE_C_STANDARD 11)

set(PICO_BOARD pico2_w)

pico_sdk_init()

add_executable(bmc_sim bmc_sim.c)

target_link_libraries(bmc_sim
    pico_stdlib
    hardware_uart
    hardware_adc
    pico_cyw43_arch_lwip_threadsafe_background
)

# USB for debug printf; UART0 pins are freed for data output
pico_enable_stdio_usb(bmc_sim 1)
pico_enable_stdio_uart(bmc_sim 0)

target_include_directories(bmc_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

pico_add_extra_outputs(bmc_sim)
