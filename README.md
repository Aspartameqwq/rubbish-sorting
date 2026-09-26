# rubbish-sorting

STM32F103C8T6 project generated with STM32CubeMX and built with CMake.

## Build requirements

- CMake 3.22 or newer
- Ninja
- GNU Arm Embedded Toolchain (`arm-none-eabi-gcc` and related tools)
- STM32CubeF1 firmware package v1.8.7

Point `STM32CUBE_F1_FW_ROOT` to the STM32CubeF1 v1.8.7 installation directory, either through the environment or as a CMake cache variable:

```sh
cmake --preset Debug -DSTM32CUBE_F1_FW_ROOT=/path/to/STM32Cube_FW_F1_V1.8.7
cmake --build --preset Debug
```
