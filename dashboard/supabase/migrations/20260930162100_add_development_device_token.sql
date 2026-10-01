-- Provisions device zero, the development device.
--
-- The token hash is deliberately left null here and applied separately from
-- dashboard/.env by `bun run dev-db`. Keeping a real hash out of the
-- repository means no credential is committed to git, and rotating the
-- development token never requires editing a migration.
--
-- An empty string is not a valid substitute: token_hash is constrained to
-- either NULL or 128 lowercase hexadecimal characters, so '' fails the check.

insert into device_data.devices (device_id, bottle_count, owner_name, owner_email, token_hash)
values (
  0,
  24,
  'RUSIM team',
  'example@example.com',
  null
)
on conflict (device_id) do update
set
  bottle_count = excluded.bottle_count,
  owner_name = excluded.owner_name,
  owner_email = excluded.owner_email,
  -- Preserves any hash already applied by `bun run dev-db`, so applying
  -- this migration does not silently lock the device out.
  token_hash = device_data.devices.token_hash;