-- TEMPORARY: test fixture proving cross-device row isolation.
-- Reverted by 20260930164700_remove_auth_test_fixture.sql. Do not keep.

insert into device_data.devices (device_id, bottle_count, owner_name, owner_email)
values (99, 2, 'Isolation Test Device', 'isolation@example.com')
on conflict (device_id) do nothing;

insert into device_data.readings (device_id, bottle_id, experiment_id, "timestamp", ph, pressure, temp, co2, ch4)
values
  (0, 0, 0, now(), 6.5, 100.0, 20.0, 300.0, 40.0),
  (99, 0, 0, now(), 7.5, 102.0, 25.0, 900.0, 70.0),
  (99, 1, 0, now(), 7.6, 103.0, 26.0, 950.0, 75.0)
on conflict (device_id, bottle_id, experiment_id) do nothing;