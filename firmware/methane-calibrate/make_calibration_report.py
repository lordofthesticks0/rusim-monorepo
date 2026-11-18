#!/usr/bin/env python3
"""Generate calibration charts and tables for the four TGS2611 methane sensors.

Input : results.txt (serial log of main.cpp, JSON lines)
Output: report/ (CSV tables + PNG charts, one folder per sensor)

Run from anywhere:  python make_calibration_report.py

Data sources
- Measured points (1470, 1707, 6064 ppm): averaged from fixed line ranges of
  results.txt (see MEASURED_POINTS). Reference ppm values come from the earlier
  calibration CSVs; their original source is not recorded in the repository.
- Simulated points (3000, 4500 ppm): NOT measurements. Their relative error is
  linearly interpolated from the three measured points, then Rs is back-computed
  with the firmware's own power-law model. They are always labelled "Simulasi".
"""
import csv
import json
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
RESULTS = HERE / "results.txt"
OUT = HERE / "report"

# --- Constants copied from main.cpp (keep in sync) -------------------------
NUM_SENSORS = 4
VCC = 5.0
RL = 1000.0
PPM_AIR = 4.0
PPM_SPAN = 5000.0
RS_AIR = [914.2, 1309.9, 1540.9, 1723.4]  # also used as Ro
RS_SPAN = [99.2, 402.5, 175.2, 152.2]
EXPONENT = [
    math.log10(PPM_SPAN / PPM_AIR) / math.log10(a / s) for a, s in zip(RS_AIR, RS_SPAN)
]

# --- Where the measured points live in results.txt (1-indexed, inclusive) ---
# Each tuple: (reference ppm, session id, first line, last line)
MEASURED_POINTS = [
    (1470, "B", 2298, 2309),
    (1707, "B", 2269, 2281),
    (6064, "A", 2202, 2209),
]
# time_s restarts at every reboot, so each session has its own time axis.
SESSIONS = {"A": (2093, 2237), "B": (2243, 2411)}
SIMULATED_PPM = [3000, 4500]

# --- Style -----------------------------------------------------------------
plt.rcParams.update(
    {
        "font.family": "DejaVu Sans",
        "font.size": 11,
        "axes.titlesize": 13,
        "axes.titleweight": "bold",
        "axes.spines.top": False,
        "axes.spines.right": False,
        "axes.grid": True,
        "grid.color": "#e5e5e5",
        "grid.linewidth": 0.8,
        "figure.dpi": 100,
    }
)
C_MEAS = "#0072B2"  # measured point
C_SIM = "#D55E00"  # simulated point
C_FIT = "#222222"
CONC_COLORS = {1470: "#0072B2", 1707: "#E69F00", 6064: "#009E73"}
NOTE = "Titik terisi = terukur; titik berongga = simulasi (bukan pengukuran)"


# --- Parsing ---------------------------------------------------------------
def read_lines():
    return RESULTS.read_text(encoding="utf-8", errors="ignore").splitlines()


def parse_range(lines, first, last):
    """Return JSON records from 1-indexed inclusive line range."""
    rows = []
    for text in lines[first - 1 : last]:
        text = text.strip()
        if not text.startswith("{"):
            continue
        try:
            rows.append(json.loads(text))
        except json.JSONDecodeError:
            continue
    return rows


def column(rows, key):
    """Values of `key`, skipping null/missing entries (paired with time_s)."""
    return [(r["time_s"], r[key]) for r in rows if r.get(key) is not None]


def rs_to_ppm(rs, sensor):
    """Firmware model: ppm = PPM_AIR * (RS_AIR / rs) ** n"""
    return PPM_AIR * (RS_AIR[sensor] / rs) ** EXPONENT[sensor]


def ppm_to_rs(ppm, sensor):
    """Inverse of rs_to_ppm."""
    return RS_AIR[sensor] * (PPM_AIR / ppm) ** (1.0 / EXPONENT[sensor])


def rs_to_volt(rs):
    """Voltage across the load resistor (ADC input)."""
    return VCC * RL / (rs + RL)


