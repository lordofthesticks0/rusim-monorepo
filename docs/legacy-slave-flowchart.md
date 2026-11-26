# Legacy slave flowcharts (`legacy/slave/new-slave.cpp`)

Scope: flowcharts of the legacy slave firmware: one general figure (Figure 0) and
detailed figures per function group (Figures 1 to 11). Behaviour
is described in [legacy-slave.md](legacy-slave.md); this document only draws it.
Diagram labels are in Indonesian because they are meant for the project report.

Source of truth: `legacy/slave/new-slave.cpp` as of commit `4cc2697`. If the code
changes, update the Mermaid block below and re-render (section 3).

## 1. How to read the diagrams

| Shape | Meaning |
|---|---|
| Rounded box | Start, end, or return |
| Rectangle | A step |
| Rectangle with double side bars | A call to another function |
| Diamond | A decision |

The firmware has one rule that explains most of the diagrams: every long
operation calls `serviceMasterPing()`. Sensors are sampled continuously into
cached arrays, and a PING never starts a measurement. It only sends the cache.

## 2. Figures

| Figure | Function(s) | Image |
|---|---|---|
| 0 | General algorithm (one-page summary for the report) | `img/legacy-slave-flowchart/00-general` |
| 1 | `setup()` and `loop()` | `img/legacy-slave-flowchart/01-overview` |
| 2 | `serviceMasterPing()`, `handlePingStream()` | `img/legacy-slave-flowchart/02-ping-handler` |
| 3 | `serviceUsbTelemetry()`, `masterPollImminent()` | `img/legacy-slave-flowchart/03-usb-stream` |
| 4 | `sendTelemetryJson()` | `img/legacy-slave-flowchart/04-json-telemetry` |
| 5 | `updatePhAndTemp()` | `img/legacy-slave-flowchart/05-ph-temperature` |
| 6 | `readPhRaw()`, `convertPhCount()` | `img/legacy-slave-flowchart/06-ph-conversion` |
| 7 | `updateAllMethane()` | `img/legacy-slave-flowchart/07-methane` |
| 8 | `updatePressure()` | `img/legacy-slave-flowchart/08-pressure` |
| 9 | `initPressureSensor()`, `waitHx711Ready()` | `img/legacy-slave-flowchart/09-hx711-init` |
| 10 | `readCo2Ppm()` | `img/legacy-slave-flowchart/10-co2-pwm` |
| 11 | `waitForLevel()` | `img/legacy-slave-flowchart/11-co2-wait-for-level` |

Each image exists as `.svg` (vector, for the report) and `.png` (3x scale).

### Figure 0: general algorithm

```mermaid
%% name: 00-general
flowchart TD
  A([Mulai]) --> B["Inisialisasi<br/>Serial USB dan RS485 (9600 baud)<br/>kurva kalibrasi metana<br/>sensor suhu dan HX711"]
  B --> C{"Ada permintaan<br/>#35;1:PING* dari master?"}
  C -- Ya --> D["Kirim data terakhir (cache)<br/>dalam format JSON"]
  C -- Tidak --> E
  D --> E["Baca pH, suhu, dan metana<br/>semua botol, simpan ke cache"]
  E --> F["Botol 1"]
  F --> G["Baca tekanan (HX711)"]
  G --> H["Baca CO2 (sinyal PWM)"]
  H --> I["Perbarui pH, suhu, metana"]
  I --> J{"Botol 4<br/>selesai?"}
  J -- Tidak --> K["Botol berikutnya"]
  K --> G
  J -- Ya --> C
  N["Catatan: PING juga dilayani<br/>di tengah setiap pembacaan sensor"] -.-> E
```

### Figure 1: setup() and loop()

