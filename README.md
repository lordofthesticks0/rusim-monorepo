# In-Vitro Rumen Simulation

## Overview
- [Documentation and notes](docs/)
- [MCU code](firmware/)

## TODO
- [x] Verify sensors work -- See [calibration documentation](docs/calibration.md)
- [ ] Calibrate sensors -- See [calibration documentation](docs/calibration.md)
- [ ] PCB redesign -- See [PCB documentation](docs/PCB.md)
- [ ] Integrate to data collection system

## Stack
### 1. Dashboard (Frontend)
TBD
### 2. Dashboard (Backend)
TBD
### 3. Firmware
Firmware uses [PlatformIO](https://platformio.org/). Install the CLI first. See the [Installation Guide](https://docs.platformio.org/en/latest/core/installation/index.html).

Install dependencies with:
```bash
pio pkg install
```

### 4. Hardware

Hardware (PCB) development uses EasyEDA. Documentation is available [here](docs/hardware.md).
---

**Last Updated: 17/09/2025**
