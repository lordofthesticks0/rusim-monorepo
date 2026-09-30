-- Isolated R&D device schema. Kept separate from the existing public tables
-- so both versions can coexist until the old schema is removed later.

create schema device_data;
revoke all on schema device_data from public, anon, authenticated;

create table device_data.devices (
  device_id integer primary key check (device_id >= 0),
  bottle_count integer not null,
  owner_name text,
  owner_email text,
  token_hash text unique,
  constraint devices_token_hash_sha512_check
    check (token_hash is null or token_hash ~ '^[0-9a-f]{128}$')
);

create table device_data.readings (
  device_id integer not null references device_data.devices (device_id),
  bottle_id integer not null,
  experiment_id integer not null check (experiment_id >= 0),
  "timestamp" timestamptz not null,
  ph real not null,
  pressure real not null,
  temp real not null,
  co2 real not null,
  ch4 real not null,
  primary key (device_id, bottle_id, experiment_id)
);

create index readings_experiment_timestamp_idx
  on device_data.readings (experiment_id, "timestamp" desc);

alter table device_data.devices enable row level security;
alter table device_data.readings enable row level security;

-- Device zero is the only row present initially. Its token is unset until one
-- is provisioned; all sensor readings and experiment rows remain empty.
insert into device_data.devices (device_id, bottle_count, owner_name)
values (0, 24, 'Development');
