# ESP to database sync — plan

> **Status: approved plan (2026-10-16), re-scoped (2026-10-16), steps 1, 2, 3, 7, 8 built (2026-10-16).** Decisions in
> §6 are resolved. §8 is the build checklist.
>
> **Sensor chain deferred.** The Mega Mini sampling firmware, the WT32 RS485
> master, and the display sensor registers (§8 steps 4–6) are out of the current
> build order and will be implemented in a later pass. Nothing in the current
> order samples sensors; the write path is written against a future sensor
> source and stays dormant until that pass lands.

Scope: how the WT32-ETH01 (`firmware/network-master`) pulls configuration
and pushes sensor readings to Supabase, what already exists, and what is missing.

`db-sync-experiment-ntp.md` documents the pull path that is already built. This
document covers the whole round trip, including the push path, which does not
exist at any layer. `network-master.md` documents the portal and Modbus
machinery this builds on. `database.md` documents the schema.

## 1. Current state

### Pull: partially built

`firmware/network-master/main.cpp` already authenticates and reads.

| Function | Line | What it does |
|---|---|---|
| `ensureNtpSync()` | 818 | `configTime` against two NTP servers, 10 x 1 s attempts, sets `TIME_OK` (0x0006). Gives up quietly and callers continue. |
| `sbAuthenticate()` | 845 | POSTs the plaintext device token to `/rest/v1/rpc/authenticate`. Keeps the returned 64-hex `session_token` in RAM only. |
| `sbFetchLatestExp()` | 900 | GETs `/rest/v1/readings?select=experiment_id&order=experiment_id.desc&limit=1` with `apikey` and `x-device-session`. Re-authenticates once on 401 or 403. |
| `pushDbState()` | 934 | Writes `DB_LATEST` (0x0005) and `TIME_OK` to the display over Modbus. |

`syncDbLatest()` runs every 30 s in `NEED_CREDS` and `RUN`, and is forced once
after a successful portal login.

The only thing pulled is the latest experiment number, so the display can warn
when a locally chosen number collides with data already in the database.

Two fields already come back from `authenticate()` and are discarded:
`bottle_count`, `owner_name`, and `owner_email`. `sbAuthenticate()` scans the
reply for the first 64-character hex run and keeps only that.

### Push: absent

There is no write path. No firmware function attempts a POST to
`/rest/v1/readings`, no migration grants `INSERT`, and no sensor-reading program
exists.

## 2. The database forbids writes

`dashboard/supabase/migrations/20260930162000_add_device_data_token_auth.sql:209-210`:

```sql
revoke all on table device_data.readings from anon, authenticated;
grant select on table device_data.readings to anon, authenticated;
```

`readings` carries `SELECT` only, and both RLS policies (`:188`, `:194`) are
`for select`. A caller holding a valid session still receives permission denied
on `POST /rest/v1/readings`. This is the hard blocker and it needs a migration.

Note the asymmetry: the browser client is an anonymous reader, and the database
was built that way on purpose. Adding a writer is a new capability, not a
missing grant.

## 3. Nothing reads the sensors

This is the larger gap, and no migration addresses it.

> **Deferred (2026-10-16).** The sensor-side work described in this section is
> not in the current build order. It stays here as the design the later pass
> will follow.

The five sensors are wired to the **ATmega2560 Mega Mini**
(`docs/hardware.md` §5): HX710B pressure, TGS2611 methane, MH-Z19C CO2,
DS18B20 temperature, PH-402C pH.

`firmware/` contains no Mega Mini program that samples on a schedule. What
exists is a few standalone calibration sketches: `co2-calibrate`,
`methane-calibrate`, `pressure-calibrate`, and `temp-probe`.

`firmware/display-v9/fake_data.h` models the values as arriving from the WT32
over UART ("what the WT32 will push over UART every 5 minutes"; `sensorNull`
means "WT32 failed to fetch this field this cycle"). Today an LVGL timer jitters
random numbers into that model.

So both hops of the intended chain are missing:

```
Mega Mini (sensors, 8 KB SRAM)  --RS485 Modbus-->  WT32-ETH01  --HTTPS-->  Supabase
                                                        ^
                                              display (ESP32-S3), Modbus slave
```

