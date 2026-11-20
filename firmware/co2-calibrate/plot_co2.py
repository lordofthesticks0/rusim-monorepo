#!/usr/bin/env python3
"""Build a self-contained, interactive ppm chart from a CO2 serial log."""

import argparse
import json
import math
from pathlib import Path


HTML = r'''<!doctype html>
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Fluktuasi CO₂ (ppm)</title>
<style>
:root{color-scheme:light;--ink:#17212b;--muted:#657384;--line:#dce3ea;--paper:#f4f7fa;--card:#fff}
*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:1250px;margin:0 auto;padding:28px 22px 40px}h1{font-size:clamp(24px,4vw,34px);margin:0 0 5px}p{margin:0;color:var(--muted)}
.head{display:flex;justify-content:space-between;gap:20px;align-items:end;margin-bottom:20px}.meta{text-align:right;font-variant-numeric:tabular-nums}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(205px,1fr));gap:12px;margin:18px 0}
.card,.chart-card{background:var(--card);border:1px solid var(--line);border-radius:12px;box-shadow:0 3px 12px #102a3a0a}
.card{padding:14px 16px}.card h2{font-size:15px;margin:0 0 9px}.numbers{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;font-size:12px;color:var(--muted)}.numbers b{display:block;color:var(--ink);font-size:16px;font-variant-numeric:tabular-nums}
.chart-card{padding:16px}.legend{display:flex;gap:9px;flex-wrap:wrap;margin:0 0 12px}.legend button{border:1px solid var(--line);background:#fff;border-radius:999px;padding:6px 11px;color:var(--ink);cursor:pointer;font:inherit}.legend button[aria-pressed="false"]{opacity:.45;text-decoration:line-through}.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px}
.canvas-wrap{position:relative;width:100%;height:min(62vh,540px);min-height:310px}canvas{width:100%;height:100%;display:block;touch-action:pan-y}.tip{position:absolute;z-index:2;display:none;pointer-events:none;background:#14212e;color:#fff;border-radius:8px;padding:9px 11px;font-size:12px;line-height:1.55;box-shadow:0 4px 15px #0003;white-space:nowrap;font-variant-numeric:tabular-nums}.tip strong{display:block;margin-bottom:3px}.tip-row{display:flex;gap:8px;align-items:center}.tip-row i{width:8px;height:8px;border-radius:50%}
.foot{font-size:12px;margin-top:12px}.source{font-family:ui-monospace,Consolas,monospace}
@media(max-width:650px){main{padding:20px 12px}.head{display:block}.meta{text-align:left;margin-top:8px}.chart-card{padding:10px}.canvas-wrap{height:440px;min-height:320px}}
</style>
</head>
<body><main>
<header class="head"><div><h1>Fluktuasi CO₂ (ppm)</h1><p>Nilai ppm per pin terhadap waktu log. Arahkan cursor ke grafik untuk membaca nilai tiap sampel.</p></div><p class="meta" id="meta"></p></header>
<section class="stats" id="stats" aria-label="Ringkasan ppm"></section>
<section class="chart-card"><nav class="legend" id="legend" aria-label="Tampilkan atau sembunyikan pin"></nav><div class="canvas-wrap"><canvas id="plot" aria-label="Grafik nilai ppm terhadap waktu"></canvas><div class="tip" id="tip"></div></div><p class="foot">Sumbu X memakai waktu sejak boot yang tercatat dalam log. Nilai kosong tidak digambar dan tidak dianggap nol.</p></section>
</main>
<script>
const rows=__DATA__;
const pins=__PINS__;
const colors=['#1677c8','#e0563b','#18966a','#8b5ac2','#d29117','#168b98'];
const colorFor=Object.fromEntries(pins.map((p,i)=>[p,colors[i%colors.length]]));
const shown=new Set(pins);
const canvas=document.getElementById('plot'),ctx=canvas.getContext('2d'),tip=document.getElementById('tip'),wrap=canvas.parentElement;
const fmt=n=>Number(n).toLocaleString('id-ID',{maximumFractionDigits:1});
document.getElementById('meta').textContent=`${rows.length} sampel · ${rows.length?fmt(rows[0].t):0}–${rows.length?fmt(rows.at(-1).t):0} detik`;
const stats=document.getElementById('stats'),legend=document.getElementById('legend');
for(const pin of pins){
 const vals=rows.map(r=>r.v[String(pin)]).filter(Number.isFinite);
 const avg=vals.reduce((a,b)=>a+b,0)/Math.max(vals.length,1);
 const min=vals.length?fmt(Math.min(...vals)):'—',max=vals.length?fmt(Math.max(...vals)):'—',mean=vals.length?fmt(avg):'—';
 const card=document.createElement('article');card.className='card';
 card.innerHTML=`<h2><span class="dot" style="background:${colorFor[pin]}"></span>Pin D${pin}</h2><div class="numbers"><span>Min<b>${min}</b></span><span>Rata-rata<b>${mean}</b></span><span>Maks<b>${max}</b></span></div>`;stats.append(card);
 const button=document.createElement('button');button.type='button';button.setAttribute('aria-pressed','true');button.innerHTML=`<span class="dot" style="background:${colorFor[pin]}"></span>Pin D${pin}`;
 button.onclick=()=>{if(shown.has(pin))shown.delete(pin);else shown.add(pin);button.setAttribute('aria-pressed',String(shown.has(pin)));draw()};legend.append(button);
}
const allVals=rows.flatMap(r=>pins.map(p=>r.v[String(p)]).filter(Number.isFinite));
const yMax=Math.max(500,Math.ceil(Math.max(...allVals,0)/500)*500);
let box;
function resize(){const dpr=window.devicePixelRatio||1,rect=canvas.getBoundingClientRect();canvas.width=Math.round(rect.width*dpr);canvas.height=Math.round(rect.height*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);box={w:rect.width,h:rect.height,left:62,right:18,top:16,bottom:42};draw()}
function draw(){if(!box||!rows.length)return;const {w,h,left,right,top,bottom}=box,iw=w-left-right,ih=h-top-bottom;ctx.clearRect(0,0,w,h);ctx.font='12px system-ui';ctx.lineWidth=1;
 for(let i=0;i<=5;i++){const val=yMax*i/5,y=top+ih-(ih*i/5);ctx.strokeStyle='#e6ebf0';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(w-right,y);ctx.stroke();ctx.fillStyle='#657384';ctx.textAlign='right';ctx.textBaseline='middle';ctx.fillText(fmt(val),left-9,y)}
 const first=rows[0].t,last=rows.at(-1).t,span=Math.max(last-first,1);for(let i=0;i<=5;i++){const x=left+iw*i/5;ctx.strokeStyle='#f0f3f6';ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,h-bottom);ctx.stroke();ctx.fillStyle='#657384';ctx.textAlign='center';ctx.textBaseline='top';ctx.fillText(`${fmt(first+span*i/5)}s`,x,h-bottom+10)}
 ctx.fillStyle='#455363';ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillText('Waktu sejak boot (detik)',left+iw/2,h-8);ctx.save();ctx.translate(14,top+ih/2);ctx.rotate(-Math.PI/2);ctx.fillText('CO₂ (ppm)',0,0);ctx.restore();
 for(const pin of shown){ctx.strokeStyle=colorFor[pin];ctx.lineWidth=1.65;ctx.beginPath();let active=false;for(const row of rows){const val=row.v[String(pin)];if(!Number.isFinite(val)){active=false;continue}const x=left+(row.t-first)/span*iw,y=top+(yMax-val)/yMax*ih;if(!active){ctx.moveTo(x,y);active=true}else ctx.lineTo(x,y)}ctx.stroke()}
}
function showAt(evt){if(!rows.length)return;const rect=canvas.getBoundingClientRect(),x=evt.clientX-rect.left,{left,right}=box,iw=box.w-left-right;const ratio=Math.max(0,Math.min(1,(x-left)/iw)),time=rows[0].t+ratio*Math.max(rows.at(-1).t-rows[0].t,1);let lo=0,hi=rows.length-1;while(lo<hi){const mid=(lo+hi)>>1;if(rows[mid].t<time)lo=mid+1;else hi=mid}const idx=lo>0&&Math.abs(rows[lo-1].t-time)<Math.abs(rows[lo].t-time)?lo-1:lo,row=rows[idx],cx=left+(row.t-rows[0].t)/Math.max(rows.at(-1).t-rows[0].t,1)*iw;
 draw();ctx.save();ctx.strokeStyle='#27374688';ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(cx,box.top);ctx.lineTo(cx,box.h-box.bottom);ctx.stroke();ctx.setLineDash([]);for(const pin of shown){const val=row.v[String(pin)];if(Number.isFinite(val)){const cy=box.top+(yMax-val)/yMax*(box.h-box.top-box.bottom);ctx.fillStyle='#fff';ctx.strokeStyle=colorFor[pin];ctx.lineWidth=2;ctx.beginPath();ctx.arc(cx,cy,4,0,Math.PI*2);ctx.fill();ctx.stroke()}}ctx.restore();
 tip.innerHTML=`<strong>Sampel ${idx+1} · ${fmt(row.t)} s</strong>`+pins.map(pin=>`<span class="tip-row"><i style="background:${colorFor[pin]}"></i>Pin D${pin}: ${Number.isFinite(row.v[String(pin)])?fmt(row.v[String(pin)])+' ppm':'—'}</span>`).join('');tip.style.display='block';const tx=Math.min(Math.max(8,x+14),box.w-tip.offsetWidth-8);tip.style.left=`${tx}px`;tip.style.top=`${Math.max(6,Math.min(evt.clientY-rect.top+10,box.h-tip.offsetHeight-8))}px`;
}
canvas.addEventListener('pointermove',showAt);canvas.addEventListener('pointerleave',()=>{tip.style.display='none';draw()});window.addEventListener('resize',resize);resize();
</script></body></html>'''


