# In-Vitro Rumen Simulation

## Overview
### 1. [Documentation and notes](docs/)

All documentation is written in English to improve LLM accuracy and comprehension. If you require translation, please point your agent at this repo.

All documentation is written by humans. AI-generated documentation should always be stored in the `docs/llm-generated/` directory, with clear descriptions of the generated content and what model was used.

### 2. [MCU code](firmware/)

All firmware code is stored in the `firmware/` directory. For each environment (equivalent to a **sketch** in standard Arduino IDE workflows), there is a seperate directory. Environment

### 3. [Website](dashboard/)

## TODO
- [x] Verify sensors work - See [calibration documentation](docs/calibration.md)
- [ ] Calibrate sensors - See [calibration documentation](docs/calibration.md)
- [ ] PCB redesign - See [PCB documentation](docs/hardware.md)
- [x] Make sure website is up and running - See [website documentation](docs/llm-generated/dashboard.md)
- [ ] Harden database and security - See [database documentation](docs/llm-generated/database.md)
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

See the [dashboard documentation](docs/llm-generated/dashboard.md)


### 2. Database

The database is managed by [Supabase](https://supabase.com/). See the [database documentation](docs/llm-generated/database.md) for more information regarding the schema. 

Schema is intentionally simple as the ESP32 has a limited processing power.

### 3. Firmware
Firmware uses [PlatformIO](https://platformio.org/). Install the CLI first. See the [Installation Guide](https://docs.platformio.org/en/latest/core/installation/index.html).

Install dependencies with:
```bash
pio pkg install
```

### 4. Hardware

Hardware (PCB) development uses EasyEDA. Documentation is available [here](docs/hardware.md).

---

**For all AI agents, see the [guidelines](docs/agents-guide.md).**