# --- Build the 5-point dataset --------------------------------------------
def build_points(lines, sensor):
    pts = []
    for ref, _sess, first, last in MEASURED_POINTS:
        rows = parse_range(lines, first, last)
        rs_vals = [v for _, v in column(rows, f"rs_{sensor}")]
        ppm_vals = [v for _, v in column(rows, f"ppm_{sensor}")]
        rs = float(np.mean(rs_vals))
        reading = float(np.mean(ppm_vals))
        sd = float(np.std(ppm_vals, ddof=1))
        pts.append(
            dict(
                ref=ref, status="Terukur", n=len(ppm_vals), rs=rs,
                volt=rs_to_volt(rs), ratio=rs / RS_AIR[sensor],
                reading=reading, sd=sd, reps=ppm_vals,
            )
        )
    pts.sort(key=lambda p: p["ref"])

    # Simulated points: interpolate relative error from the measured points.
    refs = [p["ref"] for p in pts]
    rel_err = [(p["reading"] - p["ref"]) / p["ref"] for p in pts]
    for ref in SIMULATED_PPM:
        e = float(np.interp(ref, refs, rel_err))
        reading = ref * (1.0 + e)
        rs = ppm_to_rs(reading, sensor)
        pts.append(
            dict(
                ref=ref, status="Simulasi", n=None, rs=rs, volt=rs_to_volt(rs),
                ratio=rs / RS_AIR[sensor], reading=reading, sd=None, reps=None,
            )
        )
    pts.sort(key=lambda p: p["ref"])
    for p in pts:
        p["err"] = p["reading"] - p["ref"]
        p["err_pct"] = 100.0 * p["err"] / p["ref"]
    return pts


# --- Regression ------------------------------------------------------------
def r_squared(y, y_hat):
    y, y_hat = np.asarray(y), np.asarray(y_hat)
    ss_res = float(np.sum((y - y_hat) ** 2))
    ss_tot = float(np.sum((y - y.mean()) ** 2))
    return 1.0 - ss_res / ss_tot


def power_fit(ratio, ppm):
    """Fit ppm = a * ratio**b via linear regression in log-log space."""
    lx, ly = np.log(ratio), np.log(ppm)
    b, ln_a = np.polyfit(lx, ly, 1)
    r2 = r_squared(ly, b * lx + ln_a)
    return math.exp(ln_a), float(b), r2


def fit_for(pts):
    ratio = [p["ratio"] for p in pts]
    ppm = [p["ref"] for p in pts]
    return power_fit(ratio, ppm)


# --- Charts ----------------------------------------------------------------
def scatter_points(ax, pts, x_key, y_key, label_meas, label_sim, **kw):
    meas = [p for p in pts if p["status"] == "Terukur"]
    sim = [p for p in pts if p["status"] != "Terukur"]
    ax.scatter([p[x_key] for p in meas], [p[y_key] for p in meas], s=70,
               color=C_MEAS, zorder=4, label=label_meas, **kw)
    ax.scatter([p[x_key] for p in sim], [p[y_key] for p in sim], s=70,
               facecolors="white", edgecolors=C_SIM, linewidths=2, zorder=4,
               label=label_sim, **kw)


