-- Database performance and storage (report checklist §5 "Performance and cost").
--   bun verification/db-check.ts --suite performance --device 0 --exp 4
-- EXPLAIN runs as the connecting role (RLS bypassed for postgres); the
-- explicit device_id filter stands in for the policy's session filter.

-- name: table_size
-- doc: Rows and on-disk size of device_data.readings (table + indexes + toast).
select (select count(*) from device_data.readings) as rows,
       pg_size_pretty(pg_total_relation_size('device_data.readings')) as total_size,
       pg_total_relation_size('device_data.readings') as total_bytes,
       round(pg_total_relation_size('device_data.readings')::numeric / greatest((select count(*) from device_data.readings), 1), 1) as bytes_per_row,
       pg_size_pretty(pg_database_size(current_database())) as database_size;

-- name: rows_per_experiment
-- doc: Rows, bottles and span per experiment of the device (storage per run).
select experiment_id, count(*) as rows, count(distinct bottle_id) as bottles,
       min("timestamp") as first_sample, max("timestamp") as last_sample
from device_data.readings where device_id = :device
group by experiment_id order by experiment_id;

-- name: explain_experiment_page
-- doc: Plan of the dashboard's first experiment page (fetchExperimentReadings, 1000 rows). Look for readings_experiment_timestamp_idx.
explain (analyze, buffers, format text)
select device_id, bottle_id, experiment_id, "timestamp", ph, pressure, temp, co2, ch4
from device_data.readings
where device_id = :device and experiment_id = :exp
order by "timestamp" asc, bottle_id asc limit 1000;

-- name: explain_latest_page
-- doc: Plan of the dashboard's newest-first read (fetchReadings, first 1000 rows).
explain (analyze, buffers, format text)
select device_id, bottle_id, experiment_id, "timestamp", ph, pressure, temp, co2, ch4
from device_data.readings
where device_id = :device
order by "timestamp" desc limit 1000;

-- name: index_usage
-- doc: Scan counts per index on readings since the statistics were last reset.
select indexrelname as index_name, idx_scan, idx_tup_read, idx_tup_fetch
from pg_stat_user_indexes where schemaname = 'device_data' and relname = 'readings';
