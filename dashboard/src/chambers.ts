import type { Reading } from "./types/domain";

/**
 * Supabase and ESP32 keep zero-based internal bottle ids. The dashboard maps
 * those ids to 1-based chamber numbers only when showing them to a person.
 *
 * Keeping this conversion in one module prevents display code from applying
 * the offset twice. This module has no Supabase import so it can be tested
 * without a configured client.
 */
export const FIRST_INTERNAL_CHAMBER_ID = 0;

/** Maps the stored/ESP32 bottle id to its user-facing chamber number. */
export function displayChamberNumber(internalId: number): number {
  return internalId + 1;
}

/** Returns internal ids for all chambers exposed by the device. */
export function internalChamberIds(chamberCount: number): number[] {
  return Array.from(
    { length: Math.max(0, chamberCount) },
    (_, index) => FIRST_INTERNAL_CHAMBER_ID + index,
  );
}

/**
 * Determines which chambers reported at a given sample time.
 *
 * A chamber that is configured on the device but has no row at the latest sample
 * is treated as disconnected: it sent nothing at all for that timestamp. This is
 * distinct from a row that exists with null measurements, which means the chamber
 * reported but that one sensor failed.
 *
 * Returns the internal ids found in Supabase, including for disconnected chambers.
 */
export function splitBottlesByPresence(
  readings: Reading[],
  bottleCount: number,
): { latestSample: string | null; connected: number[]; disconnected: number[] } {
  const all = internalChamberIds(bottleCount);
  if (readings.length === 0) {
    return { latestSample: null, connected: [], disconnected: all };
  }

  const latestSample = readings.reduce(
    (latest, reading) => (reading.timestamp > latest ? reading.timestamp : latest),
    readings[0]!.timestamp,
  );
  const reporting = new Set(
    readings.filter((reading) => reading.timestamp === latestSample).map((reading) => reading.bottle_id),
  );

  return {
    latestSample,
    connected: all.filter((chamberId) => reporting.has(chamberId)),
    disconnected: all.filter((chamberId) => !reporting.has(chamberId)),
  };
}
