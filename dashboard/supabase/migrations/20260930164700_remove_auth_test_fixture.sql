-- Removes the cross-device isolation fixture added in
-- 20260930164600_add_auth_test_fixture.sql. Isolation has been verified: a
-- session scoped to device 0 returns no rows for device 99 even when those
-- rows are present.

delete from device_data.readings where device_id = 99;
delete from device_data.devices where device_id = 99;