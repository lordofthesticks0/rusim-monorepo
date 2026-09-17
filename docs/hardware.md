# PCB Documentation
The PCB uses a couple modules. The documentation are available below. Access the gerber files [here](https://example.com) (link not ready yet)

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
| LLM Generated Summary | [wt32-eth01-LLM.md](llm-summary/wt32-eth01-LLM.md) |

## 3. ESP32-8048S050

This module is a ESP32 display module made by Sunton. Documentation is scarce and difficult to find. See the LLM generated summary [here](llm-summary/sunton-LLM.md).

## 4. Sensors

Currently, 5 sensors are used. See [calibration documentation](calibration.md) for calibration details.


### 4.1 HX710B (pressure)

This is a piezoelectric differential sensor. Uses a similar protocol to I2C for communication. Clock speeds are far lower. Depends on a [HX711 library](https://www.arduinolibraries.info/libraries/hx711). The actual HX710B chip is the name for the ADC IC. Not sure what the whole module is named. 

#### Pinout

| Module | Pin | Description |
| --- | --- | --- | 
| 1 | VCC / VIN | Power supply input. Accepts 3.3V to 5V DC. |
| 2 | GND | Ground connection. |
| 3 | OUT / DOUT / DATA | Digital data output from the HX710B ADC. Connect this to a digital input on your microcontroller. |
| 4 | SCK / CLK / SLC | Serial clock input. Connect this to a digital output on your microcontroller to clock out the data. |

### 4.2 TGS2611 (CH4)

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

### 4.3 MH-Z19C (CO2)

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


### 5. PH-402C (pH)

The sensor is a pH probe that allows you to connect a BNC glass electrode to something accurately measured. Calibration is a pain. If issues were to be encountered in the future, try replacing this with something.

#### Pinout
| Pin | Name | Description |
| --- | --- | --- | --- |
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

---

Last updated: 17/09/2026
