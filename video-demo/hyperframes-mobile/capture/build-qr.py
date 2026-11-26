# /// script
# requires-python = ">=3.10"
# dependencies = ["segno==1.6.6"]
# ///
"""Writes assets/qr.svg, the QR code on the end card of the dashboard demo video.

Run from anywhere: uv run video-demo/hyperframes/capture/build-qr.py [URL]

The default URL is the end card's address. The SVG has no quiet zone: the white
card around it in compositions/endcard.html gives the four-module margin. Each
module is one unit of the viewBox, so the card can scale it to whole pixels.
"""

import sys
from pathlib import Path

import segno

url = sys.argv[1] if len(sys.argv) > 1 else "https://unpadrumensim.netlify.app"
qr = segno.make(url, error="m", boost_error=False)
rows = [list(row) for row in qr.matrix]
n = len(rows)

# One rectangle per horizontal run of dark modules keeps the path short.
d = []
for y, row in enumerate(rows):
    x = 0
    while x < n:
        if not row[x]:
            x += 1
            continue
        start = x
        while x < n and row[x]:
            x += 1
        d.append(f"M{start} {y}h{x - start}v1h{start - x}z")

svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {n} {n}" shape-rendering="crispEdges">'
    f'<path fill="#000" d="{"".join(d)}"/></svg>\n'
)
out = Path(__file__).resolve().parent.parent / "assets" / "qr.svg"
out.write_text(svg)
print(f"QR {qr.designator} ({n} x {n} modules) for {url} -> {out}")