Work this implies, none of it started:

1. Mega Mini firmware that samples the five sensors and serves them as a Modbus
   RTU slave. 8 KB SRAM is the binding constraint; five sensors with
   blocking conversion routines will need careful sequencing.
2. A second Modbus master transport on the WT32, on the RS485 bus. The WT32 is
   already master on the TTL UART to the display. Two masters on two physical
   buses is fine, but they are separate code paths.
3. Manual DE/RE direction control on RS485. `docs/hardware.md` §5 warns these
   MAX485 modules have DE and RE tied together, so the CSE_ArduinoRS485 library
   must not be used. The former loopback sketch was removed; verify direction
   control with the final slave and master firmware on the bench.
4. A register block for 24 bottles x 5 sensors. The display to WT32 map
   (`firmware/display-v9/link_modbus.h`) has no sensor registers today.

A compact encoding is 6 registers per bottle: five values plus one null bitmask,
where a set bit means the bottle reported but that sensor produced no value,
which is the distinction `database.md:107` draws. That is 144 registers for all
24 bottles. Modbus FC 0x03 caps at 125 registers per request, so one full poll
needs two requests. At 9600 baud, 144 registers is roughly 300 bytes and about
0.35 s, which is acceptable at a 5-minute cadence.

## 4. Why the device must not send `token_hash`

This came up while choosing the write grant, and the answer settles it for any
authentication change on the device.

The column stores SHA-512 of the plaintext token (`database.md:95`). Sending
the hash as the credential would replace this in `authenticate()`:

```sql
-- now: the client proves knowledge of the token
requested_hash := encode(sha512(convert_to(p_token, 'UTF8')), 'hex');
-- proposed: the client sends the stored value
requested_hash := p_hash;
```

That compiles and works. What it destroys is the reason the column exists.

**A hash that is sent is a credential, not a verifier.** Today the database
holds a one-way function of the secret. If someone dumps
`device_data.devices.token_hash`, they cannot log in; recovering the token means
inverting SHA-512. Under the proposed scheme the same dump is an immediate,
working login for that device, permanently.

**The database stops being the safe place to keep the secret.** Column grants
(:206) already hide `token_hash` from PostgREST, but the value is still in the
same Postgres instance as everything else: Supabase backups, replication, and
anyone with database access. Under the current design, compromising that table
costs nothing. Under the proposed design it costs a device.

**Rotation stops working.** `dev-db.ts:499-507` refuses to assign a hash to a
second device, and offers `--token-hash ""` to clear it so the token can move.
Both rely on the hash being a pure function of a token the operator still holds.
If the hash is the live credential and it leaks, the only fix is to change
`token_hash` itself, which locks out every device presenting the old value.
With plaintext-token auth a leak means the plaintext token leaked, and rotation
is one `bun run dev-db --only token` away.

**Anything that logs a request becomes a credential store.** With the current
scheme the plaintext token appears in an HTTP log or a `request.headers` dump.
With the proposed scheme the permanent credential appears there, and it cannot
be rotated without replacing it everywhere it has been copied.

**`setInsecure()` turns this from a theoretical risk into a certain one.** See
§5. On a campus network the wire credential is readable by anyone on-path. A
leaked plaintext token is rotatable; a leaked hash is not.

If the goal is to remove the bearer token from the device entirely, the
alternative is asymmetric: the device holds a private key, the database holds
the corresponding public key, and the device signs a server-supplied nonce. A
database dump then yields nothing usable and the wire credential is scoped and
non-replayable. The ESP32 can do ECDSA through mbedtls at no flash cost worth
mentioning; the ATmega2560 cannot do it in reasonable firmware size. Provisioning
is the real cost, and the current `token_hash` column would need a companion
public-key column rather than a hash.

## 5. Credential exposure on the WT32

### TLS is unverified

Both Supabase calls use `client.setInsecure()` (`:854` authenticate, `:906`
fetch), as do the four captive-portal calls. The portal genuinely cannot be
verified, because the interception sits above TLS termination
(`captive-portal-workaround.md` §2.4). Supabase is different: it is a known
host with a stable certificate chain.

