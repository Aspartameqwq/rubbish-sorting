# TB6600 wiring and first bring-up

This is the project's single source of truth for physical TB6600 wiring and DIP selections. The topology and settings below are **SELECTED** from the user's plan and the photographed module label. Electrical compatibility and all physical behavior remain **PENDING** measurement; selected does not mean verified.

The photo shows a PUFEIDE-marked TB6600 module labeled `DC 9–42VDC`. No separate schematic or exact-revision manufacturer manual is available. Board-specific input behavior is therefore not inferred beyond the visible terminal labels and switch table.

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

The signal-return terminals and the module's high-voltage `GND` terminal have different roles in this connection table. Connect STM32 GND to `PUL-`, `DIR-`, and `ENA-`; connect the 24 V supply return to the module's `GND` power terminal. Do not assume these terminals are internally isolated or internally tied together; no module schematic is available. If the actual supply or another board ties their returns, record that in the bench notes.

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
8. With TB6600 24 V power still disconnected, send `STEPPER ENABLE`, then `STEPPER MOVE 1 20`. Wait for the move to complete and measure MCU-driven current and voltage; capture PUL pulse current/voltage with suitable instrumentation. Stop if any line exceeds the 8 mA project gate or an MCU output falls outside its datasheet-guaranteed output range under load. Send `STEPPER DISABLE` after the measurement so ENA returns LOW before powering the driver. This unpowered check does not prove that the module recognizes the logic levels.
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
