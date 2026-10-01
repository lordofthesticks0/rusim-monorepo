import { getParameterValue, PARAMETER_NAMES, PARAMETER_UNITS, type ParameterName } from "./data";
import type { Reading } from "./types/domain";

/**
 * The export's columns, in the order the dashboard shows its parameters.
 *
 * Each sensor header carries its unit, so a downloaded file can be read without
 * the dashboard. Parameter names and units come from data.ts rather than being
 * repeated here, and getParameterValue does the field lookup, so a sensor cannot
 * end up exported under the wrong name or column.
 */
const COLUMNS: ReadonlyArray<string | ParameterName> = [
  "bottle_id",
  "timestamp",
  ...PARAMETER_NAMES.map(withUnit),
];

const NEEDS_QUOTING = /[",\r\n]/;

/**
 * Serialises one experiment's readings as RFC 4180 CSV.
 *
 * A sensor that produced no value becomes an empty field rather than a zero,
 * because a missing measurement and a measurement of zero are different facts.
 * Rows are ordered by sample time and then bottle, which is how the experiment is
 * read, rather than in whatever order the database returned them.
 *
 * The returned string uses CRLF line endings, as the RFC requires, and is empty
 * when there is nothing to export.
 */
export function toCsv(readings: Reading[]): string {
  if (readings.length === 0) return "";

  const rows = [...readings].sort(
    (left, right) =>
      Date.parse(left.timestamp) - Date.parse(right.timestamp) || left.bottle_id - right.bottle_id,
  );

  const lines = rows.map((reading) => [
    reading.bottle_id,
    reading.timestamp,
    ...PARAMETER_NAMES.map((parameter) => getParameterValue(reading, parameter)),
  ].map(formatCell).join(","));

  return `${headerRow()}\r\n${lines.join("\r\n")}\r\n`;
}

/**
 * Names an export after the experiment and the moment it was taken.
 *
 * The timestamp keeps repeated downloads of the same experiment apart, so a
 * later file is never mistaken for an earlier one in a downloads folder.
 */
export function csvFileName(experimentId: number, downloadedAt: Date = new Date()): string {
  const stamp = downloadedAt.toISOString().replace(/[:.]/g, "-").slice(0, 19);
  return `experiment-${experimentId}-readings-${stamp}.csv`;
}

/**
 * Hands a CSV export to the browser as a file download.
 *
 * The object URL is revoked on the next tick: revoking it synchronously can
 * cancel the download in some browsers, which would leave the operator with
 * nothing and no error.
 */
export function downloadCsv(csv: string, fileName: string): void {
  const url = URL.createObjectURL(new Blob([csv], { type: "text/csv;charset=utf-8" }));
  const link = document.createElement("a");
  link.href = url;
  link.download = fileName;
  link.click();
  window.setTimeout(() => URL.revokeObjectURL(url), 0);
}

/** Renders the header line, quoting it only where a header needs it. */
function headerRow(): string {
  return COLUMNS.map((column) => formatCell(typeof column === "string" ? column : withUnit(column))).join(",");
}

/** Appends a parameter's unit, so the file carries its own units. */
function withUnit(parameter: ParameterName): string {
  const unit = PARAMETER_UNITS[parameter];
  return unit === "" ? parameter : `${parameter} (${unit})`;
}

/** Quotes a field only when its contents would otherwise break the row. */
function formatCell(value: string | number | null): string {
  if (value === null) return "";
  const text = String(value);
  return NEEDS_QUOTING.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}