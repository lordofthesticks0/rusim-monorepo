-- Regenerates the dummy readings for experiment 0 using the corrected timestamp
-- layout, superseding the rows written by the earlier seed migration.
--
-- Each experiment now occupies a window that starts one full span
-- (sample_count * gap) further into the past than the previous experiment's, so
-- experiments on one device can coexist without colliding on the
-- (device_id, bottle_id, timestamp) primary key.
--
-- The canonical, re-runnable script is supabase/seed_readings.sql, which takes
-- these values as psql variables. They are inlined here because `supabase db
-- push` cannot pass -v flags, so this migration records only the initial
-- population. Use `bun run add-fake-data` to regenerate.


begin;

-- Make sure the target device exists, so the readings have a parent row. Its
-- bottle_count becomes the number of bottles the dashboard offers, of which
-- only the first `bottle_count` actually report.
insert into device_data.devices (device_id, bottle_count, owner_name, owner_email)
values (0, 24, 'RUSIM team', 'example@example.com')
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
where device_id = 0
  and experiment_id = 0;

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
    0::integer as device_id,
    0::integer as experiment_id,
    24::integer as sample_count,
    5::integer as sample_gap_minutes,
    8::integer as bottle_count,
    0.0005::double precision as null_rate
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
where device_id = 0
  and experiment_id = 0;

-- Bottles the device claims to have but which never reported. The dashboard
-- reads this absence as "disconnected".
select missing.bottle_id
from device_data.devices d
cross join lateral generate_series(0, d.bottle_count - 1) as missing(bottle_id)
where d.device_id = 0
  and not exists (
    select 1
    from device_data.readings r
    where r.device_id = d.device_id
      and r.bottle_id = missing.bottle_id
  );