"""Build an interactive HTML chart from results.txt (PlatformIO serial log).

Usage (from this folder):
    python plot_results.py                  # results.txt -> results_plot.html
    python plot_results.py in.txt out.html

Only lines like "Pressure: 123 Pa" are used. The monitor header, boot
messages and blank lines are ignored by the regex below.

The output is one self-contained HTML file (no internet, no libraries).
Hover the chart to read the exact sample number, raw value and median.
Only the Python standard library is needed.
"""

import json
import re
import statistics
import sys
from pathlib import Path

DATA_LINE = re.compile(r"^Pressure:\s*(-?\d+(?:\.\d+)?)\s*Pa\s*$")
SMOOTH_WINDOW = 9  # samples; odd number so the median is centred

# Stable windows chosen from the chart, one per pressure step (sample numbers,
# 1-based, inclusive). Each starts after the rise has levelled off and ends
# before the next rise begins. Edit these if the data changes.
PLATEAUS = [
    (1, 131, 170),
    (2, 196, 228),
    (3, 251, 305),
    (4, 321, 425),
]

HTML = """<!doctype html>
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kalibrasi sensor tekanan</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 16px; color: #222; background: #fff; }
  h1 { font-size: 1.1rem; margin: 0 0 4px; }
  p.sub { margin: 0 0 8px; color: #666; font-size: .85rem; }
  #wrap { position: relative; width: 100%; height: 70vh; min-height: 320px; }
  canvas { width: 100%; height: 100%; display: block; touch-action: none; }
  #tip { position: absolute; pointer-events: none; display: none; background: rgba(255,255,255,.96);
         border: 1px solid #999; border-radius: 4px; padding: 6px 8px; font-size: .8rem;
         line-height: 1.4; white-space: nowrap; box-shadow: 0 2px 6px rgba(0,0,0,.15); }
  .key { display: inline-block; width: 12px; height: 3px; margin-right: 6px; vertical-align: middle; }
  #legend { font-size: .85rem; margin-top: 6px; }
  #legend span { margin-right: 16px; }
  table { border-collapse: collapse; margin-top: 14px; font-size: .85rem; }
  th, td { border: 1px solid #ccc; padding: 4px 10px; text-align: right; }
  th { background: #f3f3f3; }
  h2 { font-size: 1rem; margin: 18px 0 0; }
  p.note { color: #666; font-size: .8rem; margin: 4px 0 0; max-width: 60em; }
</style>
</head>
<body>
<h1>Kalibrasi sensor tekanan (HX711) - hasil pembacaan serial</h1>
<p class="sub">__SUMMARY__. Arahkan kursor ke grafik untuk melihat nilai tiap titik.</p>
<div id="wrap"><canvas id="c"></canvas><div id="tip"></div></div>
<div id="legend">
  <span><i class="key" style="background:#9ab7d3"></i>Data mentah</span>
  <span><i class="key" style="background:#c0392b"></i>Median bergerak (__WINDOW__ sampel)</span>
  <span><i class="key" style="background:#2e7d32"></i>Rata-rata kelas stabil (pita hijau)</span>
</div>
<h2>Rata-rata tekanan tiap kelas saat stabil</h2>
__TABLE__
<p class="note">Rata-rata dihitung dari data mentah di dalam rentang sampel stabil. Drift adalah kemiringan
garis regresi: makin dekat 0, makin datar kelasnya.</p>
<script>
const RAW = __RAW__;
const MED = __MED__;
const BANDS = __BANDS__;
const N = RAW.length;
const M = { l: 62, r: 18, t: 16, b: 46 };
const canvas = document.getElementById("c");
const ctx = canvas.getContext("2d");
const tip = document.getElementById("tip");
let W = 0, H = 0, hover = -1;

// Y range: data range plus 5 % padding so the lines do not touch the frame.
const yMinRaw = Math.min(...RAW), yMaxRaw = Math.max(...RAW);
const pad = (yMaxRaw - yMinRaw) * 0.05 || 1;
const yMin = yMinRaw - pad, yMax = yMaxRaw + pad;

const px = i => M.l + (N > 1 ? (i / (N - 1)) * (W - M.l - M.r) : 0);   // index -> pixel X
const py = v => H - M.b - ((v - yMin) / (yMax - yMin)) * (H - M.t - M.b); // value -> pixel Y

// Pick "round" tick values (1, 2, 5 x 10^n) so labels are easy to read.
function niceTicks(min, max, count) {
  const step0 = (max - min) / count;
  const mag = Math.pow(10, Math.floor(Math.log10(step0)));
  const norm = step0 / mag;
  const step = (norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10) * mag;
  const out = [];
  for (let v = Math.ceil(min / step) * step; v <= max + 1e-9; v += step) out.push(Math.round(v / step) * step);
  return out;
}

function line(data, color, width) {
  ctx.beginPath();
  data.forEach((v, i) => (i ? ctx.lineTo(px(i), py(v)) : ctx.moveTo(px(i), py(v))));
  ctx.strokeStyle = color; ctx.lineWidth = width; ctx.lineJoin = "round"; ctx.stroke();
}

function draw() {
  ctx.clearRect(0, 0, W, H);
  ctx.font = "12px system-ui, sans-serif";
  ctx.fillStyle = "#444"; ctx.strokeStyle = "#e3e3e3"; ctx.lineWidth = 1;

  ctx.textAlign = "right"; ctx.textBaseline = "middle";
  for (const v of niceTicks(yMin, yMax, 8)) {
    const y = Math.round(py(v)) + 0.5;
    ctx.beginPath(); ctx.moveTo(M.l, y); ctx.lineTo(W - M.r, y); ctx.stroke();
    ctx.fillText(v, M.l - 8, y);
  }
  ctx.textAlign = "center"; ctx.textBaseline = "top";
  for (const s of niceTicks(1, N, Math.max(4, Math.floor(W / 90)))) {
    const x = Math.round(px(s - 1)) + 0.5;
    ctx.beginPath(); ctx.moveTo(x, M.t); ctx.lineTo(x, H - M.b); ctx.stroke();
    ctx.fillText(s, x, H - M.b + 6);
  }
  ctx.fillText("Nomor sampel (tidak ada timestamp di log)", (M.l + W - M.r) / 2, H - 18);
  ctx.save(); ctx.translate(14, (M.t + H - M.b) / 2); ctx.rotate(-Math.PI / 2);
  ctx.fillText("Tekanan (Pa)", 0, 0); ctx.restore();

  if (yMin < 0 && yMax > 0) { // zero reference line
    ctx.strokeStyle = "#888"; ctx.setLineDash([5, 4]);
    ctx.beginPath(); ctx.moveTo(M.l, py(0)); ctx.lineTo(W - M.r, py(0)); ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.strokeStyle = "#444"; ctx.strokeRect(M.l + 0.5, M.t + 0.5, W - M.l - M.r, H - M.t - M.b);

  // Stable windows: green band, dashed mean line and a "K<n>: mean" label.
  for (const b of BANDS) {
    const x0 = px(b.start - 1), x1 = px(b.end - 1);
    ctx.fillStyle = "rgba(46,125,50,0.12)"; ctx.fillRect(x0, M.t, x1 - x0, H - M.t - M.b);
    ctx.strokeStyle = "#2e7d32"; ctx.lineWidth = 1.5; ctx.setLineDash([6, 3]);
    ctx.beginPath(); ctx.moveTo(x0, py(b.mean)); ctx.lineTo(x1, py(b.mean)); ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = "#1b5e20"; ctx.textAlign = "center"; ctx.textBaseline = "bottom";
    ctx.fillText("K" + b.k + ": " + b.mean.toFixed(0) + " Pa", (x0 + x1) / 2, py(b.mean) - 6);
  }

  line(RAW, "#9ab7d3", 1.2);
  line(MED, "#c0392b", 2);

  if (hover >= 0) {
    const x = px(hover);
    ctx.strokeStyle = "#555"; ctx.lineWidth = 1; ctx.setLineDash([3, 3]);
    ctx.beginPath(); ctx.moveTo(x, M.t); ctx.lineTo(x, H - M.b); ctx.stroke(); ctx.setLineDash([]);
    for (const [v, c] of [[RAW[hover], "#3b78b0"], [MED[hover], "#c0392b"]]) {
      ctx.beginPath(); ctx.arc(x, py(v), 4.5, 0, 2 * Math.PI);
      ctx.fillStyle = c; ctx.fill(); ctx.strokeStyle = "#fff"; ctx.lineWidth = 1.5; ctx.stroke();
    }
  }
}

function resize() {
  const r = canvas.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
  W = r.width; H = r.height;
  canvas.width = W * dpr; canvas.height = H * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  draw();
}

canvas.addEventListener("pointermove", e => {
  const r = canvas.getBoundingClientRect(), x = e.clientX - r.left;
  const i = Math.round(((x - M.l) / (W - M.l - M.r)) * (N - 1));
  if (i < 0 || i >= N) return leave();
  hover = i; draw();
  const b = BANDS.find(b => i + 1 >= b.start && i + 1 <= b.end);
  tip.innerHTML = "<b>Sampel " + (i + 1) + "</b><br>Mentah: " + RAW[i] + " Pa<br>Median: " + MED[i] + " Pa" +
    (b ? "<br>Kelas " + b.k + " (rata-rata " + b.mean.toFixed(1) + " Pa)" : "");
  tip.style.display = "block";
  const tx = px(i) + 14 + tip.offsetWidth > W ? px(i) - 14 - tip.offsetWidth : px(i) + 14;
  tip.style.left = tx + "px"; tip.style.top = Math.max(M.t, e.clientY - r.top - 50) + "px";
});
function leave() { hover = -1; tip.style.display = "none"; draw(); }
canvas.addEventListener("pointerleave", leave);
window.addEventListener("resize", resize);
resize();
</script>
</body>
</html>
"""


