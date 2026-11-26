// Records the real RuSim dashboard at phone size for the mobile HyperFrames demo video.
//
// Needs the dashboard dev server: `cd dashboard && bun run dev`.
// Run: `cd video-demo/hyperframes-mobile/capture && bun install && bun run capture.ts`
//
// The phone is a 390 x 844 CSS px viewport (a common phone size) with touch input, at a
// device pixel ratio of 4, so every screenshot is 1560 px wide and the film can zoom in
// about 4x and stay sharp. Below 860 px the dashboard folds its sidebar into a drawer
// behind the menu button, so every change of view goes through that drawer.
//
// Writes, inside video-demo/hyperframes-mobile/:
//   assets/screens/*.png   stills of each UI state (full page or one screen, 1560 px wide)
//   assets/clips/*.mp4     the app's own animations, one screen, recorded frame by frame
//   assets/data/*          the downloaded CSV and the Unpad emblem
//   assets/capture.json    element rectangles in page CSS pixels and the scroll position
//                          of every clip, read by the film
//
// How the frame-by-frame recording works: before an interaction, CSS animations are
// frozen through the Chrome DevTools Protocol (Animation.setPlaybackRate 0) and the
// page's JavaScript timers are paused with Playwright's fake clock. Each step then
// advances the clock by 1/30 s and sets every running animation to its exact time,
// so a slow screenshot never changes what the frame shows.

import { chromium, type Locator } from "playwright-core";
import { mkdirSync, rmSync, writeFileSync, readFileSync, copyFileSync } from "node:fs";
import { join, resolve } from "node:path";

const BASE = process.env.DASHBOARD_URL ?? "http://localhost:5173/"; // Vite listens on localhost (::1)
// Development token of device 0. It is committed in firmware/network-master/main.cpp.
const TOKEN = process.env.RUSIM_DEMO_TOKEN ?? "RUSIM_DEVEL_TOKEN";
const CHROME =
  process.env.CHROME_PATH ??
  `${process.env.HOME}/.cache/puppeteer/chrome-headless-shell/linux-156.0.8078.47/chrome-headless-shell-linux64/chrome-headless-shell`;
const VIEW = { width: 390, height: 844 };
const DPR = 4; // 390 x 844 CSS px -> 1560 x 3376 device px
const FPS = 30;
const CHAMBER = "03"; // the chamber the film follows
const EXPERIMENT = "Experiment 4"; // the concluded run the film opens (the desktop film's run)
const ROOT = resolve(import.meta.dir, "..");
const SCREENS = join(ROOT, "assets/screens");
const CLIPS = join(ROOT, "assets/clips");
const DATA = join(ROOT, "assets/data");
const FRAMES = join(ROOT, ".capture-frames");
for (const dir of [SCREENS, CLIPS, DATA]) mkdirSync(dir, { recursive: true });

type Rect = { x: number; y: number; w: number; h: number };
const manifest: any = { view: VIEW, dpr: DPR, fps: FPS, chamber: CHAMBER, screens: {}, clips: {}, rects: {}, scroll: {}, data: {} };

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
const context = await browser.newContext({
  viewport: VIEW,
  deviceScaleFactor: DPR,
  isMobile: true,
  hasTouch: true,
  acceptDownloads: true,
});
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
const scrollY = () => page.evaluate(() => Math.round(window.scrollY));
async function scrollTo(y: number) {
  await page.evaluate((yy) => window.scrollTo(0, yy), y);
  await page.waitForTimeout(150);
  const got = await scrollY();
  if (Math.abs(got - y) > 1) log(`WARN asked to scroll to ${y}, page is at ${got}`);
  return got;
}

