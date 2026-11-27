import { describe, expect, mock, test } from "bun:test";
import { PostgrestClient } from "@supabase/postgrest-js";

/**
 * A fake PostgREST server over an in-memory readings table.
 *
 * It understands only what data.ts sends: eq/lt/gte column filters, the
 * newest-first keyset `or` filter, multi-column `order`, `select` and `limit`.
 * The real postgrest-js client builds the request, so the URL shape is
 * exercised too.
 */
const requests: URL[] = [];
let table: Array<Record<string, unknown>> = [];

const RESERVED = new Set(["select", "order", "limit", "or"]);

const parsed = new Map<string, number>();

/** Timestamps compare as instants (parsed once each), every other column as a number. */
function value(column: string, raw: unknown): number {
  if (column !== "timestamp") return Number(raw);
  const text = String(raw);
  let instant = parsed.get(text);
  if (instant === undefined) parsed.set(text, (instant = Date.parse(text)));
  return instant;
}

function fakeFetch(input: RequestInfo | URL): Promise<Response> {
  const url = new URL(String(input));
  requests.push(url);
  let rows = table;

  for (const [column, filter] of url.searchParams) {
    if (RESERVED.has(column)) continue;
    const [, op, operand] = /^(eq|lt|gte)\.(.*)$/.exec(filter) ?? [];
    if (!op) throw new Error(`unexpected filter: ${column}=${filter}`);
    const target = value(column, operand);
    rows = rows.filter((row) => {
      const actual = value(column, row[column]);
      return op === "eq" ? actual === target : op === "lt" ? actual < target : actual >= target;
    });
  }

  const or = url.searchParams.get("or");
  if (or) {
    // postgrest-js wraps the filter as or=(...), which PostgREST requires.
    const m = /^\(timestamp\.lt\."([^"]+)",and\(timestamp\.eq\."([^"]+)",bottle_id\.lt\.(\d+)\)\)$/.exec(or);
    if (!m) throw new Error(`unexpected or filter: ${or}`);
    const before = Date.parse(m[1]!);
    const same = Date.parse(m[2]!);
    const bottle = Number(m[3]);
    rows = rows.filter((row) => {
      const at = Date.parse(row.timestamp as string);
      return at < before || (at === same && (row.bottle_id as number) < bottle);
    });
  }

  const order = (url.searchParams.get("order") ?? "").split(",").filter(Boolean).map((part) => {
    const [column, direction] = part.split(".");
    return { column: column!, sign: direction === "desc" ? -1 : 1 };
  });
  rows = [...rows].sort((left, right) => {
    for (const { column, sign } of order) {
      const difference = value(column, left[column]) - value(column, right[column]);
      if (difference !== 0) return sign * difference;
    }
    return 0;
  });

  const columns = (url.searchParams.get("select") ?? "*").split(",");
  const projected = rows
    .slice(0, Number(url.searchParams.get("limit")))
    .map((row) => (columns[0] === "*" ? row : Object.fromEntries(columns.map((column) => [column, row[column]]))));
  return Promise.resolve(
    new Response(JSON.stringify(projected), {
      status: 200,
      headers: { "Content-Type": "application/json" },
    }),
  );
}

mock.module("./supabase", () => ({
  getSupabase: () => new PostgrestClient("http://fake.local/rest/v1", { fetch: fakeFetch as typeof fetch }),
}));

const {
  EXPERIMENT_WINDOW_MS,
  fetchExperimentList,
  fetchExperimentReadings,
  fetchLatestReading,
  MAX_EXPERIMENT_ROWS,
} = await import("./data");

/** Rounds in buildTable are 30 s apart, the firmware default. */
const ROUND_MS = 30_000;

/**
 * `rounds` sample times 30 s apart, each holding one row per bottle, in key
 * order. `startRound` offsets the clock so experiments do not overlap.
 */
function buildTable(rounds: number, bottles: number, experimentId = 1, startRound = 0) {
  const out: Array<Record<string, unknown>> = [];
  for (let i = startRound; i < startRound + rounds; i++) {
    const timestamp = new Date(Date.UTC(2026, 0, 1) + i * ROUND_MS).toISOString().replace("Z", "+00:00");
    for (let b = 0; b < bottles; b++) {
      out.push({
        device_id: 1,
        bottle_id: b,
        experiment_id: experimentId,
        timestamp,
        ph: 7,
        pressure: 101_000,
        temp: 38,
        co2: 1000,
        ch4: 500,
      });
    }
  }
  return out;
}

/** Row identity that ignores timestamp formatting ("Z" vs "+00:00"). */
const key = (row: Record<string, unknown> | { timestamp: string; bottle_id: number }) =>
  `${Date.parse(String((row as Record<string, unknown>).timestamp))}#${(row as Record<string, unknown>).bottle_id}`;