def chart_calibration(path, sensor, pts, fit):
    a, b, r2 = fit
    fig, ax = plt.subplots(figsize=(8, 5.4))
    x = [p["ratio"] for p in pts]
    xs = np.linspace(min(x) * 0.94, max(x) * 1.06, 200)
    ax.plot(xs, a * xs**b, color=C_FIT, lw=2, label="Kurva regresi (pangkat)")
    scatter_points(ax, pts, "ratio", "ref", "Data terukur", "Data simulasi")
    for p in pts:
        ax.annotate(f"{p['ref']}", (p["ratio"], p["ref"]), textcoords="offset points",
                    xytext=(9, 6), fontsize=9, color="#444")
    eq = f"ppm = {a:.4g} · (Rs/Ro)$^{{{b:.3f}}}$\nR² = {r2:.4f}"
    ax.text(0.97, 0.95, eq, transform=ax.transAxes, ha="right", va="top", fontsize=11,
            bbox=dict(boxstyle="round,pad=0.4", fc="white", ec="#bbb"))
    ax.set_xlabel("Rs/Ro")
    ax.set_ylabel("Konsentrasi CH₄ referensi (ppm)")
    ax.set_title(f"Kurva Kalibrasi CH₄ — Sensor {sensor}")
    ax.legend(loc="lower left", frameon=False)
    fig.text(0.01, 0.01, NOTE, fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(path, dpi=200)
    plt.close(fig)


def chart_response(path, sensor, lines):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5), sharey=True,
                             gridspec_kw={"width_ratios": [1, 1.15]})
    all_rs = []
    for ax, (sess, (first, last)), title in zip(
        axes, SESSIONS.items(), ("Sesi A", "Sesi B")
    ):
        rows = parse_range(lines, first, last)
        t, rs = zip(*column(rows, f"rs_{sensor}"))
        ax.plot(t, rs, color="#555", lw=1.4, marker="o", ms=2.5)
        for ref, s, f, l in MEASURED_POINTS:
            if s != sess:
                continue
            w = parse_range(lines, f, l)
            t0, t1 = w[0]["time_s"], w[-1]["time_s"]
            ax.axvspan(t0, t1, color=CONC_COLORS[ref], alpha=0.25)
            ax.text((t0 + t1) / 2, 0.97, f"{ref} ppm", transform=ax.get_xaxis_transform(),
                    rotation=90, ha="center", va="top", fontsize=9,
                    color=CONC_COLORS[ref], fontweight="bold")
        all_rs.extend(rs)
        ax.set_xlabel("Waktu (detik)")
        ax.set_title(f"{title} (area berwarna = jendela rata-rata)", fontsize=11)
    lo, hi = min(all_rs), max(all_rs)
    pad = (hi - lo) * 0.08
    axes[0].set_ylim(lo - pad, hi + pad)  # axes share y, so this sets both
    axes[0].set_ylabel("Rs (Ω)")
    sec = axes[1].secondary_yaxis(
        "right",
        # Inverse of rs_to_volt; clip v so the axis edge at 0 V does not divide by zero.
        functions=(rs_to_volt, lambda v: VCC * RL / np.maximum(v, 1e-6) - RL),
    )
    sec.set_ylabel("Tegangan output (V)")
    sec.spines["right"].set_visible(True)
    fig.suptitle(f"Respons Sensor terhadap Waktu — Sensor {sensor}", fontweight="bold",
                 fontsize=13)
    fig.tight_layout()
    fig.savefig(path, dpi=200)
    plt.close(fig)


def chart_accuracy(path, sensor, pts):
    fig, ax = plt.subplots(figsize=(7.2, 6.2))
    top = max(max(p["ref"], p["reading"]) for p in pts) * 1.12
    ax.plot([0, top], [0, top], ls="--", color="#777", lw=1.5, label="Ideal (y = x)")
    ref = np.array([p["ref"] for p in pts], float)
    rd = np.array([p["reading"] for p in pts], float)
    m, c = np.polyfit(ref, rd, 1)
    r2 = r_squared(rd, m * ref + c)
    ax.plot([0, top], [c, m * top + c], color=C_FIT, lw=1.6,
            label=f"Regresi: y = {m:.3f}x {c:+.0f}  (R² = {r2:.4f})")
    meas = [p for p in pts if p["status"] == "Terukur"]
    ax.errorbar([p["ref"] for p in meas], [p["reading"] for p in meas],
                yerr=[p["sd"] for p in meas], fmt="none", ecolor=C_MEAS,
                capsize=4, lw=1.2, zorder=3)
    scatter_points(ax, pts, "ref", "reading", "Data terukur (± SD)", "Data simulasi")
    # Points rise left to right, so alternate the label side: even points get
    # their label below-left, odd points above-right. Neighbours such as
    # 1470 and 1707 ppm then never overprint or get mixed up.
    for i, p in enumerate(pts):
        below_left = i % 2 == 0
        ax.annotate(f"{p['err_pct']:+.1f}%", (p["ref"], p["reading"]),
                    textcoords="offset points",
                    xytext=(-10, -15) if below_left else (10, 8),
                    ha="right" if below_left else "left", fontsize=9, color="#444")
    ax.set_xlim(0, top)
    ax.set_ylim(0, top)
    ax.set_aspect("equal")
    ax.set_xlabel("Konsentrasi CH₄ standar / referensi (ppm)")
    ax.set_ylabel("Pembacaan sensor (ppm)")
    ax.set_title(f"Akurasi Sensor — Sensor {sensor}")
    ax.legend(loc="upper left", frameon=False, fontsize=9)
    fig.text(0.01, 0.01, NOTE + "; angka = galat relatif", fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(path, dpi=200)
    plt.close(fig)


def chart_repeatability(path, sensor, pts):
    fig, ax = plt.subplots(figsize=(8, 6.6))
    for p in pts:
        if p["status"] != "Terukur":
            continue
        color = CONC_COLORS[p["ref"]]
        n = np.arange(1, p["n"] + 1)
        cv = 100.0 * p["sd"] / p["reading"]
        ax.plot(n, p["reps"], marker="o", ms=5, lw=1.4, color=color,
                label=f"{p['ref']} ppm  (rata-rata {p['reading']:.0f}, SD {p['sd']:.1f}, CV {cv:.1f}%)")
        ax.hlines(p["ref"], 1, p["n"], colors=color, linestyles="--", lw=1.2, alpha=0.8)
    ax.plot([], [], ls="--", color="#666", label="Nilai referensi (garis putus-putus)")
    ax.set_xlabel("Pengulangan ke-")
    ax.set_ylabel("Pembacaan sensor (ppm)")
    ax.set_title(f"Repeatability — Sensor {sensor}")
    ax.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
    # Legend below the axes so it never covers the data lines.
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.14), frameon=False, fontsize=9)
    fig.text(0.01, 0.01, "Hanya titik terukur; titik simulasi tidak memiliki pengulangan.",
             fontsize=8, color="#666")
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(path, dpi=200)
    plt.close(fig)


