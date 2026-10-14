#!/usr/bin/env bash
#
# Packages a finished build (run tools/build.sh first) for a GitHub release:
#
#   tools/package_release.sh v1.03          # writes dist/
#   tools/package_release.sh v1.03 out      # writes out/
#
# The output folder gets virtuanes_3ds.cia, .3dsx, .smdh and the title image,
# virtuanes_3ds-NAME-sdcard.zip (extract to the SD card root: the .3dsx,
# .smdh and title image go to /3ds/virtuanes_3ds/, and the title image also
# goes to the root, where the CIA version looks for it) and SHA256SUMS.txt.
#
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 NAME [OUTDIR]" >&2
    exit 2
fi
NAME="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mkdir -p "${2:-$ROOT/dist}" && cd "${2:-$ROOT/dist}" && pwd)"

FILES=(virtuanes_3ds.cia virtuanes_3ds.3dsx virtuanes_3ds.smdh virtuanes_3ds_top.png)
for f in "${FILES[@]}"; do
    [ -f "$ROOT/$f" ] || { echo "$f is missing; run tools/build.sh first" >&2; exit 1; }
done

ZIP="virtuanes_3ds-$NAME-sdcard.zip"
rm -f "$OUT/$ZIP" "$OUT/SHA256SUMS.txt"
for f in "${FILES[@]}"; do
    cp "$ROOT/$f" "$OUT/"
done

SD="$(mktemp -d)"
trap 'rm -rf "$SD"' EXIT
mkdir -p "$SD/3ds/virtuanes_3ds"
cp "$ROOT/virtuanes_3ds.3dsx" "$ROOT/virtuanes_3ds.smdh" "$ROOT/virtuanes_3ds_top.png" "$SD/3ds/virtuanes_3ds/"
cp "$ROOT/virtuanes_3ds_top.png" "$SD/"
(cd "$SD" && zip -qr "$OUT/$ZIP" 3ds virtuanes_3ds_top.png)

(cd "$OUT" && sha256sum "${FILES[@]}" "$ZIP" > SHA256SUMS.txt)
echo "packaged $NAME in $OUT:"
(cd "$OUT" && ls -l "${FILES[@]}" "$ZIP" SHA256SUMS.txt)