// Records `frames` frames of one screen after `action`. The clip sits at the page's scroll
// position, so the film places it at page y = scrollY.
async function sequence(
  name: string,
  frames: number,
  action: () => Promise<unknown>,
  opts: { ready?: () => Promise<unknown>; frozen?: boolean } = {},
) {
  if (!opts.frozen) await freeze();
  await begin();
  const y0 = await scrollY();
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
  const y1 = await scrollY();
  if (y1 !== y0) log(`WARN ${name}: the page scrolled from ${y0} to ${y1} during the clip`);
  const out = join(CLIPS, `${name}.mp4`);
  const ff = Bun.spawnSync([
    "ffmpeg", "-v", "error", "-y", "-framerate", String(FPS),
    "-i", join(dir, "%04d.jpg"), "-c:v", "libx264", "-preset", "slow", "-crf", "14",
    "-pix_fmt", "yuv420p", "-movflags", "+faststart", out,
  ]);
  if (ff.exitCode !== 0) throw new Error(`ffmpeg ${name}: ${ff.stderr.toString()}`);
  rmSync(dir, { recursive: true, force: true });
  manifest.clips[name] = {
    file: `assets/clips/${name}.mp4`,
    frames,
    duration: +(frames / FPS).toFixed(4),
    page: { x: 0, y: y1, w: VIEW.width, h: VIEW.height },
  };
  log(`clip ${name}: ${frames} frames at page y ${y1}`);
}