describe("fetchExperimentReadings (newest first, one-day window)", () => {
  test("the window is 24 h and the ceiling is one window of 30 s rounds for 24 chambers", () => {
    expect(EXPERIMENT_WINDOW_MS).toBe(24 * 60 * 60 * 1000);
    expect(MAX_EXPERIMENT_ROWS).toBe(24 * 2880);
  });

  test("returns every row once, oldest first, when a page boundary splits a round", async () => {
    // 3 bottles x 700 rounds = 2100 rows (under 6 h); pages of 1000 end mid-round.
    table = buildTable(700, 3);
    requests.length = 0;

    const { readings, truncated } = await fetchExperimentReadings("session", 1);

    expect(readings).toHaveLength(2100);
    const keys = readings.map(key);
    expect(new Set(keys).size).toBe(2100);
    expect(keys).toEqual(table.map(key));
    expect(truncated).toBe(false);
    expect(requests).toHaveLength(3);
    expect(requests[0]!.searchParams.get("or")).toBeNull();
    expect(requests[1]!.searchParams.get("or")).not.toBeNull();
  });

  test("stops after a short page without an extra request", async () => {
    table = buildTable(10, 4);
    requests.length = 0;

    const { readings, truncated } = await fetchExperimentReadings("session", 1);

    expect(readings).toHaveLength(40);
    expect(truncated).toBe(false);
    expect(requests).toHaveLength(1);
  });

  test("keeps the last 24 h before the newest sample and marks the older start as cut", async () => {
    table = buildTable(3000, 3); // 25 h of 30 s rounds
    requests.length = 0;

    const { readings, truncated } = await fetchExperimentReadings("session", 1);

    // The window is inclusive: the round exactly 24 h before the newest counts.
    const rounds = EXPERIMENT_WINDOW_MS / ROUND_MS + 1;
    expect(readings).toHaveLength(rounds * 3);
    expect(key(readings.at(-1)!)).toBe(key(table.at(-1)!));
    expect(key(readings[0]!)).toBe(key(table[table.length - rounds * 3]!));
    expect(truncated).toBe(true);
    // Paging stopped at the first page reaching past the window.
    expect(requests).toHaveLength(Math.ceil((rounds * 3) / 1000));
  });

  test("an experiment of exactly one day is not marked truncated", async () => {
    table = buildTable(EXPERIMENT_WINDOW_MS / ROUND_MS + 1, 2);

    const { readings, truncated } = await fetchExperimentReadings("session", 1);

    expect(readings).toHaveLength(table.length);
    expect(truncated).toBe(false);
  });

  test("at the row ceiling, keeps the newest rows and cuts every bottle at the same time", async () => {
    table = buildTable(2000, 40); // 80,000 rows within 17 h: the ceiling binds first

    const { readings, truncated } = await fetchExperimentReadings("session", 1);

    expect(readings).toHaveLength(MAX_EXPERIMENT_ROWS);
    for (const bottle of [0, 17, 39]) {
      expect(readings.filter((r) => r.bottle_id === bottle)).toHaveLength(MAX_EXPERIMENT_ROWS / 40);
    }
    expect(key(readings.at(-1)!)).toBe(key(table.at(-1)!));
    expect(truncated).toBe(true);
  });

  test("reads only the requested experiment", async () => {
    table = [...buildTable(5, 2, 1, 0), ...buildTable(5, 2, 2, 100)];

    const { readings } = await fetchExperimentReadings("session", 2);

    expect(readings).toHaveLength(10);
    expect(readings.every((r) => r.experiment_id === 2)).toBe(true);
  });

  test("`since` limits the read to rows at or after that time, without the window", async () => {
    table = buildTable(100, 2);
    const since = new Date(Date.parse(table[180]!.timestamp as string)).toISOString();

    const { readings, truncated } = await fetchExperimentReadings("session", 1, { since });

    expect(readings.map(key)).toEqual(table.slice(180).map(key));
    expect(truncated).toBe(false);
  });
});

describe("fetchLatestReading", () => {
  test("returns the newest row across experiments", async () => {
    table = [...buildTable(5, 3, 1, 0), ...buildTable(5, 3, 2, 100)];

    const latest = await fetchLatestReading("session");

    expect(latest?.experiment_id).toBe(2);
    expect(key(latest!)).toBe(key(table.at(-1)!));
  });

  test("returns null when the device has no readings", async () => {
    table = [];
    expect(await fetchLatestReading("session")).toBeNull();
  });
});

describe("fetchExperimentList skip scan", () => {
  test("finds every experiment, newest first, with its true first and last sample", async () => {
    table = [...buildTable(4, 2, 0, 0), ...buildTable(6, 2, 2, 50), ...buildTable(3, 2, 5, 200)];
    requests.length = 0;

    const list = await fetchExperimentList("session", Date.UTC(2030, 0, 1));

    expect(list.map((experiment) => experiment.experiment_id)).toEqual([5, 2, 0]);
    const two = list[1]!;
    expect(Date.parse(two.first_timestamp!)).toBe(Date.parse(buildTable(1, 1, 2, 50)[0]!.timestamp as string));
    expect(Date.parse(two.last_timestamp!)).toBe(Date.parse(buildTable(1, 1, 2, 55)[0]!.timestamp as string));
    expect(list.every((experiment) => experiment.concluded)).toBe(true);
    // 3 ids + 1 empty probe, then 2 bounds queries per experiment.
    expect(requests).toHaveLength(4 + 3 * 2);
  });

  test("returns an empty list for a device with no readings", async () => {
    table = [];
    expect(await fetchExperimentList("session")).toEqual([]);
  });
});
