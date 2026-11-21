import { describe, expect, mock, test } from "bun:test";
import { PostgrestClient } from "@supabase/postgrest-js";

/**
 * A fake PostgREST server over an in-memory readings table.
 *
 * It understands only what fetchExperimentReadings sends: the keyset `or`
 * filter, `limit`, and the (timestamp, bottle_id) ordering. The real
 * postgrest-js client builds the request, so the URL shape is exercised too.
 */
const requests: URL[] = [];
let table: Array<Record<string, unknown>> = [];

function fakeFetch(input: RequestInfo | URL): Promise<Response> {
  const url = new URL(String(input));
  requests.push(url);
  let rows = table;

  const or = url.searchParams.get("or");
  if (or) {
    // postgrest-js wraps the filter as or=(...), which PostgREST requires.
    const m = /^\(timestamp\.gt\."([^"]+)",and\(timestamp\.eq\."([^"]+)",bottle_id\.gt\.(\d+)\)\)$/.exec(or);
    if (!m) throw new Error(`unexpected or filter: ${or}`);
    const [, after, same, bottle] = m;
    rows = rows.filter(
      (r) =>
        (r.timestamp as string) > after! ||
        ((r.timestamp as string) === same && (r.bottle_id as number) > Number(bottle)),
    );
  }
  const limit = Number(url.searchParams.get("limit"));
  return Promise.resolve(
    new Response(JSON.stringify(rows.slice(0, limit)), {
      status: 200,
      headers: { "Content-Type": "application/json" },
    }),
  );
}

mock.module("./supabase", () => ({
  getSupabase: () => new PostgrestClient("http://fake.local/rest/v1", { fetch: fakeFetch as typeof fetch }),
}));

const { fetchExperimentReadings, MAX_EXPERIMENT_ROWS } = await import("./data");

/** `rounds` sample times, each holding one row per bottle, already in key order. */
function buildTable(rounds: number, bottles: number) {
  const out: Array<Record<string, unknown>> = [];
  for (let i = 0; i < rounds; i++) {
    const timestamp = new Date(Date.UTC(2026, 0, 1) + i * 30_000).toISOString().replace("Z", "+00:00");
    for (let b = 0; b < bottles; b++) {
      out.push({
        device_id: 1,
        bottle_id: b,
        experiment_id: 1,
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

describe("fetchExperimentReadings keyset paging", () => {
  test("returns every row once when a page boundary splits a round", async () => {
    // 3 bottles x 700 rounds = 2100 rows; pages of 1000 end mid-round (1000 % 3 = 1).
    table = buildTable(700, 3);
    requests.length = 0;

    const result = await fetchExperimentReadings("session", 1);

    expect(result).toHaveLength(2100);
    const keys = result.map((r) => `${r.timestamp}#${r.bottle_id}`);
    expect(new Set(keys).size).toBe(2100);
    expect(keys).toEqual(table.map((r) => `${r.timestamp}#${r.bottle_id}`));
    expect(requests).toHaveLength(3);
    expect(requests[0]!.searchParams.get("or")).toBeNull();
    expect(requests[1]!.searchParams.get("or")).not.toBeNull();
  });

  test("stops after a short page without an extra request", async () => {
    table = buildTable(10, 4);
    requests.length = 0;

    expect(await fetchExperimentReadings("session", 1)).toHaveLength(40);
    expect(requests).toHaveLength(1);
  });

  test("at the cap, every bottle is cut off at the same time, never dropped", async () => {
    table = buildTable(20_000, 3); // 60,000 rows, over the cap
    requests.length = 0;

    const result = await fetchExperimentReadings("session", 1);

    expect(result).toHaveLength(MAX_EXPERIMENT_ROWS);
    for (const bottle of [0, 1, 2]) {
      const mine = result.filter((r) => r.bottle_id === bottle);
      expect(mine.length).toBeGreaterThan(16_000);
    }
    // Oldest first: the missing rows are the end of the experiment.
    expect(result[0]!.timestamp).toBe(table[0]!.timestamp as string);
  });
});
