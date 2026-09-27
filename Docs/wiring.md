# 硬件接线与首次上电

This is the project's single source of truth for board-to-module wiring, TB6600 wiring and DIP selections. The topology and settings below are **SELECTED** from the user's plan and the photographed module label. Electrical compatibility and all physical behavior remain **PENDING** measurement; selected does not mean verified.

The photo shows a PUFEIDE-marked TB6600 module labeled `DC 9–42VDC`. No separate schematic or exact-revision manufacturer manual is available. Board-specific input behavior is therefore not inferred beyond the visible terminal labels and switch table.

## STM32F103C8T6 硬件资源与接线表

| STM32 引脚/网络 | 外设/对象 | 外部设备端子 | 用途 | 状态 |
|---|---|---|---|---|
| PA0 | TIM2_CH1 | 舵机信号线 | Pitch 舵机 PWM | 已选定 |
| PA6 | TIM3_CH1 | TB6600 PUL+ | Yaw 步进脉冲 | 已选定 |
| PB12 | GPIO 输出 | TB6600 DIR+ | Yaw 方向 | 已选定 |
| PB13 | GPIO 输出 | TB6600 ENA+ | TB6600 使能 | 已选定 |
| PA9 | USART1_TX | HC-04 RX | 向模块发送 UART 数据 | 已选定 |
| PA10 | USART1_RX | HC-04 TX | 接收模块 UART 数据 | 已选定 |
| PA13 | SWDIO | J-Link SWDIO | 调试数据 | 用户报告此前可用；本轮未复测 |
| PA14 | SWCLK | J-Link SWCLK | 调试时钟 | 用户报告此前可用；本轮未复测 |
| STM32 GND | 地 | TB6600 PUL- | PUL 信号回路 | 已选定 |
| STM32 GND | 地 | TB6600 DIR- | DIR 信号回路 | 已选定 |
| STM32 GND | 地 | TB6600 ENA- | ENA 信号回路 | 已选定 |

此表记录当前选定的接线方案；不代表已在当前台架上测量所有导线、电压或信号。

## 舵机接线与供电

| 舵机引线 | STM32/电源连接 | 状态 |
|---|---|---|
| 信号线 | PA0 / TIM2_CH1 | 已选定 |
| GND | 控制系统地 | 已选定 |
| V+ | 与舵机具体型号匹配的独立稳压电源 | 待确认 |

Do not power a high-current Servo from an STM32 GPIO or the 3.3 V rail. Confirm the Servo's rated supply voltage and stall/current requirement from its exact model documentation. Connect Servo ground to the control ground so the PWM signal has a shared reference; size the external supply for the Servo load.

## HC-04 接线与电压

| STM32 端 | HC-04 端 | 状态 |
|---|---|---|
| PA9 / USART1_TX | RX | 已选定 |
| PA10 / USART1_RX | TX | 已选定 |
| 控制系统地 | GND | 已选定 |
| 模块 VCC | 符合该模块具体版本要求的电源 | 待确认 |

Confirm the actual module's supply range and UART logic levels from its board markings or documentation. Do not infer that every board sold as HC-04 has the same regulator, level shifting, or pinout.

## J-Link SWD 接线

| J-Link 信号 | STM32F103C8T6 | 状态 |
|---|---|---|
| SWDIO | PA13 | 用户报告此前可用；本轮未复测 |
| SWCLK | PA14 | 用户报告此前可用；本轮未复测 |
| GND | GND | 已选定 |
| VTref | 目标板逻辑电压参考 | 已选定 |

Keep PA13 and PA14 reserved for SWD. VTref is the target reference connection; do not use it as target power unless the J-Link and target documentation explicitly allow that wiring.

## STM32 至 TB6600 信号端子接线

Use the selected 3.3 V common-cathode direct-GPIO topology. Do not add external transistors, MOSFETs, buffers, level shifters, or optocouplers in this task.

