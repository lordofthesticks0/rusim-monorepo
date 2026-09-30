-- RUSIM MVP database schema.
-- Authentication, authorization policies, realtime publication, seed data,
-- experiment results, and user profile tables are intentionally deferred.

create table public.devices (
  id uuid primary key default gen_random_uuid(),
  device_code text not null,
  name text not null,
  serial_number text,
  status text not null default 'provisioning',
  last_seen_at timestamptz,
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  constraint devices_device_code_key unique (device_code),
  constraint devices_serial_number_key unique (serial_number),
  constraint devices_status_check check (
    status in ('provisioning', 'online', 'offline', 'degraded', 'maintenance', 'retired')
  )
);

create index devices_status_idx on public.devices (status);
create index devices_last_seen_at_idx on public.devices (last_seen_at);

create table public.nodes (
  id uuid primary key default gen_random_uuid(),
  device_id uuid not null references public.devices(id),
  node_code text not null,
  name text not null,
  status text not null default 'provisioning',
  last_seen_at timestamptz,
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  constraint nodes_device_code_key unique (device_id, node_code),
  constraint nodes_status_check check (
    status in ('provisioning', 'online', 'offline', 'degraded', 'fault', 'maintenance', 'retired')
  )
);

create index nodes_device_id_idx on public.nodes (device_id);
create index nodes_status_idx on public.nodes (status);
create index nodes_last_seen_at_idx on public.nodes (last_seen_at);

create table public.bottles (
  id uuid primary key default gen_random_uuid(),
  node_id uuid not null references public.nodes(id),
  bottle_code text not null,
  name text,
  status text not null default 'configured',
  enabled boolean not null default true,
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  constraint bottles_node_code_key unique (node_id, bottle_code),
  constraint bottles_status_check check (
    status in ('configured', 'ready', 'running', 'complete', 'empty', 'missing', 'fault', 'disabled')
  )
);

create index bottles_node_id_idx on public.bottles (node_id);
create index bottles_status_idx on public.bottles (status);
create index bottles_node_id_enabled_idx on public.bottles (node_id, enabled);

create table public.experiments (
  id uuid primary key default gen_random_uuid(),
  experiment_code text not null,
  name text not null,
  description text,
  status text not null default 'draft',
  planned_start_at timestamptz,
  started_at timestamptz,
  ended_at timestamptz,
  created_by uuid,
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  constraint experiments_experiment_code_key unique (experiment_code),
  constraint experiments_status_check check (
    status in ('draft', 'scheduled', 'active', 'paused', 'completed', 'cancelled', 'failed')
  ),
  constraint experiments_ended_at_check check (
    ended_at is null or started_at is null or ended_at >= started_at
  )
);

create index experiments_status_idx on public.experiments (status);
create index experiments_started_at_idx on public.experiments (started_at);
create index experiments_created_by_idx on public.experiments (created_by);

create table public.experiment_devices (
  experiment_id uuid not null references public.experiments(id),
  device_id uuid not null references public.devices(id),
  role text not null default 'primary',
  assigned_at timestamptz not null default now(),
  removed_at timestamptz,
  metadata jsonb not null default '{}'::jsonb,
  primary key (experiment_id, device_id),
  constraint experiment_devices_role_check check (
    role in ('primary', 'secondary', 'gateway', 'reference')
  ),
  constraint experiment_devices_removed_at_check check (
    removed_at is null or removed_at >= assigned_at
  )
);

create index experiment_devices_experiment_id_idx
  on public.experiment_devices (experiment_id);
create index experiment_devices_device_id_idx
  on public.experiment_devices (device_id);

create table public.experiment_nodes (
  id uuid primary key default gen_random_uuid(),
  experiment_id uuid not null references public.experiments(id),
  node_id uuid not null references public.nodes(id),
  assigned_at timestamptz not null default now(),
  removed_at timestamptz,
  metadata jsonb not null default '{}'::jsonb,
  constraint experiment_nodes_experiment_node_key unique (experiment_id, node_id),
  -- Supports telemetry's composite FK so the node association and experiment match.
  constraint experiment_nodes_id_experiment_key unique (id, experiment_id),
  constraint experiment_nodes_removed_at_check check (
    removed_at is null or removed_at >= assigned_at
  )
);

create index experiment_nodes_experiment_id_idx
  on public.experiment_nodes (experiment_id);
create index experiment_nodes_node_id_idx
  on public.experiment_nodes (node_id);

create table public.experiment_sessions (
  id uuid primary key default gen_random_uuid(),
  experiment_id uuid not null references public.experiments(id),
  session_number integer not null,
  status text not null default 'active',
  started_at timestamptz not null default now(),
  ended_at timestamptz,
  resume_of_session_id uuid,
  interruption_reason text,
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  constraint experiment_sessions_experiment_number_key unique (experiment_id, session_number),
  -- Supports telemetry's composite FK so the session and experiment match.
  constraint experiment_sessions_id_experiment_key unique (id, experiment_id),
  constraint experiment_sessions_resume_fk
    foreign key (resume_of_session_id, experiment_id)
    references public.experiment_sessions (id, experiment_id),
  constraint experiment_sessions_session_number_check check (session_number > 0),
  constraint experiment_sessions_status_check check (
    status in ('active', 'paused', 'interrupted', 'completed', 'abandoned')
  ),
  constraint experiment_sessions_ended_at_check check (
    ended_at is null or ended_at >= started_at
  )
);