**Decision taken: pin the Supabase CA.** Replace `setInsecure()` on the Supabase
calls with `setCACert()` and the root certificate. Leave the portal calls alone.
This is the change that makes the device token defensible at all. Without it,
anything on the wired network can read the token off the wire during
authenticate and then hold a permanent credential.

`setCACert()` is strictly better than it looks. In this core
(`framework-arduinoespressif32` 4.20017.260907) it sets `_use_insecure = false`
(`WiFiClientSecure.cpp:272`), and `esp_tls.h:162` documents that a null
`common_name` means the certificate CN must match the hostname. The pin therefore
turns on chain validation *and* hostname checking, which `setInsecure()` disables
together.

The chain observed on 2026-10-16 for `mdjpqjkduyexrlimepmb.supabase.co`:

| Level | Subject | Issuer |
|---|---|---|
| Leaf | `CN=supabase.co` | `Google Trust Services, CN=WE1` |
| Intermediate | `GTS WE1` | `Google Trust Services LLC, CN=GTS Root R4` |
| Root | `GTS Root R4` | `GlobalSign Root CA` (cross-signature) |

Leaf validity ran 2026-08-26 to 2026-11-24, a 90-day automated renewal. SANs are
`supabase.co` and `*.supabase.co`, so the per-project subdomain matches by
wildcard and hostname verification will pass. Check again with:

```bash
openssl s_client -connect <ref>.supabase.co:443 -servername <ref>.supabase.co \
  -showcerts </dev/null 2>/dev/null | grep -E "^ *[0-9]+ s:|^ *i:"
```

### What a pin breaks, and what it does not

| Change | Pin survives |
|---|---|
| Leaf renewal every 90 days | Yes. New leaf, same root. This is the common case. |
| CDN, load balancer, or region change | Yes, while the chain still terminates at a pinned root. |
| Supabase moving to a different CA | Only if that root is in the bundle. |
| The root CA itself retiring | Yes. Public roots do not retire; the CA/B baseline requirements mandate successor certificates with long overlap. |

Pinning a root survives the thing people actually worry about, rotation, because
rotation happens below the root. What breaks a pin is a change of *trust anchor*,
which is rare and slow.

The mitigation is cheap. `setCACert()` takes a PEM string and mbedTLS parses
concatenated blocks, so several roots go in one buffer. Bundle `GTS Root R4`,
`GlobalSign Root CA R3` (the cross-sign above), and `ISRG Root X1` (Let's Encrypt,
the plausible move if Supabase changes provider), at roughly 1 KB each.

Three API notes for whoever implements this:

- `setCACert()` **replaces** the trust store. It does not add to the bundled one,
  so every root above must be passed explicitly.
- Do not reach for `setCACertBundle()`. It loads the full Arduino CRT bundle at
  around 200 KB, which does not fit the ~316 KB of free app partition recorded in
  `network-master.md` §6.
- `verify(fingerprint, domain)` pins a leaf and breaks on every renewal. Wrong
  tool here.

### Failure mode if the pin does break

The handshake fails, `sbAuthenticate()` returns false, and `syncDbLatest()` logs
the failure. Portal login and NTP use separate `setInsecure()` clients and are
unaffected, so the device keeps running and the dashboard simply stops updating.
`DB_LATEST` reverts to `0xFFFF`, which `link_modbus.h` already defines as unknown.
The degradation is soft and visible.

Do not add an automatic fallback to `setInsecure()`. It would silently reopen the
hole being closed, and this failure is the kind that goes unnoticed for weeks.

The residual cost is that a genuine trust-anchor change needs a reflash, and this
firmware has no OTA. Bundling the three roots makes that unlikely; if units ever
leave the lab, OTA becomes worth building before it becomes urgent.

### What Postgres already computes

`authenticate(p_token)` already does this. Line `:113`:

```sql
requested_hash := encode(sha512(convert_to(p_token, 'UTF8')), 'hex');
```

