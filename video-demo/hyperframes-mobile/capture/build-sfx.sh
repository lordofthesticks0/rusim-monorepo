#!/usr/bin/env bash
# Rebuilds assets/sfx/ for the mobile dashboard demo video from the fframes project's sounds
# (video-demo/media/sfx-*.wav). Run from anywhere: bash video-demo/hyperframes-mobile/capture/build-sfx.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MEDIA="$HERE/../../media"
OUT="$HERE/../assets/sfx"
mkdir -p "$OUT"
cp "$MEDIA/sfx-tap.wav" "$MEDIA/sfx-swell.wav" "$MEDIA/sfx-whoosh.wav" "$OUT/"
cd "$OUT"

# Two more takes of the click, so repeated clicks do not sound identical.
ffmpeg -v error -y -i sfx-tap.wav -af "asetrate=48000*0.94,aresample=48000" tap-b.wav
ffmpeg -v error -y -i sfx-tap.wav -af "asetrate=48000*1.06,aresample=48000,volume=0.9" tap-c.wav

# ticks OUT COUNT GAP_MS LOWPASS_HZ GAIN...: COUNT softened clicks, GAP_MS apart, one track.
ticks() {
  local out=$1 n=$2 gap=$3 lp=$4; shift 4
  local gains=("$@") f="[0]lowpass=f=$lp,asplit=$n" mix=""
  for ((i = 0; i < n; i++)); do f="$f[a$i]"; done
  f="$f;"
  for ((i = 0; i < n; i++)); do
    f="$f[a$i]volume=${gains[$((i % ${#gains[@]}))]},adelay=$((i * gap)):all=1[b$i];"
    mix="$mix[b$i]"
  done
  ffmpeg -v error -y -i sfx-tap.wav -filter_complex "$f${mix}amix=inputs=$n:normalize=0:duration=longest" "$out"
}
# Token typing: 17 characters, 164 ms apart.
ticks typing.wav 17 164 3500 0.55 0.45 0.6 0.5 0.52 0.42 0.58 0.48 0.55 0.44 0.6 0.5 0.47 0.56 0.43 0.58 0.5
# CSV lines arriving: 11 lines, 140 ms apart.
ticks lines.wav 11 140 2500 0.36 0.3 0.34 0.28 0.35 0.31 0.33 0.29 0.36 0.3 0.32
# End-card URL typing: 25 characters, 55 ms apart.
ticks url.wav 25 55 3000 0.26 0.34 0.30 0.28 0.32
echo "sounds written to $OUT"
