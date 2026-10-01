
# WT32-ETH01 — LLM Documentation

## 1. Overview

The **WT32-ETH01** is an embedded serial-to-Ethernet module based on the ESP32 series, launched by **Wireless-tag Technology Co., Ltd.** (also known as 启明云端 / Qiming Cloud). It integrates an optimized TCP/IP protocol stack, making it easy to add networking capability to embedded devices with minimal development effort.

It is effectively a small, cheap ESP32 development board with **Ethernet, Wi‑Fi, Bluetooth, and GPIO pins**. While it ships with AT‑command firmware for serial‑to‑Ethernet conversion, most developers flash their own programs and use standard ESP32 networking libraries.

**Key use cases:** IoT gateways, Modbus TCP controllers, remote sensor nodes, web servers, and any application requiring a reliable wired network connection combined with wireless fallback.

---

## 2. Key Specifications

| Category | Details |
|---|---|
| **SoC** | ESP32‑D0WD (dual‑core Xtensa LX6, up to 240 MHz) |
| **Flash** | 4 MB (32 Mbit) onboard SPI flash |
| **Ethernet PHY** | LAN8720A via RMII, using ESP32’s internal EMAC |
| **Ethernet Port** | RJ45, 10/100 Mbps, auto‑MDIX (cross‑direct adaptive) |
| **Wi‑Fi** | 802.11 b/g/n/e/i (2.4 GHz, up to 150 Mbps) |
| **Bluetooth** | v4.2 BR/EDR and BLE |
| **Operating Voltage** | 3.3 V **or** 5 V (binary choice; do not supply both simultaneously) |
| **Operating Current** | Average ~80 mA; minimum supply current 500 mA |
| **Serial Baud Rate** | 80 – 5,000,000 bps |
| **Operating Temperature** | −40 °C to +85 °C |
| **Dimensions** | Compact board with 2×13 through‑holes and castellated half‑pads |
| **Certifications** | FCC / CE / RoHS |



---

## 3. Pinout (WT32‑ETH01 & WT32‑ETH02)

The original WT32‑ETH01 and the WT32‑ETH02 share the **same 2×13 pin layout**. The silkscreen on the ETH02 labels some pins by their AT‑firmware function rather than GPIO number, but the physical signals are identical.

### 3.1 Debug / Programming Header (6 pins)

| Pin | Name | Description |
|---|---|---|
| 1 | EN | Active‑high enable (reset) |
| 2 | GND | Ground |
| 3 | 3V3 | 3.3 V power |
| 4 | TXD | IO1, TXD0 (serial output) |
| 5 | RXD | IO3, RXD0 (serial input) |
| 6 | IO0 | Boot mode select (pull low to program) |



### 3.2 Main IO Header (2×10 pins)

| Pin | GPIO | Notes |
|---|---|---|
| CFG | IO32 | ADC1_CH4 |
| 485_EN | IO33 | ADC1_CH5 |
| TXD (AT) | IO17 | ADC2_CH6 |
| RXD (AT) | IO5 | ADC2_CH3; boot strapping (debug messages) |
| IO39 | IO39 | **Input only**, ADC1_CH3 |
| IO36 | IO36 | **Input only**, ADC1_CH0 |
| IO15 | IO15 | Boot strapping (SD card timing) |
| IO14 | IO14 | ADC2_CH6 |
| IO12 | IO12 | **Must float at boot** (MTDI) |
| IO35 | IO35 | **Input only**, ADC1_CH7 |
| IO4 | IO4 | ADC2_CH0 |
| IO2 | IO2 | **Must float at boot**; Ethernet link LED |
| LINK | — | Ethernet status LED |

**Power pins:** 3V3, 5V, GND (multiple)



### 3.3 Critical Pin Warnings

- **IO0:** At boot, must be pulled low to enter programming mode; must float or be pulled high to boot normally. After boot, it receives the 50 MHz Ethernet clock (enabled by IO16). **Best avoided for other uses.**
- **IO1 / IO3:** ESP32 serial output/input used for programming and debug. **Best avoided.**
- **IO2:** At boot, must float or be pulled low to program. Can be used after boot but ensure nothing pulls it high during boot.
- **IO12 (MTDI):** At boot, must float or be pulled low, or the chip will not work. Can be used after boot.
- **IO5 / IO15 (MTDO):** IO5 controls debug messages at boot; IO5 + IO15 together control SD card timing. Can be used but be aware of boot‑time effects.
- **IO35, IO36, IO39:** Input‑only pins (no internal pull‑up/down), otherwise free to use.



---

## 4. Product Variants

Wireless‑Tag sells several boards under similar names. **They are not all compatible.**

| Feature | **WT32‑ETH01** (original) | **WT32‑ETH02 / ‑PLUS** | **WT32‑ETH01‑EVO** |
|---|---|---|---|
| **SoC** | ESP32‑D0WD (dual‑core Xtensa) | ESP32‑SOLO‑1 / S0WD (single‑core Xtensa) | ESP32‑C3 (single‑core RISC‑V) |
| **Max Clock** | 240 MHz | 160 MHz | 160 MHz |
| **Flash** | 4 MB | 16 MB | 4 MB |
| **Ethernet** | LAN8720A PHY via RMII | LAN8720A (same as ETH01) | DM9051NP MAC+PHY via SPI |
| **PoE** | No | ‑PLUS variant adds IEEE 802.3af PoE | Reserved PoE pads |
| **Pinout** | 2×13 layout | Same 2×13 layout | **Different 2×15 layout** |
| **Wi‑Fi / BT** | 2.4 GHz WiFi + BT classic + LE | 2.4 GHz WiFi + BT classic + LE | 2.4 GHz WiFi + BLE only |