# --- Tables ----------------------------------------------------------------
def write_csv(path, header, rows):
    # utf-8-sig adds a BOM so Excel reads the Unicode headers (Ω, ...) correctly.
    with open(path, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def main():
    OUT.mkdir(exist_ok=True)
    lines = read_lines()
    cal_rows, test_rows, summary_rows = [], [], []

    for s in range(NUM_SENSORS):
        pts = build_points(lines, s)
        a, b, r2 = fit_for(pts)
        meas = [p for p in pts if p["status"] == "Terukur"]
        r2_meas = power_fit([p["ratio"] for p in meas], [p["ref"] for p in meas])[2]
        mape_all = float(np.mean([abs(p["err_pct"]) for p in pts]))
        mape_meas = float(np.mean([abs(p["err_pct"]) for p in meas]))

        sdir = OUT / f"sensor_{s}"
        sdir.mkdir(exist_ok=True)
        chart_calibration(sdir / "1_kalibrasi.png", s, pts, (a, b, r2))
        chart_response(sdir / "2_respons_waktu.png", s, lines)
        chart_accuracy(sdir / "3_akurasi.png", s, pts)
        chart_repeatability(sdir / "4_repeatability.png", s, pts)

        for i, p in enumerate(pts, 1):
            cal_rows.append([
                f"Sensor {s}", i, p["status"], p["ref"], f"{p['volt']:.4f}",
                f"{p['rs']:.2f}", f"{RS_AIR[s]:.1f}", f"{p['ratio']:.4f}",
            ])
            test_rows.append([
                f"Sensor {s}", i, p["status"], p["ref"], f"{p['reading']:.1f}",
                f"{p['err']:+.1f}", f"{p['err_pct']:+.2f}",
                "" if p["sd"] is None else f"{p['sd']:.1f}",
                "" if p["n"] is None else p["n"],
            ])
        summary_rows.append([
            f"Sensor {s}", f"ppm = {a:.4g} x (Rs/Ro)^{b:.3f}", f"{a:.4g}", f"{b:.3f}",
            f"{r2:.4f}", f"{r2_meas:.4f}", f"{mape_all:.2f}", f"{mape_meas:.2f}",
            f"{EXPONENT[s]:.3f}",
        ])
        print(f"sensor {s}: a={a:.4g} b={b:.3f} R2={r2:.4f} (3 pts {r2_meas:.4f}) "
              f"MAPE5={mape_all:.2f}% MAPE3={mape_meas:.2f}%")

    write_csv(OUT / "tabel_kalibrasi.csv",
              ["Sensor", "Titik", "Status data", "Konsentrasi CH4 (ppm)", "Tegangan (V)",
               "Rs (ohm)", "Ro (ohm)", "Rs/Ro"], cal_rows)
    write_csv(OUT / "tabel_pengujian.csv",
              ["Sensor", "Titik", "Status data", "Referensi (ppm)", "Sensor (ppm)",
               "Error (ppm)", "Error (%)", "SD sensor (ppm)", "Jumlah sampel"], test_rows)
    write_csv(OUT / "ringkasan_regresi.csv",
              ["Sensor", "Persamaan kalibrasi", "a", "b", "R2 (5 titik)",
               "R2 (3 titik terukur)", "MAPE 5 titik (%)", "MAPE 3 titik terukur (%)",
               "Eksponen firmware n"], summary_rows)
    print(f"Done. Output in {OUT}")


if __name__ == "__main__":
    main()
