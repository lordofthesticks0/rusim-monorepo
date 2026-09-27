-- Deterministic development seed for the RUSIM chamber-level MVP schema.
-- The fixed timestamp is intentionally in the past relative to the current
-- development date. All telemetry values are dummy development values and do
-- not claim physical units.

-- Fixed identifiers make all seed relationships explicit and repeatable.
-- Device: 00000000-0000-0000-0000-000000000001
-- Nodes:  00000000-0000-0000-0000-000000000101 through ...0106
-- Chambers: ...1001 through ...1024
-- Experiment: ...2001
-- Experiment nodes: ...3001 and ...3002
-- Experiment chambers: ...4001 through ...4008
-- Session: ...5001

insert into public.devices (
  id,
  device_code,
  name,
  status,
  metadata,
  created_at,
  updated_at
)
values (
  '00000000-0000-0000-0000-000000000001',
  'DEV-001',
  'RUSIM Device 01',
  'online',
  '{"source":"development-seed"}'::jsonb,
  timestamptz '2026-09-27 11:00:00+00',
  timestamptz '2026-09-27 11:00:00+00'
)
on conflict do nothing;

insert into public.nodes (
  id,
  device_id,
  node_code,
  name,
  status,
  metadata,
  created_at,
  updated_at
)
values
  ('00000000-0000-0000-0000-000000000101', '00000000-0000-0000-0000-000000000001', 'NODE-01', 'Node 01', 'online',       '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000000102', '00000000-0000-0000-0000-000000000001', 'NODE-02', 'Node 02', 'online',       '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000000103', '00000000-0000-0000-0000-000000000001', 'NODE-03', 'Node 03', 'provisioning', '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000000104', '00000000-0000-0000-0000-000000000001', 'NODE-04', 'Node 04', 'provisioning', '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000000105', '00000000-0000-0000-0000-000000000001', 'NODE-05', 'Node 05', 'provisioning', '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000000106', '00000000-0000-0000-0000-000000000001', 'NODE-06', 'Node 06', 'provisioning', '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00')
on conflict do nothing;

