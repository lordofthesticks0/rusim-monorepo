-- Data-integrity checks for one experiment run (report checklist §5).
-- Variables (psql syntax): :device, :exp, :interval_s (the run's sample_interval_s).
--   psql "$DATABASE_URL" -v device=0 -v exp=4 -v interval_s=30 -f verification/sql/integrity.sql
-- or: bun verification/db-check.ts --device 0 --exp 4 --interval-s 30
-- Each "-- name:" block is one query; db-check.ts runs them one by one.

-- name: run_window
-- doc: Time span, row count and reporting bottles of the run.
select count(*) as rows,
       count(distinct bottle_id) as bottles,
       min("timestamp") as first_sample,
       max("timestamp") as last_sample,
       round(extract(epoch from max("timestamp") - min("timestamp")) / 3600.0, 3) as span_h
from device_data.readings
where device_id = :device and experiment_id = :exp;

-- name: completeness
-- doc: Rows per bottle vs rows expected over the observed span at :interval_s, and vs the configured duration.
with b as (
  select bottle_id, count(*) as rows, min("timestamp") as t0, max("timestamp") as t1
  from device_data.readings
  where device_id = :device and experiment_id = :exp
  group by bottle_id
), e as (
  select duration_h from device_data.experiments
  where device_id = :device and experiment_id = :exp
)
select b.bottle_id, b.rows,
       floor(extract(epoch from b.t1 - b.t0) / :interval_s)::int + 1 as expected_in_span,
       round(100.0 * b.rows / (floor(extract(epoch from b.t1 - b.t0) / :interval_s) + 1), 2) as pct_in_span,
       (select duration_h * 3600 / :interval_s from e) as expected_for_duration,
       round(100.0 * b.rows / nullif((select duration_h * 3600 / :interval_s from e), 0), 2) as pct_of_duration
from b order by b.bottle_id;

-- name: spacing
-- doc: Spacing between consecutive samples of each bottle, seconds; gaps = spacings above 1.5 x interval.
with d as (
  select bottle_id,
         extract(epoch from "timestamp" - lag("timestamp") over (partition by bottle_id order by "timestamp")) as dt
  from device_data.readings
  where device_id = :device and experiment_id = :exp
)
select bottle_id, count(dt) as n, min(dt) as min_s, round(avg(dt)::numeric, 3) as mean_s,
       max(dt) as max_s, round(stddev_samp(dt)::numeric, 3) as sd_s,
       count(*) filter (where dt > 1.5 * :interval_s) as gaps
from d group by bottle_id order by bottle_id;

-- name: gaps
-- doc: The 100 longest gaps (spacing above 1.5 x interval), for the outage timeline.
with d as (
  select bottle_id, "timestamp",
         lag("timestamp") over (partition by bottle_id order by "timestamp") as prev
  from device_data.readings
  where device_id = :device and experiment_id = :exp
)
select bottle_id, prev as gap_start, "timestamp" as gap_end,
       extract(epoch from "timestamp" - prev) as gap_s
from d where extract(epoch from "timestamp" - prev) > 1.5 * :interval_s
order by gap_s desc limit 100;

-- name: rounds
-- doc: Bottles per sample round (one timestamp). Rounds with fewer bottles = missing rows (chamber silent).
select bottles_in_round, count(*) as rounds
from (
  select "timestamp", count(*) as bottles_in_round
  from device_data.readings
  where device_id = :device and experiment_id = :exp
  group by "timestamp"
) r group by bottles_in_round order by bottles_in_round;

-- name: nulls
-- doc: NULL fields per bottle (sensor failed, row present). Compare with "rounds" (row missing).
select bottle_id, count(*) as rows,
       count(*) filter (where ph is null) as ph_null,
       count(*) filter (where temp is null) as temp_null,
       count(*) filter (where co2 is null) as co2_null,
       count(*) filter (where ch4 is null) as ch4_null,
       count(*) filter (where pressure is null) as pressure_null,
       count(*) filter (where ph is null and temp is null and co2 is null and ch4 is null and pressure is null) as all_null
from device_data.readings
where device_id = :device and experiment_id = :exp
group by bottle_id order by bottle_id;

-- name: foreign_rows
-- doc: Rows of this device inside the run's time window that carry another experiment id (should be 0).
with w as (
  select min("timestamp") as t0, max("timestamp") as t1
  from device_data.readings where device_id = :device and experiment_id = :exp
)
select experiment_id, count(*) as rows
from device_data.readings r, w
where r.device_id = :device and r.experiment_id <> :exp and r."timestamp" between w.t0 and w.t1
group by experiment_id order by experiment_id;

-- name: out_of_range_time
-- doc: Rows outside the accepted window (before 2025-01-01 or more than 10 min in the future). Must be 0.
select count(*) filter (where "timestamp" < '2025-01-01T00:00:00Z') as before_2025,
       count(*) filter (where "timestamp" > now() + interval '10 minutes') as future
from device_data.readings where device_id = :device;

-- name: ingest_latency
-- doc: inserted_at - timestamp, seconds (needs migration 20261124000000). Rows inserted before it have NULL and are skipped.
select count(inserted_at) as n,
       round(min(extract(epoch from inserted_at - "timestamp"))::numeric, 3) as min_s,
       round(avg(extract(epoch from inserted_at - "timestamp"))::numeric, 3) as mean_s,
       round((percentile_disc(0.5) within group (order by extract(epoch from inserted_at - "timestamp")))::numeric, 3) as p50_s,
       round((percentile_disc(0.95) within group (order by extract(epoch from inserted_at - "timestamp")))::numeric, 3) as p95_s,
       round((percentile_disc(0.99) within group (order by extract(epoch from inserted_at - "timestamp")))::numeric, 3) as p99_s,
       round(max(extract(epoch from inserted_at - "timestamp"))::numeric, 3) as max_s
from device_data.readings
where device_id = :device and experiment_id = :exp and inserted_at is not null;
