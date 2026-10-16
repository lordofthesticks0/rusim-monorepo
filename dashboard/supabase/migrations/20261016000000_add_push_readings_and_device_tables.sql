-- Device write path + experiment/config pull tables (esp-db-sync plan steps 7-8).
--
-- Two parts:
--
-- (a) device_data.push_readings(jsonb): the only write entry point for
--     devices. SECURITY DEFINER so anon never needs INSERT on readings
--     (which keeps its SELECT-only grants). device_id is forced from
--     session_device_id(); the payload carries no device identity, so a
--     forged device_id is impossible by construction (unknown keys are
--     ignored). Replays are idempotent via ON CONFLICT DO NOTHING on
--     (device_id, bottle_id, timestamp).
--
-- (b) device_data.experiments + device_data.device_config: the full pull
--     scope. Session-scoped SELECT, same pattern as devices/readings.
--     device_config is seeded once from the existing devices columns.

-- ---------------------------------------------------------------- experiments

create table device_data.experiments (
  device_id integer not null
    references device_data.devices (device_id) on delete cascade,
  experiment_id integer not null check (experiment_id >= 0),
  started_at timestamptz not null default now(),
  duration_h integer check (duration_h is null or (duration_h >= 1 and duration_h <= 99)),
  sample_interval_s integer check (sample_interval_s is null or sample_interval_s > 0),
  setpoints jsonb not null default '{}'::jsonb,
  primary key (device_id, experiment_id)
);

comment on table device_data.experiments is
  'Per-device experiment configs pulled by the WT32 every 30 s. experiment_id groups readings; it is not part of the readings key.';

-- -------------------------------------------------------------- device_config

create table device_data.device_config (
  device_id integer primary key
    references device_data.devices (device_id) on delete cascade,
  bottle_count integer not null check (bottle_count > 0),
  owner_name text,
  owner_email text,
  updated_at timestamptz not null default now()
);

comment on table device_data.device_config is
  'One row per device for bottle_count and owner contact, pulled by the WT32. Mirrors the devices columns; seeded from devices on creation.';

insert into device_data.device_config (device_id, bottle_count, owner_name, owner_email)
select device_id, bottle_count, owner_name, owner_email
from device_data.devices
on conflict (device_id) do nothing;

-- ------------------------------------------------------------------ RLS/grants

alter table device_data.experiments enable row level security;
alter table device_data.device_config enable row level security;

create policy "session can read own device experiments"
on device_data.experiments
for select
to anon, authenticated
using (device_id = device_data.session_device_id());

create policy "session can read own device config"
on device_data.device_config
for select
to anon, authenticated
using (device_id = device_data.session_device_id());

revoke all on table device_data.experiments from public, anon, authenticated;
grant select on table device_data.experiments to anon, authenticated;

revoke all on table device_data.device_config from public, anon, authenticated;
grant select on table device_data.device_config to anon, authenticated;

-- --------------------------------------------------------------- push_readings

create or replace function device_data.push_readings(p_readings jsonb)
returns integer
language plpgsql
security definer
set search_path = pg_catalog, device_data
as $$
declare
  caller_device integer;
  dev_bottles integer;
  default_exp integer;
  n integer;
  r record;
  elem jsonb;
  v_bottle integer;
  v_ts timestamptz;
  v_exp integer;
  v_ph real;
  v_pressure real;
  v_temp real;
  v_co2 real;
  v_ch4 real;
  inserted integer := 0;
  affected integer;
begin
  caller_device := device_data.session_device_id();
  if caller_device is null then
    raise exception 'No live device session.'
      using errcode = '28000';
  end if;

  if p_readings is null or jsonb_typeof(p_readings) <> 'array' then
    raise exception 'Readings must be a JSON array.'
      using errcode = '22023';
  end if;

  n := jsonb_array_length(p_readings);
  if n = 0 then
    return 0;
  end if;
  if n > 24 then
    raise exception 'At most 24 readings per call.'
      using errcode = '22023';
  end if;

  select d.bottle_count into dev_bottles
  from device_data.devices d
  where d.device_id = caller_device;
  if not found then
    raise exception 'Unknown device.'
      using errcode = '28000';
  end if;

  -- Active experiment when the payload omits experiment_id: the device's
  -- highest experiment so far, falling back to 0 (testing experiment).
  select max(e.experiment_id) into default_exp
  from device_data.experiments e
  where e.device_id = caller_device;
  if default_exp is null then
    default_exp := 0;
  end if;

  for r in select value from jsonb_array_elements(p_readings) loop
    elem := r.value;
    if jsonb_typeof(elem) <> 'object' then
      raise exception 'Each reading must be a JSON object.'
        using errcode = '22023';
    end if;

    begin
      v_bottle := (elem ->> 'bottle_id')::integer;
    exception when others then
      raise exception 'bottle_id must be an integer.'
        using errcode = '22023';
    end;
    if v_bottle is null or v_bottle < 0 or v_bottle >= dev_bottles then
      raise exception 'bottle_id % out of range (device allows 0-%).', v_bottle, dev_bottles - 1
        using errcode = '22023';
    end if;

    begin
      v_ts := (elem ->> 'timestamp')::timestamptz;
    exception when others then
      raise exception 'timestamp must be a timestamptz.'
        using errcode = '22023';
    end;
    if v_ts is null then
      raise exception 'timestamp is required.'
        using errcode = '22023';
    end if;
    -- Clock-gating backstop: matches the firmware rule that rows are
    -- stamped at sample time and never sent while NTP is unsynced.
    if v_ts < timestamptz '2025-01-01 00:00:00+00'
       or v_ts > now() + interval '10 minutes' then
      raise exception 'timestamp % out of range.', v_ts
        using errcode = '22023';
    end if;

    begin
      v_exp := coalesce((elem ->> 'experiment_id')::integer, default_exp);
    exception when others then
      raise exception 'experiment_id must be an integer.'
        using errcode = '22023';
    end;
    if v_exp < 0 then
      raise exception 'experiment_id must be >= 0.'
        using errcode = '22023';
    end if;

    -- Nullable sensor values: a missing key or JSON null means the bottle
    -- reported but that sensor produced no value. Anything non-numeric
    -- raises and rejects the whole batch (no partial writes below either:
    -- each insert is idempotent, but a bad row aborts before later rows).
    begin
      v_ph := (elem ->> 'ph')::real;
      v_pressure := (elem ->> 'pressure')::real;
      v_temp := (elem ->> 'temp')::real;
      v_co2 := (elem ->> 'co2')::real;
      v_ch4 := (elem ->> 'ch4')::real;
    exception when others then
      raise exception 'Sensor values must be numeric or null.'
        using errcode = '22023';
    end;

    -- device_id comes from the session only. A device_id key in the
    -- payload, forged or otherwise, is never read and has no effect.
    insert into device_data.readings
      (device_id, bottle_id, experiment_id, "timestamp", ph, pressure, temp, co2, ch4)
    values
      (caller_device, v_bottle, v_exp, v_ts, v_ph, v_pressure, v_temp, v_co2, v_ch4)
    on conflict (device_id, bottle_id, "timestamp") do nothing;
    get diagnostics affected = row_count;
    inserted := inserted + affected;
  end loop;

  return inserted;
end;
$$;

comment on function device_data.push_readings(jsonb) is
  'Inserts one sample round (up to 24 bottle objects) for the calling session device. device_id is taken from the session; timestamps are sample times; replays are idempotent.';

revoke all on function device_data.push_readings(jsonb) from public;
grant execute on function device_data.push_readings(jsonb) to anon, authenticated;

notify pgrst, 'reload schema';
