// Records the real RuSim dashboard for the HyperFrames demo video.
//
// Needs the dashboard dev server: `cd dashboard && bun run dev`.
// Run: `cd video-demo/hyperframes/capture && bun install && bun run capture.ts`
// `ONLY=login bun run capture.ts` records only the login clip and keeps every other asset.
//
// Writes, inside video-demo/hyperframes/:
//   assets/screens/*.png   stills of each UI state (full page, 3840 px wide)
//   assets/clips/*.mp4     the app's own animations, recorded frame by frame
//   assets/data/*          the downloaded CSV and the RuSim mark
//   assets/capture.json    element rectangles in page CSS pixels, read by the film
//
// How the frame-by-frame recording works: before an interaction, CSS animations are
// frozen through the Chrome DevTools Protocol (Animation.setPlaybackRate 0) and the
// page's JavaScript timers are paused with Playwright's fake clock. Each step then
// advances the clock by 1/30 s and sets every running animation to its exact time,
// so a slow screenshot never changes what the frame shows.

import { chromium, type Locator, type Page } from "playwright-core";
import { mkdirSync, rmSync, writeFileSync, readFileSync, copyFileSync } from "node:fs";
import { join, resolve } from "node:path";

const BASE = process.env.DASHBOARD_URL ?? "http://localhost:5173/"; // Vite listens on localhost (::1)
// Development token of device 0. It is committed in firmware/network-master/main.cpp.
const TOKEN = process.env.RUSIM_DEMO_TOKEN ?? "RUSIM_DEVEL_TOKEN";
const CHROME =
  process.env.CHROME_PATH ??
  `${process.env.HOME}/.cache/puppeteer/chrome-headless-shell/linux-156.0.8078.47/chrome-headless-shell-linux64/chrome-headless-shell`;
const VIEW = { width: 1440, height: 810 };
const DPR = 8 / 3; // 1440 x 810 CSS px -> 3840 x 2160 device px
const FPS = 30;
const CHAMBER = "03"; // the chamber the film follows
const ROOT = resolve(import.meta.dir, "..");
const SCREENS = join(ROOT, "assets/screens");
const CLIPS = join(ROOT, "assets/clips");
const DATA = join(ROOT, "assets/data");
const FRAMES = join(ROOT, ".capture-frames");
for (const dir of [SCREENS, CLIPS, DATA]) mkdirSync(dir, { recursive: true });

type Rect = { x: number; y: number; w: number; h: number };
const manifest: any = { view: VIEW, dpr: DPR, fps: FPS, chamber: CHAMBER, screens: {}, clips: {}, rects: {}, data: {} };

// Page-side helpers: track every animation and seek it to virtual time t (ms).
const SEQ_HELPERS = `
  window.__seqBegin = () => {
    const known = new Map();
    for (const a of document.getAnimations()) known.set(a, { born: 0, base: a.currentTime ?? 0 });
    window.__seq = { known, t: 0 };
  };
  window.__seqSeek = (t) => {
    const s = window.__seq;
    for (const a of document.getAnimations()) {
      let k = s.known.get(a);
      if (!k) { k = { born: s.t, base: 0 }; s.known.set(a, k); }
      a.currentTime = k.base + (t - k.born);
    }
    s.t = t;
  };
`;

const browser = await chromium.launch({ executablePath: CHROME });
const context = await browser.newContext({ viewport: VIEW, deviceScaleFactor: DPR, acceptDownloads: true });
const page = await context.newPage();
await page.addInitScript(SEQ_HELPERS);
await page.clock.install();
const cdp = await context.newCDPSession(page);
await cdp.send("Animation.enable");

const log = (...a: unknown[]) => console.log(`[capture] ${a.join(" ")}`);

async function freeze() {
  await cdp.send("Animation.setPlaybackRate", { playbackRate: 0 });
  const now = await page.evaluate(() => Date.now());
  await page.clock.pauseAt(now + 50);
}
async function thaw(settleMs = 1200) {
  await cdp.send("Animation.setPlaybackRate", { playbackRate: 1 });
  await page.clock.resume();
  await page.waitForTimeout(settleMs);
}
const begin = () => page.evaluate(() => (window as any).__seqBegin());

