-- ===========================================================================
-- Dummy readings for the testing experiment (experiment_id = 0)
-- ===========================================================================
--
-- Purpose
-- -------
-- Fills the dashboard with realistic-looking data for device 0 while no
-- firmware is writing to the database yet. Safe to re-run at any time: it
-- removes its own previous output first and then repopulates.
--
-- What it produces
-- ----------------
--   * Bottles 0-7 only. Bottles 8-23 are deliberately absent from every sample,
--     which is how a disconnected bottle group shows up in the data.
--   * 24 samples spaced 5 minutes apart, so 2 hours of history.
--   * Each sample covers all 8 bottles; experiment 0's newest sample is the
--     current minute.
--   * Experiment N sits one full span (sample_count * gap) behind experiment N-1,
--     so experiments have disjoint timestamp windows and coexist safely.
--
-- Two kinds of missing data are simulated, and they mean different things:
--
--   1. An absent row means the bottle reported nothing for that sample, i.e. the
--      bottle was disconnected. The dashboard treats a bottle with no row at the
--      latest timestamp as offline.
--
--   2. A NULL value in a row that exists means the bottle reported, but that one
--      sensor produced no value. The dashboard renders these as "-".
--
-- Knobs
--   EXPERIMENT_ID which experiment to generate. 0 is the testing experiment,
--                 1 and above are regular experiments. Experiment 0 ends at the
--                 current minute and each higher id is one full experiment span
--                 further back, so experiments never share a timestamp.
--
-- Every knob can be overridden on the command line, for example:
--   psql "$DATABASE_URL" -f supabase/seed_readings.sql \
--     -v experiment_id=3 -v sample_count=48 -v bottle_count=12
-- The interactive wrapper `bun run add-fake-data` prompts for all of them and
-- then calls this file. Defaults are used for anything not supplied.
--
-- Re-running
-- ----------
--   Locally, seed.sql includes this file, so `supabase db reset` applies it.
--   Against the hosted project, run it with:
--     psql "$DATABASE_URL" -f supabase/seed_readings.sql
--   or paste it into the Supabase SQL editor.
-- ===========================================================================

-- Defaults for any variable the caller did not provide.
\if :{?device_id}
\else
\set device_id 0
\endif
\if :{?experiment_id}
\else
\set experiment_id 0
\endif
\if :{?sample_count}
\else
\set sample_count 24
\endif
\if :{?sample_gap_minutes}
\else
\set sample_gap_minutes 5
\endif
\if :{?bottle_count}
\else
\set bottle_count 8
\endif
\if :{?null_rate}
\else
\set null_rate 0.0005
\endif
\if :{?bottle_total}
\else
\set bottle_total 24
\endif

begin;

-- Make sure the target device exists, so the readings have a parent row. Its
-- bottle_count becomes the number of bottles the dashboard offers, of which
-- only the first `bottle_count` actually report.
insert into device_data.devices (device_id, bottle_count, owner_name, owner_email)
values (:device_id, :bottle_total, 'RUSIM team', 'example@example.com')
on conflict (device_id) do update
set bottle_count = excluded.bottle_count;

-- Remove this experiment's previous rows so re-running replaces it in place.
--
-- Scoped to a single experiment. The timestamps below give each experiment a
-- window that starts one full span further into the past than the previous
-- experiment's, so windows are disjoint and this delete cannot touch another
-- experiment.
--
-- Regenerating with a different sample_count changes an experiment's span and
-- therefore shifts its window. That is the one case the window can overlap a
-- neighbour's, and it fails loudly on the primary key rather than overwriting,
-- which is the right outcome: a device cannot genuinely report two experiments
-- for one bottle at the same instant. Keep sample_count fixed across runs for a
-- given device, or delete that device's readings before changing it.
delete from device_data.readings
where device_id = :device_id
  and experiment_id = :experiment_id;

