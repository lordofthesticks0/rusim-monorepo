# ESP to database sync — analysis and plan

Scope: how the WT32-ETH01 (`firmware/network-master`) could pull configuration
and push sensor readings to Supabase, what already exists, and what is missing.

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

The five sensors are wired to the **ATmega2560 Mega Mini**
(`docs/hardware.md` §5): HX710B pressure, TGS2611 methane, MH-Z19C CO2,
DS18B20 temperature, PH-402C pH.

`firmware/` contains no Mega Mini program that samples on a schedule. What
exists is one-shot calibration and bring-up sketches: `co2-calibrate`,
`methane-calibrate`, `pressure-calibrate`, `loadcell-test`, `max485-test`.

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
   must not be used, and the loopback pattern in `firmware/max485-test/` is the
   reference.
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
calls with `setCACert()` and the Supabase root certificate. Leave the portal
calls alone. Cost is roughly 1-2 KB against the ~316 KB of free flash recorded in
`network-master.md` §6.

This is the change that makes the device token defensible at all. Without it,
anything on the wired network can read the token off the wire during
authenticate and then hold a permanent credential.

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

## 6. Open decisions

### How to grant the write

Three options, unresolved:

| Option | Mechanism | Cost |
|---|---|---|
| `SECURITY DEFINER` RPC | New `device_data.push_readings(jsonb)` that forces `device_id` from `session_device_id()`. | A plpgsql batch insert function. The device never sends its own id. No table `INSERT` grant to `anon`. |
| Column grant plus policy | `grant insert` on `readings` to `anon`, plus a `for insert` policy with `with check (device_id = device_data.session_device_id())`. | Least new code. The payload carries `device_id` and RLS rejects mismatches. |
| Dedicated ingest role | A Postgres role with `INSERT` on `readings` and no access to `devices` or `token_hash`, keyed by a second secret on the device. | Strongest separation, and a second secret to manage. |

Recommended: the `SECURITY DEFINER` RPC. It keeps the identity assertion in the
database, where the read path already keeps it, and it does not widen what
`anon` can do against the table.

Whatever is chosen must preserve one property the read path already has: the
device never asserts its own `device_id`. `RUSIM_DEVICE_ID` is defined but
deliberately log-only (`main.cpp:11`, `db-sync-experiment-ntp.md`).

### What the device should pull

There is no experiments table (`database.md:105`). Experiment number is a bare
integer column on `readings`, and `DURATION_H` (0x0060) is set from the display
UI, not from the database. The dashboard therefore cannot command a run, and
nothing can be pulled about run configuration because nothing stores it.

A minimal useful pull needs, server-side: an `experiments` table holding
experiment number, start time, duration, sample interval, and setpoints, plus a
`device_config` table for `bottle_count` and owner contact. The WT32 polls both
every 30 s alongside the existing latest-experiment query and pushes the result
to the display over Modbus.

Cheaper interim step, no migration: stop discarding the `bottle_count`,
`owner_name`, and `owner_email` that `authenticate()` already returns, and push
`bottle_count` to the display as a new master-written register. The display's
bottle grid is hardcoded to `UI_CLUSTERS` 6 x `UI_BOTTLES_PER_CLUSTER` 4 =
24 (`ui_config.h`), which currently only agrees with the database by coincidence.

## 7. Correctness details for the push

**Clock gating.** `timestamp` is part of the primary key
`(device_id, bottle_id, timestamp)` (`database.md:109`). `ensureNtpSync()` gives
up after 10 s and every caller proceeds regardless. A push path must refuse to
write while `ntpSynced` is false, or it stamps rows at 1970.

**Buffering.** The Unpad wired network drops, so samples need a queue. Two rules
follow from the schema:

- Stamp rows at **sample** time, not flush time, or the time series is wrong.
- Make retries idempotent. A plain PostgREST `POST` is an insert, so replaying a
  buffered batch duplicates rows. Send `Prefer: resolution=merge-duplicates`
  with `on_conflict=(device_id,bottle_id,timestamp)`.

**Memory.** The WT32 has 320 KB RAM, 47 KB used, and no PSRAM. A sample round for
24 bottles x 5 sensors is 480 bytes as raw floats. A few hundred buffered rounds
is 100-150 KB, which competes with BearSSL. Use a fixed-size ring with
oldest-drop, following the bounded-buffer pattern already applied to this
firmware in `memory-safety-hardening.md`. A dropped row is better than an
overrun; decide explicitly which the ring does.

**Payload size.** 24 bottles per sample is one POST body with 24 objects, so it
lands well inside a few kilobytes. `readBodyFixed()` (`:469`) already takes an
explicit capacity argument, and `kMaxBody = 4096` is the portal's value, chosen
for an 8 KB HTML page. Size the reading body separately rather than inheriting
the portal's constant.

## 8. Implementation order

Roughly dependency order, not effort order.

1. Pin the Supabase CA in the two `sb*` functions. Smallest change, removes the
   worst exposure, and makes every later step safe to test.
2. Stop discarding `bottle_count` and owner fields from `authenticate()`.
3. Gate any future push on `ntpSynced`.
4. Write the Mega Mini Modbus-slave firmware and bring it up over the loopback
   harness in `firmware/max485-test/`.
5. Add the sensor register block to the WT32 RS485 master, reading locally first.
6. Add the display to WT32 sensor registers so the UI shows real values, replacing
   `fake_data.cpp`.
7. Write the push migration, once the read grant decision is settled.
8. Implement the batch buffer and the push call.

Steps 4 through 6 are the bulk of the work and none of it touches the database.
Steps 7 and 8 are comparatively small. Starting with the migration is the
mistake to avoid: there is nothing to push until step 5 works.

## 9. Related documents

- `db-sync-experiment-ntp.md` — the pull path as built.
- `network-master.md` — portal login, Modbus master, memory budget.
- `display-wt32-link.md` — the display to WT32 Modbus line and register map.
- `database.md` — `device_data` schema, grants, and access control.
- `security-report.md` — public deployment findings. Its §"token_hash
  unreachable via API" finding describes the browser path only and does not
  cover the firmware.
- `memory-safety-hardening.md` — bounded buffers and atomic ISR flags.
- `rs485-loopback-test.md` — MAX485 direction control reference.
- `../hardware.md` — sensor wiring and MAX485 notes. Not edited.

---
Content generated by `opencode/space-bunny-free` in `opencode`.