The device never computes SHA-512 and needs no crypto library for authentication.
The plaintext token crosses the wire once, Postgres performs the only hash, and
the stored value is a verifier rather than a credential. That ordering is what
makes a database dump harmless and what keeps §4 from applying, since the hash
never travels. It is the correct arrangement and should not be moved client-side
to save a round trip, as there is no round trip to save.

The same principle carries into the write path, with one exception:

- **Let Postgres supply `device_id`**, from `session_device_id()`. This is what
  the `SECURITY DEFINER` RPC in §6 does, and it removes the identity question
  outright rather than relying on RLS to reject a forged value.
- **Do not let Postgres supply `timestamp`.** It is tempting, since `now()` would
  remove another client-supplied field, but it is wrong for any batch buffered
  while the network was down: rows would carry flush time instead of sample time
  and the time series would lie. `timestamp` is also part of the primary key
  (`database.md:109`), so two buffered samples could collide on it. The device
  sends sample time, already NTP-gated.

### The token is baked into flash

`DEVICE_TOKEN` is a build flag (`main.cpp:116-117`), passed as
`-DDEVICE_TOKEN='"..."'` per `db-sync-experiment-ntp.md`. The plaintext token is
compiled into the firmware image and therefore into any factory image produced
by `scripts-hardware/flash-factory.ts`.

Anyone holding a `.bin` can extract it with `strings` and impersonate the
device to the dashboard. It also means every flashed unit shares one credential
unless each is built separately, which is exactly what `dev-db.ts` enforces
against on the database side: `token_hash` is unique, and the script refuses to
reassign a hash already in use.

This contradicts `network-master.md` §6, which states portal credentials are RAM
only and never written to flash. The portal credentials do honour that. The
device token does not.

**Decision taken: keep the build flag.** It is recorded here with its cost
rather than treated as resolved. Consequences to accept:

- One credential per unit, or a shared credential across all units if images are
  not built per device. The database's uniqueness constraint will reject a
  second device trying to claim it.
- Rotation requires reflashing, not `bun run dev-db --only token`.
- A leaked image discloses the token until every unit is reflashed.

Reconsider this if units leave the lab. If it is revisited, the credential can
be staged over the display Modbus link the same way portal credentials already
are, which costs one register pair and no flash.

### `authenticate()` is unthrottled

The RPC is executable by anyone holding the anon key, which is public by design.
Each call is an indexed equality test on a high-entropy digest, so it is cheap,
and each success writes a `sessions` row. Dead sessions are cleaned on the same
device's next login (`:127-129`), so the table does not grow without bound per
device, but an unauthenticated caller can still drive insert churn across
devices. Low priority for this project.

## 6. Decisions (resolved 2026-10-16)

### 6.1 Write grant: `SECURITY DEFINER` RPC

Decision: new `device_data.push_readings(jsonb)` with `SECURITY DEFINER`.

- Forces `device_id` from `device_data.session_device_id()`. The device never sends its own id. `RUSIM_DEVICE_ID` stays log-only (`main.cpp:11`).
- No table `INSERT` grant to `anon`. `readings` keeps `SELECT`-only grants; the RPC is the only write entry point. Rejected alternatives: column grant plus policy (payload carries `device_id`, wider `anon` surface), dedicated ingest role (second secret to manage).
- `set search_path = pg_catalog, device_data`, `revoke all ... from public; grant execute ... to anon, authenticated`, matching `authenticate()` pattern in `20260930162000_add_device_data_token_auth.sql`.
- Input is one JSONB array per sample round (24 objects max). Required per object: `bottle_id`, `timestamp` (sample time, timestamptz), nullable `ph`, `pressure`, `temp`, `co2`, `ch4`. Optional: `experiment_id`; when absent the function applies the device's active experiment. `bottle_id` is the internal 0-based ID, `0..bottle_count-1`; the push encoder sends it unchanged. The dashboard displays the value plus one.
- Server-side checks: reject `timestamp` before 2025-01-01 or more than 10 minutes in the future; reject `bottle_id` outside `0..bottle_count-1`; insert with `on conflict (device_id, bottle_id, timestamp) do nothing` so replays are idempotent.