def read_capture(path: Path):
    rows = []
    pins = set()
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = line.strip()
        if not line.startswith("{"):
            continue  # serial-monitor headers and blank lines
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError(f"JSON tidak valid pada baris {line_number}: {exc}") from exc
        if "uptime_ms" not in record or not isinstance(record.get("pins"), list):
            continue
        values = {}
        for item in record["pins"]:
            pin = item.get("pin")
            if not isinstance(pin, int):
                continue
            value = item.get("ppm")
            if isinstance(value, (int, float)) and math.isfinite(value):
                values[str(pin)] = value
                pins.add(pin)
            else:
                values[str(pin)] = None
                pins.add(pin)
        rows.append({"t": record["uptime_ms"] / 1000, "v": values})
    if not rows or not pins:
        raise ValueError("Tidak menemukan rekaman uptime_ms dan pins[].ppm dalam file.")
    rows.sort(key=lambda row: row["t"])
    return rows, sorted(pins)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="?", type=Path, default=Path(__file__).with_name("co2.txt"))
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("co2-fluctuation.html"))
    args = parser.parse_args()
    rows, pins = read_capture(args.input)
    page = HTML.replace("__DATA__", json.dumps(rows, separators=(",", ":"), allow_nan=False))
    page = page.replace("__PINS__", json.dumps(pins))
    args.output.write_text(page, encoding="utf-8")
    print(f"Grafik dibuat: {args.output} ({len(rows)} sampel, pin {', '.join(map(str, pins))})")


if __name__ == "__main__":
    main()
