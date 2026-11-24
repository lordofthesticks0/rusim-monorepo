-- Adds the server arrival time of each reading, for ingest-latency
-- measurement (docs/verification-toolkit.md, report checklist §5).
--
-- "timestamp" is the sample time the WT32 stamps when it collects a round,
-- so it cannot show when the row reached the database. inserted_at is set by
-- the database itself when push_readings() inserts the row; the RPC lists its
-- columns explicitly, so the default applies without changing the function.
--
-- Two statements on purpose: adding the column WITH a volatile default would
-- write the migration time into every existing row, which would look like
-- real arrival times. Added without a default first, existing rows keep NULL
-- ("arrival time unknown"); only rows inserted afterwards get now().
--
-- A replay hits ON CONFLICT DO NOTHING and leaves the original inserted_at,
-- so the column records the FIRST arrival.

alter table device_data.readings
  add column inserted_at timestamptz;

alter table device_data.readings
  alter column inserted_at set default now();

comment on column device_data.readings.inserted_at is
  'Server time at which push_readings() first inserted the row; null for rows inserted before this column existed. Ingest latency = inserted_at - "timestamp".';

notify pgrst, 'reload schema';
