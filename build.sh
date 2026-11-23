#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Build FM-1 CRAFT with fm1-base (base/, a git submodule). After CTL-1's build.sh (Copyright (C) 2026 Charles Vestal).
#   ./build.sh [--release X.Y] [--host]
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow (and libraqm, for the fonts)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-python3}"
BASE_REV=3ae2a65ddd02e15bf9ce925b5a7136d5d207ee25   # fm1-base, as CTL-1 0.x uses it
if [ ! -f base/tools/build.py ]; then             # fetched here, so the repository needs no submodule
    rm -rf base
    git clone -q https://github.com/charlesvestal/fm1-base.git base
    git -C base -c advice.detachedHead=false checkout -q "$BASE_REV"
    git -C base submodule update -q --init --recursive
fi
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
exec "$PY" base/tools/build.py app "$@"