Supabase and ESP32 retain their existing 0-based internal IDs. The dashboard adds one only for user-facing chamber labels and CSV values: internal ID 0 displays as Chamber 1, and internal ID 7 as Chamber 8. The configured capacity remains 24; chambers without readings remain disconnected or no-data, with no synthetic rows added. `pushFormatHeadRound()` sends `PushRow.bottleId` unchanged.

### 6.2 Pull scope: full `experiments` plus one config row, scoped to same `device_id`

Decision: full pull, not the interim `bottle_count` register alone.

- Server-side (new migration): `device_data.experiments` holding experiment number, `device_id`, start time, duration, sample interval, and setpoints; plus `device_data.device_config` holding one row per device for `bottle_count` and owner contact. No experiments table exists today (`database.md:105`); this migration creates both.
- RLS: `SELECT` scoped to `device_id = device_data.session_device_id()`, same pattern as `devices` / `readings` read policies. No device sees another device's rows.
- WT32 polls both every 30 s alongside the existing latest-experiment query and pushes the result to the display over Modbus. `DURATION_H` (0x0060) remains display-owned until the experiments table lands; after that the WT32 copy overwrites it.
- Interim step (no migration, still worth doing): stop discarding `bottle_count`, `owner_name`, `owner_email` from `authenticate()` and push `bottle_count` to the display as a new master-written register. The display grid is hardcoded to `UI_CLUSTERS` 6 x `UI_BOTTLES_PER_CLUSTER` 4 = 24 (`ui_config.h`).

## 7. Correctness rules for the push (decided)

**Clock gating — refuse write when unsynced.** `timestamp` is part of the primary key
`(device_id, bottle_id, timestamp)` (`database.md:109`). `ensureNtpSync()` gives
up after 10 s and every caller proceeds regardless. Decision: the push path
refuses to write while `ntpSynced` is false. Firmware guardrails (all required):

- `sbPushReadings()` returns early when `!ntpSynced`; the buffered rounds stay queued, nothing is stamped at 1970.
- `syncDbLatest()` continues to update `TIME_OK` (0x0006) so the display shows unsynced state.
- Optional server backstop: `push_readings()` rejects `timestamp < '2025-01-01'` and `timestamp > now() + interval '10 minutes'`.

**Buffering — merge duplicates.** The Unpad wired network drops, so samples need a queue. Two rules follow from the schema:

- Stamp rows at **sample** time, not flush time, or the time series is wrong.
- Retries are idempotent. Decision: send `Prefer: resolution=merge-duplicates`
  with `on_conflict=(device_id,bottle_id,timestamp)` on direct PostgREST inserts,
  and use `on conflict (device_id, bottle_id, timestamp) do nothing` inside
  `push_readings()`. Replaying a buffered batch never duplicates rows.

**Memory.** The WT32 has 320 KB RAM, 47 KB used, and no PSRAM. A sample round for
24 bottles x 5 sensors is 480 bytes as raw floats. Decision: fixed-size ring with
oldest-drop, following the bounded-buffer pattern in `memory-safety-hardening.md`.
A few hundred rounds max (100-150 KB ceiling, competes with BearSSL). A dropped row
is better than an overrun; count drops in a counter pushed to the display.

**Payload size.** 24 bottles per sample is one POST body with 24 objects, so it
lands well inside a few kilobytes. `readBodyFixed()` (`:469`) already takes an
explicit capacity argument, and `kMaxBody = 4096` is the portal's value, chosen
for an 8 KB HTML page. Size the reading body separately rather than inheriting
the portal's constant.

## 8. Build plan

Dependency order, not effort order. With the sensor chain deferred, the order is
1 -> 2 -> 3 -> 7 -> 8. Step 8 has no round source until the deferred sensor
chain lands; write it against the future sensor poller and leave it dormant.

