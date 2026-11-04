# Graph Report - dashboard  (2026-11-04)

## Corpus Check
- 38 files · ~24,142 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 7 file(s) not represented in the graph (top: (none) 2, .toml 2, .example 1)

## Summary
- 235 nodes · 502 edges · 10 communities
- Extraction: 99% EXTRACTED · 1% INFERRED · 0% AMBIGUOUS · INFERRED: 5 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `3c6dff80`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- App.tsx
- package.json
- dev-db.ts
- data.ts
- compilerOptions
- monitoring.test.ts
- csv.ts
- history.ts
- database.ts
- compare.ts

## God Nodes (most connected - your core abstractions)
1. `Dashboard()` - 25 edges
2. `compilerOptions` - 20 edges
3. `ExperimentHistoryView()` - 16 edges
4. `CompareView()` - 15 edges
5. `displayChamberNumber()` - 15 edges
6. `main()` - 12 edges
7. `Chart()` - 12 edges
8. `CompareChart()` - 11 edges
9. `Reading` - 10 edges
10. `formatExperimentId()` - 9 edges

## Surprising Connections (you probably didn't know these)
- `seedReadings()` --calls--> `displayChamberNumber()`  [EXTRACTED]
  scripts/dev-db.ts → src/chambers.ts
- `App()` --calls--> `useAuth()`  [EXTRACTED]
  src/App.tsx → src/auth.ts
- `Dashboard()` --calls--> `displayChamberNumber()`  [EXTRACTED]
  src/App.tsx → src/chambers.ts
- `Dashboard()` --calls--> `internalChamberIds()`  [EXTRACTED]
  src/App.tsx → src/chambers.ts
- `Dashboard()` --calls--> `splitBottlesByPresence()`  [EXTRACTED]
  src/App.tsx → src/chambers.ts

## Import Cycles
- None detected.

## Communities (10 total, 0 thin omitted)

### Community 0 - "App.tsx"
Cohesion: 0.11
Nodes (35): App(), BottleChart(), BrandMark(), Chart(), ChartInteraction, chartPath(), CompareActivePoint, CompareChart() (+27 more)

### Community 1 - "package.json"
Cohesion: 0.08
Nodes (31): dependencies, react@19.1.1, react-dom@19.1.1, @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13 (+23 more)

### Community 2 - "dev-db.ts"
Cohesion: 0.09
Nodes (26): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+18 more)

### Community 3 - "data.ts"
Cohesion: 0.14
Nodes (21): AuthState, readStoredSession(), StoredSession, useAuth(), writeStoredSession(), authenticate(), CONCLUDED_AFTER_MS, fetchDevice() (+13 more)

### Community 4 - "compilerOptions"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 5 - "monitoring.test.ts"
Cohesion: 0.20
Nodes (13): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), ExperimentSummary, DashboardView, DEV_FORCE_LIVE_EXPERIMENT, generateSyntheticLiveReadings(), isMonitoringPollingEligible() (+5 more)

### Community 6 - "csv.ts"
Cohesion: 0.18
Nodes (13): RFC-4180, downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+5 more)

### Community 7 - "history.ts"
Cohesion: 0.21
Nodes (14): ParameterStats, ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails, parameterValue() (+6 more)

### Community 8 - "database.ts"
Cohesion: 0.13
Nodes (14): buildClient(), DeviceDataClient, readConfig(), SESSION_HEADER, CompositeTypes, Constants, Database, DatabaseWithoutInternals (+6 more)

### Community 9 - "compare.ts"
Cohesion: 0.30
Nodes (12): CompareView(), displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor(), compareSeriesLabel() (+4 more)

## Knowledge Gaps
- **1 isolated node(s):** `RFC-4180`
  These have ≤1 connection - possible missing edges. (Counts symbols only; 87 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `react` connect `package.json` to `App.tsx`, `data.ts`?**
  _High betweenness centrality (0.166) - this node is a cross-community bridge._
- **What connects `RFC-4180` to the rest of the system?**
  _1 weakly-connected nodes found - possible missing edges._
- **Should `App.tsx` be split into smaller, more focused modules?**
  _Cohesion score 0.11153846153846154 - nodes in this community are weakly interconnected._
- **Why does `displayChamberNumber()` connect `compare.ts` to `App.tsx`, `dev-db.ts`, `data.ts`, `monitoring.test.ts`, `csv.ts`?**
  _High betweenness centrality (0.120) - this node is a cross-community bridge._
- **Should `package.json` be split into smaller, more focused modules?**
  _Cohesion score 0.08143939393939394 - nodes in this community are weakly interconnected._
- **Should `dev-db.ts` be split into smaller, more focused modules?**
  _Cohesion score 0.08870967741935484 - nodes in this community are weakly interconnected._
- **Should `data.ts` be split into smaller, more focused modules?**
  _Cohesion score 0.13666666666666666 - nodes in this community are weakly interconnected._