```mermaid
%% name: 01-overview
flowchart TD
  A([Mulai / Reset]) --> B["captureResetCause: simpan MCUSR<br/>ke resetCause sebelum main()"]
  B --> C["setup(): Serial dan Serial1<br/>9600 baud"]
  C --> D["Atur pin pH, metana, CO2 sebagai INPUT<br/>Hitung kurva metana tiap sensor<br/>(fitExponent dan rs_at_max)"]
  D --> E["Mulai 3 bus DS18B20 tanpa menunggu<br/>requestTemperatures()"]
  E --> F[["initPressureSensor() untuk 4 botol (HX711)"]]
  F --> G["Cetak status ready ke USB"]
  G --> H[["serviceMasterPing()"]]
  H --> I[["serviceUsbTelemetry()"]]
  I --> J[["updatePhAndTemp()"]]
  J --> K[["updateAllMethane()"]]
  K --> L["i = 0"]
  L --> M[["serviceMasterPing()"]]
  M --> N[["updatePressure(i)"]]
  N --> O[["serviceUsbTelemetry()"]]
  O --> P[["serviceMasterPing()"]]
  P --> Q[["readCo2Ppm(i)"]]
  Q --> R{"Berhasil?"}
  R -- Ya --> S["cachedCo2[i] = pembulatan ppm"]
  R -- Tidak --> T{"PING dilayani saat<br/>pengukuran CO2?"}
  T -- Ya --> U["Pertahankan nilai lama"]
  T -- Tidak --> V["cachedCo2[i] = -1"]
  S --> W[["serviceUsbTelemetry()"]]
  U --> W
  V --> W
  W --> X[["updatePhAndTemp()"]]
  X --> Y[["updateAllMethane()"]]
  Y --> Z{"i < 3?"}
  Z -- Ya --> L2["i = i + 1"]
  L2 --> M
  Z -- Tidak --> H
```

### Figure 2: serviceMasterPing() and handlePingStream()

```mermaid
%% name: 02-ping-handler
flowchart TD
  A(["serviceMasterPing()"]) --> B[["handlePingStream(Serial)"]]
  B --> C[["handlePingStream(Serial1)"]]
  C --> Z1([Kembali])

  S(["handlePingStream(port)"]) --> D{"Ada byte masuk<br/>di port?"}
  D -- Tidak --> R1([Return])
  D -- Ya --> E["Baca byte maksimal 50 ms ke buffer 16 byte<br/>berhenti pada tanda *<br/>Jika port = Serial: usbHostSeen = true,<br/>lastUsbHostMs = sekarang"]
  E --> F{"Ada penanda awal #35;<br/>dan titik dua setelahnya?"}
  F -- Tidak --> R2([Return])
  F -- Ya --> G["targetID = angka setelah #35;<br/>command = teks setelah titik dua"]
  G --> H{"targetID = 1 dan<br/>command = PING?"}
  H -- Tidak --> R3([Return])
  H -- Ya --> I{"port = Serial1<br/>(RS485)?"}
  I -- Ya --> J["Hitung selang dari PING sebelumnya<br/>Jika 0,5 s sampai 60 s: masterPeriodMs = selang<br/>lastMasterPingMs = sekarang"]
  I -- Tidak --> K
  J --> K[["sendTelemetryJson(port)"]]
  K --> R4([Return])
```

### Figure 3: serviceUsbTelemetry() and masterPollImminent()

```mermaid
%% name: 03-usb-stream
flowchart TD
  A(["serviceUsbTelemetry()"]) --> B{"Sudah 5000 ms sejak<br/>cetak USB terakhir?"}
  B -- Tidak --> R1([Return])
  B -- Ya --> C{"Host USB pernah mengirim byte<br/>dalam 30 s terakhir?"}
  C -- Tidak --> R2([Return, tidak ada yang membaca])
  C -- Ya --> D[["masterPollImminent()"]]
  D --> E{"PING master<br/>segera datang?"}
  E -- Ya --> R3([Return, tunda tanpa<br/>mengubah lastUsbStreamMs])
  E -- Tidak --> F["lastUsbStreamMs = sekarang"]
  F --> G[["sendTelemetryJson(Serial)"]]
  G --> R4([Return])

  M(["masterPollImminent()"]) --> M1{"masterPeriodMs = 0?<br/>(belum ada 2 PING)"}
  M1 -- Ya --> MF([false])
  M1 -- Tidak --> M2{"Master diam lebih dari<br/>3 periode?"}
  M2 -- Ya --> MF
  M2 -- Tidak --> M3{"PING berikutnya jatuh tempo dalam 600 ms<br/>atau lewat tidak lebih dari 150 ms?"}
  M3 -- Ya --> MT([true])
  M3 -- Tidak --> MF
```

### Figure 4: sendTelemetryJson()

```mermaid
%% name: 04-json-telemetry
flowchart TD
  A(["sendTelemetryJson(port)"]) --> B["Buat StaticJsonDocument 1536 byte"]
  B --> C["Tulis rst, up_ms, free_ram<br/>sebelum array bottles"]
  C --> D["Buat array bottles"]
  D --> E["Untuk botol 1 sampai 4, isi dari cache:<br/>id, ph, ph_raw, methane,<br/>temp, co2, press"]
  E --> F["serializeJson ke port<br/>lalu println"]
  F --> G([Selesai])
```

