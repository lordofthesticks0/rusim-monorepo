-- Temporary development-only read access for the unauthenticated dashboard.
-- Replace these public read policies before production if telemetry is not
-- intended to be publicly accessible.

revoke all on table public.devices from anon, authenticated;
grant select on table public.devices to anon, authenticated;

create policy "dev anon authenticated can read devices"
on public.devices
for select
to anon, authenticated
using (true);

revoke all on table public.nodes from anon, authenticated;
grant select on table public.nodes to anon, authenticated;

create policy "dev anon authenticated can read nodes"
on public.nodes
for select
to anon, authenticated
using (true);

revoke all on table public.chambers from anon, authenticated;
grant select on table public.chambers to anon, authenticated;

create policy "dev anon authenticated can read chambers"
on public.chambers
for select
to anon, authenticated
using (true);

revoke all on table public.experiment_chambers from anon, authenticated;
grant select on table public.experiment_chambers to anon, authenticated;

create policy "dev anon authenticated can read experiment chambers"
on public.experiment_chambers
for select
to anon, authenticated
using (true);

revoke all on table public.telemetry_readings from anon, authenticated;
grant select on table public.telemetry_readings to anon, authenticated;

create policy "dev anon authenticated can read telemetry readings"
on public.telemetry_readings
for select
to anon, authenticated
using (true);
