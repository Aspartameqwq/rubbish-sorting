# Hardware wiring and first bring-up

This is the project's single source of truth for board-to-module wiring, TB6600 wiring and DIP selections. The topology and settings below are **SELECTED** from the user's plan and the photographed module label. Electrical compatibility and all physical behavior remain **PENDING** measurement; selected does not mean verified.

The photo shows a PUFEIDE-marked TB6600 module labeled `DC 9–42VDC`. No separate schematic or exact-revision manufacturer manual is available. Board-specific input behavior is therefore not inferred beyond the visible terminal labels and switch table.

## STM32F103C8T6 hardware resource and wiring table

| STM32 pin / net | Peripheral / object | External device terminal | Function | Status |
|---|---|---|---|---|
| PA0 | TIM2_CH1 | Servo Signal | Pitch Servo PWM | SELECTED |
| PA6 | TIM3_CH1 | TB6600 PUL+ | Yaw step pulse | SELECTED |
| PB12 | GPIO Output | TB6600 DIR+ | Yaw direction | SELECTED |
| PB13 | GPIO Output | TB6600 ENA+ | TB6600 enable | SELECTED |
| PA9 | USART1_TX | HC-04 RX | UART transmit to module | SELECTED |
| PA10 | USART1_RX | HC-04 TX | UART receive from module | SELECTED |
| PA13 | SWDIO | J-Link SWDIO | Debug data | USER-REPORTED WORKING PREVIOUSLY; NOT RETESTED |
| PA14 | SWCLK | J-Link SWCLK | Debug clock | USER-REPORTED WORKING PREVIOUSLY; NOT RETESTED |
| STM32 GND | Ground | TB6600 PUL- | PUL signal return | SELECTED |
| STM32 GND | Ground | TB6600 DIR- | DIR signal return | SELECTED |
| STM32 GND | Ground | TB6600 ENA- | ENA signal return | SELECTED |

This table records the selected connection plan; it does not assert that every lead, voltage, or signal has been measured on the current bench setup.

## Servo wiring and supply

| Servo lead | STM32 / supply connection | Status |
|---|---|---|
| Signal | PA0 / TIM2_CH1 | SELECTED |
| GND | Control/system ground | SELECTED |
| V+ | Separate regulated supply matching the exact Servo model | TO BE CONFIRMED |

Do not power a high-current Servo from an STM32 GPIO or the 3.3 V rail. Confirm the Servo's rated supply voltage and stall/current requirement from its exact model documentation. Connect Servo ground to the control ground so the PWM signal has a shared reference; size the external supply for the Servo load.

## HC-04 wiring and voltage

| STM32 | HC-04 | Status |
|---|---|---|
| PA9 / USART1_TX | RX | SELECTED |
| PA10 / USART1_RX | TX | SELECTED |
| Control ground | GND | SELECTED |
| Module VCC | Supply allowed by the exact module variant | TO BE CONFIRMED |

Confirm the actual module's supply range and UART logic levels from its board markings or documentation. Do not infer that every board sold as HC-04 has the same regulator, level shifting, or pinout.

## J-Link SWD wiring

| J-Link signal | STM32F103C8T6 | Status |
|---|---|---|
| SWDIO | PA13 | USER-REPORTED WORKING PREVIOUSLY; NOT RETESTED |
| SWCLK | PA14 | USER-REPORTED WORKING PREVIOUSLY; NOT RETESTED |
| GND | GND | SELECTED |
| VTref | Target logic-voltage reference | SELECTED |

Keep PA13 and PA14 reserved for SWD. VTref is the target reference connection; do not use it as target power unless the J-Link and target documentation explicitly allow that wiring.

## STM32 to TB6600 signal terminals

Use the selected 3.3 V common-cathode direct-GPIO topology. Do not add external transistors, MOSFETs, buffers, level shifters, or optocouplers in this task.

| STM32F103C8T6 | Mode | TB6600 terminal | Function | Status |
|---|---|---|---|---|
| PA6 | TIM3_CH1, alternate-function push-pull | PUL+ | Step pulse, active high | SELECTED |
| PB12 | GPIO output push-pull | DIR+ | Direction; HIGH is software FORWARD | SELECTED |
| PB13 | GPIO output push-pull | ENA+ | Enable; HIGH is software enabled | SELECTED |
| STM32 GND | Ground | PUL- | Signal common negative | SELECTED |
| STM32 GND | Ground | DIR- | Signal common negative | SELECTED |
| STM32 GND | Ground | ENA- | Signal common negative | SELECTED |

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

| Measurement | Result |
|---|---|
| PUL- ↔ power GND | PENDING — record `ISOLATED` or `INTERNALLY_COMMON` |
| DIR- ↔ power GND | PENDING — record `ISOLATED` or `INTERNALLY_COMMON` |
| ENA- ↔ power GND | PENDING — record `ISOLATED` or `INTERNALLY_COMMON` |

