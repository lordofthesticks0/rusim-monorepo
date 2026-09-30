-- Temporary development dashboard read access for experiment sessions.
-- Tighten or replace this public read policy before production if session data
-- is not intended to be publicly accessible.

revoke all on table public.experiment_sessions from anon, authenticated;
grant select on table public.experiment_sessions to anon, authenticated;

do $$
begin
  if not exists (
    select 1
    from pg_policies
    where schemaname = 'public'
      and tablename = 'experiment_sessions'
      and policyname = 'dev anon authenticated can read experiment sessions'
  ) then
    execute $policy$
      create policy "dev anon authenticated can read experiment sessions"
      on public.experiment_sessions
      for select
      to anon, authenticated
      using (true)
    $policy$;
  end if;
end
$$;