| STM32F103C8T6 | 模式 | TB6600 端子 | 用途 | 状态 |
|---|---|---|---|---|
| PA6 | TIM3_CH1 复用推挽输出 | PUL+ | 步进脉冲，高电平有效 | 已选定 |
| PB12 | GPIO 推挽输出 | DIR+ | 方向；高电平为软件定义的正向 | 已选定 |
| PB13 | GPIO 推挽输出 | ENA+ | 使能；高电平为软件定义的使能态 | 已选定 |
| STM32 GND | 地 | PUL- | 信号公共负端 | 已选定 |
| STM32 GND | 地 | DIR- | 信号公共负端 | 已选定 |
| STM32 GND | 地 | ENA- | 信号公共负端 | 已选定 |

```text
                 STM32F103C8T6
                       |
            +----------+----------+
            |          |          |
           PA6        PB12       PB13
        TIM3_CH1       |          |
            |          |          |
            v          v          v
          PUL+       DIR+       ENA+
            |          |          |
          TB6600 optocoupler inputs
            |          |          |
          PUL-       DIR-       ENA-
            +----------+----------+
                       |
                   STM32 GND
```

The signal-return terminals and the module's high-voltage `GND` terminal have different roles in this connection table. Connect STM32 GND to `PUL-`, `DIR-`, and `ENA-`; connect the 24 V supply return to the module's `GND` power terminal. Do not assume these terminals are internally isolated or internally tied together; no module schematic is available.

With **all power removed** and the module disconnected from the MCU and supply, check continuity/resistance separately between each signal return and the TB6600 power return:

| 测量项目 | 结果 |
|---|---|
| PUL- ↔ 电源 GND | 待实测——记录为“彼此隔离”或“模块内部共地” |
| DIR- ↔ 电源 GND | 待实测——记录为“彼此隔离”或“模块内部共地” |
| ENA- ↔ 电源 GND | 待实测——记录为“彼此隔离”或“模块内部共地” |

Record meter mode and observed resistance. This check characterizes this module; it does not change the selected MCU signal-return wiring. If another board or the supply ties returns together, record that too.

## TB6600 电源端子接线

| 直流电源 | TB6600 端子 | 状态 |
|---|---|---|
| +24 V DC | VCC | 已选定 |
| 24 V 回路负端 / 0 V | 电源 GND 端子 | 已选定 |

The module label shows a 9–42 V DC range; the project selects a 24 V supply for bring-up. Confirm supply polarity and measured voltage before connecting it. **Never connect 24 V to an STM32 pin or 3.3 V rail.** The actual connected supply and module voltage have not been measured.

## 步进电机端子接线

The user reports a four-wire, two-phase bipolar motor with a 1.8° step angle and approximately 1.5 A current. The four leads are reported connected to the driver's A/B output terminals. Confirm coil pairing with the motor documentation or, with every supply disconnected, use a meter to identify the two low-resistance winding pairs.

| 电机绕组 | TB6600 端子 | 状态 |
|---|---|---|
| A 相绕组，线端 1 | A+ | 已选定 |
| A 相绕组，线端 2 | A- | 已选定 |
| B 相绕组，线端 1 | B+ | 已选定 |
| B 相绕组，线端 2 | B- | 已选定 |

```text
24 V PSU +  ---------------------- TB6600 VCC
24 V PSU -  ---------------------- TB6600 GND (power return)

Motor coil A  -------------------- TB6600 A+ / A-
Motor coil B  -------------------- TB6600 B+ / B-
```

Never connect or disconnect `A+`, `A-`, `B+`, or `B-` while the driver is powered. Power the complete system down before changing motor wiring or DIP switches.

## 已选定的 DIP 拨码位置

Set the switch lever toward the case's printed `ON` marking. The switch positions are a **project selection** based on the photographed table; visually confirm them with all power off before the first power-up.

