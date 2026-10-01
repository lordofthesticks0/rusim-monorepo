-- Clears the smoke-test rows left in device_data by manual verification.
--
-- 20260930164600 added a reading on device 0 alongside the isolation fixture so
-- that cross-device filtering could be observed; that device 0 row is synthetic
-- and has no real device behind it. Sessions are issued at runtime and are
-- cleared here because each one belongs to a browser session that no longer
-- exists; they expire on their own within a day.

delete from device_data.readings
where device_id = 0
  and bottle_id = 0
  and experiment_id = 0;

delete from device_data.sessions;