// Records `frames` frames after `action`. `crop` is a viewport region in CSS px.
async function sequence(
  name: string,
  frames: number,
  action: () => Promise<unknown>,
  opts: { crop?: Rect; skip?: number; ready?: () => Promise<unknown>; frozen?: boolean } = {},
) {
  if (!opts.frozen) await freeze();
  await begin();
  await action();
  if (opts.ready) await opts.ready();
  const dir = join(FRAMES, name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  let elapsed = 0;
  for (let f = 0; f < frames; f++) {
    const t = Math.round((f * 1000) / FPS);
    if (t > elapsed) {
      await page.clock.runFor(t - elapsed);
      elapsed = t;
    }
    await page.evaluate((tt) => (window as any).__seqSeek(tt), t);
    await page.screenshot({ path: join(dir, `${String(f).padStart(4, "0")}.jpg`), type: "jpeg", quality: 94, caret: "hide" });
  }
  const scrollY = await page.evaluate(() => scrollY);
  const crop = opts.crop ?? { x: 0, y: 0, w: VIEW.width, h: VIEW.height };
  const px = (v: number) => Math.round(v * DPR);
  const skip = opts.skip ?? 0;
  const out = join(CLIPS, `${name}.mp4`);
  const vf = `crop=${px(crop.w)}:${px(crop.h)}:${px(crop.x)}:${px(crop.y)}`;
  const ff = Bun.spawnSync([
    "ffmpeg", "-v", "error", "-y", "-framerate", String(FPS), "-start_number", String(skip),
    "-i", join(dir, "%04d.jpg"), "-vf", vf, "-c:v", "libx264", "-preset", "slow", "-crf", "14",
    "-pix_fmt", "yuv420p", "-movflags", "+faststart", out,
  ]);
  if (ff.exitCode !== 0) throw new Error(`ffmpeg ${name}: ${ff.stderr.toString()}`);
  rmSync(dir, { recursive: true, force: true });
  manifest.clips[name] = {
    file: `assets/clips/${name}.mp4`,
    frames: frames - skip,
    duration: +((frames - skip) / FPS).toFixed(4),
    page: { x: crop.x, y: crop.y + scrollY, w: crop.w, h: crop.h },
  };
  log(`clip ${name}: ${frames - skip} frames, page y ${crop.y + scrollY}`);
}

async function still(name: string, fullPage = true) {
  await page.screenshot({ path: join(SCREENS, `${name}.png`), fullPage, caret: "hide" });
  const size = await page.evaluate(() => ({ w: document.documentElement.scrollWidth, h: document.documentElement.scrollHeight }));
  manifest.screens[name] = { file: `assets/screens/${name}.png`, w: VIEW.width, h: fullPage ? size.h : VIEW.height };
  log(`still ${name} (${manifest.screens[name].h} px tall)`);
}

async function rectOf(loc: Locator): Promise<Rect> {
  return loc.first().evaluate(
    (el) => {
      const r = el.getBoundingClientRect();
      return { x: Math.round(r.x + scrollX), y: Math.round(r.y + scrollY), w: Math.round(r.width), h: Math.round(r.height) };
    },
    undefined,
    { timeout: 4000 },
  );
}
async function rects(group: string, entries: Record<string, Locator>) {
  manifest.rects[group] ??= {};
  for (const [key, loc] of Object.entries(entries)) {
    try {
      manifest.rects[group][key] = await rectOf(loc);
    } catch {
      log(`WARN no element for rect ${group}.${key}`);
    }
  }
}
const parkMouse = () => page.mouse.move(VIEW.width - 4, VIEW.height - 4);
const toTop = () => page.evaluate(() => scrollTo(0, 0));

// 1. Login: the entrance, the curve drawing and the bubbles, from the first frame.
await page.goto(BASE, { waitUntil: "commit" });
await freeze();
await page.locator(".login-panel").waitFor();
await page.evaluate(() => document.fonts.ready);
// 480 frames = 16 s: the film shows the login page for 14.6 s, starting 0.6 s in.
await sequence("login", 480, async () => {}, { frozen: true });
// ONLY=login re-records just this clip and keeps every other asset.
if (process.env.ONLY === "login") {
  const kept = JSON.parse(readFileSync(join(ROOT, "assets/capture.json"), "utf8"));
  kept.clips.login = manifest.clips.login;
  writeFileSync(join(ROOT, "assets/capture.json"), JSON.stringify(kept, null, 2));
  rmSync(FRAMES, { recursive: true, force: true });
  log("done (login only)");
  await browser.close();
  process.exit(0);
}
await thaw(800);
await rects("login", {
  panel: page.locator(".login-panel"),
  field: page.locator(".login-panel input").first(),
  submit: page.locator(".login-panel button[type=submit]"),
  title: page.locator(".login-aside h1"),
});

// 2. Token typed, then the "Verifying token" state, then the hand-off.
await page.locator(".login-panel input").first().fill(TOKEN);
await parkMouse();
await page.waitForTimeout(500);
await still("login-typed", false);
await page.route("**/rpc/authenticate**", async (route) => {
  await new Promise((r) => setTimeout(r, 1500));
  await route.continue();
});
await sequence("handoff", 54, async () => {
  await page.locator(".login-panel button[type=submit]").click();
  await page.waitForTimeout(250);
  await still("login-verifying", false);
}, {
  ready: async () => {
    await page.locator(".presence-cell").nth(23).waitFor({ timeout: 30000 });
    await page.locator(".kpi").nth(4).waitFor();
    await page.waitForTimeout(400);
  },
});
await page.unroute("**/rpc/authenticate**");
await thaw(1500);
await parkMouse();
await still("mon-ch1");
const cell = (n: string) => page.locator(".presence-cell", { hasText: n });
await rects("monitoring", {
  logo: page.locator("aside svg").first(),
  brand: page.locator("aside").first().locator("*", { hasText: "RuSim" }).first(),
  navMonitoring: page.locator(".nav-item", { hasText: "Monitoring" }),
  navHistory: page.locator(".nav-item", { hasText: "Experiment history" }),
  navCompare: page.locator(".nav-item", { hasText: "Compare" }),
  heading: page.locator("h1").first(),
  strip: page.locator('[aria-label="Chamber presence"]'),
  cell01: cell("01"),
  cell03: cell(CHAMBER),
  cell07: cell("07"),
  cell19: cell("19"),
  kpis: page.locator(".kpis"),
  kpiCH4: page.locator(".kpi").nth(3),
  notice: page.getByText("Missing chamber data").locator(".."),
});

// 3. Monitoring: select the chamber the film follows.
await cell(CHAMBER).hover();
await sequence("mon-ch3", 42, () => cell(CHAMBER).click());
await thaw(1500);
await parkMouse();
await still("mon-ch3");

// 4. Experiment history: the nav pill slides and the view swaps.
await page.locator(".nav-item", { hasText: "Experiment history" }).hover();
await sequence("nav-history", 18, () => page.locator(".nav-item", { hasText: "Experiment history" }).click());
await thaw(1200);
await parkMouse();
await still("hist-list");
await rects("history", { list: page.locator('[aria-label="Experiments"]'), row4: page.locator(".row-button").first(), row2: page.locator(".row-button").nth(2) });

// 5. Open Experiment 4 (the newest), then switch its charts to the chamber.
await page.locator(".row-button").first().click();
await page.locator("select").first().waitFor({ timeout: 30000 });
await page.waitForTimeout(2500);
await parkMouse();
await toTop();
await still("hist-exp4");
await rects("history", {
  detail: page.getByRole("heading", { name: /^Experiment 4$/ }).locator("xpath=ancestor::*[self::section or self::div][1]"),
  csv: page.getByRole("button", { name: "Download CSV" }),
  tabReadings: page.getByRole("button", { name: "Chamber readings" }).or(page.getByRole("tab", { name: "Chamber readings" })),
  tabResults: page.getByRole("button", { name: "Results" }).or(page.getByRole("tab", { name: "Results" })),
  select: page.locator("select").first(),
});
await page.evaluate(() => scrollTo(0, 690));
await sequence("hist-ch3", 33, () => page.locator("select").first().selectOption({ label: `Chamber ${Number(CHAMBER)}` }), {
  crop: { x: 249, y: 60, w: 1191, h: 750 },
});
await thaw(1500);
await parkMouse();
await toTop();
await still("hist-exp4-ch3");
const chartHeads = ["pH", "Temperature", "CO₂", "CH₄", "Pressure"];
for (const head of chartHeads) {
  await rects("history", { [`chart_${head}`]: page.locator("h3", { hasText: `Chamber ${Number(CHAMBER)} · ${head}` }).locator("xpath=ancestor::*[self::section or self::article or self::div][2]") });
}

// 6. Hover the lowest CH4 point for its tooltip.
const ch4Points = page.locator("h3", { hasText: `Chamber ${Number(CHAMBER)} · CH₄` }).locator("xpath=ancestor::*[self::section or self::article or self::div][2]").locator("circle");
const lowest = await ch4Points.evaluateAll((els) => {
  let best = 0, bestY = -Infinity;
  els.forEach((el, i) => { const y = el.getBoundingClientRect().y; if (y > bestY) { bestY = y; best = i; } });
  return best;
});
await ch4Points.nth(lowest).hover();
await page.waitForTimeout(600);
await rects("history", { tipPoint: ch4Points.nth(lowest) });
await still("hist-ch4-tip");
const tip = page.locator(".chart-tooltip-inner").first();
if (await tip.count()) await rects("history", { tip });
await parkMouse();
await page.waitForTimeout(400);

// 7. Results tab.
await page.evaluate(() => scrollTo(0, 600));
await page.getByRole("button", { name: "Results" }).or(page.getByRole("tab", { name: "Results" })).first().hover();
await sequence("hist-results", 15, () => page.getByRole("button", { name: "Results" }).or(page.getByRole("tab", { name: "Results" })).first().click(), {
  crop: { x: 249, y: 60, w: 1191, h: 750 },
});
await thaw(1200);
await parkMouse();
await toTop();
await still("hist-results");
await rects("history", { results: page.locator('[aria-label="Chamber results"]') });

// 8. CSV download: keep the real file.
const [download] = await Promise.all([page.waitForEvent("download"), page.getByRole("button", { name: "Download CSV" }).click()]);
const csvName = download.suggestedFilename();
await download.saveAs(join(DATA, csvName));
const csvLines = readFileSync(join(DATA, csvName), "utf8").split(/\r?\n/);
manifest.data.csv = { file: `assets/data/${csvName}`, name: csvName, lines: csvLines.length - 1, head: csvLines.slice(0, 12) };
log(`csv ${csvName}: ${csvLines.length - 1} lines`);
await parkMouse();

// 9. Compare: nav, then two chambers added from two experiments.
await page.locator(".nav-item", { hasText: "Compare" }).hover();
await sequence("nav-compare", 18, () => page.locator(".nav-item", { hasText: "Compare" }).click());
await thaw(1200);
await parkMouse();
await still("cmp-empty");
await rects("compare", {
  expSelect: page.locator("select").nth(0),
  chSelect: page.locator("select").nth(1),
  add: page.getByRole("button", { name: "Add chamber" }),
});
const addSeries = async (exp: string, n: number, name: string) => {
  await page.locator("select").nth(0).selectOption({ label: exp });
  await page.locator("select").nth(1).selectOption({ label: `Chamber ${Number(CHAMBER)}` });
  await page.waitForTimeout(400);
  await parkMouse();
  await still(`${name}-picked`, false);
  await page.getByRole("button", { name: "Add chamber" }).hover();
  await sequence(name, 39, () => page.getByRole("button", { name: "Add chamber" }).click(), {
    ready: () => page.locator('[aria-label="Series legend"] > *').nth(n - 1).waitFor({ timeout: 30000 }),
  });
  await thaw(1500);
  await parkMouse();
  await still(name);
};
await addSeries("Experiment 4", 1, "cmp-add1");
await addSeries("Experiment 2", 2, "cmp-add2");
await rects("compare", {
  chips: page.locator('[aria-label="Selected chambers"]'),
  chart: page.locator("h3", { hasText: "comparison" }).locator("xpath=ancestor::*[self::section or self::article or self::div][2]"),
  legend: page.locator('[aria-label="Series legend"]'),
});

// 10. The RuSim mark, as the app draws it.
manifest.data.brandMark = await page.locator("aside svg").first().evaluate((el) => {
  const clone = el.cloneNode(true) as SVGElement;
  const cs = getComputedStyle(el);
  clone.setAttribute("color", cs.color);
  return clone.outerHTML;
});
copyFileSync(resolve(ROOT, "../../dashboard/public/unpad-mono.svg"), join(DATA, "unpad-mono.svg"));

writeFileSync(join(ROOT, "assets/capture.json"), JSON.stringify(manifest, null, 2));
rmSync(FRAMES, { recursive: true, force: true });
log("done");
await browser.close();