Record meter mode and observed resistance. This check characterizes this module; it does not change the selected MCU signal-return wiring. If another board or the supply ties returns together, record that too.

## TB6600 power terminals

| DC supply | TB6600 terminal | Status |
|---|---|---|
| +24 V DC | VCC | SELECTED |
| 24 V return / 0 V | GND power terminal | SELECTED |

The module label shows a 9–42 V DC range; the project selects a 24 V supply for bring-up. Confirm supply polarity and measured voltage before connecting it. **Never connect 24 V to an STM32 pin or 3.3 V rail.** The actual connected supply and module voltage have not been measured.

## Motor terminals

The user reports a four-wire, two-phase bipolar motor with a 1.8° step angle and approximately 1.5 A current. The four leads are reported connected to the driver's A/B output terminals. Confirm coil pairing with the motor documentation or, with every supply disconnected, use a meter to identify the two low-resistance winding pairs.

| Motor winding | TB6600 terminal | Status |
|---|---|---|
| Coil A, end 1 | A+ | SELECTED |
| Coil A, end 2 | A- | SELECTED |
| Coil B, end 1 | B+ | SELECTED |
| Coil B, end 2 | B- | SELECTED |

```text
24 V PSU +  ---------------------- TB6600 VCC
24 V PSU -  ---------------------- TB6600 GND (power return)

Motor coil A  -------------------- TB6600 A+ / A-
Motor coil B  -------------------- TB6600 B+ / B-
```

Never connect or disconnect `A+`, `A-`, `B+`, or `B-` while the driver is powered. Power the complete system down before changing motor wiring or DIP switches.

## Selected DIP positions

Set the switch lever toward the case's printed `ON` marking. The switch positions are a **project selection** based on the photographed table; visually confirm them with all power off before the first power-up.

| Switch | State | Label-table meaning |
|---|---|---|
| SW1 | OFF | 8 microstep row |
| SW2 | ON | 8 microstep row |
| SW3 | OFF | 8 microstep row |
| SW4 | ON | 1.5 A current row |
| SW5 | ON | 1.5 A current row |
| SW6 | OFF | 1.5 A current row |

For the pictured module's table, the selected microstep row is 8 microsteps and 1600 PUL pulses/revolution for a 1.8° motor. That corresponds nominally to 0.225° per PUL pulse with direct coupling. The selected current row is marked `Current(A) 1.5` and `PK Current 1.7`; this is a transcription of the case label, not a measurement or confirmation of how the motor rating is specified. Keep current verification pending.

## GPIO and current limits

Keep PA6 on TIM3_CH1 alternate-function push-pull, PB12/PB13 on push-pull GPIO outputs, and the current CubeMX output-speed settings. PA6, PB12 and PB13 remain the selected pins; the PA/PB port letters do not provide a special high-current GPIO mode. PC13/PC14/PC15 have a lower 3 mA drive restriction and are not suitable substitutes for this direct-input scheme. See ST's [STM32F103x8/xB datasheet (DS5319)](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf) and [RM0008 reference manual](https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101-103-105-107-stm32f100-series-armbased-32bit-mcus-stmicroelectronics.pdf).

The project uses **8 mA per signal as a conservative direct-drive acceptance gate**, not as an absolute-maximum rating or proof that the module accepts 3.3 V. First screen each optocoupler input with a 3.3 V bench source current-limited to 8 mA while the STM32 signal pin and driver power stage are disconnected. If the source enters current limit before reaching a valid 3.3 V level, direct GPIO drive is rejected. After that screen passes, confirm current and voltage at the actual MCU output; an ammeter must be inserted in series in the signal-positive path, never placed across an output and ground. For pulsed PUL, use a suitable series shunt and scope/peak measurement; a DMM's average current reading can understate the active-pulse current. Record both signal voltage and current. If any input exceeds 8 mA, or its active level is not valid, record `DIRECT_GPIO_DRIVE_REJECTED` and stop this bring-up; a separate buffer/transistor design must be reviewed before continuing.

STM32 output current also has aggregate VDD/VSS limits. Do not use the datasheet's relaxed-output-voltage current figures as normal design targets. This project has not yet measured the module input current or confirmed reliable 3.3 V recognition.

## Software-selected levels and startup

| Signal | Selected software level | Physical status |
|---|---|---|
| ENA | HIGH enables; LOW disables | ENA effect PENDING |
| DIR | HIGH means logical FORWARD; LOW means REVERSE | Mechanical direction PENDING |
| PUL | Active HIGH; PWM1 | Recognition and waveform PENDING |

CubeMX starts PB12 and PB13 LOW. `TB6600_Init()` leaves ENA inactive and does not start PWM; `App_Init()` does not call `Stepper_Enable()`. PA6 must remain pulse-free until a move explicitly starts the timer. The 10 µs active pulse width and 20–10,000 PUL/s software limits remain initial software values pending measurement.

## First hardware bring-up

Do these checks with the motor mechanically safe and the driver power disabled until the current gate passes.

