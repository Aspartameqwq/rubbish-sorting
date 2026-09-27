cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME rubbish-sorting)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
# Core project settings
project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# CubeMX-generated CMake may contain machine-local package paths. Resolve the
# CubeF1 package through this user-maintained module instead.
include(cmake/stm32cube_f1.cmake)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# CubeMX-owned sources are listed here so regeneration cannot rewrite package
# lookup paths. Review and synchronize this list after changing peripherals.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/dma.c
    Core/Src/tim.c
    Core/Src/usart.c
    Core/Src/stm32f1xx_it.c
    Core/Src/stm32f1xx_hal_msp.c
    Core/Src/system_stm32f1xx.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
    startup_stm32f103xb.s
    ${STM32CUBE_F1_HAL_SOURCES}
    App/Src/app.c
    BSP/Src/hc04.c
    BSP/Src/servo.c
    BSP/Src/tb6600.c
    BSP/Src/tb6600_timing.c
    Motion/Src/stepper.c
    Motion/Src/stepper_profile.c
    Control/Src/pitch_axis.c
    Control/Src/yaw_axis.c
    Diagnostics/Src/debug_state.c
    Protocol/Src/protocol.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/Inc
    BSP/Inc
    Config
    Control/Inc
    Diagnostics/Inc
    Motion/Inc
    Protocol/Inc
    ${STM32CUBE_F1_INCLUDE_DIRS}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F103xB
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Debug>:DEBUG_CONTROL_ENABLE=1>
    $<$<NOT:$<CONFIG:Debug>>:DEBUG_CONTROL_ENABLE=0>
)

# Add linked libraries.
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)
