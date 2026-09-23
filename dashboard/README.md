# RuSim dashboard

Install dependencies with Bun:

```bash
bun install
```

Start the development server:

```bash
bun run dev
```

Create `.env.local` from `.env.example` and add your Supabase project URL and anon key. The temporary data preview remains visible until the `time_series_readings` table returns rows.

Expected table columns: `device_id`, `recorded_at`, and `parameter_1` through `parameter_5`.

Netlify is configured in `netlify.toml`; use `bun run build` for a production build or `bunx netlify deploy` to deploy.
