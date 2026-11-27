import { describe, expect, test } from "bun:test";
import { boxOnto, cameraShift, centreOf, squareAt, type Box, type Point, type Shift } from "./brandTransition";

/** Where `point` ends up under `shift` applied about `origin` (CSS: translate, then scale about the origin). */
function apply(shift: Shift, origin: Point, point: Point): Point {
  return {
    x: origin.x + shift.x + (point.x - origin.x) * shift.scale,
    y: origin.y + shift.y + (point.y - origin.y) * shift.scale,
  };
}

const topLeft = (box: Box): Point => ({ x: box.left, y: box.top });

describe("squareAt", () => {
  test("puts the square's centre on the point at the requested size", () => {
    const home: Box = { left: 660, top: 300, width: 120, height: 120 };
    const centre = { x: 34, y: 34 };
    const shift = squareAt(home, centre, 28);
    const corner = apply(shift, topLeft(home), topLeft(home));
    expect(shift.scale * home.width).toBeCloseTo(28);
    expect(corner.x + 14).toBeCloseTo(centre.x);
    expect(corner.y + 14).toBeCloseTo(centre.y);
  });
});

describe("boxOnto", () => {
  test("lays the box's top-left on the target's and matches its height", () => {
    const word: Box = { left: 600, top: 470, width: 230, height: 92 };
    const target: Box = { left: 820, top: 210, width: 46, height: 19 };
    const shift = boxOnto(word, target);
    const corner = apply(shift, topLeft(word), topLeft(word));
    expect(corner.x).toBeCloseTo(target.left);
    expect(corner.y).toBeCloseTo(target.top);
    expect(word.height * shift.scale).toBeCloseTo(target.height);
  });
});

describe("cameraShift", () => {
  test("carries the anchor point to the destination and scales distances about it", () => {
    const from = { x: 34, y: 34 };
    const to = { x: 720, y: 450 };
    const shift = cameraShift(from, to, 6);
    expect(apply(shift, from, from)).toEqual(to);
    // A point 10px right of the anchor lands 60px right of the destination.
    expect(apply(shift, from, { x: 44, y: 34 })).toEqual({ x: 780, y: 450 });
  });

  test("the stage mark and the magnified sidebar mark stay the same square", () => {
    // Outro end state: the sidebar mark (28px) zoomed to the stage mark's size about the screen centre.
    const sidebar: Box = { left: 20, top: 20, width: 28, height: 28 };
    const home: Box = { left: 655, top: 330, width: 130, height: 130 };
    const centre = { x: 720, y: 450 };
    const camera = cameraShift(centreOf(sidebar), centre, home.width / sidebar.width);
    const zoomedCorner = apply(camera, centreOf(sidebar), topLeft(sidebar));
    const stage = squareAt(home, centre, home.width);
    const stageCorner = apply(stage, topLeft(home), topLeft(home));
    expect(zoomedCorner.x).toBeCloseTo(stageCorner.x);
    expect(zoomedCorner.y).toBeCloseTo(stageCorner.y);
  });
});