### Figure 5: updatePhAndTemp()

```mermaid
%% name: 05-ph-temperature
flowchart TD
  A(["updatePhAndTemp()"]) --> B["i = 0"]
  B --> C[["readPhRaw(i)"]]
  C --> D["cachedPhRaw[i] = rata-rata mentah<br/>cachedPh[i] = convertPhCount(rata-rata)"]
  D --> E[["serviceMasterPing()"]]
  E --> F{"i < 3?"}
  F -- Ya --> B2["i = i + 1"]
  B2 --> C
  F -- Tidak --> G{"Sudah 800 ms sejak<br/>requestTemperatures terakhir?"}
  G -- Tidak --> Z([Selesai])
  G -- Ya --> H["j = 0"]
  H --> I["suhu = getTempC(alamat ROM botol j)"]
  I --> J{"suhu < -50 atau suhu = 85?"}
  J -- Ya --> K["cachedTemp[j] = -999"]
  J -- Tidak --> L["cachedTemp[j] = suhu"]
  K --> M[["serviceMasterPing()"]]
  L --> M
  M --> N{"j < 3?"}
  N -- Ya --> H2["j = j + 1"]
  H2 --> I
  N -- Tidak --> O["requestTemperatures() pada 3 bus<br/>lastTempReqMs = sekarang"]
  O --> Z
```

### Figure 6: readPhRaw() and convertPhCount()

```mermaid
%% name: 06-ph-conversion
flowchart TD
  A(["readPhRaw(i)"]) --> B["Baca ADC 1 kali dan buang<br/>(sisa muatan multiplexer)"]
  B --> C["s = 0"]
  C --> D["Baca ADC, jumlahkan,<br/>catat nilai terkecil dan terbesar<br/>delay 5 ms"]
  D --> E[["serviceMasterPing()"]]
  E --> F{"s < 19?"}
  F -- Ya --> C2["s = s + 1"]
  C2 --> D
  F -- Tidak --> G["rata-rata = (jumlah - terkecil - terbesar) / 18"]
  G --> H([Return rata-rata mentah 0 sampai 1023])

  P(["convertPhCount(count)"]) --> Q{"count tidak lebih dari 0, NaN,<br/>atau count ≥ 1023?"}
  Q -- Ya --> X([Return -1])
  Q -- Tidak --> R["pH = 7 + PH_SIGN × (count - 512) × 14/1023<br/>PH_SIGN = -1"]
  R --> S{"pH tidak finite?"}
  S -- Ya --> X
  S -- Tidak --> T["Batasi 0 sampai 14<br/>bulatkan 2 desimal"]
  T --> U([Return pH])
```

### Figure 7: updateAllMethane()

```mermaid
%% name: 07-methane
flowchart TD
  A(["updateAllMethane()"]) --> B["sum dan clipped semua botol = 0"]
  B --> C["k = 0"]
  C --> D["Untuk tiap botol: buang 1 bacaan,<br/>baca ADC, jumlahkan<br/>raw ≤ 0 atau ≥ 1023 menandai clipped"]
  D --> E["delay 2 ms"]
  E --> F[["serviceMasterPing()"]]
  F --> G{"k < 31?"}
  G -- Ya --> C2["k = k + 1"]
  C2 --> D
  G -- Tidak --> H["i = 0"]
  H --> I{"clipped, RL tidak valid,<br/>atau kalibrasi gagal?"}
  I -- Ya --> X["cachedMethane[i] = -1"]
  I -- Tidak --> J["raw_avg = sum / 32<br/>Rs = (1024 / raw_avg - 1) × RL"]
  J --> K{"Rs tidak valid atau<br/>Rs < rs_at_max?"}
  K -- Ya --> X
  K -- Tidak --> L{"Rs = rs_at_max?"}
  L -- Ya --> M["cachedMethane[i] = 10000"]
  L -- Tidak --> N["ppm = PPM_SPAN × (RS_SPAN / Rs) ^ exponent"]
  N --> O{"ppm tidak valid<br/>atau ppm > 10000?"}
  O -- Ya --> X
  O -- Tidak --> P["cachedMethane[i] = ppm"]
  X --> Q{"i < 3?"}
  M --> Q
  P --> Q
  Q -- Ya --> H2["i = i + 1"]
  H2 --> I
  Q -- Tidak --> Z([Selesai])
```

