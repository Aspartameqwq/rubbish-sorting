# Resolve the CubeF1 package outside the CubeMX-generated CMake directory.
set(STM32CUBE_F1_FW_ROOT "" CACHE PATH "Path to STM32Cube_FW_F1_V1.8.7")

if(NOT STM32CUBE_F1_FW_ROOT)
    if(DEFINED ENV{STM32CUBE_F1_FW_ROOT})
        set(STM32CUBE_F1_FW_ROOT "$ENV{STM32CUBE_F1_FW_ROOT}")
    elseif(WIN32 AND DEFINED ENV{USERPROFILE})
        set(STM32CUBE_F1_FW_ROOT
            "$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7")
    endif()
endif()

if(NOT EXISTS "${STM32CUBE_F1_FW_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_hal.h")
    message(FATAL_ERROR
        "STM32CubeF1 v1.8.7 was not found. Set STM32CUBE_F1_FW_ROOT to the package root.")
endif()

set(STM32CUBE_F1_HAL_DIR "${STM32CUBE_F1_FW_ROOT}/Drivers/STM32F1xx_HAL_Driver")
set(STM32CUBE_F1_DEVICE_DIR "${STM32CUBE_F1_FW_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx")
set(STM32CUBE_F1_CMSIS_DIR "${STM32CUBE_F1_FW_ROOT}/Drivers/CMSIS")

set(STM32CUBE_F1_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/Core/Inc"
    "${STM32CUBE_F1_HAL_DIR}/Inc"
    "${STM32CUBE_F1_HAL_DIR}/Inc/Legacy"
    "${STM32CUBE_F1_DEVICE_DIR}/Include"
    "${STM32CUBE_F1_CMSIS_DIR}/Include"
)

set(STM32CUBE_F1_HAL_SOURCES
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_tim.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_tim_ex.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_rcc.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_gpio.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_dma.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_cortex.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_pwr.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_flash.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_exti.c"
    "${STM32CUBE_F1_HAL_DIR}/Src/stm32f1xx_hal_uart.c"
)