// A full-page still is placed at page y 0; a one-screen still at the scroll position.
async function still(name: string, fullPage = true) {
  await page.screenshot({ path: join(SCREENS, `${name}.png`), fullPage, caret: "hide" });
  const h = await page.evaluate(() => document.documentElement.scrollHeight);
  const y = fullPage ? 0 : await scrollY();
  manifest.screens[name] = { file: `assets/screens/${name}.png`, w: VIEW.width, h: fullPage ? h : VIEW.height, y };
  log(`still ${name} (${manifest.screens[name].h} px tall, page y ${y})`);
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
const toggle = () => page.locator(".menu-toggle");
const navItem = (label: string) => page.locator(".nav-item", { hasText: label });

// Opens the drawer from the top of the page (recorded), holds it as a one-screen still,
// then taps a view in it (recorded): the drawer slides out and the view swaps.
async function viaDrawer(tag: string, view: string, nextStill: string) {
  await scrollTo(0);
  await sequence(`drawer-${tag}`, 12, () => toggle().tap());
  await thaw(800);
  await still(`drawer-${tag}`, false);
  if (tag === "h") {
    await rects("drawer", {
      panel: page.locator("aside.sidebar"),
      brand: page.locator(".brand"),
      logo: page.locator("aside svg").first(),
      navMonitoring: navItem("Monitoring"),
      navHistory: navItem("Experiment history"),
      navCompare: navItem("Compare"),
    });
  }
  if (!view) return;
  await sequence(`nav-${tag}`, 18, () => navItem(view).tap());
  await thaw(1200);
  await still(nextStill);
}

log(`hover media query matches: ${await page.evaluate(() => matchMedia("(hover: hover) and (pointer: fine)").matches)}`);

// 1. Login: the entrance and the drifting bubbles, from the first frame.
await page.goto(BASE, { waitUntil: "commit" });
await freeze();
await page.locator(".login-panel").waitFor();
await page.evaluate(() => document.fonts.ready);
// 480 frames = 16 s: the film shows the login page for 14.6 s, starting 0.6 s in.
await sequence("login", 480, async () => {}, { frozen: true });
await thaw(800);
await rects("login", {
  panel: page.locator(".login-panel"),
  field: page.locator(".login-panel .login-input").first(),
  input: page.locator(".login-panel input").first(),
  submit: page.locator(".login-panel button[type=submit]"),
  title: page.locator(".login-aside h1"),
  logo: page.locator(".login-panel .rusim-logo"),
});

// 2. Token typed, then the "Verifying token" state, then the hand-off.
await page.locator(".login-panel input").first().fill(TOKEN);
await page.waitForTimeout(500);
await still("login-typed", false);
await page.route("**/rpc/authenticate**", async (route) => {
  await new Promise((r) => setTimeout(r, 1500));
  await route.continue();
});
await sequence("handoff", 54, async () => {
  await page.locator(".login-panel button[type=submit]").tap();
  await page.waitForTimeout(250);
  // Time is frozen here, and the new "Verifying token" label fades in over 180 ms, so the
  // still is taken with every animation moved 400 ms on, then moved back for the clip.
  await page.evaluate(() => (window as any).__seqSeek(400));
  await still("login-verifying", false);
  await page.evaluate(() => (window as any).__seqSeek(0));
}, {
  ready: async () => {
    await page.locator(".presence-cell").nth(23).waitFor({ timeout: 30000 });
    await page.locator(".kpi").nth(4).waitFor();
    await page.waitForTimeout(400);
  },
});
await page.unroute("**/rpc/authenticate**");
await thaw(1500);
await still("mon-ch1");
const cell = (n: string) => page.locator(".presence-cell", { hasText: n });
await rects("monitoring", {
  toggle: toggle(),
  crumbs: page.locator(".crumbs"),
  refresh: page.locator(".btn.refresh"),
  eyebrow: page.locator(".eyebrow").first(),
  heading: page.locator("h1").first(),
  stripCard: page.locator('[aria-label="Chamber presence"]').locator("xpath=ancestor::*[self::section or self::div][1]"),
  strip: page.locator('[aria-label="Chamber presence"]'),
  cell01: cell("01"),
  cell03: cell(CHAMBER),
  cell07: cell("07"),
  notice: page.getByText("Missing chamber data").locator(".."),
  kpis: page.locator(".kpis"),
  kpiPH: page.locator(".kpi").nth(0),
  kpiPressure: page.locator(".kpi").nth(4),
});

// 3. Monitoring: scroll so the chamber grid and all five sensor cards share the screen,
// then select the chamber the film follows.
const kpis = manifest.rects.monitoring.kpis as Rect;
manifest.scroll.mon = await scrollTo(kpis.y + kpis.h - (VIEW.height - 8));
await sequence("mon-ch3", 42, () => cell(CHAMBER).tap());
await thaw(1500);
await still("mon-ch3");

// 4. Experiment history through the drawer.
await viaDrawer("h", "Experiment history", "hist-list");
const row = (label: string) => page.locator(".row-button", { hasText: label });
await rects("history", { list: page.locator('[aria-label="Experiments"]'), rowFirst: page.locator(".row-button").first(), rowExp: row(EXPERIMENT) });

// 5. Open the run, then switch its charts to the chamber.
await row(EXPERIMENT).tap();
await page.getByRole("heading", { name: new RegExp(`^${EXPERIMENT}$`) }).waitFor({ timeout: 30000 });
await page.locator("select").first().waitFor({ timeout: 30000 });
await page.waitForTimeout(2500);
await still("hist-exp4");
const tabResults = page.getByRole("button", { name: "Results" }).or(page.getByRole("tab", { name: "Results" })).first();
await rects("history", {
  detail: page.getByRole("heading", { name: new RegExp(`^${EXPERIMENT}$`) }).locator("xpath=ancestor::*[self::section or self::div][1]"),
  csv: page.getByRole("button", { name: "Download CSV" }),
  tabReadings: page.getByRole("button", { name: "Chamber readings" }).or(page.getByRole("tab", { name: "Chamber readings" })).first(),
  tabResults,
  select: page.locator("select").first(),
  chart_first: page.locator("h3", { hasText: "· pH" }).first().locator("xpath=ancestor::*[self::section or self::article or self::div][2]"),
});
// The chamber select, the tabs and the whole pH chart on one screen.
const pH = manifest.rects.history.chart_first as Rect;
manifest.scroll.hist = await scrollTo(pH.y + pH.h - (VIEW.height - 12));
await sequence("hist-ch3", 33, () => page.locator("select").first().selectOption({ label: `Chamber ${Number(CHAMBER)}` }));
await thaw(1500);
await still("hist-exp4-ch3");
const chartHeads = ["pH", "Temperature", "CO₂", "CH₄", "Pressure"];
for (const head of chartHeads) {
  await rects("history", { [`chart_${head}`]: page.locator("h3", { hasText: `Chamber ${Number(CHAMBER)} · ${head}` }).locator("xpath=ancestor::*[self::section or self::article or self::div][2]") });
}

// 6. Tap the lowest CH4 point for its tooltip, with the CH4 chart in the middle of the screen.
const ch4 = manifest.rects.history["chart_CH₄"] as Rect;
manifest.scroll.tip = await scrollTo(Math.round(ch4.y + ch4.h / 2 - VIEW.height / 2));
const ch4Points = page.locator("h3", { hasText: `Chamber ${Number(CHAMBER)} · CH₄` }).locator("xpath=ancestor::*[self::section or self::article or self::div][2]").locator("circle");
const lowest = await ch4Points.evaluateAll((els) => {
  let best = 0, bestY = -Infinity;
  els.forEach((el, i) => { const y = el.getBoundingClientRect().y; if (y > bestY) { bestY = y; best = i; } });
  return best;
});
await ch4Points.nth(lowest).tap({ force: true });
await page.waitForTimeout(600);
if ((await scrollY()) !== manifest.scroll.tip) log(`WARN the tap scrolled the page to ${await scrollY()}`);
await rects("history", { tipPoint: ch4Points.nth(lowest) });
const tip = page.locator(".chart-tooltip-inner").first();
if (await tip.count()) await rects("history", { tip });
else log("WARN no tooltip after the tap");
await still("hist-ch4-tip");

// 7. Results tab, from the same place as the chamber select.
await scrollTo(manifest.scroll.hist);
await sequence("hist-results", 15, () => tabResults.tap());
await thaw(1200);
await still("hist-results");
await rects("history", { results: page.locator('[aria-label="Chamber results"]'), csvAfter: page.getByRole("button", { name: "Download CSV" }) });
const table = manifest.rects.history.results as Rect;
manifest.scroll.table = Math.max(0, table.y + table.h - (VIEW.height - 12));

// 8. CSV download: keep the real file.
const [download] = await Promise.all([page.waitForEvent("download"), page.getByRole("button", { name: "Download CSV" }).tap()]);
const csvName = download.suggestedFilename();
await download.saveAs(join(DATA, csvName));
const csvLines = readFileSync(join(DATA, csvName), "utf8").split(/\r?\n/);
manifest.data.csv = { file: `assets/data/${csvName}`, name: csvName, lines: csvLines.length - 1, head: csvLines.slice(0, 12) };
log(`csv ${csvName}: ${csvLines.length - 1} lines`);

// 9. Compare through the drawer, then two chambers added from two experiments.
await viaDrawer("c", "Compare", "cmp-empty");
await rects("compare", {
  card: page.getByText("Chambers to compare").locator("xpath=ancestor::*[self::section or self::div][2]"),
  expSelect: page.locator("select").nth(0),
  chSelect: page.locator("select").nth(1),
  add: page.getByRole("button", { name: "Add chamber" }),
});
const pick = async (index: number, label: string, name: string) => {
  await page.locator("select").nth(index).selectOption({ label });
  await page.waitForTimeout(400);
  await still(name);
};
const addSeries = async (n: number, name: string) => {
  await sequence(name, 39, () => page.getByRole("button", { name: "Add chamber" }).tap(), {
    ready: () => page.locator('[aria-label="Series legend"] > *').nth(n - 1).waitFor({ timeout: 30000 }),
  });
  await thaw(1500);
  await still(name);
};
// The first run: both selects change (the experiment select opens on the newest run).
await pick(0, EXPERIMENT, "cmp-add1-exp");
await pick(1, `Chamber ${Number(CHAMBER)}`, "cmp-add1-picked");
await addSeries(1, "cmp-add1");
// The second run: scroll so the controls and the chart share the screen.
const cmpH = await page.evaluate(() => document.documentElement.scrollHeight);
manifest.scroll.cmp = await scrollTo(Math.max(0, cmpH - VIEW.height));
await pick(0, "Experiment 2", "cmp-add2-picked");
await addSeries(2, "cmp-add2");
await rects("compare", {
  addAfter: page.getByRole("button", { name: "Add chamber" }),
  expSelectAfter: page.locator("select").nth(0),
  chips: page.locator('[aria-label="Selected chambers"]'),
  chart: page.locator("h3", { hasText: "comparison" }).locator("xpath=ancestor::*[self::section or self::article or self::div][2]"),
  legend: page.locator('[aria-label="Series legend"]'),
});

// 10. The drawer once more for the ending, then the RuSim mark as the app draws it.
await viaDrawer("end", "", "");
manifest.data.brandMark = await page.locator("aside svg").first().evaluate((el) => {
  const clone = el.cloneNode(true) as SVGElement;
  clone.setAttribute("color", getComputedStyle(el).color);
  return clone.outerHTML;
});
copyFileSync(resolve(ROOT, "../../dashboard/public/unpad-mono.svg"), join(DATA, "unpad-mono.svg"));

writeFileSync(join(ROOT, "assets/capture.json"), JSON.stringify(manifest, null, 2));
rmSync(FRAMES, { recursive: true, force: true });
log("done");
await browser.close();
