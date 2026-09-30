-- Add the dashboard's current-state and telemetry tables to Supabase Realtime.
-- Existing publication membership is checked so this migration can be applied
-- safely when one or more tables are already present in supabase_realtime.
do $$
declare
  target_table text;
begin
  for target_table in
    select table_name
    from (values
      ('telemetry_readings'),
      ('devices'),
      ('nodes'),
      ('chambers'),
      ('experiments')
    ) as requested(table_name)
  loop
    if not exists (
      select 1
      from pg_publication_tables
      where pubname = 'supabase_realtime'
        and schemaname = 'public'
        and tablename = target_table
    ) then
      execute format(
        'alter publication supabase_realtime add table public.%I',
        target_table
      );
    end if;
  end loop;
end
$$;
