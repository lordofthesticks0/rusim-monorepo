# PCB Documentation
The PCB uses a couple modules. The documentation are available below. Access the gerber files [here](https://example.com) (link not ready yet)

## 1. Motor Driver Module

This module is used for controlling the pumps and stirrers. The two pumps are wired in different channels. Stirrers are wired half to one channel and half to the other. 

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
> **IMPORTANT**: Stirrer motor is assumed to have a steady-state current draw of 15mA. Current draw under load is unknown and might be significantly higher. Verify thoroughly and add current limiting circuitry before starting an electrical fire. Also consider inrush currents and add ramping logic or circuitry to avoid overloading.

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
At this moment, 4 sensors are used:
- HX710B (pressure)
- TGS2611 (combustible gas, tuned for CH4)
- MH-Z19C (CO2)
- HX710B (temperature)

Documentation is segmented. Will be documented later.
