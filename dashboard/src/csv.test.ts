import { describe, expect, test } from "bun:test";
import { toCsv } from "./csv";
import type { Reading } from "./types/domain";

function reading(bottle_id: number, timestamp: string): Reading {
  return {
    device_id: 1,
    bottle_id,
    experiment_id: 1,
    timestamp,
    ph: 7,
    pressure: 100,
    temp: 20,
    co2: 300,
    ch4: 40,
  };
}

describe("CSV chamber numbering", () => {
  test("exports internal ids as displayed chamber numbers exactly once", () => {
    const csv = toCsv([
      reading(0, "2026-01-01T00:01:00.000Z"),
      reading(7, "2026-01-01T00:02:00.000Z"),
    ]);
    const [header, first, second] = csv.split("\r\n");

    expect(header).toBe("bottle_id,timestamp,pH,Temp (°C),CO2 (ppm),CH4 (ppm),Pressure (kPa)");
    expect(first!.startsWith("1,")).toBe(true);
    expect(second!.startsWith("8,")).toBe(true);
  });

  test("orders rows by chamber id after timestamp", () => {
    const csv = toCsv([
      reading(7, "2026-01-01T00:01:00.000Z"),
      reading(0, "2026-01-01T00:01:00.000Z"),
    ]);
    const [, first, second] = csv.split("\r\n");

    expect(first!.startsWith("1,")).toBe(true);
    expect(second!.startsWith("8,")).toBe(true);
  });
});