### Figure 8: updatePressure()

```mermaid
%% name: 08-pressure
flowchart TD
  A(["updatePressure(idx)"]) --> B{"Sensor sudah<br/>diinisialisasi?"}
  B -- Tidak --> C[["initPressureSensor(idx)"]]
  C --> D{"Berhasil?"}
  D -- Tidak --> E["cachedPress[idx] = -1<br/>dicoba lagi pada pemanggilan berikutnya"]
  E --> Z([Return])
  D -- Ya --> D2["pressInitialized[idx] = true"]
  D2 --> F
  B -- Ya --> F[["waitHx711Ready(idx, 250 ms)"]]
  F --> G{"HX711 siap?"}
  G -- Ya --> H["get_units(5) × 100<br/>cachedPress[idx] = hasil dalam Pa"]
  G -- Tidak --> I["Pertahankan nilai lama"]
  H --> Z
  I --> Z
```

### Figure 9: initPressureSensor() and waitHx711Ready()

```mermaid
%% name: 09-hx711-init
flowchart TD
  A(["initPressureSensor(idx)"]) --> B["begin(DOUT, SCK, doReset = false)<br/>tanpa reset agar tidak macet<br/>saat HX711 tidak terpasang"]
  B --> C[["waitHx711Ready(idx, 1500 ms)"]]
  C --> D{"Siap?"}
  D -- Tidak --> E([Return false])
  D -- Ya --> F["set_scale(CALIBRATION_FACTORS[idx])"]
  F --> G["tare(3): rata-rata 3 bacaan<br/>menjadi titik nol"]
  G --> H([Return true])

  W(["waitHx711Ready(idx, timeout)"]) --> W1{"is_ready()?"}
  W1 -- Ya --> WT([Return true])
  W1 -- Tidak --> W2[["serviceMasterPing()"]]
  W2 --> W3{"Melewati timeout?"}
  W3 -- Ya --> WF([Return false])
  W3 -- Tidak --> W4["delay 5 ms"]
  W4 --> W1
```

### Figure 10: readCo2Ppm()

```mermaid
%% name: 10-co2-pwm
flowchart TD
  A(["readCo2Ppm(pin)"]) --> B["pingHandledDuringCo2 = false"]
  B --> C[["waitForLevel(LOW)"]]
  C --> D[["waitForLevel(HIGH)"]]
  D --> E["tRise = micros()"]
  E --> F[["waitForLevel(LOW)"]]
  F --> G["tFall = micros()"]
  G --> H[["waitForLevel(HIGH)"]]
  H --> I["tRise2 = micros()"]
  I --> J["high = tFall - tRise<br/>low = tRise2 - tFall<br/>periode = high + low"]
  J --> K{"periode 950 sampai 1060 ms<br/>dan high 1 sampai 1002 ms?"}
  K -- Tidak --> X([Return false])
  K -- Ya --> L["ppm = 5000 × (high - 2) / (periode - 4)<br/>jika ppm < 0 maka ppm = 0"]
  L --> Y([Return true])
  C -. timeout .-> X
  D -. timeout .-> X
  F -. timeout .-> X
  H -. timeout .-> X
```

### Figure 11: waitForLevel()

```mermaid
%% name: 11-co2-wait-for-level
flowchart TD
  A(["waitForLevel(pin, level)"]) --> B{"Level pin = level?"}
  B -- Ya --> T([Return true])
  B -- Tidak --> C{"Ada byte di<br/>Serial atau Serial1?"}
  C -- Ya --> D[["serviceMasterPing()"]]
  D --> E["pingHandledDuringCo2 = true"]
  E --> F
  C -- Tidak --> F{"Sudah lebih dari 2 s?"}
  F -- Ya --> X([Return false])
  F -- Tidak --> B
```

## 3. Regenerating the images

Images are rendered from the Mermaid blocks above with `@mermaid-js/mermaid-cli`
through Bun. The renderer needs a Chromium browser; on Windows, Edge works. Each
block starts with `%% name: NN-name`, which becomes the output file name.

```bash
PUPPETEER_SKIP_DOWNLOAD=1 bunx @mermaid-js/mermaid-cli \
  -p puppeteer.json -b white -s 3 -i NN-name.mmd -o NN-name.png
```

`puppeteer.json` points `executablePath` at the browser. The same command with
`-o NN-name.svg` produces the vector file.

---
Updated at 2026-11-24-22:10.