insert into public.chambers (
  id,
  node_id,
  chamber_code,
  name,
  status,
  enabled,
  metadata,
  created_at,
  updated_at
)
values
  ('00000000-0000-0000-0000-000000001001', '00000000-0000-0000-0000-000000000101', 'CH-001', 'Chamber 001', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001002', '00000000-0000-0000-0000-000000000101', 'CH-002', 'Chamber 002', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001003', '00000000-0000-0000-0000-000000000101', 'CH-003', 'Chamber 003', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001004', '00000000-0000-0000-0000-000000000101', 'CH-004', 'Chamber 004', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001005', '00000000-0000-0000-0000-000000000102', 'CH-005', 'Chamber 005', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001006', '00000000-0000-0000-0000-000000000102', 'CH-006', 'Chamber 006', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001007', '00000000-0000-0000-0000-000000000102', 'CH-007', 'Chamber 007', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001008', '00000000-0000-0000-0000-000000000102', 'CH-008', 'Chamber 008', 'running',    true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001009', '00000000-0000-0000-0000-000000000103', 'CH-009', 'Chamber 009', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001010', '00000000-0000-0000-0000-000000000103', 'CH-010', 'Chamber 010', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001011', '00000000-0000-0000-0000-000000000103', 'CH-011', 'Chamber 011', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001012', '00000000-0000-0000-0000-000000000103', 'CH-012', 'Chamber 012', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001013', '00000000-0000-0000-0000-000000000104', 'CH-013', 'Chamber 013', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001014', '00000000-0000-0000-0000-000000000104', 'CH-014', 'Chamber 014', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001015', '00000000-0000-0000-0000-000000000104', 'CH-015', 'Chamber 015', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001016', '00000000-0000-0000-0000-000000000104', 'CH-016', 'Chamber 016', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001017', '00000000-0000-0000-0000-000000000105', 'CH-017', 'Chamber 017', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001018', '00000000-0000-0000-0000-000000000105', 'CH-018', 'Chamber 018', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001019', '00000000-0000-0000-0000-000000000105', 'CH-019', 'Chamber 019', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001020', '00000000-0000-0000-0000-000000000105', 'CH-020', 'Chamber 020', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001021', '00000000-0000-0000-0000-000000000106', 'CH-021', 'Chamber 021', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001022', '00000000-0000-0000-0000-000000000106', 'CH-022', 'Chamber 022', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001023', '00000000-0000-0000-0000-000000000106', 'CH-023', 'Chamber 023', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00'),
  ('00000000-0000-0000-0000-000000001024', '00000000-0000-0000-0000-000000000106', 'CH-024', 'Chamber 024', 'configured', true, '{"source":"development-seed"}'::jsonb, timestamptz '2026-09-27 11:00:00+00', timestamptz '2026-09-27 11:00:00+00')
on conflict do nothing;

insert into public.experiments (
  id,
  experiment_code,
  name,
  description,
  status,
  started_at,
  metadata,
  created_at,
  updated_at
)
values (
  '00000000-0000-0000-0000-000000002001',
  'EXP-DEV-001',
  'Development Experiment 001',
  'Development seed experiment for dashboard testing.',
  'active',
  timestamptz '2026-09-27 12:00:00+00',
  '{"source":"development-seed"}'::jsonb,
  timestamptz '2026-09-27 11:00:00+00',
  timestamptz '2026-09-27 11:00:00+00'
)
on conflict do nothing;

insert into public.experiment_devices (experiment_id, device_id, role, assigned_at, metadata)
values (
  '00000000-0000-0000-0000-000000002001',
  '00000000-0000-0000-0000-000000000001',
  'primary',
  timestamptz '2026-09-27 12:00:00+00',
  '{"source":"development-seed"}'::jsonb
)
on conflict do nothing;

insert into public.experiment_nodes (id, experiment_id, node_id, assigned_at, metadata)
values
  ('00000000-0000-0000-0000-000000003001', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000000101', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000003002', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000000102', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb)
on conflict do nothing;

insert into public.experiment_sessions (
  id,
  experiment_id,
  session_number,
  status,
  started_at,
  metadata,
  created_at
)
values (
  '00000000-0000-0000-0000-000000005001',
  '00000000-0000-0000-0000-000000002001',
  1,
  'active',
  timestamptz '2026-09-27 12:00:00+00',
  '{"source":"development-seed"}'::jsonb,
  timestamptz '2026-09-27 11:00:00+00'
)
on conflict do nothing;

insert into public.experiment_chambers (
  id,
  experiment_id,
  chamber_id,
  assigned_at,
  metadata
)
values
  ('00000000-0000-0000-0000-000000004001', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001001', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004002', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001002', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004003', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001003', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004004', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001004', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004005', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001005', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004006', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001006', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004007', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001007', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb),
  ('00000000-0000-0000-0000-000000004008', '00000000-0000-0000-0000-000000002001', '00000000-0000-0000-0000-000000001008', timestamptz '2026-09-27 12:00:00+00', '{"source":"development-seed"}'::jsonb)
on conflict do nothing;

-- One minute per sample, 60 samples for each of the 8 active chambers: 480 rows.
-- The formulas are deterministic, varied, and intentionally unit-agnostic.
with active_chambers as (
  select
    ec.id as experiment_chamber_id,
    row_number() over (order by c.chamber_code)::integer as chamber_offset
  from public.experiment_chambers ec
  join public.chambers c on c.id = ec.chamber_id
  where ec.experiment_id = '00000000-0000-0000-0000-000000002001'
), samples as (
  select sample_index
  from generate_series(0, 59) as sample_series(sample_index)
)
insert into public.telemetry_readings (
  experiment_id,
  experiment_chamber_id,
  session_id,
  recorded_at,
  ch4,
  co2,
  ph,
  temperature,
  pressure,
  quality,
  source_sequence,
  received_at,
  metadata
)
select
  '00000000-0000-0000-0000-000000002001',
  active_chambers.experiment_chamber_id,
  '00000000-0000-0000-0000-000000005001',
  timestamptz '2026-09-27 12:00:00+00' + samples.sample_index * interval '1 minute',
  round((45.00000 + active_chambers.chamber_offset * 0.70000 + samples.sample_index * 0.12000 + mod(samples.sample_index, 7) * 0.03000)::numeric, 5),
  round((380.00000 + active_chambers.chamber_offset * 4.00000 + samples.sample_index * 0.65000 + mod(samples.sample_index, 5) * 0.08000)::numeric, 5),
  round((6.8000 + active_chambers.chamber_offset * 0.0150 + samples.sample_index * 0.0030)::numeric, 4),
  round((36.5000 + active_chambers.chamber_offset * 0.0800 + samples.sample_index * 0.0100)::numeric, 4),
  round((101.00000 + active_chambers.chamber_offset * 0.25000 + samples.sample_index * 0.04000)::numeric, 5),
  'valid',
  samples.sample_index + 1,
  timestamptz '2026-09-27 12:00:00+00' + samples.sample_index * interval '1 minute' + interval '5 seconds',
  '{"source":"development-seed"}'::jsonb
from active_chambers
cross join samples
on conflict (experiment_chamber_id, session_id, recorded_at) do nothing;

insert into public.device_status_events (
  device_id,
  experiment_id,
  recorded_at,
  status,
  message,
  metadata
)
select
  d.id,
  '00000000-0000-0000-0000-000000002001',
  timestamptz '2026-09-27 11:59:00+00',
  'online',
  'Development seed status event',
  '{"source":"development-seed"}'::jsonb
from public.devices d
where d.id = '00000000-0000-0000-0000-000000000001'
  and not exists (
    select 1
    from public.device_status_events e
    where e.device_id = d.id
      and e.experiment_id = '00000000-0000-0000-0000-000000002001'
      and e.recorded_at = timestamptz '2026-09-27 11:59:00+00'
      and e.status = 'online'
  );

insert into public.node_status_events (
  node_id,
  experiment_id,
  recorded_at,
  status,
  message,
  metadata
)
select
  n.id,
  '00000000-0000-0000-0000-000000002001',
  timestamptz '2026-09-27 11:59:00+00',
  n.status,
  'Development seed status event',
  '{"source":"development-seed"}'::jsonb
from public.nodes n
where n.device_id = '00000000-0000-0000-0000-000000000001'
  and not exists (
    select 1
    from public.node_status_events e
    where e.node_id = n.id
      and e.experiment_id = '00000000-0000-0000-0000-000000002001'
      and e.recorded_at = timestamptz '2026-09-27 11:59:00+00'
      and e.status = n.status
  );

insert into public.chamber_status_events (
  chamber_id,
  experiment_id,
  recorded_at,
  status,
  message,
  metadata
)
select
  c.id,
  '00000000-0000-0000-0000-000000002001',
  timestamptz '2026-09-27 11:59:00+00',
  c.status,
  'Development seed status event',
  '{"source":"development-seed"}'::jsonb
from public.chambers c
where not exists (
    select 1
    from public.chamber_status_events e
    where e.chamber_id = c.id
      and e.experiment_id = '00000000-0000-0000-0000-000000002001'
      and e.recorded_at = timestamptz '2026-09-27 11:59:00+00'
      and e.status = c.status
  );