insert into device_data.readings (
  device_id,
  bottle_id,
  experiment_id,
  "timestamp",
  ph,
  pressure,
  temp,
  co2,
  ch4
)
with settings as (
  select
    :device_id::integer as device_id,
    :experiment_id::integer as experiment_id,
    :sample_count::integer as sample_count,
    :sample_gap_minutes::integer as sample_gap_minutes,
    :bottle_count::integer as bottle_count,
    :null_rate::double precision as null_rate
),
samples as (
  select
    generate_series(0, settings.sample_count - 1) as sample_offset,
    settings.device_id,
    settings.experiment_id,
    settings.sample_count,
    settings.sample_gap_minutes,
    settings.bottle_count,
    settings.null_rate
  from settings
),
timed as (
  select
    -- Timestamp layout, newest first.
    --
    -- Experiment 0 (the testing experiment) ends at the current minute, so it
    -- reads as live. Each higher experiment id is shifted one full experiment
    -- span further into the past, which guarantees two experiments on the same
    -- device never share a timestamp.
    --
    -- The stride has to be the whole span, sample_count * sample_gap_minutes,
    -- not the 5 minutes between samples. A 5 minute stride looks right for a
    -- single sample per experiment but makes neighbouring experiments overlap
    -- almost completely: with 24 samples every 5 minutes, experiment 1's newest
    -- sample would land on experiment 0's second-newest, and the two would
    -- collide on the primary key (device_id, bottle_id, timestamp). Stepping by
    -- a full span keeps every experiment's window disjoint.
    date_trunc('minute', now())
      - make_interval(mins => experiment_id * (sample_count * sample_gap_minutes))
      - make_interval(mins => (sample_count - 1 - sample_offset) * sample_gap_minutes) as sampled_at,
    device_id,
    experiment_id,
    sample_count,
    sample_gap_minutes,
    bottle_count,
    null_rate
  from samples
),
expanded as (
  select
    bottle.bottle_id,
    timed.device_id,
    timed.experiment_id,
    timed.sampled_at,
    timed.null_rate
  from timed
  cross join lateral generate_series(0, timed.bottle_count - 1) as bottle(bottle_id)
)
select
  expanded.device_id,
  expanded.bottle_id,
  expanded.experiment_id,
  expanded.sampled_at as "timestamp",

  -- Each bottle gets a stable offset from the others so the bottles stay
  -- visually distinguishable across samples instead of being pure noise.
  case when random() < expanded.null_rate then null else
    round((4.20 + expanded.bottle_id * 0.11 + random() * 0.35)::numeric, 4)::real
  end as ph,

  case when random() < expanded.null_rate then null else
    round((98.5 + expanded.bottle_id * 0.28 + random() * 1.4)::numeric, 4)::real
  end as pressure,

  case when random() < expanded.null_rate then null else
    round((17.5 + expanded.bottle_id * 0.42 + random() * 2.2)::numeric, 4)::real
  end as temp,

  case when random() < expanded.null_rate then null else
    round((240 + expanded.bottle_id * 55 + random() * 160)::numeric, 2)::real
  end as co2,

  case when random() < expanded.null_rate then null else
    round((12 + expanded.bottle_id * 6 + random() * 40)::numeric, 4)::real
  end as ch4
from expanded;

commit;

-- ===========================================================================
-- Report what was written.
-- ===========================================================================

select
  count(*)                                             as total_rows,
  count(distinct bottle_id)                            as bottles_reporting,
  count(*) filter (where ph is null)                   as null_ph,
  count(*) filter (where ch4 is null)                  as null_ch4,
  min("timestamp")                                     as first_sample,
  max("timestamp")                                     as last_sample
from device_data.readings
where device_id = :device_id
  and experiment_id = :experiment_id;

-- Bottles the device claims to have but which never reported. The dashboard
-- reads this absence as "disconnected".
select missing.bottle_id
from device_data.devices d
cross join lateral generate_series(0, d.bottle_count - 1) as missing(bottle_id)
where d.device_id = :device_id
  and not exists (
    select 1
    from device_data.readings r
    where r.device_id = d.device_id
      and r.bottle_id = missing.bottle_id
  );