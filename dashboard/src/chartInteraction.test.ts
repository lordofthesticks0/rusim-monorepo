import { describe, expect, test } from "bun:test";
import { nearestIndex, nearestSeries, showMarkers, stepIndex, viewBoxX, viewBoxY } from "./chartInteraction";

describe("nearestIndex", () => {
  test("maps a position to the closest of evenly spread samples", () => {
    // 5 samples across 0..400 sit at 0, 100, 200, 300, 400.
    expect(nearestIndex(0, 5, 0, 400)).toBe(0);
    expect(nearestIndex(149, 5, 0, 400)).toBe(1);
    expect(nearestIndex(151, 5, 0, 400)).toBe(2);
    expect(nearestIndex(400, 5, 0, 400)).toBe(4);
  });

  test("clamps positions outside the plot", () => {
    expect(nearestIndex(-50, 5, 0, 400)).toBe(0);
    expect(nearestIndex(999, 5, 0, 400)).toBe(4);
  });

  test("handles empty and single-sample series", () => {
    expect(nearestIndex(10, 0, 0, 400)).toBeNull();
    expect(nearestIndex(10, 1, 0, 400)).toBe(0);
  });
});

describe("stepIndex", () => {
  test("arrows step one sample and clamp at the ends", () => {
    expect(stepIndex("ArrowRight", 3, 10)).toBe(4);
    expect(stepIndex("ArrowLeft", 3, 10)).toBe(2);
    expect(stepIndex("ArrowRight", 9, 10)).toBe(9);
    expect(stepIndex("ArrowLeft", 0, 10)).toBe(0);
  });

  test("starting with nothing active, left begins at the newest sample and right at the oldest", () => {
    expect(stepIndex("ArrowLeft", null, 10)).toBe(9);
    expect(stepIndex("ArrowRight", null, 10)).toBe(0);
  });

  test("page keys move a tenth of the series; Home and End jump", () => {
    expect(stepIndex("PageDown", 0, 2880)).toBe(288);
    expect(stepIndex("PageUp", 100, 2880)).toBe(0);
    expect(stepIndex("Home", 50, 100)).toBe(0);
    expect(stepIndex("End", 50, 100)).toBe(99);
  });

  test("other keys and empty series are ignored", () => {
    expect(stepIndex("a", 3, 10)).toBeNull();
    expect(stepIndex("ArrowRight", null, 0)).toBeNull();
  });
});

describe("showMarkers", () => {
  test("dots each sample only when they are at least 6 px apart", () => {
    expect(showMarkers(40, 800)).toBe(true);
    expect(showMarkers(2880, 800)).toBe(false);
    expect(showMarkers(1, 10)).toBe(true);
  });
});

describe("viewBox conversion", () => {
  test("scales client coordinates into viewBox units", () => {
    expect(viewBoxX(150, { left: 100, width: 200 }, 800)).toBe(200);
    expect(viewBoxY(60, { top: 10, height: 100 }, 260)).toBe(130);
    expect(viewBoxX(150, { left: 100, width: 0 }, 800)).toBe(0);
  });
});

describe("nearestSeries", () => {
  test("picks the series whose point is closest to the pointer, skipping gaps", () => {
    expect(nearestSeries(100, [40, 95, null, 130])).toBe(1);
    expect(nearestSeries(100, [null, null])).toBeNull();
  });
});
