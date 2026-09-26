# rubbish-sorting

STM32 下位机工程，目标为 STM32F103C8T6（LQFP48），使用 STM32CubeMX、STM32 HAL 和 CMake。

## 当前状态

- 仓库基线：STM32CubeMX 6.12.0 工程，STM32CubeF1 HAL v1.8.7；本轮已补齐目标配置头文件和工程文档。
- 已在当前 `.ioc` / 生成代码中确认：HSE × 9、SYSCLK/HCLK 72 MHz、APB1 36 MHz、APB2 72 MHz、APB1 Timer Clock 72 MHz；SWD 保持 PA13/PA14。
- CubeMX 已生成并核对 TIM2_CH1/PA0、USART1/PA9/PA10、DMA1 Channel 5 Circular 及所需 NVIC 中断；系统时钟和 SWD 保持原配置。
- 已实现舵机 PWM 驱动、HC-04 DMA/IDLE transport、有限命令协议、App 主循环调度和集中配置。
- HC-04 使用 115200 作为生成配置匹配的测试默认值，但实际模块和对端波特率仍需确认（`TO_BE_CONFIRMED`）；舵机 1400–1600 µs 仅为窄测试窗口，实际行程需标定。
- Debug 和 Release 的 CMake configure/build 均已通过，新增代码无编译器警告；尚未做开发板、HC-04 和舵机实机验证。
- TB6600/Stepper 引脚和 DMA 资源只做预留；当前不实现步进电机、K230、FreeRTOS 或垃圾分类业务逻辑。

CubeMX 外设配置已生成并核对；配置细节见 [硬件与 CubeMX 配置](Docs/hardware.md)。后续代码集成和构建流程见 [开发指南](Docs/development.md)。

## 目录

```text
Core/                    CubeMX 生成的 HAL 工程
Config/                  项目级集中配置
App/                     应用调度
BSP/                     舵机与 HC-04 硬件驱动
Protocol/                文本命令解析
Docs/                    架构、硬件、协议和开发规范
cmake/stm32cubemx/       HAL 与 CubeMX 生成源文件的 CMake 集成
```

`Config/`、`App/`、`BSP/`、`Protocol/` 已包含本轮实现；TB6600/Stepper 目录和驱动仍未加入。

## 构建要求

- CMake 3.22 或更新版本
- Ninja
- GNU Arm Embedded Toolchain（`arm-none-eabi-gcc` 等）
- STM32CubeF1 v1.8.7

配置并构建 Debug：

```powershell
cmake --preset Debug -DSTM32CUBE_F1_FW_ROOT="C:/Users/<user>/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7"
cmake --build --preset Debug
```

也可以将 `STM32CUBE_F1_FW_ROOT` 设置为环境变量。当前仓库已包含 CubeMX 生成的 TIM、USART 和 DMA 文件；重新生成后请按开发指南检查 CMake 中的 CubeF1 路径是否仍为可移植配置。

## 文档

- [架构与模块边界](Docs/architecture.md)
- [硬件资源和 CubeMX 配置](Docs/hardware.md)
- [串口命令协议](Docs/protocol.md)
- [开发、构建和实机验证流程](Docs/development.md)
- [编码规范](Docs/coding-style.md)

## 验证状态

软件构建、静态审查、配置核对和实机验证会分开记录。没有连接硬件完成的项目不会标记为实机验证通过。