| 拨码开关 | 状态 | 面板表格含义 |
|---|---|---|
| SW1 | OFF | 对应 8 细分档 |
| SW2 | ON | 对应 8 细分档 |
| SW3 | OFF | 对应 8 细分档 |
| SW4 | ON | 对应 1.5 A 电流档 |
| SW5 | ON | 对应 1.5 A 电流档 |
| SW6 | OFF | 对应 1.5 A 电流档 |

For the pictured module's table, the selected microstep row is 8 microsteps and 1600 PUL pulses per motor revolution for a 1.8° motor. Firmware names the output-axis scale `YAW_AXIS_PULSES_PER_REV`; its current value of 1600 assumes a 1:1 coupling from motor to Yaw platform. Under that assumption, one PUL nominally corresponds to 0.225° of platform rotation. The actual switch positions, transmission ratio, and pulse-to-platform angle have not been verified. The selected current row is marked `Current(A) 1.5` and `PK Current 1.7`; this is a transcription of the case label, not a measurement or confirmation of how the motor rating is specified. Keep current verification pending.

## GPIO 与电流限制

Keep PA6 on TIM3_CH1 alternate-function push-pull, PB12/PB13 on push-pull GPIO outputs, and the current CubeMX output-speed settings. PA6, PB12 and PB13 remain the selected pins; the PA/PB port letters do not provide a special high-current GPIO mode. PC13/PC14/PC15 have a lower 3 mA drive restriction and are not suitable substitutes for this direct-input scheme. See ST's [STM32F103x8/xB datasheet (DS5319)](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf) and [RM0008 reference manual](https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101-103-105-107-stm32f100-series-armbased-32bit-mcus-stmicroelectronics.pdf).

The project uses **8 mA per signal as a conservative direct-drive acceptance gate**, not as an absolute-maximum rating or proof that the module accepts 3.3 V. First screen each optocoupler input with a 3.3 V bench source current-limited to 8 mA while the STM32 signal pin and driver power stage are disconnected. If the source enters current limit before reaching a valid 3.3 V level, direct GPIO drive is rejected. After that screen passes, confirm current and voltage at the actual MCU output; an ammeter must be inserted in series in the signal-positive path, never placed across an output and ground. For pulsed PUL, use a suitable series shunt and scope/peak measurement; a DMM's average current reading can understate the active-pulse current. Record both signal voltage and current. If any input exceeds 8 mA, or its active level is not valid, record `DIRECT_GPIO_DRIVE_REJECTED` and stop this bring-up; a separate buffer/transistor design must be reviewed before continuing.

STM32 output current also has aggregate VDD/VSS limits. Do not use the datasheet's relaxed-output-voltage current figures as normal design targets. This project has not yet measured the module input current or confirmed reliable 3.3 V recognition.

## 软件选定电平与启动状态

| 信号 | 软件选定电平 | 实物验证状态 |
|---|---|---|
| ENA | HIGH 表示使能；LOW 表示禁用 | ENA 实际效果待实测 |
| DIR | HIGH 表示逻辑正向；LOW 表示反向 | 机械转向待实测 |
| PUL | 高电平有效；PWM1 | 驱动器识别与波形待实测 |

CubeMX starts PB12 and PB13 LOW. `TB6600_Init()` leaves ENA inactive and does not start PWM; `App_Init()` does not call `Stepper_Enable()`. PA6 must remain pulse-free until a move explicitly starts the timer. The 10 µs active pulse width and TB6600-layer 20–10,000 PUL/s timing range remain initial software values pending measurement. YawAxis applies a separate 20–500 PUL/s conservative mechanism command range; 500 is not a driver or motor rating.

## 首次硬件上电检查

Do these checks with the motor mechanically safe and the driver power disabled until the current gate passes.