def read_pressures(path: Path) -> list[float]:
    values = []
    with path.open(encoding="utf-8", errors="replace") as f:
        for line in f:
            m = DATA_LINE.match(line.strip())
            if m:
                values.append(float(m.group(1)))
    return values


def rolling_median(values: list[float], window: int) -> list[float]:
    """Median of a sliding window; removes single-sample spikes."""
    half = window // 2
    out = []
    for i in range(len(values)):
        chunk = sorted(values[max(0, i - half) : i + half + 1])
        out.append(chunk[len(chunk) // 2])
    return out


def plateau_stats(values: list[float]) -> list[dict]:
    """Mean, spread and drift of the raw readings inside each stable window."""
    stats = []
    for k, start, end in PLATEAUS:
        seg = values[start - 1 : end]  # sample numbers are 1-based
        xs = list(range(start, end + 1))
        stats.append(
            {
                "k": k,
                "start": start,
                "end": end,
                "n": len(seg),
                "mean": statistics.fmean(seg),
                "sd": statistics.stdev(seg),
                "min": min(seg),
                "max": max(seg),
                # Least-squares slope: how much the level drifts per sample.
                "slope": statistics.linear_regression(xs, seg).slope,
            }
        )
    return stats


def table_html(stats: list[dict]) -> str:
    rows = "".join(
        f"<tr><td>{s['k']}</td><td>{s['start']}&ndash;{s['end']}</td><td>{s['n']}</td>"
        f"<td><b>{s['mean']:.1f}</b></td><td>{s['sd']:.1f}</td>"
        f"<td>{s['min']:.0f} &ndash; {s['max']:.0f}</td><td>{s['slope']:+.2f}</td></tr>"
        for s in stats
    )
    return (
        "<table><thead><tr><th>Kelas</th><th>Sampel</th><th>n</th>"
        "<th>Rata-rata (Pa)</th><th>Simpangan baku (Pa)</th><th>Min &ndash; Maks (Pa)</th>"
        "<th>Drift (Pa/sampel)</th></tr></thead><tbody>"
        f"{rows}</tbody></table>"
    )


def main() -> None:
    here = Path(__file__).parent
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "results.txt"
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "results_plot.html"

    pressures = read_pressures(src)
    if not pressures:
        sys.exit(f"No 'Pressure: ... Pa' lines found in {src}")

    smooth = rolling_median(pressures, SMOOTH_WINDOW)
    stats = plateau_stats(pressures)
    for s in stats:
        print(f"Kelas {s['k']}: sampel {s['start']}-{s['end']} (n={s['n']}) "
              f"rata-rata {s['mean']:.1f} Pa, SD {s['sd']:.1f}, drift {s['slope']:+.2f} Pa/sampel")
    summary = (
        f"{len(pressures)} sampel, min {min(pressures):.0f} Pa, "
        f"maks {max(pressures):.0f} Pa"
    )
    html = (
        HTML.replace("__RAW__", json.dumps(pressures))
        .replace("__MED__", json.dumps(smooth))
        .replace("__BANDS__", json.dumps(stats))
        .replace("__TABLE__", table_html(stats))
        .replace("__WINDOW__", str(SMOOTH_WINDOW))
        .replace("__SUMMARY__", summary)
    )
    dst.write_text(html, encoding="utf-8")
    print(f"{summary} | disimpan: {dst}")


if __name__ == "__main__":
    main()