create index experiment_sessions_experiment_id_idx
  on public.experiment_sessions (experiment_id);
create index experiment_sessions_experiment_number_idx
  on public.experiment_sessions (experiment_id, session_number);
create index experiment_sessions_status_idx
  on public.experiment_sessions (status);

create table public.telemetry_readings (
  id bigint generated always as identity primary key,
  experiment_id uuid not null references public.experiments(id),
  experiment_node_id uuid not null,
  session_id uuid not null,
  recorded_at timestamptz not null,
  ch4 numeric(14,5) not null,
  co2 numeric(14,5) not null,
  ph numeric(8,4) not null,
  temperature numeric(10,4) not null,
  pressure numeric(14,5) not null,
  quality text not null default 'valid',
  source_sequence bigint,
  received_at timestamptz not null default now(),
  metadata jsonb not null default '{}'::jsonb,
  -- Composite FKs prevent readings from crossing experiment boundaries.
  constraint telemetry_readings_experiment_node_fk
    foreign key (experiment_node_id, experiment_id)
    references public.experiment_nodes (id, experiment_id),
  constraint telemetry_readings_session_fk
    foreign key (session_id, experiment_id)
    references public.experiment_sessions (id, experiment_id),
  constraint telemetry_readings_quality_check check (
    quality in ('valid', 'suspect', 'invalid')
  ),
  constraint telemetry_readings_identity_key unique (
    experiment_node_id, session_id, recorded_at
  )
);

create index telemetry_readings_experiment_recorded_at_idx
  on public.telemetry_readings (experiment_id, recorded_at desc);
create index telemetry_readings_experiment_node_recorded_at_idx
  on public.telemetry_readings (experiment_node_id, recorded_at desc);
create index telemetry_readings_session_recorded_at_idx
  on public.telemetry_readings (session_id, recorded_at desc);
create index telemetry_readings_recorded_at_idx
  on public.telemetry_readings (recorded_at desc);

create table public.device_status_events (
  id bigint generated always as identity primary key,
  device_id uuid not null references public.devices(id),
  experiment_id uuid references public.experiments(id),
  recorded_at timestamptz not null default now(),
  status text not null,
  message text,
  metadata jsonb not null default '{}'::jsonb,
  constraint device_status_events_status_check check (
    status in ('provisioning', 'online', 'offline', 'degraded', 'maintenance', 'retired')
  )
);

create index device_status_events_device_recorded_at_idx
  on public.device_status_events (device_id, recorded_at desc);
create index device_status_events_experiment_recorded_at_idx
  on public.device_status_events (experiment_id, recorded_at desc);
create index device_status_events_status_idx
  on public.device_status_events (status);

create table public.node_status_events (
  id bigint generated always as identity primary key,
  node_id uuid not null references public.nodes(id),
  experiment_id uuid references public.experiments(id),
  recorded_at timestamptz not null default now(),
  status text not null,
  message text,
  metadata jsonb not null default '{}'::jsonb,
  constraint node_status_events_status_check check (
    status in ('provisioning', 'online', 'offline', 'degraded', 'fault', 'maintenance', 'retired')
  )
);

create index node_status_events_node_recorded_at_idx
  on public.node_status_events (node_id, recorded_at desc);
create index node_status_events_experiment_recorded_at_idx
  on public.node_status_events (experiment_id, recorded_at desc);
create index node_status_events_status_idx
  on public.node_status_events (status);

create table public.bottle_status_events (
  id bigint generated always as identity primary key,
  bottle_id uuid not null references public.bottles(id),
  experiment_id uuid references public.experiments(id),
  recorded_at timestamptz not null default now(),
  status text not null,
  message text,
  metadata jsonb not null default '{}'::jsonb,
  constraint bottle_status_events_status_check check (
    status in ('configured', 'ready', 'running', 'complete', 'empty', 'missing', 'fault', 'disabled')
  )
);

create index bottle_status_events_bottle_recorded_at_idx
  on public.bottle_status_events (bottle_id, recorded_at desc);
create index bottle_status_events_experiment_recorded_at_idx
  on public.bottle_status_events (experiment_id, recorded_at desc);
create index bottle_status_events_status_idx
  on public.bottle_status_events (status);

create function public.set_updated_at()
returns trigger
language plpgsql
as $$
begin
  new.updated_at = now();
  return new;
end;
$$;

create trigger devices_set_updated_at
before update on public.devices
for each row execute function public.set_updated_at();

create trigger nodes_set_updated_at
before update on public.nodes
for each row execute function public.set_updated_at();

create trigger bottles_set_updated_at
before update on public.bottles
for each row execute function public.set_updated_at();

create trigger experiments_set_updated_at
before update on public.experiments
for each row execute function public.set_updated_at();

alter table public.devices enable row level security;
alter table public.nodes enable row level security;
alter table public.bottles enable row level security;
alter table public.experiments enable row level security;
alter table public.experiment_devices enable row level security;
alter table public.experiment_nodes enable row level security;
alter table public.experiment_sessions enable row level security;
alter table public.telemetry_readings enable row level security;
alter table public.device_status_events enable row level security;
alter table public.node_status_events enable row level security;
alter table public.bottle_status_events enable row level security;