1. Disconnect all power sources. Set SW1–SW6 as listed above, with `ON` toward the case marking.
2. Confirm the two motor coil pairs from the motor documentation or with a resistance meter while disconnected. Verify A and B pairs before wiring.
3. Connect each motor coil to its A/B terminal pair. Do not hot-plug motor leads.
4. Wire the 24 V supply only to TB6600 VCC/GND. With it disconnected from the module, measure its output polarity and voltage.
5. Prepare the selected signal wiring, but leave PA6/PB12/PB13 disconnected from the module. Connect STM32 GND to PUL-/DIR-/ENA-. Check every connection and confirm no 24 V conductor reaches the MCU.
6. With the MCU outputs disconnected and driver power off, screen PUL, DIR and ENA one at a time using a 3.3 V current-limited source set to an 8 mA ceiling. Use the matching negative input as the source return. Stop if current limit is reached or the active voltage is not valid.
7. Connect PUL+/DIR+/ENA+ to PA6/PB12/PB13. Power only the STM32. Confirm PB12/PB13 LOW and PA6 idle with no PUL edges. Confirm actual ENA disabled behavior only after driver power is available; do not infer it from the logic level alone.
8. With TB6600 24 V power still disconnected and the Stepper disabled, manually place Yaw at cable neutral (Pitch wiring naturally routed, without visible twist). In Ozone send `DEBUG_CMD_SET_YAW_ZERO` and confirm `reference_state == MANUAL` and `commanded_mdeg == 0`. Then send `STEPPER ENABLE`, followed by `STEPPER MOVE 100 20`. The repeated low-frequency pulses make PUL+ loaded voltage, active current, HIGH width and LOW width measurable; use a suitable series shunt and scope/peak measurement for PUL current. Measure DIR and ENA current/voltage as well. Stop if any line exceeds the 8 mA project gate or an MCU output falls outside its datasheet-guaranteed output range under load. Send `STEPPER DISABLE` after the measurement so ENA returns LOW; this also invalidates Yaw reference. This unpowered check does not prove that the module recognizes the logic levels.
9. After the input-current and MCU-output-voltage checks pass, apply the selected 24 V to TB6600 and confirm the measured voltage at VCC/GND. Keep the motor mechanically secured and ENA LOW while checking the powered driver's inactive behavior.
10. With the motor mechanically secured and the Stepper disabled, manually restore cable neutral and issue Ozone `DEBUG_CMD_SET_YAW_ZERO` again. Enable the Stepper only after confirming `reference_state == MANUAL`. Scope PUL inactive level, HIGH width, LOW width and frequency. Check exactly 1, 2, 10 and 100 pulses and clean stop edges at a low rate.
11. With cable clearance confirmed for a small move, command 100 PUL at 20 PUL/s and measure the actual Yaw platform angle. The current 22.5° expectation follows only from 1600 PUL per output revolution and the assumed 1:1 coupling. Do not use a full revolution as the first scale test. If the measured angle differs, stop and update `YAW_AXIS_PULSES_PER_REV` for the actual transmission before relying on the derived ±800 PUL cable range. Record the measured ratio and angle.
12. Verify that the powered driver recognizes ENA and PUL at the selected levels, then establish cable zero while disabled and check logical direction with a mechanically safe, small-angle reverse move. If the active levels are not recognized, record `DIRECT_GPIO_DRIVE_REJECTED` and stop. If rotation is opposite the project's logical FORWARD, change only `TB6600_DIR_FORWARD_LEVEL`; do not simultaneously swap motor phase leads.
13. Record module marking, supply voltage, DIP positions, GPIO input currents, waveform measurements, actual Yaw scale, direction result and motor movement. Until recorded, all physical checks below remain PENDING.

## Yaw scale verification gate

The current `YAW_AXIS_PULSES_PER_REV=1600` value is a software configuration
assumption for one complete Yaw output-axis/platform revolution. Its basis is
the reported 1.8° motor, the pictured 8-microstep row, and assumed 1:1
motor-to-platform coupling. Firmware does not read the DIP switches or sense
the Yaw shaft. Complete and record these checks before treating the software
cable range as a physical angular boundary:

