-- Temporary development-only public read access for experiment display.
-- Review and replace this policy before production if experiment data is not
-- intended to be publicly accessible.

revoke all on table public.experiments from anon, authenticated;
grant select on table public.experiments to anon, authenticated;

create policy "dev anon authenticated can read experiments"
on public.experiments
for select
to anon, authenticated
using (true);
