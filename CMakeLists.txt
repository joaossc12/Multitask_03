# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "C:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(Multitask_03 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(Multitask_03 Multitask_03.c lib/ssd1306.c)

pico_set_program_name(Multitask_03 "Multitask_03")
pico_set_program_version(Multitask_03 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Multitask_03 0)
pico_enable_stdio_usb(Multitask_03 1)

# Add the standard library to the build
target_link_libraries(Multitask_03
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(Multitask_03 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(Multitask_03 
        hardware_i2c
        hardware_pwm
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4
        )

pico_add_extra_outputs(Multitask_03)