**Important:** The **WT32‑ETH01‑EVO** is neither hardware nor software compatible with the original ETH01. It uses a different SoC family, a different Ethernet chip, and a different pinout.

---

## 5. Power Supply

- **Dual‑voltage input:** You may supply **3.3 V** on the `3V3` pins **or 5 V** on the `5V` pins — **but never both at once.**
- **On‑board regulator:** The schematic lists an “LM1117F‑1.8V” regulator, but the actual output is **3.3 V** (this is a known documentation error).
- **Power supply modes:**
  - Through‑hole pins (Dupont wires or breadboard)
  - Half‑pad soldering directly to a carrier board
- **Current:** Ensure your supply can deliver at least **500 mA** (average consumption is ~80 mA, but peaks are higher).



---

## 6. Software & Development

### 6.1 Supported Environments

| Environment | Support |
|---|---|
| **Arduino IDE** | Official variant in `arduino-esp32` (`variants/wt32-eth01`) |
| **ESP‑IDF** | Fully supported (ESP32 target) |
| **PlatformIO** | Supported (board: `wt32-eth01`) |
| **Tasmota** | Template available |
| **Rust (no_std)** | `esp-hal` examples with `Wt32Eth01` helpers |



### 6.2 Arduino Libraries (WT32‑ETH01 specific)

- **`WebServer_WT32_ETH01`** — Simple Ethernet WebServer / HTTP(S) client wrapper for LAN8720.
- **`AsyncWebServer_WT32_ETH01`** — Asynchronous HTTP and WebSocket server library.
- **`AsyncUDP_WT32_ETH01`** — Fully asynchronous UDP library.
- **`AsyncDNSServer_WT32_ETH01`** — Asynchronous DNS server.

These libraries are compatible with the `esp32` Arduino architecture.

### 6.3 Factory AT Firmware

The module ships with firmware that accepts **AT commands** over 3.3 V serial for serial‑to‑Ethernet conversion. Common commands include:

| Command | Function |
|---|---|
| `AT` | Test startup |
| `AT+RST` | Restart module |
| `AT+GMR` | Query firmware version |
| `AT+RESTORE` | Restore factory defaults |
| `AT+UART_DEF` | Set default UART configuration (saved in flash) |
| `AT+CIFSR` | Get IP address |
| `AT+CIPSTART` | Establish TCP/UDP connection |
| `AT+CIPSEND` | Send data |
| `AT+PASSCHANNEL` | Configure transparent transmission mode |



### 6.4 Programming Procedure

1. Connect a USB‑to‑TTL adapter to the debug header (TXD, RXD, GND, 3V3/5V).
2. Pull **IO0 low** (connect to GND) during reset to enter programming mode.
3. Release IO0 and reset normally to run your firmware.
4. Use `esptool.py`, Arduino IDE, or ESP‑IDF flashing tools.



---

## 7. Ethernet Configuration (Arduino Example)

```cpp
#include <ETH.h>

void setup() {
  Serial.begin(115200);
  ETH.begin();
  ETH.setHostname("wt32-eth01");
  // Optional static IP:
  // ETH.config(IPAddress(192,168,1,50), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
}

void loop() {
  if (ETH.linkUp()) {
    Serial.print("IP: "); Serial.println(ETH.localIP());
    delay(5000);
  }
}
```

The LAN8720 PHY address is **1**, and the PHY type is `PHY_LAN8720`. The RMII clock is provided by an external 50 MHz oscillator enabled via **IO16**.

---

## 8. Summary of Key Gotchas

- **Do not power from both 3V3 and 5V simultaneously.**
- **IO0, IO1, IO3, IO12** have critical boot‑time constraints — avoid using them for other functions if possible.
- **IO35, IO36, IO39** are input‑only.
- **The WT32‑ETH01‑EVO is a completely different board** — do not assume compatibility.
- **Documentation from Wireless‑Tag is sparse**; the community‑maintained GitHub repository by `egnor` is the most reliable unofficial reference.

---

## 9. Official Documentation Links

| Document | Link |
|---|---|
| Datasheet V1.4 (EN) | [WT32‑ETH01_datasheet_V1.4-en.pdf](https://en.wireless-tag.com/product-item-2.html) |
| Getting Started Guide | [Getting+Started+Guide+for+WT32‑ETH01.pdf](https://en.wireless-tag.com/product-item-2.html) |
| Unofficial Community Guide | [github.com/egnor/wt32-eth01](https://github.com/egnor/wt32-eth01) |
| Arduino Variant | [pins_arduino.h](https://github.com/espressif/arduino-esp32/tree/master/variants/wt32-eth01) |
---
Content is generated by `deepseek/deepseek-v4.1-flash`. Full conversation is available [here](https://chat.deepseek.com/share/2i9ei17mrjvbtzbgpw).
