#!/bin/bash
# FM-1 CRAFT: build the firmware in WSL (Ubuntu). From Windows: double-click build_firmware.bat.
# The first run downloads the tools (a few minutes); the next ones only build.
# Result: out/craft.fwsc (and the simulator's screenshots, out/*.png) next to this script.
set -euo pipefail
SRC="$(cd "$(dirname "$0")" && pwd)"
WORK="$HOME/fm1-craft"
OUT="$SRC/out"
BASE_REV=3ae2a65ddd02e15bf9ce925b5a7136d5d207ee25     # fm1-base, as CTL-1 0.x uses it

echo "== 1/5 Ubuntu packages"
if ! dpkg -s git curl xz-utils build-essential python3-pil libraqm0 rsync >/dev/null 2>&1; then
    sudo apt-get update -qq
    sudo apt-get install -y -qq git curl xz-utils build-essential python3 python3-pil libraqm0 libfribidi0 rsync
fi

echo "== 2/5 source (fm1-base $BASE_REV)"
mkdir -p "$WORK"
rsync -a --delete --exclude base --exclude app/build --exclude out --exclude '*.bat' "$SRC/" "$WORK/"
if [ ! -d "$WORK/base/.git" ]; then
    git clone -q https://github.com/charlesvestal/fm1-base.git "$WORK/base"
fi
git -C "$WORK/base" cat-file -e "$BASE_REV^{commit}" 2>/dev/null || git -C "$WORK/base" fetch -q origin
git -C "$WORK/base" -c advice.detachedHead=false checkout -q "$BASE_REV"
git -C "$WORK/base" submodule update -q --init --recursive

echo "== 3/5 JieLi toolchain"
[ -e "$HOME/.jieli/toolchain" ] || sh "$WORK/base/tools/get_toolchain.sh"

echo "== 4/5 JieLi AC79 SDK"
[ -f "$HOME/fw-AC79_AIoT_SDK/cpu/wl82/tools/uboot.boot" ] || sh "$WORK/base/bsp/tools/get_sdk.sh" "$HOME/fw-AC79_AIoT_SDK"

echo "== 5/5 build"
cd "$WORK"
chmod +x build.sh tests/run_tests.sh
./build.sh
mkdir -p "$OUT"
cp app/build/craft.fwsc "$OUT/"
echo "   firmware: $OUT/craft.fwsc"

echo "== simulator (screenshots, a check of the screen)"
if ./build.sh --host > "$OUT/sim-build.log" 2>&1; then
    if app/build/host/craft_host app/tests/basic.txt app/build/host > "$OUT/sim.log" 2>&1; then
        echo "   simulator: ok"
    else
        echo "   simulator: some checks failed (out/sim.log)"
    fi
    cp app/build/host/*.png "$OUT/" 2>/dev/null || true
else
    echo "   simulator: did not build (out/sim-build.log)"
fi
echo
echo "Done: $OUT/craft.fwsc"
