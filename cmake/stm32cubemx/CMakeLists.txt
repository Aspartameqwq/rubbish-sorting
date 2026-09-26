cmake_minimum_required(VERSION 3.22)

project(stm32cubemx)
add_library(stm32cubemx INTERFACE)

# Locate STM32CubeF1 without embedding a machine-specific installation path.
set(STM32CUBE_F1_FW_ROOT "" CACHE PATH "Path to STM32Cube_FW_F1_V1.8.7")
if(NOT STM32CUBE_F1_FW_ROOT AND DEFINED ENV{STM32CUBE_F1_FW_ROOT})
    set(STM32CUBE_F1_FW_ROOT "$ENV{STM32CUBE_F1_FW_ROOT}")
endif()
if(NOT STM32CUBE_F1_FW_ROOT AND WIN32 AND DEFINED ENV{USERPROFILE})
    set(STM32CUBE_F1_FW_ROOT "$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7")
endif()
if(NOT EXISTS "${STM32CUBE_F1_FW_ROOT}/Drivers/STM32F1xx_HAL_Driver")
    message(FATAL_ERROR
        "STM32CubeF1 v1.8.7 was not found. Set STM32CUBE_F1_FW_ROOT to its installation directory.")
endif()

set(STM32F1_HAL_DIR "${STM32CUBE_F1_FW_ROOT}/Drivers/STM32F1xx_HAL_Driver")
set(STM32_CMSIS_DIR "${STM32CUBE_F1_FW_ROOT}/Drivers/CMSIS")

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(stm32cubemx INTERFACE 
	USE_HAL_DRIVER 
	STM32F103xB
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(stm32cubemx INTERFACE
    ../../Core/Inc
    ${STM32F1_HAL_DIR}/Inc
    ${STM32F1_HAL_DIR}/Inc/Legacy
    ${STM32_CMSIS_DIR}/Device/ST/STM32F1xx/Include
    ${STM32_CMSIS_DIR}/Include
)

target_sources(stm32cubemx INTERFACE
    ../../Core/Src/main.c
    ../../Core/Src/gpio.c
    ../../Core/Src/dma.c
    ../../Core/Src/tim.c
    ../../Core/Src/usart.c
    ../../Core/Src/stm32f1xx_it.c
    ../../Core/Src/stm32f1xx_hal_msp.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_tim.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_tim_ex.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_rcc.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_gpio.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_dma.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_cortex.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_pwr.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_flash.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_exti.c
    ${STM32F1_HAL_DIR}/Src/stm32f1xx_hal_uart.c
    ../../Core/Src/system_stm32f1xx.c
    ../../Core/Src/sysmem.c
    ../../Core/Src/syscalls.c
    ../../startup_stm32f103xb.s
)

target_link_directories(stm32cubemx INTERFACE
)

target_link_libraries(stm32cubemx INTERFACE
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()
