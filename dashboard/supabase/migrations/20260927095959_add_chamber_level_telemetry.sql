-- Move telemetry ownership from experiment nodes to experiment-specific chambers.
-- The prior MVP migration is already deployed and is intentionally not modified.

alter table public.bottles rename to chambers;
alter table public.chambers rename column bottle_code to chamber_code;
alter table public.chambers
  rename constraint bottles_node_code_key to chambers_node_code_key;
alter table public.chambers
  rename constraint bottles_status_check to chambers_status_check;

alter index public.bottles_node_id_idx rename to chambers_node_id_idx;
alter index public.bottles_status_idx rename to chambers_status_idx;
alter index public.bottles_node_id_enabled_idx rename to chambers_node_id_enabled_idx;
alter trigger bottles_set_updated_at on public.chambers
  rename to chambers_set_updated_at;

alter table public.bottle_status_events rename to chamber_status_events;
alter table public.chamber_status_events rename column bottle_id to chamber_id;
alter table public.chamber_status_events
  rename constraint bottle_status_events_status_check to chamber_status_events_status_check;

alter index public.bottle_status_events_bottle_recorded_at_idx
  rename to chamber_status_events_chamber_recorded_at_idx;
alter index public.bottle_status_events_experiment_recorded_at_idx
  rename to chamber_status_events_experiment_recorded_at_idx;
alter index public.bottle_status_events_status_idx
  rename to chamber_status_events_status_idx;

create table public.experiment_chambers (
  id uuid primary key default gen_random_uuid(),
  experiment_id uuid not null references public.experiments(id),
  chamber_id uuid not null references public.chambers(id),
  assigned_at timestamptz not null default now(),
  removed_at timestamptz,
  metadata jsonb not null default '{}'::jsonb,
  constraint experiment_chambers_experiment_chamber_key unique (experiment_id, chamber_id),
  constraint experiment_chambers_id_experiment_key unique (id, experiment_id),
  constraint experiment_chambers_removed_at_check check (
    removed_at is null or removed_at >= assigned_at
  )
);

create index experiment_chambers_experiment_id_idx
  on public.experiment_chambers (experiment_id);
create index experiment_chambers_chamber_id_idx
  on public.experiment_chambers (chamber_id);

alter table public.experiment_chambers enable row level security;

alter table public.telemetry_readings
  drop constraint telemetry_readings_experiment_node_fk;
alter table public.telemetry_readings
  drop constraint telemetry_readings_identity_key;

drop index public.telemetry_readings_experiment_node_recorded_at_idx;

alter table public.telemetry_readings
  drop column experiment_node_id;
alter table public.telemetry_readings
  add column experiment_chamber_id uuid not null;

-- The composite FK prevents telemetry from crossing experiment boundaries.
alter table public.telemetry_readings
  add constraint telemetry_readings_experiment_chamber_fk
  foreign key (experiment_chamber_id, experiment_id)
  references public.experiment_chambers (id, experiment_id);

alter table public.telemetry_readings
  add constraint telemetry_readings_identity_key unique (
    experiment_chamber_id, session_id, recorded_at
  );

create index telemetry_readings_experiment_chamber_recorded_at_idx
  on public.telemetry_readings (experiment_chamber_id, recorded_at desc);
