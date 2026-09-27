# rubbish-sorting

STM32F103C8T6 下位机工程，使用 STM32CubeMX 6.12.0、STM32CubeF1 HAL v1.8.7、C11 和 CMake。当前实现 HC-04 串口协议、舵机 PWM、TB6600 脉冲输出、有限步数 Stepper 控制，以及独立的整数梯形速度曲线数学模块。

## 当前状态

- CubeMX 已生成并核对：72 MHz 系统时钟、SWD、TIM2_CH1/PA0、USART1/PA9/PA10、DMA1_CH5 Circular RX、TIM3_CH1/PA6、PB12 DIR、PB13 ENA 和 TIM3 IRQ。
- Servo、HC-04、命令解析、TB6600 BSP、有限步数 Stepper、整数 profile foundation 和 App health 状态已实现。
- host 软件测试、ARM Debug/Release 构建和硬件验证状态以当前分支 CI/开发记录为准；未接实物时不得声称硬件通过。
- TB6600 项目已选择 STM32 3.3 V GPIO 共阴直连，8 细分（1600 PUL/rev）及 1.5 A 面板电流档；输入电流、波形、ENA/DIR 行为和电机运动仍待实测。完整接线见 [Docs/wiring.md](Docs/wiring.md)。
- Servo 1400–1600 µs、HC-04 115200 baud、TB6600 10 µs 脉冲宽度/频率范围和方向建立时间仍需实物验证。
- 步进位置为固件已完成的脉冲计数，不是电机轴反馈位置。没有启用 Stepper DMA，也没有实现闭环、限位、回零或业务动作。

## 目录

```text
Core/                    CubeMX 生成的 HAL 工程
Config/                  项目级集中配置
App/                     初始化、health 状态和主循环调度
BSP/                     Servo、HC-04、TB6600 和定时换算
Motion/                  Stepper 控制器及 HAL-free profile 数学
Protocol/                有限 ASCII 命令解析
Docs/                    架构、硬件、命令、驱动和开发文档
tests/host/              小型 C host 测试和 HAL/UART stub
cmake/stm32cube_f1.cmake CubeF1 包查找和 HAL 源文件列表
```

## 构建固件

要求 CMake 3.22+、Ninja、GNU Arm Embedded Toolchain 和 STM32CubeF1 v1.8.7。CubeF1 路径通过 `STM32CUBE_F1_FW_ROOT` CMake 变量或同名环境变量设置；Windows 默认查找 `%USERPROFILE%/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7`。

```powershell
cmake --preset Debug
cmake --build --preset Debug
cmake --preset Release
cmake --build --preset Release
```

## 运行 host 测试

Host 测试单独用本机 C 编译器构建，不使用 ARM toolchain：

```powershell
cmake -S tests/host -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

CubeMX 会生成 `Core/*`、`.ioc` 和 `cmake/stm32cubemx/*`。仓库 root CMake 不依赖生成的 CMake 文件，而在 `cmake/stm32cube_f1.cmake` 查找 HAL 包并维护源列表。更改或重新生成外设后，要审查并同步 root `CMakeLists.txt` 中的 Core 源文件列表。

## 文档

- [架构与模块边界](Docs/architecture.md)
- [硬件资源和 CubeMX 配置](Docs/hardware.md)
- [TB6600 唯一接线主表与首次调试](Docs/wiring.md)
- [串口命令协议](Docs/protocol.md)
- [TB6600 驱动](Docs/tb6600.md)
- [Stepper 控制与 profile](Docs/stepper-control.md)
- [开发、构建和验证流程](Docs/development.md)
- [编码规范](Docs/coding-style.md)

## Contributing

改动请基于当前集成分支开短期功能分支，通过 Pull Request review。提交外设改动时一并更新 `.ioc` / 生成代码、root CMake 源文件列表、host 测试和对应硬件文档；未做实机验证的行为请明确标注 `PENDING`。本仓库当前未声明开源许可证；如需在仓库外复用代码，请先确认许可证安排。
