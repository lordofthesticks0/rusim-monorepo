# PCB
The PCB uses a couple modules. The documentation are available below. Access the gerber files [here](https://drive.google.com/drive/folders/1VZM8af3p2Q1-N58rCPVywv5uHUcG0zeH?usp=drive_link).

This section is work in progress.

# 3D Models
3D modelling uses Tinkercad. Models will be available soon. Contact me if I forgot to update this section.

This section is work in progress.

# Electronics

## 1. Motor Driver Module

This module is used for controlling the pumps and stirrers. The two pumps are wired in different channels. Stirrers are wired one per channel.

### Motor State Table
| Enable (ENA/ENB) | IN1 (or IN3)| IN2 (or IN4) | Motor State |
|--------|---|---|------------|
| 0 (Low)| X | X | Stop (Coast) |
| (High) | 0 | 0 | Brake (Active stop) |
| 1 (High) | 0 | 1 | Rotate Clockwise (Forward) |
| 1 (High) | 1 | 0 | Rotate Counter-Clockwise (Reverse) |
| 1 (High) | 1 | 1 | Brake (Active stop) |
> Note: Forward current flows from OUT1 to OUT2 for IN1 and IN2. IN3 and IN4 maps to OUT3 to OUT4.

### Technical Specifications
| Parameter | Symbol | Min | Typical | Max | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Motor Supply Voltage | Vs | 5 | 12 | 35 | V |
| Continuous Output Current (per channel) | IO-cont | - | 2 | - | A |
| Peak Output Current | IO-peak | - | - | 3 | A |
| Logic Supply Voltage | VSS | 4.5 | 5 | 7 | V |
| Output Voltage Drop | VCEsat | 1.8 | - | 4.9 | V |
| Power Dissipation | Plot | - | - | 25 | W |
| Operating Temperature | Top | -2.5 | - | 130 | °C |
> **IMPORTANT**: Stirrer motor is assumed to have a steady-state current draw of 600mA. Current draw under load is unknown and might be significantly higher. Verify thoroughly and add current limiting circuitry before starting an electrical fire. Also consider inrush currents and add ramping logic or circuitry to avoid overloading.

See [further details](https://www.digi-electronics.com/en/blogs/l298n-motor-driver-guide-pinout-wiring-pwm-speed-control-troubleshooting/260.html#1) for more information.

## 2. WT32-ETH01

This module is used for the main interface to upload to the network. Wi-Fi does work but sometimes it's a bit of a hassle to set up. We decided on Ethernet because the device itself will be there almost permanently. Wi-Fi exists on this module as a backup option. Documentation is extensive. This section only recaps what's already available.

| Document | Link |
|---|---|
| Datasheet V1.4 (EN) | [WT32‑ETH01_datasheet_V1.4-en.pdf](https://en.wireless-tag.com/product-item-2.html) |
| Getting Started Guide | [Getting+Started+Guide+for+WT32‑ETH01.pdf](https://en.wireless-tag.com/product-item-2.html) |
| Unofficial Community Guide | [github.com/egnor/wt32-eth01](https://github.com/egnor/wt32-eth01) |
| LLM Generated Summary | [wt32-eth01-LLM.md](llm-generated/wt32-eth01.md) |

### Important Notes

The WT32-ETH01 does not come with a USB port. A USB to UART adapter is required to flash the device. Here is the one we used:

![The Layout](https://content.instructables.com/FPA/UUCA/JCUUFDPF/FPAUUCAJCUUFDPF.jpg)

> Note: We only use RX, VCC, GND, and TX. Note that for firmware flashing, GPIO0 must be pulled low.


## 3. ESP32-8048S050

This module is a ESP32 display module made by Sunton. Documentation is scarce and difficult to find. See the LLM generated summary [here](llm-generated/SUNTON-luna.md). Another summary is available [here](llm-generated/SUNTON-qwen.md).

## 4. ATmega2560 Mega Mini
![ATmega2560 Mega Mini Pinout](https://m.media-amazon.com/images/I/714vZN-0BbL.jpg)
### Pin Layout Overview

The Mega Mini's I/O pins are arranged along the board edges. Based on vendor documentation and community references, the typical physical arrangement is:

- **Left edge:** D22–D53 (extended digital pins, sequentially)
- **Right edge:** D0–D21, A0–A15, plus power pins (3.3V, 5V, GND, VIN)

This is confirmed by RobotDyn's documentation for the Mega 2560 PRO (Embed) variant, which states: "Left edge: D22–D53 (port pins, sequentially) — Right edge: D0–D21, A0–A15, plus power (3.3V, 5V, GND, Vin)".

> **Physical note:** The board does not accept standard Arduino shields. Connections are made via 0.1" (2.54 mm) male headers, which can be soldered to a protoboard, PCB, or connected with female jumper wires.

---

#### Complete Digital Pin Mapping (D0–D53)

The following table maps each Arduino digital pin to its corresponding ATmega2560 port pin, along with alternate functions (PWM, UART, external interrupt, SPI, I²C). This data is derived from the official Arduino ATmega2560 pin mapping documentation.

| Arduino Pin | ATmega2560 Port Pin | Alternate Functions | PWM? | External Interrupt? |
|---|---|---|---|---|
| D0 | PE0 | RXD0 (Serial 0 RX), PCINT8 | — | — |
| D1 | PE1 | TXD0 (Serial 0 TX) | — | — |
| D2 | PE4 | OC3B, INT4 | **Yes** | **INT4** |
| D3 | PE5 | OC3C, INT5 | **Yes** | **INT5** |
| D4 | PG5 | OC0B | **Yes** | — |
| D5 | PE3 | OC3A, AIN1 | **Yes** | — |
| D6 | PH3 | OC4A | **Yes** | — |
| D7 | PH4 | OC4B | **Yes** | — |
| D8 | PH5 | OC4C | **Yes** | — |
| D9 | PH6 | OC2B | **Yes** | — |
| D10 | PB4 | OC2A, PCINT4 | **Yes** | — |
| D11 | PB5 | OC1A, PCINT5 | **Yes** | — |
| D12 | PB6 | OC1B, PCINT6 | **Yes** | — |
| D13 | PB7 | OC0A, OC1C, PCINT7 | **Yes** | — |
| D14 | PJ1 | TXD3 (Serial 3 TX) | — | — |
| D15 | PJ0 | RXD3 (Serial 3 RX) | — | — |
| D16 | PH1 | TXD2 (Serial 2 TX) | — | — |
| D17 | PH0 | RXD2 (Serial 2 RX) | — | — |
| D18 | PD3 | TXD1 (Serial 1 TX), INT3 | — | **INT3** |
| D19 | PD2 | RXD1 (Serial 1 RX), INT2 | — | **INT2** |
| D20 | PD1 | SDA (I²C Data), INT1 | — | **INT1** |
| D21 | PD0 | SCL (I²C Clock), INT0 | — | **INT0** |
| D22 | PA0 | ADC0 | — | — |
| D23 | PA1 | ADC1 | — | — |
| D24 | PA2 | ADC2 | — | — |
| D25 | PA3 | ADC3 | — | — |
| D26 | PA4 | ADC4 | — | — |
| D27 | PA5 | ADC5 | — | — |
| D28 | PA6 | ADC6 | — | — |
| D29 | PA7 | ADC7 | — | — |
| D30 | PC7 | — | — | — |
| D31 | PC6 | — | — | — |
| D32 | PC5 | — | — | — |
| D33 | PC4 | — | — | — |
| D34 | PC3 | — | — | — |
| D35 | PC2 | — | — | — |
| D36 | PC1 | — | — | — |
| D37 | PC0 | — | — | — |
| D38 | PD7 | T0 | — | — |
| D39 | PG2 | — | — | — |
| D40 | PG1 | — | — | — |
| D41 | PG0 | — | — | — |
| D42 | PL7 | — | — | — |
| D43 | PL6 | — | — | — |
| D44 | PL5 | OC5C | **Yes** | — |
| D45 | PL4 | OC5B | **Yes** | — |
| D46 | PL3 | OC5A | **Yes** | — |
| D47 | PL2 | T5 | — | — |
| D48 | PL1 | ICP5 | — | — |
| D49 | PL0 | ICP4 | — | — |
| D50 | PB3 | MISO (SPI), PCINT3 | — | — |
| D51 | PB2 | MOSI (SPI), PCINT2 | — | — |
| D52 | PB1 | SCK (SPI), PCINT1 | — | — |
| D53 | PB0 | SS (SPI), PCINT0 | — | — |

> **Sources:** The port mappings and alternate functions are taken from the official Arduino ATmega2560 pin mapping table. External interrupt assignments (INT0–INT5) are confirmed by the ATmega2560 technical reference.

---

#### Analog Input Pins (A0–A15)

The Mega Mini provides **16 analog input channels** with 10-bit resolution (0–1023). These pins are on Port F (ADC0–ADC7) and Port K (ADC8–ADC15) of the ATmega2560.

| Arduino Analog Pin | ATmega2560 Port Pin | ADC Channel | Can Also Be Used As |
|---|---|---|---|
| A0 | PF0 | ADC0 | Digital I/O |
| A1 | PF1 | ADC1 | Digital I/O |
| A2 | PF2 | ADC2 | Digital I/O |
| A3 | PF3 | ADC3 | Digital I/O |
| A4 | PF4 | ADC4 | Digital I/O |
| A5 | PF5 | ADC5 | Digital I/O |
| A6 | PF6 | ADC6 | Digital I/O |
| A7 | PF7 | ADC7 | Digital I/O |
| A8 | PK0 | ADC8 | Digital I/O |
| A9 | PK1 | ADC9 | Digital I/O |
| A10 | PK2 | ADC10 | Digital I/O |
| A11 | PK3 | ADC11 | Digital I/O |
| A12 | PK4 | ADC12 | Digital I/O |
| A13 | PK5 | ADC13 | Digital I/O |
| A14 | PK6 | ADC14 | Digital I/O |
| A15 | PK7 | ADC15 | Digital I/O |

**Important notes:**
- The analog pins can also be used as digital I/O. In the Arduino Mega 2560 pin numbering scheme, A0–A15 correspond to digital pins **54–69**.
- The analog reference voltage (AREF) pin is available separately. By default, the ADC uses the 5V rail as the reference. You can change this using `analogReference()`.
- **Internal pull-ups** are available on all analog pins when used as digital inputs.

---

#### PWM Output Pins

The Mega Mini provides **15 PWM channels**, generated by the ATmega2560's Timer/Counter peripherals. The PWM-capable pins are:

| PWM Pin | ATmega2560 Timer | Notes |
|---|---|---|
| D2 | Timer 3 (OC3B) | 8-bit |
| D3 | Timer 3 (OC3C) | 8-bit |
| D4 | Timer 0 (OC0B) | 8-bit (used for `millis()` timing) |
| D5 | Timer 3 (OC3A) | 8-bit |
| D6 | Timer 4 (OC4A) | 16-bit |
| D7 | Timer 4 (OC4B) | 16-bit |
| D8 | Timer 4 (OC4C) | 16-bit |
| D9 | Timer 2 (OC2B) | 8-bit |
| D10 | Timer 2 (OC2A) | 8-bit |
| D11 | Timer 1 (OC1A) | 16-bit |
| D12 | Timer 1 (OC1B) | 16-bit |
| D13 | Timer 0 (OC0A) / Timer 1 (OC1C) | 8-bit / 16-bit |
| D44 | Timer 5 (OC5C) | 16-bit |
| D45 | Timer 5 (OC5B) | 16-bit |
| D46 | Timer 5 (OC5A) | 16-bit |

**Caveats:**
- **Pin D4** is tied to Timer 0, which is also used for `millis()` and `micros()`. Changing its PWM frequency will affect timing functions.
- **Pins D11 and D12** (Timer 1) are used by the `Servo` library on some Arduino cores. If you use `Servo.h`, you may lose PWM on these pins.
- The **`analogWrite()`** function accepts values from 0 (always off) to 255 (always on) for 8-bit timers, and 0–255 for 16-bit timers (the Arduino core scales the value).

---

#### Serial Communication (UART) Pins

The ATmega2560 has **four hardware serial ports** (UARTs). On the Mega Mini, these are exposed on the following pins:

| Serial Port | RX Pin | TX Pin | Notes |
|---|---|---|---|
| **Serial 0** | D0 (PE0) | D1 (PE1) | Used by the bootloader for programming via FTDI |
| **Serial 1** | D19 (PD2) | D18 (PD3) | Free for peripherals |
| **Serial 2** | D17 (PH0) | D16 (PH1) | Free for peripherals |
| **Serial 3** | D15 (PJ0) | D14 (PJ1) | Free for peripherals |

**Critical note:** Pins D0 and D1 are **reserved for USB-serial communication** during programming. If your sketch uses them for other purposes, the upload process may interfere. For most projects, use Serial1, Serial2, or Serial3 for external devices.

---

#### I²C (TWI) Pins

The Mega Mini has a dedicated hardware I²C (TWI) interface on:

| Signal | Arduino Pin | ATmega2560 Pin |
|---|---|---|
| **SDA** (Data) | D20 | PD1 |
| **SCL** (Clock) | D21 | PD0 |

**Important:** The Mega Mini does **not** have onboard I²C pull-up resistors. When connecting I²C devices, you must add external **4.7 kΩ pull-up resistors** from SDA and SCL to the 5V rail. Without these, I²C communication will fail or be unreliable.

---

#### SPI Pins

The Mega Mini has a dedicated hardware SPI interface on the following pins:

| Signal | Arduino Pin | ATmega2560 Pin |
|---|---|---|
| **MISO** (Master In, Slave Out) | D50 | PB3 |
| **MOSI** (Master Out, Slave In) | D51 | PB2 |
| **SCK** (Serial Clock) | D52 | PB1 |
| **SS** (Slave Select) | D53 | PB0 |

**Important:** The SPI pins are also available on the **6-pin ICSP header** (see below). When using the SPI library, the SS pin (D53) must be kept as an output (or set to input with pull-up) for the SPI hardware to operate correctly in master mode. If D53 is configured as an input without a pull-up, the ATmega2560 will automatically revert to slave mode.

---

#### External Interrupt Pins

The Mega Mini supports external interrupts on the following pins:

| Interrupt | Arduino Pin | ATmega2560 Pin | Notes |
|---|---|---|---|
| **INT0** | D21 | PD0 | Also SCL |
| **INT1** | D20 | PD1 | Also SDA |
| **INT2** | D19 | PD2 | Also RX1 |
| **INT3** | D18 | PD3 | Also TX1 |
| **INT4** | D2 | PE4 | Also PWM |
| **INT5** | D3 | PE5 | Also PWM |

These are confirmed by the ATmega2560 technical reference. Use `attachInterrupt()` with `digitalPinToInterrupt(pin)` to configure them.

---

#### ICSP Header (6-Pin)

The Mega Mini includes a **6-pin ICSP (In-Circuit Serial Programming) header** on the board edge. This allows you to program the ATmega2560 directly using an external programmer (AVR ISP, USBasp, etc.), bypassing the bootloader entirely.

| ICSP Pin | Signal | ATmega2560 Pin |
|---|---|---|
| 1 | MISO | PB3 (D50) |
| 2 | VCC | 5V |
| 3 | SCK | PB1 (D52) |
| 4 | MOSI | PB2 (D51) |
| 5 | RESET | RESET |
| 6 | GND | GND |

**Note:** The ICSP header is not the same as the FTDI header. The ICSP header is for direct microcontroller programming; the FTDI header is for uploading sketches via the bootloader.

---

#### FTDI / USB-TTL Header

On the PRO Mini version (no onboard USB), programming is done through a dedicated **FTDI header**. The pinout is:

| FTDI Adapter Pin | Mega 2560 PRO Mini Pin |
|---|---|
| TX | RX0 (D0) |
| RX | TX0 (D1) |
| DTR | DTR (auto-reset) |
| GND | GND |
| VCC (5V) | VCC |

The **DTR line** is connected to the reset circuit via a capacitor. When the Arduino IDE initiates an upload, the DTR signal pulses, resetting the ATmega2560 and allowing the bootloader to run. This enables **auto-reset** during programming.

> **Wiring caution:** A common mistake is to connect TX→TX and RX→RX. The correct wiring is **TX→RX** and **RX→TX** (crossover).

---

#### Power Pins

| Pin | Description |
|---|---|
| **VIN** | External power input (6–12 V recommended, 6–20 V limits). Connects to the onboard voltage regulator. |
| **5V** | Regulated 5V output/input. Can be used to power the board from a regulated 5V supply, or to power external 5V devices. |
| **3.3V** | Regulated 3.3V output (max ~800 mA). Derived from the 5V rail via an onboard LDO. |
| **GND** | Ground (multiple GND pins are available on the board). |
| **AREF** | Analog reference voltage for the ADC. By default, the ADC uses the 5V rail. |
| **RESET** | Active-low reset pin. Pull low to reset the ATmega2560. |

**Power notes:**
- The onboard LDO can supply approximately **800 mA on the 5V rail** and **800 mA on the 3.3V rail**. However, the higher the input voltage, the lower the available output current due to thermal dissipation.
- The ATmega2560 itself draws ~20–30 mA at 16 MHz, 5V. The remaining current budget is available for peripherals.
- The **3.3V rail** is derived from the 5V rail. If you need significant current at 3.3V, consider an external regulator.

---

#### Quick Reference: Pin Count Summary

| Category | Count | Pins |
|---|---|---|
| Digital I/O | 54 | D0–D53 |
| PWM Output | 15 | D2–D13, D44–D46 |
| Analog Input | 16 | A0–A15 |
| Hardware UART | 4 | Serial0–Serial3 |
| I²C (TWI) | 1 | SDA (D20), SCL (D21) |
| SPI | 1 | MISO (D50), MOSI (D51), SCK (D52), SS (D53) |
| External Interrupt | 6 | INT0–INT5 (see table above) |

See [further documentation](llm-generated/atmega2560-mega-mini.md) if needed.

## 5. Sensors

Currently, 5 sensors are used. See [calibration documentation](calibration.md) for calibration details.


### 5.1 HX710B (pressure)

This is a piezoelectric differential sensor. Uses a similar protocol to I2C for communication. Clock speeds are far lower. Depends on a [HX711 library](https://www.arduinolibraries.info/libraries/hx711). The actual HX710B chip is the name for the ADC IC. Not sure what the whole module is named. 

#### Pinout

| Module | Pin | Description |
| --- | --- | --- | 
| 1 | VCC / VIN | Power supply input. Accepts 3.3V to 5V DC. |
| 2 | GND | Ground connection. |
| 3 | OUT / DOUT / DATA | Digital data output from the HX710B ADC. Connect this to a digital input on your microcontroller. |
| 4 | SCK / CLK / SLC | Serial clock input. Connect this to a digital output on your microcontroller to clock out the data. |

### 5.2 TGS2611 (CH4)

This is a gas sensor that detects combustible gases. Current implementation is tuned for CH4 (methane). Detects between 500 to 10,000 ppm.

#### Pinout

| Pin Number | Function | Description |
| --- | --- | --- |
| 1 | Vcc | Source voltage (5V) |
| 2 | Sensor Electrode (-) | Negative sensor electrode |
| 3 | Sensor Electrode (+) | Positive sensor electrode |
| 4 | GND | Ground connection. |

#### Specifications

- Detection Range: 1 ~ 25% LEL (Lower Explosive Limit), or approximately 500 ~ 10,000 ppm
- Heater Voltage (VH): 5.0 ± 0.2 V AC/DC
- Circuit Voltage (VC): 5.0 ± 0.2 V DC
- Heater Current: 56 ± 5 mA
- Heater Power Consumption: 280 ± 25 mW
- Sensor Resistance (Rs): 0.68 ~ 6.8 kΩ in 5000 ppm methane
- Package: TO-5 metal can

### 5.3 MH-Z19C (CO2)

This is a CO2 sensor that detects the concentration of carbon dioxide in the air. Detects between 400 to 5,000 ppm.
> Note: Rumen headspace gas is ~70% CO2 (~700,000 ppm). This sensor is fundamentally incompatible. This documentation exist for the sake of documentation. Future readers should consider fixing this.

### Pinout

| Pin Number | Function | Description |
| --- | --- | --- |
| 1 | PWM | PWM output |
| 2 | Tx | UART (TXD) TTL Level data output |
| 3 | Rx | UART (RXD) TTL Level data input |
| 4 | Vin | Positive pole of power (+5V) |
| 5 | GND | Negative pole of power (GND) |
| 6 | Vo | Analog Output (not typically used) |
| 7 | Hd | HD (zero point calibration; low level for >7s is effective) |

#### Specifications

- Detection Gas: Carbon Dioxide (CO₂)
- Working Voltage: 5.0 ± 0.1V DC
- Average Current: < 40mA (@5V power supply)
- Peak Current: 125mA (@5V power supply)
- Interface Level: 3.3V (Compatible with 5V)
- Detection Range: 400–5000 ppm (optional up to 10000 ppm)
- Output Signal: UART (TTL level 3.3V) and PWM
- Preheat Time: 1 minute
- Response Time: T90 < 120 seconds
- Working Temperature: -10°C to 50°C
- Working Humidity: 0–95% RH (no condensation)
- Storage Temperature: -20°C to 60°C
- Weight: 5 g
- Lifespan: > 5 years (some sources state >10 years)
- Accuracy: ±(50ppm + 5% of reading value)

### 4.4 DS18B20 (temperature)

Sensor is a one-wire digital temperature sensor. Uses its own [library](https://github.com/adafruit/DHT-sensor-library).


#### Pinout
| Pin | Symbol | Description |
| --- | --- | --- |
| 1 | GND | Ground |
| 2 | DQ | Data Input/Output. Open-drain 1-Wire interface pin. |
| 3 | VDD | Optional power supply pin (3.0V to 5.5V). Must be grounded for parasitic power operation. |

#### Specifications

- Temperature Range: Measures temperatures from -55°C to +125°C (-67°F to +257°F).
- Accuracy: ±0.5°C accuracy over the range of -10°C to +85°C.
- Resolution: Programmable from 9 bits to 12 bits (user-selectable).
- Supply Voltage: Operates from 3.0V to 5.5V.
- Interface: 1-Wire® bus — requires only one data line (and ground) for communication with a central microprocessor.
- Power Modes: Can be powered externally via VDD or operate in parasitic power mode, drawing power directly from the data line (only DQ and GND needed).
- Unique ID: Each device has a unique 64-bit serial code stored in on-board ROM, allowing multiple sensors on the same bus.
- Alarm Function: User-definable nonvolatile upper and lower temperature trigger points with alarm search command.


### 5.5. PH-402C (pH)

The sensor is a pH probe that allows you to connect a BNC glass electrode to something accurately measured. Calibration is a pain. If issues were to be encountered in the future, try replacing this with something.

#### Pinout
| Pin | Name | Description |
| --- | --- | --- |
| VCC | Power Supply | Connect to a 5V DC power source. |
| GND | Board Ground | Ground connection for the module's power supply. |
| GND | Probe Ground | Dedicated ground for the pH probe's BNC connector shield. Connect this to the same ground as the board GND. |
| PO | Analog pH Output | The primary output. Provides an analog voltage (0–5V) proportional to the measured pH. Connect this to an analog input pin on your microcontroller (e.g., Arduino A0). |
| DO | Digital Output | A digital comparator output. It goes HIGH when the measured pH crosses a threshold set by the on-board potentiometer (POT2). Useful for triggering alarms or pumps. |
| TO | Temperature Output | An analog output for temperature compensation. It is designed to work with a separate temperature sensor (often a DS18B20) to correct pH readings based on solution temperature. |

#### Specifications

- Supply Voltage: 5V DC ±0.2V (AC/DC)
- Working Current: 5–10 mA
- Power Consumption: ≤ 0.5W
- pH Detection Range: 0 to 14 pH
- Operating Temperature: 0°C to 60°C (some sources list up to 80°C for the detection range, but the module's working temperature is lower)
- Accuracy: ±0.1 pH (at 25°C)
- Response Time: ≤ 5 seconds (stable time ≤ 60 seconds)
- Analog Output (PO): 0–5V
- Probe Connector: BNC
- Board Dimensions: Approximately 43mm × 32mm

## 5. MAX485
A serial-to-parallel converter that allows you to use RS-485 communication with your microcontroller. Used for Modbus RTU communication. The one we're using is NOT bi-directional. Manual direction control is strictly required. Library can be found [here](https://github.com/CIRCUITSTATE/CSE_ArduinoRS485)

#### Pinout

| Pin | Name | Description
|--- | --- | --- |
| 1 | RO | Receiver Output – TTL/CMOS level data received from the RS‑485 bus. Connect to the RX pin of your microcontroller. |
| 2 | RE | Receiver Enable – Active LOW. When pulled LOW, the receiver is enabled and the RO pin outputs data. When HIGH, RO is in a high‑impedance state. |
| 3 | DE | Driver Enable – Active HIGH. When pulled HIGH, the driver is enabled, and data on the DI pin is transmitted onto the RS‑485 bus. When LOW, the driver outputs are disabled. |
| 4 | DI | Driver Input – TTL/CMOS level data to be transmitted. Connect to the TX pin of your microcontroller. |
| 5 | GND | Ground – Common ground for the module and the logic supply. |
| 6 | A | Non‑inverting RS‑485 bus line – Connect to the A line of the RS‑485 network. |
| 7 | B | Inverting RS‑485 bus line – Connect to the B line of the RS‑485 network. |
| 8 | VCC | Power Supply – Typically +5V (see specs below). |

#### Specifications

- Main chip: MAX485 (low‑power, slew‑rate‑limited RS‑485/RS‑422 transceiver).
- Supply voltage: +5V nominal (operating range typically 4.75V – 5.25V).
- Quiescent current: ~300µA (typical static current).
- Communication mode: Half‑duplex (cannot transmit and receive simultaneously).
- Data rate: Up to 2.5Mbps (with the standard MAX485).
- Bus loading: Supports up to 32 transceivers on the same bus.
- Logic levels: TTL/CMOS compatible on RO, DI, RE, and DE pins.
- RS‑485 bus interface: Differential pair A (non‑inverting) and B (inverting). A 120Ω termination resistor is often recommended across A and B at the ends of the bus.
- Protection: Driver outputs include current limiting and thermal shutdown for overload protection.
- Form factor: Breadboard‑friendly 0.1 inch (2.54mm) pin spacing. Many boards also include an on‑board 2‑pin screw terminal for the A/B bus lines.
- Typical dimensions: ~45mm × 15mm (varies by manufacturer).

#### Direction control (DE / RE)

The MAX485 has two independent enable pins: DE (driver enable, active HIGH) and RE (receiver enable, active LOW). All four combinations are valid and each is useful:

| DE | RE | Driver | Receiver | Use |
|----|----|--------|----------|-----|
| LOW | LOW | off | on | Normal receive (idle state) |
| HIGH | LOW | on | on | Transmit + self-echo: you read back your own bytes, useful for collision detection on a multi-master bus or as a single-node loopback self-test |
| HIGH | HIGH | on | off | Transmit, receiver off (no echo) |
| LOW | HIGH | off | off | Shutdown: lowest quiescent draw, RO high‑impedance, bus fully released |

Two wiring styles exist:

- **Tied (single-pin control).** DE and RE are jumpered together and driven by one MCU pin: HIGH = transmit, LOW = receive. This only covers the receive and transmit-deaf states. It saves a GPIO and is all most half‑duplex setups need.
- **Separate (dual-pin control).** DE and RE are driven by two MCU pins, unlocking self-echo monitoring and shutdown mode.

> **WARNING: never mix the two.** Firmware that drives DE and RE as separate pins (e.g. the [CSE_ArduinoRS485 library](https://github.com/CIRCUITSTATE/CSE_ArduinoRS485), which asserts DE and RE oppositely) must NOT be used on hardware where DE and RE are tied together: it will drive one MCU pin HIGH against the other LOW through the tie, which is a sustained **pin-to-pin short** for the whole transmission. Conversely, single-pin firmware leaves a separately-wired RE floating. Match the firmware to the wiring. Our [test code](../../firmware/max485-test/main.cpp) uses manual single-pin-style control (both direction pins per module always driven to the SAME value) because our modules have DE and RE tied. **This might be the cause as to why previous implementations wasn't working**; the pins were soldered together but the library was used.