1. Disconnect all power sources. Set SW1–SW6 as listed above, with `ON` toward the case marking.
2. Confirm the two motor coil pairs from the motor documentation or with a resistance meter while disconnected. Verify A and B pairs before wiring.
3. Connect each motor coil to its A/B terminal pair. Do not hot-plug motor leads.
4. Wire the 24 V supply only to TB6600 VCC/GND. With it disconnected from the module, measure its output polarity and voltage.
5. Prepare the selected signal wiring, but leave PA6/PB12/PB13 disconnected from the module. Connect STM32 GND to PUL-/DIR-/ENA-. Check every connection and confirm no 24 V conductor reaches the MCU.
6. With the MCU outputs disconnected and driver power off, screen PUL, DIR and ENA one at a time using a 3.3 V current-limited source set to an 8 mA ceiling. Use the matching negative input as the source return. Stop if current limit is reached or the active voltage is not valid.
7. Connect PUL+/DIR+/ENA+ to PA6/PB12/PB13. Power only the STM32. Confirm PB12/PB13 LOW and PA6 idle with no PUL edges. Confirm actual ENA disabled behavior only after driver power is available; do not infer it from the logic level alone.
8. With TB6600 24 V power still disconnected, send `STEPPER ENABLE`, then `STEPPER MOVE 100 20`. The repeated low-frequency pulses make PUL+ loaded voltage, active current, HIGH width and LOW width measurable; use a suitable series shunt and scope/peak measurement for PUL current. Measure DIR and ENA current/voltage as well. Stop if any line exceeds the 8 mA project gate or an MCU output falls outside its datasheet-guaranteed output range under load. Send `STEPPER DISABLE` after the measurement so ENA returns LOW before powering the driver. This unpowered check does not prove that the module recognizes the logic levels.
9. After the input-current and MCU-output-voltage checks pass, apply the selected 24 V to TB6600 and confirm the measured voltage at VCC/GND. Keep the motor mechanically secured and ENA LOW while checking the powered driver's inactive behavior.
10. With the motor mechanically secured, enable the Stepper. Scope PUL inactive level, HIGH width, LOW width and frequency. Check exactly 1, 2, 10 and 100 pulses and clean stop edges at a low rate.
11. Verify that the powered driver recognizes ENA and PUL at the selected levels, then check logical direction with a mechanically safe low-speed motor test. If the active levels are not recognized, record `DIRECT_GPIO_DRIVE_REJECTED` and stop. If rotation is opposite the project's logical FORWARD, change only `TB6600_DIR_FORWARD_LEVEL`; do not simultaneously swap motor phase leads.
12. Record module marking, supply voltage, DIP positions, GPIO input currents, waveform measurements, direction result and motor movement. Until recorded, all physical checks below remain PENDING.

## Verification status

| Item | Status |
|---|---|
| PA6→PUL+, PB12→DIR+, PB13→ENA+ | SELECTED |
| STM32 GND→PUL-/DIR-/ENA- | SELECTED |
| 24 V supply→VCC/GND | SELECTED; physical connection and voltage PENDING |
| Motor windings→A/B terminal pairs | SELECTED; coil pairing not independently verified |
| SW1 OFF, SW2 ON, SW3 OFF; SW4 ON, SW5 ON, SW6 OFF | SELECTED; physical switch positions PENDING |
| GPIO input current ≤8 mA per signal | PENDING |
| PUL levels, pulse width, frequency and count | PENDING |
| ENA physical behavior | PENDING |
| DIR mechanical orientation | PENDING |
| Noise immunity and motor rotation | PENDING |

The Toshiba [TB6600HG bare-IC datasheet](https://toshiba.semicon-storage.com/info/docget.jsp?did=12780&prodName=TB6600HG) is not a specification for this commercial module's optocoupler input circuit.

## Bench bring-up record

Fill in **Measured** and **Result** only after performing each physical check. Until then, keep the result `PENDING`; firmware builds and host tests do not establish these values.

| Check | Expected / acceptance target | Measured | Result |
|---|---|---|---|
| Servo PA0 PWM frame | 20 ms | — | PENDING |
| Servo horizontal pulse | Around 1500 µs | — | PENDING |
| Pitch horizontal | Platform is level at Pitch 0 mdeg | — | PENDING |
| Pitch +5° | Correct documented positive direction, no binding | — | PENDING |
| Pitch -5° | Correct opposite direction, no binding | — | PENDING |
| PUL input current | At or below the 8 mA project gate | — | PENDING |
| PUL active HIGH width | Around 10 µs | — | PENDING |
| PUL 20 Hz command | Clean 20 Hz output at the selected input | — | PENDING |
| DIR orientation | Matches logical FORWARD / REVERSE | — | PENDING |
| ENA behavior | Powered TB6600 responds to enable/disable as expected | — | PENDING |
| 1600 PUL pulses | Nominally one motor revolution if directly coupled and no steps are lost | — | PENDING |