- [x] 1. Pin Supabase CA in `sbAuthenticate()` / `sbFetchLatestExp()` (`main.cpp:854,906`). Bundle GTS Root R4 + GlobalSign R3 + ISRG Root X1 via `setCACert()`. Accept: handshake passes after leaf renewal; no `setInsecure()` fallback on Supabase path. Built: `firmware/network-master/supabase_ca.h`, `sbApplyTls()` on all three Supabase calls (auth, fetch, push); portal calls stay `setInsecure()`. Live chain re-verified 2026-10-16 (leaf -> WE1 -> GTS R4 cross).
- [x] 2. Stop discarding `bottle_count`, `owner_name`, `owner_email` from `authenticate()`. Add master-written display register for `bottle_count`. Accept: `/info` shows DB value, `0xFFFF` when unknown. Built: `sbParseAuthMeta()` into `dbBottleCount`/`dbOwnerName`/`dbOwnerEmail`; `0x0007 DB_BOTTLES` on both ends; `/info` DB tile shows `latest (Nbottles)`, `--` when never synced.
- [x] 3. Firmware clock guard: `sbPushReadings()` early-return on `!ntpSynced`, keep `TIME_OK` reporting. Accept: no row with 1970 timestamp after 10 s NTP failure. Built: guard in `sbPushReadings()` plus enqueue-time refusal (`ntpSynced` and 2025-01-01 floor); `pushDbState()` still reports `TIME_OK`.
- [x] 7. Migrations: (a) `push_readings(jsonb)` per §6.1; (b) `experiments` + `device_config` tables per §6.2 with session-scoped `SELECT`. Accept: anon `POST /rest/v1/readings` still denied; `POST /rest/v1/rpc/push_readings` with valid `x-device-session` inserts; forged `device_id` impossible (not in payload). Built: `20261016000000_add_push_readings_and_device_tables.sql`, verified on the local DB (insert + replay-idempotent + default-experiment + all rejects + RLS scoping; `device_config` seeded from `devices`).
- [x] 8. Batch buffer + push call: fixed ring oldest-drop, sample-time stamps, `merge-duplicates` / `on conflict do nothing`, separate body cap from portal `kMaxBody`. Accept: network drop + replay produces no duplicates; drop counter increments instead of OOM. Built: 1200-row ring (~50 rounds), `kPushBodyCap = 6144`, 2 rounds flushed per 30 s cycle, `0x0008 PUSH_DROPS` register; queue/format logic verified by host test on the real functions; dormant (no enqueuer until the sensor pass; optimizer folds the queue away, ~0 RAM cost now, ~54 KB when materialized). The encoder writes the unchanged zero-based array index to `bottleId`.

### Deferred (2026-10-16): sensor chain

Out of the current build order; original numbers are kept so references in the
sections above still line up. These return in a later pass.

- [ ] 4. Mega Mini Modbus-slave firmware (5 sensors, 8 KB SRAM, manual DE/RE). Bring up against the WT32 over the bench RS485 link. Accept: two-request full poll (144 regs, FC 0x03) decodes cleanly.
- [ ] 5. WT32 second Modbus master on RS485 bus (separate from TTL display bus). 6 regs/bottle (5 values + null bitmask). Accept: local read of 24 bottles in ~0.35 s at 9600 baud.
- [ ] 6. Display sensor registers; replace `fake_data.cpp` with real values. Update `link_modbus.h`, `display-wt32-link.md`, `network-master.md` §3 together. Accept: UI shows live values, `sensorNull` only on per-sensor miss.

### Done criteria

Push path is done when a 24-bottle round sampled on the Mega Mini appears in `device_data.readings` with correct `device_id` (from session), sample-time `timestamp`, and nulls only where a sensor missed; pull path is done when the WT32 displays the active experiment + config for its own `device_id` polled every 30 s. The push criterion moves with the deferred sensor chain; until then, acceptance is the per-step criteria above.

## 9. Related documents

- `db-sync-experiment-ntp.md` — the pull path as built.
- `network-master.md` — portal login, Modbus master, memory budget.
- `display-wt32-link.md` — the display to WT32 Modbus line and register map.
- `database.md` — `device_data` schema, grants, and access control.
- `security-report.md` — public deployment findings. Its §"token_hash
  unreachable via API" finding describes the browser path only and does not
  cover the firmware.
- `memory-safety-hardening.md` — bounded buffers and atomic ISR flags.
- `../hardware.md` — sensor wiring and MAX485 notes. Not edited.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.

---
Content generated by `openai/gpt-6-codex` in `harness`.
