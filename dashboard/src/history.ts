import type { ParameterName, ParameterStats } from "./data";
import type { Reading } from "./types/domain";

const HISTORY_PARAMETERS: readonly ParameterName[] = ["pH", "Temp", "Pressure", "CO2", "CH4"];

export type ChamberSummary = {
  bottle_id: number;
  readings: Reading[];
  firstReading: Reading;
  lastReading: Reading;
  stats: Record<ParameterName, ParameterStats | null>;
};

function timestampValue(timestamp: string): number {
  const value = Date.parse(timestamp);
  return Number.isNaN(value) ? Number.POSITIVE_INFINITY : value;
}

export function sortHistoryReadings(readings: Reading[]): Reading[] {
  return [...readings].sort((left, right) => {
    const timestampDifference = timestampValue(left.timestamp) - timestampValue(right.timestamp);
    if (timestampDifference !== 0) return timestampDifference;
    return left.bottle_id - right.bottle_id;
  });
}

export function getExperimentChambers(readings: Reading[], experimentId: number | null): number[] {
  if (experimentId === null) return [];

  return [...new Set(
    readings
      .filter((reading) => reading.experiment_id === experimentId)
      .map((reading) => reading.bottle_id),
  )].sort((left, right) => left - right);
}

export function selectChamberReadings(
  readings: Reading[],
  experimentId: number | null,
  bottleId: number | null,
): Reading[] {
  if (experimentId === null || bottleId === null) return [];

  return sortHistoryReadings(
    readings.filter(
      (reading) => reading.experiment_id === experimentId && reading.bottle_id === bottleId,
    ),
  );
}

function parameterValue(reading: Reading, parameter: ParameterName): number | null {
  switch (parameter) {
    case "pH":
      return reading.ph;
    case "Temp":
      return reading.temp;
    case "Pressure":
      return reading.pressure;
    case "CO2":
      return reading.co2;
    case "CH4":
      return reading.ch4;
  }

  return null;
}

export type HistoryPointDetails = {
  timestamp: string;
  parameter: ParameterName;
  value: number | null;
};

export function getHistoryPointDetails(reading: Reading, parameter: ParameterName): HistoryPointDetails {
  return {
    timestamp: reading.timestamp,
    parameter,
    value: parameterValue(reading, parameter),
  };
}

export function toggleExperimentSelection(currentId: number | null, clickedId: number): number | null {
  return currentId === clickedId ? null : clickedId;
}

function summarizeValues(values: Array<number | null>): ParameterStats | null {
  const numericValues = values.filter((value): value is number => value !== null);
  if (numericValues.length === 0) return null;

  return {
    min: Math.min(...numericValues),
    average: numericValues.reduce((sum, value) => sum + value, 0) / numericValues.length,
    max: Math.max(...numericValues),
  };
}

function summarizeChamber(readings: Reading[]): Record<ParameterName, ParameterStats | null> {
  return Object.fromEntries(
    HISTORY_PARAMETERS.map((parameter) => [
      parameter,
      summarizeValues(readings.map((reading) => parameterValue(reading, parameter))),
    ]),
  ) as Record<ParameterName, ParameterStats | null>;
}

export function groupReadingsByChamber(
  readings: Reading[],
  experimentId: number | null,
): ChamberSummary[] {
  if (experimentId === null) return [];

  const grouped = new Map<number, Reading[]>();
  for (const reading of readings) {
    if (reading.experiment_id !== experimentId) continue;
    const chamberReadings = grouped.get(reading.bottle_id) ?? [];
    chamberReadings.push(reading);
    grouped.set(reading.bottle_id, chamberReadings);
  }

  return [...grouped.entries()]
    .sort(([left], [right]) => left - right)
    .map(([bottle_id, chamberReadings]) => {
      const orderedReadings = sortHistoryReadings(chamberReadings);
      return {
        bottle_id,
        readings: orderedReadings,
        firstReading: orderedReadings[0]!,
        lastReading: orderedReadings[orderedReadings.length - 1]!,
        stats: summarizeChamber(orderedReadings),
      };
    });
}
