# rubbish-sorting

STM32F103C8T6 下位机工程，使用 STM32CubeMX 6.12.0、STM32CubeF1 HAL v1.8.7、C11 和 CMake。当前实现 HC-04 串口协议、Pitch/Yaw 轴抽象、Servo PWM、TB6600 脉冲输出、有限步数 Stepper 控制、Ozone 调试遥测/命令邮箱，以及独立的整数梯形速度曲线数学模块。

## 当前状态

- CubeMX 已生成并核对：72 MHz 系统时钟、SWD、TIM2_CH1/PA0、USART1/PA9/PA10、DMA1_CH5 Circular RX、TIM3_CH1/PA6、PB12 DIR、PB13 ENA 和 TIM3 IRQ。
- Pitch 轴由 Servo 控制，项目逻辑水平中位选择为 130° / 1500 µs；该坐标锚点尚未经过实际舵机和机构校准。
- Yaw 轴由 Stepper/TB6600 控制；选定 1600 PUL/rev，即 225 mdeg/PUL。绝对 Yaw 命令需要操作员设置手动零点；Stepper 计数仍是开环估计。
- Pitch/Yaw target、commanded estimate、measured angle 已分离。当前无角度传感器，两个 measured 字段均无效。
- 软件限位框架已实现，但有效标志默认关闭，范围仍是未校准占位值；不能视作安全保护。
- Ozone 全局遥测符号与调试命令邮箱已实现；命令注入只在 Debug 构建启用，Release 不执行邮箱命令。
- HC-04、命令解析、TB6600 BSP、有限步数 Stepper、整数 profile foundation 和 App health 状态已实现。
- host 软件测试、ARM Debug/Release 构建和硬件验证状态以当前分支 CI/开发记录为准；未接实物时不得声称硬件通过。
- TB6600 项目已选择 STM32 3.3 V GPIO 共阴直连，8 细分（1600 PUL/rev）及 1.5 A 面板电流档；输入电流、波形、ENA/DIR 行为和电机运动仍待实测。完整接线见 [Docs/wiring.md](Docs/wiring.md)。
- Servo 1400–1600 µs、HC-04 115200 baud、TB6600 10 µs 脉冲宽度/频率范围和方向建立时间仍需实物验证。
- 步进位置为固件已完成的脉冲计数，不是电机轴反馈位置。没有启用 Stepper DMA、角度传感器、homing 或 PID actuator output。

## 目录

```text
Core/                    CubeMX 生成的 HAL 工程
Config/                  项目级集中配置
App/                     初始化、health 状态和主循环调度
BSP/                     Servo、HC-04、TB6600 和定时换算
Motion/                  Stepper 控制器及 HAL-free profile 数学
Control/                 PitchAxis、YawAxis 和角度/脉冲策略
Diagnostics/             Ozone 遥测镜像和 Debug 命令邮箱
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
- [Pitch/Yaw 轴控制、角度语义与 PID 路线图](Docs/axis-control.md)
- [J-Link / Ozone 调试与命令邮箱](Docs/debugging.md)
- [开发、构建和验证流程](Docs/development.md)
- [编码规范](Docs/coding-style.md)

## Contributing

改动请基于当前集成分支开短期功能分支，通过 Pull Request review。提交外设改动时一并更新 `.ioc` / 生成代码、root CMake 源文件列表、host 测试和对应硬件文档；未做实机验证的行为请明确标注 `PENDING`。Ozone 应使用 Debug ELF 与 `g_debug_state` / `g_debug_command`，不要直接修改 telemetry 或硬件寄存器。本仓库当前未声明开源许可证；如需在仓库外复用代码，请先确认许可证安排。