- [ ] With all power off, visually confirm SW1=OFF, SW2=ON, SW3=OFF and the module's 8-microstep row.
- [ ] Confirm the actual motor-to-platform coupling and record whether it is 1:1.
- [ ] With the mechanism secured and cable clearance observed, establish cable zero and test 100 PUL at 20 PUL/s.
- [ ] Measure the Yaw platform angle; 22.5° is the current theoretical value only under the 1600 PUL/output-revolution assumption.
- [ ] If the result differs, update `YAW_AXIS_PULSES_PER_REV` for the measured transmission before continuing; the angle-derived pulse bounds then rebuild from that scale.
- [ ] Record the measured scale. Set `YAW_AXIS_SCALE_VERIFIED=1` only after DIP, transmission ratio, and small-angle pulse-to-platform behavior have all been checked.

Do not rotate a full revolution to discover the scale. Begin with the 100-PUL
small-angle check, then progress through approximately ±5°, ±30°, ±60° and
±90° only after confirming cable clearance at each stage. The configured
±180° endpoints are not verified mechanical safe limits; use a smaller
software angle range if the real cable route has less travel.

## 验证状态

| 检查项目 | 状态 |
|---|---|
| PA6→PUL+，PB12→DIR+，PB13→ENA+ | 已选定 |
| STM32 GND→PUL-/DIR-/ENA- | 已选定 |
| 24 V 电源→VCC/GND | 接线方案已选定；实物连接与电压待实测 |
| 电机绕组→A/B 端子组 | 接线方案已选定；线圈配对尚未独立确认 |
| SW1 OFF、SW2 ON、SW3 OFF；SW4 ON、SW5 ON、SW6 OFF | 拨码方案已选定；实物位置待检查 |
| 8 细分与实际传动比 | `YAW_AXIS_PULSES_PER_REV=1600` 当前假设；DIP、1:1 传动和平台比例待检查 |
| 100 PUL、20 PUL/s 的平台角度 | 当前理论值 22.5°（基于假设）；待实测 |
| 每路 GPIO 输入电流 ≤8 mA | 待实测 |
| PUL 电平、脉宽、频率与脉冲计数 | 待实测 |
| ENA 实际行为 | 待实测 |
| DIR 机械转向 | 待实测 |
| 抗干扰能力与电机转动情况 | 待实测 |

The Toshiba [TB6600HG bare-IC datasheet](https://toshiba.semicon-storage.com/info/docget.jsp?did=12780&prodName=TB6600HG) is not a specification for this commercial module's optocoupler input circuit.

## 台架上电记录

Fill in **Measured** and **Result** only after performing each physical check. Until then, keep the result `PENDING`; firmware builds and host tests do not establish these values.

| 检查项目 | 预期值/验收目标 | 实测值 | 结果 |
|---|---|---|---|
| 舵机 PA0 PWM 周期 | 20 ms | — | 待实测 |
| 舵机水平位置脉宽 | 约 1500 µs | — | 待实测 |
| Pitch 水平位置 | Pitch 0 mdeg 时平台水平 | — | 待实测 |
| Pitch +5° | 与定义的正方向一致，机构无卡滞 | — | 待实测 |
| Pitch -5° | 与正方向相反，机构无卡滞 | — | 待实测 |
| PUL 输入电流 | 不超过项目 8 mA 限值 | — | 待实测 |
| PUL 高电平有效脉宽 | 约 10 µs | — | 待实测 |
| PUL 20 Hz 命令 | 驱动输入端测得稳定的 20 Hz 波形 | — | 待实测 |
| DIR 转向 | 符合逻辑正向/反向定义 | — | 待实测 |
| ENA 行为 | TB6600 上电后按预期响应使能/禁用 | — | 待实测 |
| 100 个 PUL 脉冲，20 PUL/s | 在当前 1:1 传动假设下，Yaw 平台理论转角约 22.5° | — | 待实测 |
