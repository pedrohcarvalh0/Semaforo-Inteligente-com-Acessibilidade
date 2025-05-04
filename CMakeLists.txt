cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "D:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(SemaforoInteligente C CXX ASM)
pico_sdk_init()

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(SemaforoInteligente SemaforoInteligente.c lib/ssd1306.c)

pico_generate_pio_header(SemaforoInteligente  ${CMAKE_CURRENT_LIST_DIR}/lib/ws2812.pio)

pico_set_program_name(SemaforoInteligente "SemaforoInteligente")
pico_set_program_version(SemaforoInteligente "0.1")

target_link_libraries(SemaforoInteligente
        pico_stdlib
        hardware_gpio
        hardware_pwm
        hardware_pio
        hardware_i2c
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4)

target_include_directories(SemaforoInteligente PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_uart(SemaforoInteligente 1)
pico_enable_stdio_usb(SemaforoInteligente 1)

pico_add_extra_outputs(SemaforoInteligente)

