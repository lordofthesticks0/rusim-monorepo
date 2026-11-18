# In-Vitro Rumen Simulation

## Overview
### 1. [Documentation and notes](docs/)

All documentation is written in English to improve LLM accuracy and comprehension. If you require translation, please point your agent at this repo.

All documentation lives in `docs/`. Feel free to check it.
Need a quick start? See the [index](docs/index.md).

### 2. [MCU code](firmware/)

All firmware code is stored in the `firmware/` directory. For each environment (equivalent to a **sketch** in standard Arduino IDE workflows), there is a seperate directory. Every environment can be declared, from the board used to the programs and dependencies compiled in the [PlatformIO settings file](platformio.ini).

The firmware will eventually push data to the database.

### 3. [Website](dashboard/)

The dashboard is a React Single-Page Application with TypeScript and the Bun runtime to provide a remote semi-realtime monitoring of the ongoing experiment. See [website documentation](docs/dashboard.md) for more details. All dashboard code should not exit the `dashboard/` directory.

Data is pulled from the database. There are two tables in the schema as described in the [database documentation](docs/database.md).

For access, notify me if you want env files.

## TODO
- [x] Verify sensors work - See [calibration documentation](docs/calibration.md)
- [ ] Calibrate sensors - See [calibration documentation](docs/calibration.md)
- [ ] PCB redesign - See [PCB documentation](docs/hardware.md)
- [x] Make sure website is up and running - See [website documentation](docs/dashboard.md)
- [ ] Harden database and security - See [database documentation](docs/database.md)
- [ ] Integrate to data collection system
- [ ] Write final reports

## Stack
### 1. Dashboard

Dashboard is a React-Vite app, utilizing TypeScript and Bun as the runtime. Clone the repository, then run:

```bash
bun install
bun dev
```

Test the database with
```bash
bun run dev-db # Note: env is required for running the script
```

**Ask for env variables.**

See the [dashboard documentation](docs/dashboard.md)


### 2. Database

The database is managed by [Supabase](https://supabase.com/). See the [database documentation](docs/database.md) for more information regarding the schema. 

Schema is intentionally simple as the ESP32 has a limited processing power.

### 3. Firmware
Firmware uses [PlatformIO](https://platformio.org/). Install the CLI first. See the [Installation Guide](https://docs.platformio.org/en/latest/core/installation/index.html).

Install dependencies with:
```bash
pio pkg install
```

### 4. Hardware

Hardware (PCB) development uses EasyEDA. Documentation is available [here](docs/hardware.md).
